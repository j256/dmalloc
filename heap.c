/*
 * system specific memory routines
 *
 * Copyright 2000 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://dmalloc.com/
 *
 * $Id: heap.c,v 1.55 2000/05/15 22:22:28 gray Exp $
 */

/*
 * These are the system/machine specific routines for allocating space on the
 * heap as well as reporting the current position of the heap.
 */

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
#ifdef __GNUC__
#ident "$Id: heap.c,v 1.55 2000/05/15 22:22:28 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: heap.c,v 1.55 2000/05/15 22:22:28 gray Exp $";
#endif
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
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
    _dmalloc_message("extending heap space by %d bytes", incr);
  }
  
#if HAVE_SBRK
  ret = sbrk(incr);
  if (ret == SBRK_ERROR) {
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CATCH_NULL)) {
      char	str[128];
      int	len;
      len = loc_snprintf(str, sizeof(str),
			 "\r\ndmalloc: critical error: could not extend heap %u more bytes\r\n", incr);
      (void)write(STDERR, str, len);
      _dmalloc_die(0);
    }
    dmalloc_errno = ERROR_ALLOC_FAILED;
    dmalloc_error("heap_extend");
  }
#endif
  
  return ret;
}

/**************************** exported functions *****************************/

/*
 * int _heap_startup
 *
 * DESCRIPTION:
 *
 * Initialize heap pointers.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * None.
 */
int	_heap_startup(void)
{
  long		diff;
  
  _heap_base = heap_extend(0);
  if (_heap_base == SBRK_ERROR) {
    return 0;
  }
  
  /* align the heap-base */
  diff = BLOCK_SIZE - ((long)_heap_base % BLOCK_SIZE);
  if (diff == BLOCK_SIZE) {
    diff = 0;
  }
  
  if (diff > 0) {
    if (heap_extend(diff) == SBRK_ERROR) {
      return 0;
    }
    _heap_base = (char *)HEAP_INCR(_heap_base, diff);
  }
  
  _heap_last = _heap_base;
  
  return 1;
}

/*
 * Function to get SIZE memory bytes from the end of the heap.  it
 * returns a pointer to any external blocks in EXTERN_P and the number
 * of blocks in EXTERN_CP.
 */
void	*_heap_alloc(const unsigned int size, void **extern_p,
		     int *extern_cp)
{
  void		*heap_new, *heap_diff;
  long		diff;
  int		block_n = 0;
  
  /* set our external memory pointer to where the heap should be */ 
  if (extern_p != NULL) {
    *extern_p = _heap_last;
  }
  
  while (1) {
    
    /* extend the heap by our size */
    heap_new = heap_extend(size);
    if (heap_new == SBRK_ERROR) {
      return HEAP_ALLOC_ERROR;
    }
    
    /* is the heap linear? */
    if (heap_new == _heap_last) {
      _heap_last = HEAP_INCR(heap_new, size);
      if (extern_cp != NULL) {
	*extern_cp = 0;
      }
      return heap_new;
    }
    
    /* if we went down then this is a real error! */
    if ((! IS_GROWTH(heap_new, _heap_last))
	|| ! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_NONLINEAR)) {
      dmalloc_errno = ERROR_ALLOC_NONLINEAR;
      dmalloc_error("_heap_alloc");
      return HEAP_ALLOC_ERROR;
    }
    
    /* adjust chunk admin information to align to blocksize */
    block_n += BLOCKS_BETWEEN(heap_new, _heap_last);
    
    /* move heap last forward */
    _heap_last = HEAP_INCR(heap_new, size);
    
    /* calculate bytes needed to align to block boundary */
    diff = BLOCK_SIZE - ((long)heap_new % BLOCK_SIZE);
    if (diff == BLOCK_SIZE) {
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
	_dmalloc_message("corrected non-linear heap for %d blocks", block_n);
      }
      /* if external sbrk asked for full block(s) then no need to correct */
      break;
    }
    
    /* account for the partial block that we need to fill */
    block_n++;
    
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
      _dmalloc_message("corrected non-linear non-aligned heap for %d blocks",
		       block_n);
    }
    
    /* shift the heap a bit to account for non block alignment */
    heap_diff = heap_extend(diff);
    if (heap_diff == SBRK_ERROR) {
      return HEAP_ALLOC_ERROR;
    }
    
    /* if we got what we expected, then we are done */
    if (heap_diff == _heap_last) {
      /* shift the new pointer up to align it */
      heap_new = HEAP_INCR(heap_new, diff);
      /* move the heap last pointer past the diff section */
      _heap_last = HEAP_INCR(heap_diff, diff);
      break;
    }
    
    /*
     * We may have a wierd sbrk race condition here.  We hope that we
     * are not just majorly confused which may mean that we sbrk till
     * the cows come home -- or we die from lack of memory.
     */
    
    /* move the heap last pointer past the diff section */
    _heap_last = HEAP_INCR(heap_diff, diff);
    
    /* start over again */
  }
  
  if (extern_cp != NULL) {
    *extern_cp = block_n;
  }
  
  return heap_new;
}
