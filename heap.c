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
LOCAL	char	*rcs_id =
  "$Id: heap.c,v 1.42 1995/06/21 18:20:07 gray Exp $";
#endif

/* external routines */
#if HAVE_SBRK
IMPORT	char		*sbrk(const int incr);	/* to extend the heap */
#define SBRK_ERROR	((char *)-1)		/* sbrk error code */
#endif

/* exported variables */
EXPORT	void		*_heap_base = NULL;	/* base of our heap */
EXPORT	void		*_heap_last = NULL;	/* end of our heap */

/*
 * check to make sure the heap is still linear.  avoids race conditions
 * returns [NO]ERROR
 */
EXPORT	int	_heap_check(void)
{
  int		diff, blockn;
  void		*ret;
  
#if HAVE_SBRK
  ret = sbrk(0);
  if (ret != _heap_last) {
    /* if we went down then real error! */
    if (ret < _heap_last
	|| ! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_NONLINEAR)) {
      dmalloc_errno = ERROR_ALLOC_NONLINEAR;
      dmalloc_error("_heap_alloc");
      return ERROR;
    }
    
    /* adjust chunk admin information */
    blockn = ((char *)ret - (char *)_heap_last) / BLOCK_SIZE + 1;
    if (_chunk_note_external(blockn, ret) != NOERROR)
      return ERROR;
    
    /* compensate for wasted space */
    diff = BLOCK_SIZE - ((long)ret % BLOCK_SIZE);
    _heap_last = (char *)ret + diff;
    
    /* align back to BLOCK_SIZE */
    if (sbrk(diff) == SBRK_ERROR)
      return ERROR;
  }
#endif
  
  return NOERROR;
}

/*
 * function to get SIZE memory bytes from the end of the heap
 */
EXPORT	void	*_heap_alloc(const unsigned int size)
{
  void		*ret = HEAP_ALLOC_ERROR;
  
#if HAVE_SBRK
  ret = sbrk(size);
  if (ret == SBRK_ERROR) {
    dmalloc_errno = ERROR_ALLOC_FAILED;
    dmalloc_error("_heap_alloc");
    ret = HEAP_ALLOC_ERROR;
  }
#endif
  
  /* did we not allocate any heap space? */
  if (ret == HEAP_ALLOC_ERROR) {
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CATCH_NULL)) {
      char	str[128];
      (void)sprintf(str, "\r\ndmalloc: critical error: could not allocate %u more bytes from heap\r\n",
		    size);
      (void)write(STDERR, str, strlen(str));
      _dmalloc_die(FALSE);
    }
    return HEAP_ALLOC_ERROR;
  }
  
#if HAVE_SBRK
  if (ret != _heap_last) {
    dmalloc_errno = ERROR_ALLOC_NONLINEAR;
    dmalloc_error("_heap_alloc");
    return HEAP_ALLOC_ERROR;
  }
  
  /* increment last pointer */
  _heap_last = (char *)ret + size;
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
    dmalloc_errno = ERROR_ALLOC_FAILED;
    dmalloc_error("_heap_end");
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CATCH_NULL)) {
      char	str[128];
      (void)sprintf(str, "\r\ndmalloc: critical error: could not find the end of the heap\r\n");
      (void)write(STDERR, str, strlen(str));
      _dmalloc_die(FALSE);
    }
    ret = HEAP_ALLOC_ERROR;
  }
#endif
  
  return ret;
}

/*
 * initialize heap pointers.  returns [NO]ERROR
 */
EXPORT	int	_heap_startup(void)
{
  _heap_base = _heap_end();
  if (_heap_base == HEAP_ALLOC_ERROR)
    return ERROR;
  
  _heap_last = _heap_base;
  if (_heap_last == HEAP_ALLOC_ERROR)
    return ERROR;
  
  return NOERROR;
}

/*
 * align (by extending) _heap_base to block_size byte boundary
 */
EXPORT	void	*_heap_align_base(void)
{
  long	diff;
  
  diff = BLOCK_SIZE - ((long)_heap_base % BLOCK_SIZE);
  _heap_base = (char *)_heap_base + diff;
  
  return _heap_alloc(diff);
}
