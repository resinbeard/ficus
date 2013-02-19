/* test_rtqueue.c
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
#include <stdlib.h>
#include "rtqueue.h"

int
main()
{
  rtqueue_t *fifo[2];
  float fl;
  int nframes = 128000;
  float n = 0;
  int i, count = 0;

  fifo[0] = rtqueue_init(nframes);
  fifo[1] = rtqueue_init(nframes);

  /* i highly recommend using rtqueue within a
     multi-threaded application. */

  for (count=0; count < 2; count++)
    {
      
      for (i = 0; i < nframes; i++)
	{
	  n = random();
	  fprintf(stderr, "count: %d, (%d, %f) added to queue\n", count, i, n);
	  rtqueue_enq(fifo[count], n);
	}
      
      for (i = 0; i < nframes; i++)
	{
	  fl = rtqueue_deq(fifo[count]);
	  fprintf(stderr, "%d: (%d, %f)\n", count, i, fl);
	}
    }

  return 0;
}
