/* fifo.h: Queue (first in, first out)
//
// Written and Copyright (C) 1997 by Michael J. Gourlay
//
// Provided as is.  No warrentees, express or implied.
*/

#ifndef _FIFO_H__INCLUDED_
#define _FIFO_H__INCLUDED_

typedef struct {
  void **queue;
  int length;
  int first;
  int count;
} FifoT;



void fifoPrint(FifoT *this);
FifoT * fifoNew(int length);
void fifoReset(FifoT *this);
void fifoFree(FifoT *this);
void fifoDestroy(FifoT *this);
int fifoAdd(FifoT *this, void *item);
void * fifoPop(FifoT *this);
void fifoTest(void);



#endif
