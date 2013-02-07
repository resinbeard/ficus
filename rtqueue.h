/* rtqueue.h
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

'rtqueue.h' is a simple FIFO linked list intended to hold 
JACK sample data for process()'ing.

Copyright 2013 murray foster */

#ifndef rtqueue_h__
#define rtqueue_h__

#include <jack/jack.h>

/* JACK sample size, set by JACK server */
const size_t smpl_size = sizeof (jack_default_audio_sample_t) ;

typedef struct queue
{
  int head;
  int tail;
  int recordlimit;
  int records;
  float *queue;
} rtqueue_t;


rtqueue_t *
rtqueue_init(int recordlimit)
{
  rtqueue_t *q = (rtqueue_t *) malloc(sizeof(rtqueue_t));
  q->queue = (float *) malloc(smpl_size * (recordlimit + 1));
  q->head = 0;
  q->tail = 0;
  q->recordlimit = recordlimit;
  return q;
}

int
rtqueue_numrecords(rtqueue_t *q)
{
  return q->records;
}

int
rtqueue_isfull(rtqueue_t *q)
{
  /* if queue is full, return 1 */
  if ((q->tail + 1) % (q->recordlimit + 1) == q->head)
    return 1;
  else
    return 0;
}

int
rtqueue_isempty(rtqueue_t *q)
{
  /* if queue is empty, return 1 */
  if (q->head == q->tail)
    return 1;
  else
    return 0;
}

int
rtqueue_enq(rtqueue_t *q, float data)
{
  /* if queue is full, return */
  while ((q->tail + 1) % (q->recordlimit + 1) == q->head)
    {}

  q->queue[q->tail] = data;
  q->tail = (q->tail + 1) % (q->recordlimit + 1);
  q->records+=1;

  return 0;
}

float
rtqueue_deq(rtqueue_t *q)
{
  float data;

  /* if queue is empty, return */
  while (q->head == q->tail)
    {}

  /* dequeue and return data at the head */
  data = q->queue[q->head];
  q->head = (q->head + 1) % (q->recordlimit + 1);
  q->records-=1;

  return data;
}

#endif
