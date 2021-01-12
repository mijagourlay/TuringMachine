/* fifo.c: Queue (first in, first out)
//
// Written and Copyright (C) 1997 by Michael J. Gourlay
//
// Provided as is.  No warrentees, express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>




#include "fifo.h"




void
fifoPrint(FifoT *this)
{
  printf("fifo: %p\n", this);
  printf("fifo queue  %p\n", this->queue);
  printf("fifo length %i\n", this->length);
  printf("fifo first  %i\n", this->first);
  printf("fifo count  %i\n", this->count);
}




FifoT *
fifoNew(int length)
{
  FifoT *this;

  if(length < 1) {
    fprintf(stderr, "fifoNew: fifo must be at least 1 item long\n");
    return NULL;
  }
  if((this = calloc(1, sizeof(FifoT))) == NULL) {
    return NULL;
  }

  if((this->queue = calloc(length+2, sizeof(void *))) == NULL) {
    fprintf(stderr, "fifoNew: out of memory\n");
    return this;
  }
  this->length = length;
  this->first = 0;
  this->count = 0;

  return this;
}




void
fifoReset(FifoT *this)
{
  this->first = 0;
  this->count = 0;
}




void
fifoFree(FifoT *this)
{
  if(this->queue != NULL) {
    free(this->queue);
    this->queue = NULL;
  }
}




void
fifoDestroy(FifoT *this)
{
  if(this != NULL) {
    fifoFree(this);
    free(this);
  }
}




/* NAME
//   fifoAdd: add an element to the end of a fifo
//
//
// DESCRIPTION
//   if the queue is full then
//     report a diagnostic
//     return error status
//   set queue element at index [first+count] to item
//   increment count
//
*/
int
fifoAdd(FifoT *this, void *item)
{
#if (VERBOSE >= 2)
  fifoPrint(this);
#endif

  if(this->count == this->length) {
    fprintf(stderr, "fifoAdd: queue is full\n");
    return 1;
  } else if((this->count < 0) || (this->count > this->length)) {
    fprintf(stderr, "fifoAdd: fifo count is impossible\n");
    abort();
  }

  this->queue[this->first + this->count] = item;

  this->count ++;

  return 0;
}




/* NAME
//   fifoPop: pop an element from the start of a fifo
//
// DESCRIPTION
//   if queue is empty then
//     report a diagnostic
//     return error status (NULL)
//   grab the value at queue element [first]
//   increment first, modulo length.
//   decrement count.
//   return the item.
//
// RETURN VALUE
//   Returns NULL if the queue is empty.  This is indistinguishable from
//   the case where NULL is actually in the queue, except that when the queue
//   is empty, a diagnostic is reported.
//
//   Returns the item at the start of the queue.
*/
void *
fifoPop(FifoT *this)
{
  void * const item = this->queue[this->first];

#if (VERBOSE >= 2)
  fifoPrint(this);
#endif

  if(0 == this->count) {
#ifdef VERBOSE
    fprintf(stdout, "fifoPop: fifo is empty\n");
#endif
    return NULL;
  } else if((this->count < 0) || (this->count > this->length)) {
    fprintf(stderr, "fifoPop: fifo count is impossible\n");
    abort();
  }

  this->first ++;
  this->first %= this->length;
  this->count --;

  return item;
}




#ifdef DEBUG
void
fifoTest(void)
{
  FifoT *fifo = fifoNew(3);

  fifoAdd(fifo, (void*)1);
  fifoAdd(fifo, (void*)2);
  fifoAdd(fifo, (void*)3);

  printf("should err: fifo full\n");
  fifoAdd(fifo, (void*)4);

  printf("fifo pop: %p\n", fifoPop(fifo));
  printf("fifo pop: %p\n", fifoPop(fifo));
  printf("fifo pop: %p\n", fifoPop(fifo));

  printf("should err: fifo empty\n");
  printf("fifo pop: %p\n", fifoPop(fifo));

  fifoAdd(fifo, (void*)1);
  printf("fifo pop: %p\n", fifoPop(fifo));
  fifoAdd(fifo, (void*)2);
  printf("fifo pop: %p\n", fifoPop(fifo));
  fifoAdd(fifo, (void*)3);
  printf("fifo pop: %p\n", fifoPop(fifo));

  printf("should err: fifo empty\n");
  printf("fifo pop: %p\n", fifoPop(fifo));

  fifoDestroy(fifo);
}
#endif
