
/* playspeedtest.c
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
#include "libficus.h"


int
main (int narg, char * args [])
{
  /* setup ficus,
     "libficus" -JACK client name,
     "capturedwavs/" -path to store captured wavs
     "test" -prefix under which to store captured wavs (
        Newly captured samples saved to:
           capturedwavs/test0.wav,capturedwavs/test1.wav,capturedwavs/test2.wav,etc.
     24 -bitdepth under which to capture new wavs to (8,16,24,or32)
  */
  if (ficus_setup("libficus", "capturedwavs/", "test", 24) == 1)
    {
      fprintf(stderr, "libficus setup failed.");
      return 1;
    }

  /* load mono WAV file at './samp.wav' to sample 0 */
  ficus_loadfile("samp.wav", 0);
  
  /* set playback of sample 0 to channel 0, 1=ON */
  ficus_setmixout(0,0,1);
    
  /* loop sample 0 to 1=ON */
  ficus_loop(0, 1);
  fprintf(stderr, "\nLOOP BANK 0\n");
  
  sleep(1);
  fprintf(stderr, "\n1...");
  sleep(1);

  fprintf(stderr, "\n2...");
  sleep(1);

  fprintf(stderr, "\n3...");
  sleep(1);
  
  fprintf(stderr, "\n4...");
  sleep(1);

  fprintf(stderr, "\n5..."); 
  sleep(1);

  fprintf(stderr, "\n11!!!\n\n");
  
  /* playback audio from sample 0 */
  ficus_playback(0);
  fprintf(stderr, "\nPLAY BANK 0\n");
  sleep(1);
  
  ficus_playback_speed(0,.25);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to .25\n");
  sleep(2);

  ficus_playback_speed(0, .5);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to .5.\n");
  sleep(2);

  ficus_playback_speed(0, .75);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to .75\n");
  sleep(3);
  
  ficus_playback_speed(0,1);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to 1.0\n");
  sleep(2);

  ficus_playback_speed(0,1.25);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to 1.25\n");
  sleep(2);

  ficus_playback_speed(0,1.5);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to 1.5\n");
  sleep(3);

  ficus_playback_speed(0,1.75);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to 1.75\n");
  sleep(2);

  ficus_playback_speed(0, 2);
  ficus_playback(0);
  fprintf(stderr, "\nspeed change to 2.0\n");
  sleep(2);

  ficus_playback_speed(0,-2);
  ficus_playback(0);
  fprintf(stderr, "\nsample 0 reversed.\n");
  sleep(2);

  ficus_playback_speed(0,-1.75);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to 1.75.\n");
  sleep(2);

  ficus_playback_speed(0,-1.5);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to 1.5.\n");
  sleep(2);
  
  ficus_playback_speed(0,-1.25);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to 1.25.\n");
  sleep(2);

  ficus_playback_speed(0,-1.0);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to 1.0.\n");
  sleep(2);
  
  ficus_playback_speed(0,-.75);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to .75.\n");
  sleep(2);
  
  ficus_playback_speed(0,-.5);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to .5.\n");
  sleep(2);
  
  ficus_playback_speed(0,-.25);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to .25.\n");
  sleep(2);
  
  ficus_playback_speed(0,0);
  ficus_playback(0);
  fprintf(stderr, "\nspeed changed to 0.");

  sleep(30);
  
  /* ficus teardown */
  ficus_clean();
  puts ("") ;
  
  return 0;
} /* main */
