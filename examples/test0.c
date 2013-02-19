
/* test0.c
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
  /* load mono WAV file at './samp2.wav' to sample 8 */
  ficus_loadfile("samp2.wav", 8);
  ficus_loadfile("samp3.wav", 16);
  ficus_loadfile("samp.wav", 24);	
  ficus_loadfile("samp2.wav", 32);
  ficus_loadfile("samp3.wav", 40);
  
  /* set playback of sample 0 to channel 0, 1=ON */
  ficus_setmixout(0,0,1);
  /* set playback of sample 8 to channel 1, 1=ON */
  ficus_setmixout(8,1,1);
  ficus_setmixout(16,0,1);
  ficus_setmixout(24,1,1);
  ficus_setmixout(32,0,1);
  ficus_setmixout(40,1,1);
  
  /* set capture of sample 0 to channel 0, 1=ON */
  ficus_setmixin(0, 0, 1);
  /* set capture of sample 8 to channel 1, 1=ON */
  ficus_setmixin(8, 1, 1);
  
  /* capture audio to sample 0 for 6 seconds */
  ficus_capture(0, 6);
  fprintf(stderr, "\nRECORD BANK 0 for 6 seconds.\n");
  
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

  fprintf(stderr, "\n6...");  
  sleep(1);

  fprintf(stderr, "\n7..."); 
  sleep(1);

  fprintf(stderr, "\n8...");  
  sleep(1);

  fprintf(stderr, "\n9..."); 
  sleep(1);

  fprintf(stderr, "\n10...");  
  sleep(1);

  fprintf(stderr, "\n11!!!\n\n");
  
  /* playback audio from sample 0 */
  ficus_playback(0);
  fprintf(stderr, "\nPLAY BANK 0\n");
  sleep(1);
  
  /* playback audio from sample 8 */
  ficus_playback(8);
  fprintf(stderr, "\nPLAY BANK 8\n");
  sleep(2);

  ficus_capture(8, 5);
  fprintf(stderr, "\nRECORD BANK 8 for 5 seconds.\n");

  ficus_playback(16);
  fprintf(stderr, "\nPLAY BANK 16\n");
  sleep(3);
  
  ficus_loop(0,0);
  fprintf(stderr, "UNLOOP BANK 0\n");
  
  ficus_playback(24);
  fprintf(stderr, "\nPLAY BANK 24\n");
  sleep(6);

  ficus_playback(32);
  fprintf(stderr, "\nPLAY BANK 32\n");
  sleep(4);

  ficus_playback(40);
  fprintf(stderr, "\nPLAY BANK 40\n");
  
  sleep(15);
  
  /* ficus teardown */
  ficus_clean();
  puts ("") ;
  
  return 0;
} /* main */
