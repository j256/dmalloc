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
 * heap as well as reporting the current position of the heap.
 */

#define MALLOC_DEBUG_DISABLE

#include "malloc_dbg.h"
#include "conf.h"

#include "chunk.h"
#include "compat.h"
#include "dbg_values.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "malloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: heap.c,v 1.24 1993/07/19 16:31:48 gray Exp $";
#endif

/* external routines */
#if HAVE_SBRK
IMPORT	char		*sbrk(int incr);	/* to extend the heap */
#define SBRK_ERROR	((char *)-1)		/* sbrk error code */
#endif

/* exported variables */
EXPORT	void		*_heap_base = NULL;	/* base of our heap */
EXPORT	void		*_heap_last = NULL;	/* end of our heap */

/*
 * function to get SIZE memory bytes from the end of the heap
 */
EXPORT	void	*_heap_alloc(const unsigned int size)
{
  void		*ret = HEAP_ALLOC_ERROR;
  
#if HAVE_SBRK
  ret = sbrk(size);
  if (ret == SBRK_ERROR) {
    malloc_errno = MALLOC_ALLOC_FAILED;
    _malloc_perror("_heap_alloc");
    ret = HEAP_ALLOC_ERROR;
  }
#endif
  
  /* did we not allocate any heap space? */
  if (ret == HEAP_ALLOC_ERROR) {
    if (BIT_IS_SET(_malloc_debug, DEBUG_CATCH_NULL)) {
      char	str[128];
      (void)sprintf(str, "\r\nmalloc_dbg: critical error: could not allocate %u more bytes from heap\r\n",
		    size);
      (void)write(STDERR, str, strlen(str));
      _malloc_die();
    }
  }
  
#if HAVE_SBRK
  if (ret != HEAP_ALLOC_ERROR) {
    /* did someone else extend the heap in our absence.  VERY BAD! */
    if (ret != _heap_last) {
      malloc_errno = MALLOC_ALLOC_NONLINEAR;
      _malloc_perror("_heap_alloc");
      ret = HEAP_ALLOC_ERROR;
    }
    else {
      /* increment last pointer */
      (char *)_heap_last += size;
    }
  }
#endif
  
  return ret;
}

/*
 * return a pointer to the current end of the heap
 */
EXPORT	void	*_heap_end(void)
{
  void		*ret = HEAP_ALLOC_ERROR;
  
#if HAVE_SBRK
  ret = sbrk(0);
  if (ret == SBRK_ERROR) {
    malloc_errno = MALLOC_ALLOC_FAILED;
    _malloc_perror("_heap_end");
    if (BIT_IS_SET(_malloc_debug, DEBUG_CATCH_NULL)) {
      char	str[128];
      (void)sprintf(str, "\r\nmalloc_dbg: critical error: could not find the end of the heap\r\n");
      (void)write(STDERR, str, strlen(str));
      _malloc_die();
    }
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
  _heap_base = _heap_end();
  _heap_last = _heap_base;
}

/*
 * align (by extending) _heap_base to BASE byte boundary
 */
EXPORT	void	*_heap_align_base(const long base)
{
  long	diff;
  
  diff = (long)_heap_base % base;
  (char *)_heap_base += base - diff;
  
  return _heap_alloc(base - diff);
}
