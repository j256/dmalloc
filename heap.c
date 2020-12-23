/*
 * system specific memory routines
 *
 * Copyright 2020 by Gray Watson
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
 */

/*
 * These are the system/machine specific routines for allocating space on the
 * heap as well as reporting the current position of the heap.
 */

#if HAVE_UNISTD_H
# include <unistd.h>				/* for write */
#endif
#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_MMAN_H
#  include <sys/mman.h>				/* for mmap stuff */
#endif

#define DMALLOC_DISABLE

#include "conf.h"
#include "dmalloc.h"

#include "append.h"
#include "chunk.h"
#include "compat.h"
#include "debug_tok.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "dmalloc_loc.h"

#define SBRK_ERROR	((char *)-1)		/* sbrk error code */

/* exported variables */
void		*_dmalloc_heap_low = NULL;	/* base of our heap */
void		*_dmalloc_heap_high = NULL;	/* end of our heap */

/****************************** local functions ******************************/

/*
 * static void *heap_extend
 *
 * Get more bytes from the system functions.  Returns a valid pointer or NULL on error.
 *
 * ARGUMENTS:
 *
 * incr -> Number of bytes we need.
 */
static	void	*heap_extend(const int incr)
{
  void	*ret = SBRK_ERROR;
  char	*high;
  
#if INTERNAL_MEMORY_SPACE
  {
    static char	block_o_bytes[INTERNAL_MEMORY_SPACE];
    static char *bounds_p = block_o_bytes + sizeof(block_o_bytes);
    static char *block_p = block_o_bytes;
    
    if (block_p + incr >= bounds_p) {
      ret = SBRK_ERROR;
    }
    else {
      ret = block_p;
      block_p += incr;
    }
  }
#else
#if HAVE_MMAP && USE_MMAP
#if MAP_ANON
  /* if we have and can use mmap, then do so */
  ret = mmap(0L, incr, PROT_READ | PROT_WRITE | PROT_EXEC,
	     MAP_PRIVATE | MAP_ANON, -1 /* no fd */, 0 /* no offset */);
#else
#endif
  if (ret == MAP_FAILED) {
    ret = SBRK_ERROR;
  }
#else
#if HAVE_SBRK
  ret = sbrk(incr);
#endif /* if HAVE_SBRK */
#endif /* if not HAVE_MMAP && USE_MMAP */
#endif /* if not INTERNAL_MEMORY_SPACE */
  
  if (ret == SBRK_ERROR) {
    if (BIT_IS_SET(_dmalloc_flags, DMALLOC_DEBUG_CATCH_NULL)) {
      char	str[128];
      int	len;
      len = loc_snprintf(str, sizeof(str),
			 "\r\ndmalloc: critical error: could not extend heap %u more bytes\r\n", incr);
      (void)write(STDERR, str, len);
      _dmalloc_die(0);
    }
    dmalloc_errno = DMALLOC_ERROR_ALLOC_FAILED;
    dmalloc_error("heap_extend");
  }
  
  if (_dmalloc_heap_low == NULL || (char *)ret < (char *)_dmalloc_heap_low) {
    _dmalloc_heap_low = ret;
  }
  high = (char *)ret + incr;
  if (high > (char *)_dmalloc_heap_high) {
    _dmalloc_heap_high = high;
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DMALLOC_DEBUG_LOG_ADMIN)) {
    dmalloc_message("extended heap space by %d bytes returned %p [%p, %p]",
		    incr, ret, _dmalloc_heap_low, _dmalloc_heap_high);
  }
  
  return ret;
}

/*
 * static void heap_release
 *
 * Release a memory chunk back to the sytem.
 *
 * ARGUMENTS:
 *
 * addr -> Previously used memory.
 * size -> Size of memory.
 */
static	void	heap_release(void *addr, const int size)
{
#if INTERNAL_MEMORY_SPACE
  /* no-op */
#else
#if HAVE_MUNMAP && USE_MMAP
  if (munmap(addr, size) == 0) {
    if (BIT_IS_SET(_dmalloc_flags, DMALLOC_DEBUG_LOG_ADMIN)) {
      dmalloc_message("releasing heap memory %p, size %d", addr, size);
    }
  } else {
    dmalloc_message("munmap failed to release heap memory %p, size %d",
		    addr, size);
  }
#else
  /* no-op */
#endif /* if not HAVE_MMAP && USE_MMAP */
#endif /* if not INTERNAL_MEMORY_SPACE */
}

/**************************** exported functions *****************************/

/*
 * int _heap_startup
 *
 * Initialize heap pointers.
 *
 * Returns 1 on success or 0 on failure.
 */
int	_dmalloc_heap_startup(void)
{
  return 1;
}

/*
 * void *_dmalloc_heap_alloc
 *
 * Function to get memory bytes from the heap.
 *
 * Returns a valid pointer on success or NULL on failure.
 *
 * ARGUMENTS:
 *
 * size -> Number of bytes we need.
 */
void	*_dmalloc_heap_alloc(const unsigned int size)
{
  void	*heap_new, *heap_diff;
  long	diff_size;
  
  if (size == 0) {
    dmalloc_errno = DMALLOC_ERROR_BAD_SIZE;
    dmalloc_error("_dmalloc_heap_alloc");
    return HEAP_ALLOC_ERROR;
  }
  
  /* extend the heap by our size */
  heap_new = heap_extend(size);
  if (heap_new == SBRK_ERROR) {
    return HEAP_ALLOC_ERROR;
  }
  
  /* calculate bytes needed to align to block boundary */
  diff_size = (PNT_ARITH_TYPE)heap_new % BLOCK_SIZE;
  if (diff_size == 0) {
    /* if we are already aligned then we are all set */
    return heap_new;
  }
  diff_size = BLOCK_SIZE - diff_size;
  
  /* shift the heap a bit to account for non block alignment */
  heap_diff = heap_extend(diff_size);
  if (heap_diff == SBRK_ERROR) {
    return HEAP_ALLOC_ERROR;
  }
  
  /* if heap-diff went down then probably using sbrk and stack grows down */
  if ((char *)heap_diff + diff_size == (char *)heap_new) {
    return heap_diff;
  }
  else if ((char *)heap_new + size == (char *)heap_diff) {
    /* shift up our heap to align with the block */
    return (char *)heap_new + diff_size;
  }
  
  /*
   * We got non-contiguous memory regions.  This may indicate that
   * mmap does not return page-size chunks or that we didn't determine
   * the page size right.  In any case we do the unfortunate thing to
   * allocate size + BLOCK_SIZE and then we are guaranteed a page-size
   * aligned chunk in there while wasting a page-size of extra memory.
   * Blah.
   */
  heap_release(heap_new, size);
  heap_release(heap_diff, diff_size);
  
  int new_size = size + BLOCK_SIZE;
  heap_new = heap_extend(new_size);
  if (heap_new == SBRK_ERROR) {
    return HEAP_ALLOC_ERROR;
  }
  
  dmalloc_message("WARNING: had to extend heap by %d more bytes to get page aligned %p",
		  new_size, heap_new);
  
  diff_size = (PNT_ARITH_TYPE)heap_new % BLOCK_SIZE;
  if (diff_size == 0) {
    return heap_new;
  } else {
    return heap_new + diff_size;
  }
}
