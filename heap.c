/*
 * system specific memory routines
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * NON-COMMERCIAL purpose and without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies, and that the name of Gray Watson not be used in
 * advertising or publicity pertaining to distribution of the document
 * or software without specific, written prior permission.
 *
 * Please see the PERMISSIONS file or contact the author for information
 * about commercial licenses.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted at gray.watson@letters.com
 */

/*
 * These are the system/machine specific routines for allocating space on the
 * heap as well as reporting the current position of the heap.
 */

#include <stdio.h>				/* for sprintf */

#if HAVE_UNISTD_H
# include <unistd.h>				/* for write */
#endif

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "chunk.h"
#include "compat.h"
#include "debug_val.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
static	char	*rcs_id =
  "$Id: heap.c,v 1.46 1998/09/19 00:11:38 gray Exp $";
#endif

#define SBRK_ERROR	((char *)-1)		/* sbrk error code */

/* exported variables */
void	*_heap_base = NULL;			/* base of our heap */
void	*_heap_last = NULL;			/* end of our heap */

/****************************** local functions ******************************/

/*
 * Increment the heap INCR bytes with sbrk
 */
static	void	*heap_extend(const int incr)
{
  void	*ret = SBRK_ERROR;
  
#if HAVE_SBRK
  ret = sbrk(incr);
  if (ret == SBRK_ERROR) {
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CATCH_NULL)) {
      char	str[128];
      (void)sprintf(str, "\r\ndmalloc: critical error: could not extend heap %u more bytes\r\n", incr);
      (void)write(STDERR, str, strlen(str));
      _dmalloc_die(FALSE);
    }
    dmalloc_errno = ERROR_ALLOC_FAILED;
    dmalloc_error("heap_extend");
  }
#endif
  
  return ret;
}

/**************************** exported functions *****************************/

/*
 * Initialize heap pointers.  returns [NO]ERROR
 */
int	_heap_startup(void)
{
  long		diff;
  
  _heap_base = heap_extend(0);
  if (_heap_base == SBRK_ERROR)
    return ERROR;
  
  /* align the heap-base */
  diff = BLOCK_SIZE - ((long)_heap_base % BLOCK_SIZE);
  if (diff == BLOCK_SIZE)
    diff = 0;
  
  if (diff > 0) {
    if (heap_extend(diff) == SBRK_ERROR)
      return ERROR;
    _heap_base = (char *)HEAP_INCR(_heap_base, diff);
  }
  
  _heap_last = _heap_base;
  
  return NOERROR;
}

/*
 * Function to get SIZE memory bytes from the end of the heap.  it
 * returns a pointer to any external blocks in EXTERN_P and the number
 * of blocks in EXTERN_CP.
 */
void	*_heap_alloc(const unsigned int size, void **extern_p,
		     int *extern_cp)
{
  void		*ret;
  long		diff;
  int		blockn;
  
  ret = heap_extend(size);
  if (ret == SBRK_ERROR)
    return HEAP_ALLOC_ERROR;
  
  /* is the heap linear? */
  if (ret == _heap_last) {
    _heap_last = HEAP_INCR(ret, size);
    if (extern_p != NULL)
      *extern_p = NULL;
    if (extern_cp != NULL)
      *extern_cp = 0;
    return ret;
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("correcting non-linear heap");
  
  /* if we went down then this is a real error! */
  if (! IS_GROWTH(ret, _heap_last)
      || ! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_NONLINEAR)) {
    dmalloc_errno = ERROR_ALLOC_NONLINEAR;
    dmalloc_error("_heap_alloc");
    return HEAP_ALLOC_ERROR;
  }
  
  /* adjust chunk admin information to align to blocksize */
  blockn = ((char *)ret - (char *)_heap_last) / BLOCK_SIZE;
  
  /* align back to correct values */
  diff = BLOCK_SIZE - ((long)ret % BLOCK_SIZE);
  if (diff == BLOCK_SIZE)
    diff = 0;
  
  /* do we need to correct non-linear space with sbrk */
  if (diff > 0) {
    if (heap_extend(diff) == SBRK_ERROR) {
      return HEAP_ALLOC_ERROR;
    }
    
    ret = HEAP_INCR(ret, diff);
    blockn++;
  }
  
  if (extern_p != NULL) {
    *extern_p = _heap_last;
  }
  if (extern_cp != NULL) {
    *extern_cp = blockn;
  }
  
  _heap_last = HEAP_INCR(ret, size);
  
  return ret;
}
