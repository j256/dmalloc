/*
 * system specific memory routines
 *
 * Copyright 1992 by Gray Watson and the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library (see COPYING-LIB); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * The author of the program may be contacted at gray.watson@antaire.com
 */

/*
 * These are the system/machine specific routines for allocating space on the
 * heap as well as reporting the current position of the heap.  This file at
 * some point should be full of #ifdef's and the like to describe all the
 * machine type's differences.
 */

#define HEAP_MAIN

#include "malloc.h"
#include "malloc_loc.h"

#include "chunk.h"
#include "compat.h"
#include "conf.h"
#include "error.h"
#include "heap.h"
#include "malloc_errno.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: heap.c,v 1.13 1992/11/14 21:45:28 gray Exp $";
#endif

/* exported variables */
EXPORT	char		*_heap_base = NULL;	/* base of our heap */
EXPORT	char		*_heap_last = NULL;	/* end of our heap */

#if HAVE_SBRK
IMPORT	char		*sbrk(int incr);	/* to extend the heap */
#define SBRK_ERROR	((char *)-1)
#endif

/*
 * function to get SIZE memory bytes from the end of the heap
 */
EXPORT	char	*_heap_alloc(unsigned int size)
{
  char		*ret = HEAP_ALLOC_ERROR;
  
#if HAVE_SBRK
  if ((ret = sbrk(size)) == SBRK_ERROR) {
    malloc_errno = MALLOC_ALLOC_FAILED;
    _malloc_perror("_heap_alloc");
    ret = HEAP_ALLOC_ERROR;
  }
  else {
    /* increment last pointer */
    _heap_last += size;
  }
#endif
  
  return ret;
}

/*
 * return a pointer to the current end of the heap
 */
EXPORT	char	*_heap_end(void)
{
  char		*ret = HEAP_ALLOC_ERROR;
  
#if HAVE_SBRK
  if ((ret = sbrk(0)) == SBRK_ERROR) {
    malloc_errno = MALLOC_ALLOC_FAILED;
    _malloc_perror("_heap_end");
    ret = HEAP_ALLOC_ERROR;
  }
#endif
  
  return ret;
}

/*
 * initialize heap pointers
 */
EXPORT	void	_heap_startup(void)
{
  _heap_base = _heap_last = _heap_end();
}

/*
 * align (by extending) _heap_base to BASE byte boundary
 */
EXPORT	char	*_heap_align_base(int base)
{
  int	diff;
  
  diff = (long)_heap_base % base;
  _heap_base += base - diff;
  
  return _heap_alloc(base - diff);
}
