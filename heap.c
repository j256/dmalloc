/*
 * system specific memory routines
 *
 * Copyright 1991 by the Antaire Corporation
 *
 * Written by Gray Watson
 */

/*
 * These are the system/machine specific routines for allocating space on the
 * heap as well as reporting the current position of the heap.  This file at
 * some point should be full of #ifdef's and the like to describe all the
 * machine type's differences.
 */

#include <sys/types.h>				/* for caddr_t */

#define HEAP_MAIN

#include "useful.h"

#include "chunk.h"
#include "heap.h"
#include "malloc_err.h"

RCS_ID("$Id: heap.c,v 1.3 1992/09/04 20:46:39 gray Exp $");

/*
 * system functions
 */

#if defined(sparc) || defined(i386) || defined(mips)
IMPORT	caddr_t		sbrk(/* int incr */);	/* to extend the heap */
#endif

/* exported variables */
EXPORT	char		*_heap_base = NULL;	/* base of our heap */
EXPORT	char		*_heap_last = NULL;	/* end of our heap */

/* function to get SIZE memory bytes from the end of the heap */
EXPORT	char	*_heap_alloc(size)
  unsigned int	size;
{
  char		*ret = HEAP_ALLOC_ERROR;
  
#if defined(sparc) || defined(i386) || defined(mips)
  if ((ret = sbrk(size)) == HEAP_ALLOC_ERROR) {
    _malloc_errno = MALLOC_ALLOC_FAILED;
    _malloc_perror("_heap_alloc");
  }
  
  /* increment last pointer */
  _heap_last += size;
#endif
  
  return ret;
}

/* return a pointer to the current end of the heap */
EXPORT	char	*_heap_end()
{
  char		*ret = HEAP_ALLOC_ERROR;
  
#if defined(sparc) || defined(i386) || defined(mips)
  if ((ret = sbrk(0)) == HEAP_ALLOC_ERROR) {
    _malloc_errno = MALLOC_ALLOC_FAILED;
    _malloc_perror("_heap_end");
  }
#endif
  
  return ret;
}

/* initialize heap pointers */
EXPORT	void	_heap_startup()
{
  _heap_base = _heap_last = _heap_end();
}

/* align (by extending) _heap_base to BASE byte boundary */
EXPORT	char	*_heap_align_base(base)
  int	base;
{
  int	diff;
  
  diff = (long)_heap_base % base;
  _heap_base += base - diff;
  
  return _heap_alloc(base - diff);
}
