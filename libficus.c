/* libficus.c
This file is a part of 'ficus'
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Copyright 2013 murray foster 
mrafoster at gmail dawt com */

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <pthread.h>

#include <jack/jack.h>

#include <sndfile.h>

#include "libficus.h"
#include "rtqueue.h"

/* COMPILE-TIME DEFAULTS */
#define NUM_SAMPLES 48 /* number of sample banks */
#define NUM_CHANNELS 8  /* number of in/out channels */
#define IN_FRAMES 50000 /* size of input buffer in FRAMES,
			   change this size based on available
			   system memory */
#define OUT_FRAMES 50000 /* same as above but for output */

#include "config.h"

typedef struct _thread_info
{	
  pthread_t thread_id ;
  SNDFILE *sndfile ;
  jack_nframes_t pos ;
  unsigned int channel_out ;
  volatile int read_done ;
  volatile int play_done;
  volatile int bank_number;
  volatile int user_interrupt;
  volatile int kill;
  volatile int reverse;
  volatile float speedmult;
  volatile float rampup;
  volatile float rampdown;
} thread_info_t ;

typedef struct _thread_info_in
{
  SNDFILE *sndfile;
  jack_nframes_t duration;
  jack_nframes_t rb_size;
  unsigned int channels;
  int bitdepth;
  char *path;
  volatile int can_capture;
  volatile int status;
  volatile int bank_number;
  volatile int total_captured;
  volatile int can_process;
  volatile int kill;
}thread_info_in_t ;

pthread_t capture_thread_id; 

const size_t sample_size = sizeof (jack_default_audio_sample_t) ;

static jack_port_t **output_port ;
static jack_port_t **input_port ;

static jack_default_audio_sample_t ** outs ;
static jack_default_audio_sample_t ** ins ;

SNDFILE *sndfile[NUM_SAMPLES] ;
SNDFILE *sndfile_in[NUM_SAMPLES] ;

SF_INFO sndfileinfo[NUM_SAMPLES] ;
SF_INFO sndfileinfo_in[NUM_SAMPLES];

/* allows disk_thread() to signal process() that there's data to read */
int samples_can_process[NUM_SAMPLES] = {0};

/* active file record for in/out of every sample,
   [0] is playback, [1] is capture */
int active_file_record[2][NUM_SAMPLES] = {{0}};

jack_client_t *client=NULL;
thread_info_t info[NUM_SAMPLES] ;
thread_info_in_t info_in[NUM_SAMPLES] ;

int jack_sr;

int capture_mix[NUM_SAMPLES][NUM_CHANNELS] = {{0}};
int playback_mix[NUM_SAMPLES][NUM_CHANNELS] = {{0}};
int loop_state[NUM_SAMPLES] = {0};

/* for recording */
long overruns = 0;

/* lock-free playback/capture data queues */
rtqueue_t *fifo_out[NUM_SAMPLES];
rtqueue_t *fifo_in[NUM_CHANNELS];

int fifo_out_clear_amount[NUM_SAMPLES]={0};
int fifo_out_clear_sig[NUM_SAMPLES]={0};
pthread_t fifo_out_clear_thread_id; 

void *
clear_fifo_out (int bank, int numrecords)
{
  int n=0;
  for(n=0;n<numrecords;n++)
    {
      if(rtqueue_isempty(fifo_out[bank]))
	break;
      rtqueue_deq(fifo_out[bank]);
    }
  fifo_out_clear_amount[bank]=0;
  return;
} /* clear_fifo_out */

void *
clear_fifo_out_thread ()
{
  int x;
  while(1)
    {
      for(x=0;x<NUM_SAMPLES;x++)
	if(fifo_out_clear_sig[x])
	  {
	    samples_can_process[x]=0;
	    clear_fifo_out(x,rtqueue_numrecords(fifo_out[x]));
	    fifo_out_clear_sig[x]=0;
	    info[x].user_interrupt=0;
	  }
    }
}

static int
process (jack_nframes_t nframes, void * arg)
{

  /* this is the function to be registered as 
     the JACK process() callback.  

     for every frame in this period, we queue channel data to
     JACK's input ports and send queued channel data
     thru JACK's output ports for every soundfile 
     loaded into the system.
  */

  float sample; 
  unsigned i, n, x; 
  int sample_count = 0;

  /* allocate all output buffers */
  for(i = 0; i < NUM_CHANNELS; i++)
    {
      outs [i] = jack_port_get_buffer (output_port[i], nframes) ;
      ins [i] = jack_port_get_buffer (input_port[i], nframes);
      memset(outs[i], 0, nframes * sample_size);
    }      

  for ( i = 0; i < nframes; i++)
    {
      /* Queue incoming audio in case it needs to go to the disk */
      for (n = 0; n < NUM_CHANNELS; n++)
	rtqueue_enq(fifo_in[n], ins[n][i]);

      for (sample_count = 0; sample_count < NUM_SAMPLES; sample_count++)
	{
	  sample = 0;	
	  
	  if( info[sample_count].user_interrupt )
	    fifo_out_clear_sig[sample_count]=1;

	  /* Has this queue run out of audio to process? */
	  if ( rtqueue_isempty(fifo_out[sample_count]) 
	       && samples_can_process[sample_count])
	    {
	      
	      /* If yes, zero out this sample's buffers for this frame,
		 (do we need to do this??) */ 
	      sample = 0;
	      samples_can_process[sample_count] = 0;
	    }
	  else
	    /* The buffer isn't empty, is there audio to process? */
	    if (samples_can_process[sample_count] && (info[sample_count].user_interrupt==0))
	      {
		/* If so, dequeue a frame for this sample */
		sample = rtqueue_deq(fifo_out[sample_count]);
	      }
	  
	  for(n = 0; n < NUM_CHANNELS; n++)
	    {
	      /* Check to make sure we can output thru this channel */
	      if(playback_mix[sample_count][n] == 1)
		/* If we can, set buffer to 0 for this frame*/
		outs[n][i] += sample;
	    }
	}
    }
  
  return 0 ;
} /* process */
//
void * 
disk_thread_in (void *arg)
{
  /* disk_thread_in() is the thread for capturing JACK
     audio data and writing to it to disk.

     there is only 1 instance of these run for every 
     libficus (JACK) client.
  */

  float channelbuf[NUM_CHANNELS];
  float *framebuf = (float *) malloc (sizeof(4));

  int i, c, n = 0;
  int write_count;
  while (1)
    {
      for (i=0; i < NUM_CHANNELS; i++)
	channelbuf[i] = rtqueue_deq(fifo_in[i]);

      /* number of banks we could possibly record to */
      for (c=0; c < NUM_SAMPLES; c++)
	{
	  /* if the sample is selected to write to disk,
	     write the buffered sample */
	  if (info_in[c].can_capture == 1)
	    {
	      /* let the world know that we're writing captured audio data */
	      active_file_record[1][c] = 1;
	      info_in[c].status = 0;
	      write_count = 0;
	      framebuf[0] = (float) 0;
	      
	      for (n=0; n < NUM_CHANNELS; n++)
		if ( capture_mix[c][n] == 1 )
		    framebuf[0] += channelbuf[n];
	      
	      write_count = sf_writef_float (info_in[c].sndfile, framebuf, 1);	      
	      
	      if (write_count != 1)
		{
		  char errstr[256];
		  sf_error_str (0, errstr, sizeof (errstr) - 1);

		  info_in[c].status = EIO; /* write failed */ 
		  info_in[c].can_capture = 0;
		  active_file_record[1][c] = 0;
		}
	      
	      /* if the write was successful, we keep track of how much 
		 we actually did write vs. how much we expected to write */
	      if (info_in[c].status != EIO)
		{
		  info_in[c].total_captured += write_count;
		  
		  /* stop writing to this soundfile if we've written enough data */
		  if ( (info_in[c].total_captured >= info_in[c].duration) || (info_in[c].kill == 1) )
		    {
		      active_file_record[1][c] = 0;
		      info_in[c].can_capture = 0;
		      info_in[c].total_captured = 0;
		      info_in[c].kill = 0;
		    }
		}
	    }
	}

      /* once in a while, check our jack overruns */
      if (overruns > 0)
	{
	  info_in[c].status = EPIPE;
	}
    }
  
  return 0;
} /* disk_thread_in */

int random_in_range (unsigned int min, unsigned int max)
{
  int base_random = rand();
  if (RAND_MAX == base_random) return random_in_range(min, max);
  int range       = max - min,
    remainder   = RAND_MAX % range,
    bucket      = RAND_MAX / range;
  if (base_random < RAND_MAX - remainder) {
    return min + base_random/bucket;
  } else {
    return random_in_range (min, max);
  }
}

static void *
disk_thread (void *arg)
{

  /* disk_thread() is the thread responsible for 
     playback of JACK audio data.

     for every call to ficus_playback(), an
     instance of this thread is started for the
     given sample bank. the hope is that the 
     operating system can allocate resources
     for these threads better than i can.
  */

  int sample_num = (int ) arg ;

  sf_count_t read_frames ;
  float buf_out[1];
  
  int count = 0;
  int count1 = 0;
  int count2 = 0;
  float ramplength = 0;
  float rampstart = 0;
  float volume_factor = 0;

  float slow_speedmult=0.0;
 
  do
    {
      do
	{
	  /* let the world know that we are currently
	     processing this soundfile */
	  active_file_record[0][info[sample_num].bank_number] = 1;
	  
	  /* seek to beginning of file, if playback is reversed seek to the end */
	  if(info[sample_num].reverse)
	    {
	      sf_seek(info[sample_num].sndfile, sndfileinfo[sample_num].frames-1, SEEK_SET);
	      info[sample_num].pos=sndfileinfo[sample_num].frames-1; 
	    }
	  else
	    {
	      sf_seek(info[sample_num].sndfile, 0, SEEK_SET);
	      info[sample_num].pos=0;
	    }
	  while (1)
	    { 
	      read_frames=0;
	      /* kill playback for this sample? */
	      /* we have to do this again so that loop_state
		 doesn't keep us in this do-while{} */
	      if( info[sample_num].kill == 1 )
		{
		  active_file_record[0][info[sample_num].bank_number] = 0;
		  break;
		}  
	      
	      /* if the playback queue is NOT full */
	      if ( (rtqueue_isfull(fifo_out[info[sample_num].bank_number]) == 0) )
		{
		  /* read ONE frame from our soundfile (4kb assumedly),
		     sometimes it's good to be a slowpoke! */
		  read_frames = sf_readf_float (info[sample_num].sndfile, buf_out, 1);


		  /*
		    NORMAL PLAYBACK OR FASTER
		   */
		  /* if this sample's playback is NOT reversed.. */
		  if(!info[sample_num].reverse)
		    {
		      /* if this sample's speed multiplier is greater than 1 */
		      if(info[sample_num].speedmult>1)
			{
			  /* advance frame position by the speed multiplier */
			  info[sample_num].pos+=(int)(info[sample_num].speedmult);
			  sf_seek(info[sample_num].sndfile,info[sample_num].pos,SEEK_SET);
			  /* account for tenths and hundreths resolution of GREATER speed multiple */
			  if(random_in_range(0,99)<((int)((info[sample_num].speedmult-(int)(info[sample_num].speedmult))*100)))
			    {
			      info[sample_num].pos++;
			      sf_seek(info[sample_num].sndfile,info[sample_num].pos,SEEK_SET);
			    }
			}
		      else
			{
			  /* if speed multiplier is 1 or slower than normal playback, advance position by 1 */
			  info[sample_num].pos++;
			  sf_seek(info[sample_num].sndfile,info[sample_num].pos,SEEK_SET);
			}
		    }
		  /* if playback is reversed.. */
		  else
		    {
		      /* if speed multiplier is greater than 1 */
		      if(info[sample_num].speedmult>1)
			{
			  /* if we're about to fall off the edge of out sample */
			  if( (info[sample_num].pos-info[sample_num].speedmult-1)<0)
			    /* reset frames to end of file */
			      info[sample_num].pos=sndfileinfo[sample_num].frames-1;
			  else
			    /* otherwise rewind our position in the soundfile */
			    info[sample_num].pos-=(int)info[sample_num].speedmult;

			  /* account for tenths and hundreths resolution of GREATER speed multiple */
			  if(random_in_range(0,99)<((int)((info[sample_num].speedmult-(int)(info[sample_num].speedmult))*100)))
			    info[sample_num].pos-=1;
			}
		      else
			/* if playback speed is normal, or slower, decrement position by 1 */
			info[sample_num].pos-=1;
		    }
		  /* finally, seek to new position in the soundfile */
		  sf_seek(info[sample_num].sndfile,info[sample_num].pos,SEEK_SET);
					  
	      
		  /* if no frames read, we assume the end of file.. */
		  if (read_frames == 0)
		    if(!info[sample_num].user_interrupt)
		      break ;
		  
		  /* fill playback queue with data  */
		  for (count = 0; count < read_frames; count++)
		    {
		      /*
			AMPLITUDE RAMPING
		      */
		      /* if this sample has a ramp DOWN envelope.. */
		      if( info[sample_num].rampup )
			{
			  /* figure out the length of the ramp */
			  ramplength=sndfileinfo[sample_num].frames * info[sample_num].rampup;

			  /* if the ramp is still in progress, calculate new value of frame */
			  if( info[sample_num].pos<ramplength )
			    {
			      volume_factor=info[sample_num].pos/ramplength;
			      buf_out[count]*=volume_factor;
			    }
			}
		      /* if this sample has a ramp UP envelope.. */
		      if( info[sample_num].rampdown )
			{
			  /* figure out length and start of this envelope */
			  ramplength=sndfileinfo[sample_num].frames * info[sample_num].rampdown;
			  rampstart=sndfileinfo[sample_num].frames-ramplength;
			  /* if this ramp has started.. */
			  if( info[sample_num].pos>rampstart )
			    {
			      /* calculate new value of frame */
			      volume_factor=(sndfileinfo[sample_num].frames-info[sample_num].pos)/ramplength;
			      buf_out[count]*=volume_factor;
			    }
			}

		      /*
			SLOW OR NORMAL PLAYBACK
		      */
		      /* if speed multiplier is less than 1.0.. */
		      if(info[sample_num].speedmult<1)
			{
			  /* coarse calculation of frames to duplicate */
			  slow_speedmult=(1-info[sample_num].speedmult)*4;
			  if(slow_speedmult>1)
			    for(count1=0; count1<(int)slow_speedmult;count1++)
			      rtqueue_enq(fifo_out[info[sample_num].bank_number],buf_out[count]);
			  else
			      rtqueue_enq(fifo_out[info[sample_num].bank_number],buf_out[count]);
			  /* fine calculation of frames to duplicate */
			  if( slow_speedmult-(int)slow_speedmult>0)
			    if(random_in_range(0,99)<(1-info[sample_num].speedmult)*100)
			      rtqueue_enq(fifo_out[info[sample_num].bank_number],buf_out[count]);
			}
		      else
			/* otherwise, queue a sample like normal */
			rtqueue_enq(fifo_out[info[sample_num].bank_number], buf_out[count]);
		    }
		  /* signal process thread there is data to process 
		     from this sample bank */
		  samples_can_process[info[sample_num].bank_number] = 1 ;
		}
	    } 
	  if( info[sample_num].kill )
	    break; 
	}
      while( (loop_state[info[sample_num].bank_number]==1) );

      /* hang here until this sample finished playing */
      while( samples_can_process[sample_num] )
	{
	  if(info[sample_num].user_interrupt)
	    break; 
	}
      if( info[sample_num].kill )
	{
	  info[sample_num].kill = 0;
	  break;
	}

    }
  while( info[sample_num].user_interrupt);

  /* done with this sample bank! */
  active_file_record[0][sample_num] = 0;

  return 0 ;
} /* disk_thread */

int
init_recbank (thread_info_in_t *info, int banknumber, int bit_depth, char *path)
{
  SF_INFO sndfile_info;
  int short_mask;

  info->path = path;

  info->channels = 1;
  info->can_process = 0;

  info->duration = 0;
  info->bitdepth = 0;

  info->total_captured = 0;
  info->kill = 0;
  
  switch (info->bitdepth)
    {
    case 8:
      short_mask = SF_FORMAT_PCM_U8;
      break;
    case 16:
      short_mask = SF_FORMAT_PCM_16;
      break;
    case 24:
      short_mask = SF_FORMAT_PCM_24;
      break;
    case 32:
      short_mask = SF_FORMAT_PCM_32;
    default:
      short_mask = SF_FORMAT_PCM_32;
    }

  sndfile_info.format = SF_FORMAT_WAV|short_mask;
  sndfile_info.channels = info->channels;
  sndfile_info.samplerate = jack_get_sample_rate (client);

  sndfileinfo_in[banknumber] = sndfile_info;

  return 0;
} /* init_recbank */

char
*build_path(char *path, char *prefix, int bank_number)
{
  char full_path[100];
  char *return_path;
  size_t len;

  sprintf(full_path, "%s/%s%d.wav", path, prefix, bank_number);

  /* Transform constant array to non-constant array */
  len = strlen(full_path) + 1;
  return_path = malloc(len);

  /* We can only do this because full_path is already null-terminated */
  strcpy(return_path, full_path);

  return return_path;
} /* build_path */

int
setup_recbanks(char *path, char *prefix, int bit_depth)
{
  int c;
  char *filepath;
  
  /* Build filename and setup the soundfile for capturing */
  for (c=0; c < NUM_SAMPLES; c++)
    {
      filepath = build_path(path, prefix, c);
      init_recbank(&info_in[c], c, bit_depth, filepath);
    }
  free(filepath);

  return 0;
} /* setup_recbanks */

int
ficus_capture ( int banknumber, int seconds)
{
  info_in[banknumber].duration = seconds;

  /* Try to create a soundfile for opening */
  if ((info_in[banknumber].sndfile = sf_open (info_in[banknumber].path, SFM_WRITE, &sndfileinfo_in[banknumber])) == NULL)
    {
      char errstr[256];
      sf_error_str (0, errstr, sizeof (errstr) - 1);
      
      return 1;
    }

  /* If duration is '0', we need to set a duration
     that will record for as long as we have enough
     disk/buffer space. */
  if (info_in[banknumber].duration == 0)
    info_in[banknumber].duration = JACK_MAX_FRAMES;
  else
    info_in[banknumber].duration *= jack_sr;

  info_in[banknumber].can_capture = 1;
  info_in[banknumber].kill = 0;

  return 0;
} /* ficus_capture */

int
ficus_capturef ( int banknumber, int captureframes)
{
  info_in[banknumber].duration = captureframes;

  /* Try to create a soundfile for opening */
  if ((info_in[banknumber].sndfile = sf_open (info_in[banknumber].path, SFM_WRITE, &sndfileinfo_in[banknumber])) == NULL)
    {
      char errstr[256];
      sf_error_str (0, errstr, sizeof (errstr) - 1);
 
      return 1;
    }

  /* If duration is '0', we need to set a duration
     that will record for as long as we have enough
     disk/buffer space. */
  if (info_in[banknumber].duration == 0)
    info_in[banknumber].duration = JACK_MAX_FRAMES;
  else
    info_in[banknumber].duration = captureframes;

  info_in[banknumber].can_capture = 1;
  info_in[banknumber].kill = 0;

  return 0;
} /* ficus_capture */

int
ficus_durationf_out( int bank_number )
{
  int duration=0;
  /* return the duration of sndfile in frames */
  duration=sndfileinfo[bank_number].frames;
  return duration;
} /* ficus_durationf */

int
ficus_durationf_in( int bank_number )
{
  int duration=0;
  /* return the duration of sndfile in frames */
  duration=sndfileinfo_in[bank_number].frames;
  return duration;
} /* ficus_durationf */

void
ficus_playback_speed(int bank_number, float speed)
{
  if(speed<0)
    {
      info[bank_number].reverse=1;
      info[bank_number].speedmult=speed*(-1);
    }
  else
    info[bank_number].speedmult=speed;
} /* ficus_playback_reverse */

void
ficus_playback(int bank_number)
{

  /* if a 'play' sample thread is already rolling, seek back the file */
  if(active_file_record[0][bank_number])
    {
      info[bank_number].user_interrupt = 1 ;
      if(info[bank_number].reverse)
	{
	  info[bank_number].pos=sndfileinfo[bank_number].frames-1;
	  sf_seek  (info[bank_number].sndfile, info[bank_number].pos, SEEK_SET) ;
	}
      else
	{
	  info[bank_number].pos=0;
	  sf_seek  (info[bank_number].sndfile, 0, SEEK_SET) ;
	}
    }
  else
    {
      /* if not, start the disk thread for this bank */
      pthread_create (&info[bank_number].thread_id, NULL, disk_thread, bank_number);
      info[bank_number].kill = 0;
      pthread_detach(info[bank_number].thread_id);
    }
} /* ficus_playback */

void 
ficus_playback_rampup(int bank_number, float rampduration)
{
  info[bank_number].rampup=rampduration;
} /* ficus_playback_rampup */

void 
ficus_playback_rampdown(int bank_number, float rampduration)
{
  info[bank_number].rampdown=rampduration;
} /* ficus_playback_rampdown */

int
ficus_loadfile(char *path, int bank_number)
{
  /* Open the soundfile. */
  sndfileinfo[bank_number].format = 0 ;
  sndfile[bank_number] = sf_open (path, SFM_READ, &sndfileinfo[bank_number]) ;
  
  /* Try to see if sf_open() was successful, otherwise return */
  if (sndfile[bank_number] == NULL)
    {
      return 1;
    };
  
  /* Warn the user that the soundfile being loaded doesn't have
     the same samplerate as JACK */
  if (sndfileinfo[bank_number].samplerate != jack_sr)
    {
      /* logging */
      printf("soundfile samplerate doesn not match JACK server.\n");
    }

  /* Init the thread info struct. */
  memset (&info[bank_number], 0, sizeof (info[bank_number])) ; 
  info[bank_number].read_done = 0 ;
  info[bank_number].play_done = 0;
  info[bank_number].sndfile = sndfile[bank_number] ;
  info[bank_number].pos = 0 ;
  info[bank_number].bank_number = bank_number;
  info[bank_number].user_interrupt = 0;
  info[bank_number].kill = 0;  
  info[bank_number].reverse = 0;
  info[bank_number].speedmult = 1.0;
  info[bank_number].rampup = 0.0;
  info[bank_number].rampdown = 0.0;
  return 0;
} /* ficus_loadfile */

int
ficus_setmixin(int numsample, int channel, int state)
{
  /* numsample - sample number */
  /* channel - channel */
  /* state - state of specified channel */

  capture_mix[numsample][channel] = state;
 
  return 0;
} /* ficus_setmixin */

int
ficus_setmixout(int numsample, int channel, int state)
{
  /* numsample - sample number */
  /* channel - channel */
  /* state - state of specified channel */

  playback_mix[numsample][channel] = state;
  
  return 0;
} /* ficus_setmixout */

int
ficus_loop(int bank_number, int state)
{
 
  loop_state[bank_number] = state;
  
  return 0;
} /* loop_bank */

int
ficus_iscapturing(int numsample)
{
  return active_file_record[1][numsample];
} /* ficus_iscapturing */


int
ficus_isplaying(int numsample)
{
  return active_file_record[0][numsample];
} /* ficus_isplaying */

int
ficus_islooping(int numsample)
{
  return loop_state[numsample]; 
} /* ficus_islooping */

int
ficus_killcapture (int banknumber)
{
  if( active_file_record[1] == 0 )
    return 1;
  
  if ( info_in[banknumber].duration != JACK_MAX_FRAMES )
      info_in[banknumber].total_captured += info_in[banknumber].duration;
  else
    info_in[banknumber].kill = 1;

  return 0;
} /* ficus_killcapture */


int
ficus_killplayback (int banknumber)
{
  if( active_file_record[0] == 0 )
    return 1;
  
  /* Set this sample's playback to die. */
  info[banknumber].user_interrupt = 1;
  info[banknumber].kill = 1;

  return 0;
} /* ficus_killplayback */

static void
jack_shutdown (void *arg)
{
  (void) arg ;
  exit (1) ;
} /* jack_shutdown */

int
jack_setup(char *client_name)
{
  /* create jack client */
  if ((client = jack_client_open(client_name, JackNullOption,NULL)) == 0)
    {
      fprintf (stderr, "Jack server not running?\n") ;
      return 1 ;
    } ;

  /* store jack server's samplerate */
  jack_sr = jack_get_sample_rate (client) ;

  return 0;
} /* jack_setup */

int
fifo_setup()
{
  int count = 0;
  
  for( count = 0; count < NUM_SAMPLES; count++)
    fifo_out[count] = rtqueue_init(OUT_FRAMES);

  for( count = 0; count < NUM_CHANNELS; count++)
    fifo_in[count] = rtqueue_init(IN_FRAMES);

  return 0;
} /* fifo_setup */

int
set_callbacks()
{
  /* Set up callbacks. */
  jack_set_process_callback (client, process, info) ;
  jack_on_shutdown (client, jack_shutdown, 0) ;
  return 0;
} /* set_callbacks */

int
activate_client()
{
  /* Activatexo client. */
  if (jack_activate (client))
    {	
      fprintf (stderr, "Cannot activate client.\n") ;
      return 1 ;
    }
  return 0;
} /* activate_client */

void
allocate_ports(int channels, int channels_in)
{
  int i = 0;
  char name [16];
  /* allocate output ports */
  output_port = calloc (channels, sizeof (jack_port_t *)) ;
  outs = calloc (channels, sizeof (jack_default_audio_sample_t *)) ;
  for (i = 0 ; i < channels; i++)
    {     
      snprintf (name, sizeof (name), "out_%d", i + 1) ;
      output_port [i] = jack_port_register (client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0) ;
    }
  
  /* allocate input ports */
  size_t in_size = channels_in * sizeof (jack_default_audio_sample_t*);
  input_port = (jack_port_t **) malloc (sizeof (jack_port_t *) * channels_in);
  ins = (jack_default_audio_sample_t **) malloc (in_size);
  memset(ins, 0, in_size);
  
  for( i = 0; i < channels_in; i++)
    {
      
      snprintf( name, sizeof(name), "in_%d", i + 1);
      input_port[i] = jack_port_register(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    }
} /* allocate_ports */

void
connect_channels(int channels_out, int channels_in)
{
  int i = 0;
  char name [64] ;
  /* auto connect all 'out' channels to 'system' (JACK-managed soundcard) */
  for (i = 0 ; i < channels_out ; i++)
    {       
      
      snprintf (name, sizeof (name), "system:playback_%d", i + 1) ;
      
      if (jack_connect (client, jack_port_name (output_port [i]), name))
	{
	  /* logging */
	  snprintf (stdout, 100, "warning, cannot connect output port %d (%s)\n", i, name) ;;
	}
    }
  
  /* auto connect all 'in' channels to 'system' */
  for ( i = 0; i < channels_in ; i++)
    {
      snprintf (name, sizeof (name), "system:capture_%d", i + 1) ;
      
      if (jack_connect (client, name, jack_port_name (input_port [i])))
	{
	  /* logging */
	  snprintf(stdout, 100, "warning cannot connect input port %d (%s).\n", i, name) ;
	}
    }
} /* connect_channels */

int
ficus_jackmonitor(int channel, int state)
{
  char inport[65];
  char outport[65];

  snprintf(inport, 65, "system:capture_%d", channel+1);
  snprintf(outport, 65, "system:playback_%d", channel+1);

  if( !state )
    {
      if(jack_disconnect(client, inport, outport))
	fprintf(stderr, "candor: failed to disconnect capture port%d and playback port%d\n", channel+1, channel+1);
    }
  else
    if(jack_connect(client, inport, outport))
      fprintf(stderr, "candor: failed to connect capture port%d and playback port%d\n", channel+1, channel+1);

  return 0;
      
} /* ficus_jackmonitor */

int
ficus_setup(char *client_name, char *path, char *prefix, int bit_depth)
{

  /* General Set-Up Method */
  jack_setup(client_name);
  
  fifo_setup();
  set_callbacks();
 
  if (activate_client() == 1)
    return 1;

  allocate_ports(NUM_CHANNELS, NUM_CHANNELS);
  connect_channels(NUM_CHANNELS, NUM_CHANNELS);

  setup_recbanks(path, prefix, bit_depth);
  
  pthread_create (&capture_thread_id, NULL, disk_thread_in, NULL);
  pthread_create (&fifo_out_clear_thread_id, NULL, clear_fifo_out_thread, NULL);

  return 0;

} /* ficus_setup */

void
ficus_clean()
{
  int i = 0;
  jack_client_close (client) ;

  for(i=0; i < NUM_SAMPLES;i++)
    {
      sf_close (sndfile[i]) ;
      sf_close (sndfile_in[i]);
    }

  free (ins) ;
  free (outs) ;
  free (output_port) ;
  free (input_port) ;


} /* ficus_clean */
