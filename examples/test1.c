/* test1.c
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
      fprintf(stderr, "ficus setup failed.");
      return 1;
    }
  
  ficus_loop(0, 1);
  fprintf(stderr, "\nLOOP BANK 0\n");
  
  ficus_capture(0, 6);
  fprintf(stderr, "\nRECORD BANK 0 for 6 seconds\n");

  ficus_capture(8, 5);
  fprintf(stderr, "\nRECORD BANK 8 for 5 seconds\n");

  sleep(6);

  ficus_loadfile("capturedwavs/test0.wav", 0);
  fprintf(stderr, "\nLOAD FILE 'test0.wav' TO BANK '0'\n");

  ficus_playback(0);
  fprintf(stderr, "\nPLAY BANK 0\n");

  /* set playback of sample 0 to channel 0, 1=ON */
  ficus_setmixout(0, 1, 1);
  sleep(1);
  /* set playback of sample 0 to channel 0, 1=OFF */
  ficus_setmixout(0, 0, 0);
  sleep(1);
  ficus_setmixout(0, 0, 1);
  sleep(1);
  ficus_setmixout(0, 0, 0);
  sleep(1);
  ficus_setmixout(0, 0, 1);
  sleep(1);
  ficus_setmixout(0, 0, 0);

  /* set playback of sample 0 to channel 1, 1=ON */
  ficus_setmixout(0, 1, 1);
  sleep(1);
  /* set playback of sample 0 to channel 1, 1=OFF */
  ficus_setmixout(0, 1, 0);
  sleep(1);
  ficus_setmixout(0, 1, 1);
  sleep(1);
  ficus_setmixout(0, 1, 0);
  sleep(1);
  ficus_setmixout(0, 1, 1);
  sleep(1);
  ficus_setmixout(0, 1, 0);

  sleep(8);

  /* ficus teardown */
  ficus_clean();

  return 0;
} /* main */
