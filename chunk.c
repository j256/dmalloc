/*
 * memory chunk low-level allocation routines
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
 * The author may be contacted via http://www.letters.com/~gray/
 *
 * $Id: chunk.c,v 1.132 1999/02/16 22:45:42 gray Exp $
 */

/*
 * This file contains algorithm level routine for the heap.  They handle the
 * manipulation and administration of chunks of memory.
 */

#include <ctype.h>
#include <stdio.h>			/* for sprintf */

#if HAVE_STRING_H
# include <string.h>
#endif

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "chunk.h"
#include "chunk_loc.h"
#include "compat.h"
#include "debug_val.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: chunk.c,v 1.132 1999/02/16 22:45:42 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: chunk.c,v 1.132 1999/02/16 22:45:42 gray Exp $";
#endif
#endif

/* local routines */
void		_chunk_log_heap_map(void);

/* local variables */

/* free lists of bblocks and dblocks */
static	bblock_t	*free_bblock[MAX_SLOTS];
static	dblock_t	*free_dblock[BASIC_BLOCK];

/* administrative structures */
static	bblock_adm_t	*bblock_adm_head = NULL; /* pointer to 1st bb_admin */
static	bblock_adm_t	*bblock_adm_tail = NULL; /* pointer to last bb_admin */
static	unsigned int	smallest_block = 0;	/* smallest size in bits */
static	unsigned int	bits[MAX_SLOTS];
static	char		fence_bottom[FENCE_BOTTOM_SIZE];
static	char		fence_top[FENCE_TOP_SIZE];

/* user information shifts for display purposes */
static	int		fence_bottom_size = 0;	/* add to pnt for display */
static	int		fence_overhead_size = 0; /* total adm per pointer */

/* memory stats */
static	long		alloc_current = 0;	/* current memory usage */
static	long		alloc_maximum = 0;	/* maximum memory usage  */
static	long		alloc_cur_given = 0;	/* current mem given */
static	long		alloc_max_given = 0;	/* maximum mem given  */
static	long		alloc_total = 0;	/* total allocation */
static	unsigned long	alloc_one_max = 0;	/* maximum at once */
static	long		free_space_count = 0;	/* count the free bytes */

/* pointer stats */
static	long		alloc_cur_pnts = 0;	/* current pointers */
static	long		alloc_max_pnts = 0;	/* maximum pointers */
static	long		alloc_tot_pnts = 0;	/* current pointers */

/* admin counts */
static	long		bblock_adm_count = 0;	/* count of bblock_admin */
static	long		dblock_adm_count = 0;	/* count of dblock_admin */
static	long		bblock_count = 0;	/* count of basic-blocks */
static	long		dblock_count = 0;	/* count of divided-blocks */
static	long		extern_count = 0;	/* count of external blocks */
static	long		check_count = 0;	/* count of heap-checks */

/* alloc counts */
static	long		malloc_count = 0;	/* count the mallocs */
static	long		calloc_count = 0;	/* # callocs, done in alloc */
static	long		realloc_count = 0;	/* count the reallocs */
static	long		recalloc_count = 0;	/* count the reallocs */
static	long		memalign_count = 0;	/* count the memaligns */
static	long		valloc_count = 0;	/* count the veallocs */
static	long		free_count = 0;		/* count the frees */

/******************************* misc routines *******************************/

/*
 * Startup the low level malloc routines
 */
int	_chunk_startup(void)
{
  unsigned int	bin_c, num;
  
  /* calculate the smallest possible block */
  for (smallest_block = DEFAULT_SMALLEST_BLOCK;
       DB_PER_ADMIN < BLOCK_SIZE / (1 << smallest_block);
       smallest_block++) {
  }
  
  /* verify that some conditions are not true */
  if (BB_PER_ADMIN <= 2
      || sizeof(bblock_adm_t) > BLOCK_SIZE
      || DB_PER_ADMIN < (BLOCK_SIZE / (1 << smallest_block))
      || sizeof(dblock_adm_t) > BLOCK_SIZE
      || (1 << smallest_block) < ALLOCATION_ALIGNMENT) {
    dmalloc_errno = ERROR_BAD_SETUP;
    dmalloc_error("_chunk_startup");
    return ERROR;
  }
  
  /* initialize free bins and queues */
  for (bin_c = 0; bin_c < MAX_SLOTS; bin_c++) {
    free_bblock[bin_c] = NULL;
  }
  for (bin_c = 0; bin_c < BASIC_BLOCK; bin_c++) {
    free_dblock[bin_c] = NULL;
  }
  
  /* make array for NUM_BITS calculation */
  bits[0] = 1;
  for (bin_c = 1, num = 2; bin_c < MAX_SLOTS; bin_c++, num *= 2) {
    bits[bin_c] = num;
  }
  
  /* assign value to add to pointers when displaying */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    fence_bottom_size = FENCE_BOTTOM_SIZE;
    fence_overhead_size = FENCE_OVERHEAD_SIZE;
  }
  else {
    fence_bottom_size = 0;
    fence_overhead_size = 0;
  }
  
  {
    unsigned FENCE_MAGIC_TYPE	value;
    char			*pos_p, *max_p;
    
    value = FENCE_MAGIC_BOTTOM;
    max_p = fence_bottom + FENCE_BOTTOM_SIZE;
    for (pos_p = fence_bottom;
	 pos_p < max_p;
	 pos_p += sizeof(FENCE_MAGIC_TYPE)) {
      if (pos_p + sizeof(FENCE_MAGIC_TYPE) <= max_p) {
	memcpy(pos_p, (char *)&value, sizeof(FENCE_MAGIC_TYPE));
      }
      else {
	memcpy(pos_p, (char *)&value, max_p - pos_p);
      }
    }
    
    value = FENCE_MAGIC_TOP;
    max_p = fence_top + FENCE_TOP_SIZE;
    for (pos_p = fence_top; pos_p < max_p; pos_p += sizeof(FENCE_MAGIC_TYPE)) {
      if (pos_p + sizeof(FENCE_MAGIC_TYPE) <= max_p) {
	memcpy(pos_p, (char *)&value, sizeof(FENCE_MAGIC_TYPE));
      }
      else {
	memcpy(pos_p, (char *)&value, max_p - pos_p);
      }
    }
  }
  
  return NOERROR;
}

/*
 * expand_chars
 *
 * DESCRIPTION:
 *
 * Copies a buffer into a output buffer while translates non-printables
 * into %03o octal values.  If it can, it will also translate certain
 * \ characters (\r, \n, etc.) into \\%c.  The routine is useful for
 * printing out binary values.
 *
 * NOTE: It does _not_ add a \0 at the end of the output buffer.
 *
 * RETURNS:
 *
 * Returns the number of characters added to the output buffer.
 *
 * ARGUMENTS:
 *
 * buf - the buffer to convert.
 *
 * buf_size - size of the buffer.  If < 0 then it will expand till it
 * sees a \0 character.
 *
 * out - destination buffer for the convertion.
 *
 * out_size - size of the output buffer.
 */
static	int	expand_chars(const void *buf, const int buf_size,
			     char *out, const int out_size)
{
  int			buf_c;
  const unsigned char	*buf_p, *spec_p;
  char	 		*max_p, *out_p = out;
  
  /* setup our max pointer */
  max_p = out + out_size;
  
  /* run through the input buffer, counting the characters as we go */
  for (buf_c = 0, buf_p = (const unsigned char *)buf;; buf_c++, buf_p++) {
    
    /* did we reach the end of the buffer? */
    if (buf_size < 0) {
      if (*buf_p == '\0') {
	break;
      }
    }
    else {
      if (buf_c >= buf_size) {
	break;
      }
    }
    
    /* search for special characters */
    for (spec_p = (unsigned char *)SPECIAL_CHARS + 1;
	 *(spec_p - 1) != '\0';
	 spec_p += 2) {
      if (*spec_p == *buf_p) {
	break;
      }
    }
    
    /* did we find one? */
    if (*(spec_p - 1) != '\0') {
      if (out_p + 2 >= max_p) {
	break;
      }
      (void)sprintf(out_p, "\\%c", *(spec_p - 1));
      out_p += 2;
      continue;
    }
    
    /* print out any 7-bit printable characters */
    if (*buf_p < 128 && isprint(*buf_p)) {
      if (out_p + 1 >= max_p) {
	break;
      }
      *out_p = *(char *)buf_p;
      out_p += 1;
    }
    else {
      if (out_p + 4 >= max_p) {
	break;
      }
      (void)sprintf(out_p, "\\%03o", *buf_p);
      out_p += 4;
    }
  }
  /* try to punch the null if we have space in case the %.*s doesn't work */
  if (out_p < max_p) {
    *out_p = '\0';
  }
  
  return out_p - out;
}

/*
 * Describe pnt from its FILE, LINE into BUF
 */
static	void	desc_pnt(char *buf, const char *file,
			 const unsigned int line)
{
  if (file == DMALLOC_DEFAULT_FILE && line == DMALLOC_DEFAULT_LINE) {
    (void)sprintf(buf, "unknown");
  }
  else if (line == DMALLOC_DEFAULT_LINE) {
    (void)sprintf(buf, "ra=%#lx", (unsigned long)file);
  }
  else if (file == DMALLOC_DEFAULT_FILE) {
    (void)sprintf(buf, "ra=ERROR(line=%u)", line);
  }
  else {
    (void)sprintf(buf, "%s:%u", file, line);
  }
}

/*
 * Display a bad pointer with FILE and LINE information
 */
char	*_chunk_display_where(const char *file, const unsigned int line)
{
  static char	buf[256];
  desc_pnt(buf, file, line);
  return buf;
}

/*
 * Small hack here.  Same as above but different buffer space so we
 * can use 2 calls in 1 printf.
 */
static	char	*chunk_display_where2(const char *file,
				      const unsigned int line)
{
  static char	buf[256];
  desc_pnt(buf, file, line);
  return buf;
}

/*
 * Display a pointer PNT and information about it.
 */
static	char	*display_pnt(const void *pnt, const overhead_t *over_p)
{
  static char	buf[256];
  char		buf2[64];
  
  (void)sprintf(buf, "%#lx", (unsigned long)pnt);
  
#if STORE_SEEN_COUNT
  (void)sprintf(buf2, "|s%lu", over_p->ov_seen_c);
  (void)strcat(buf, buf2);
#endif
  
#if STORE_ITERATION_COUNT
  (void)sprintf(buf2, "|i%lu", over_p->ov_iteration);
  (void)strcat(buf, buf2);
#endif
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
    (void)sprintf(buf2, "|w%s", _dmalloc_ptimeval(&over_p->ov_timeval, FALSE));
    (void)strcat(buf, buf2);
#else
#if STORE_TIME
    (void)sprintf(buf2, "|w%s", _dmalloc_ptime(&over_p->ov_time, FALSE));
    (void)strcat(buf, buf2);
#endif
#endif
  }
  
#if STORE_THREAD_ID
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_THREAD_ID)) {
    (void)strcat(buf, "|t");
    THREAD_ID_TO_STRING(buf2, over_p->ov_thread_id);
    (void)strcat(buf, buf2);
  }
#endif
  
  return buf;
}

/*
 * Log information about bad PNT (if PNT_KNOWN) from FILE, LINE.  Bad
 * because of REASON (if NULL then use error-code), from WHERE.
 */
static	void	log_error_info(const char *file, const unsigned int line,
			       const void *pnt, const unsigned int size,
			       const char *reason, const char *where,
			       const int dump_b)
{
  static int	dump_bottom_b = 0, dump_top_b = 0;
  char		out[(DUMP_SPACE + FENCE_BOTTOM_SIZE + FENCE_TOP_SIZE) * 4];
  const char	*reason_str;
  const void	*dump_pnt = pnt;
  int		out_len, dump_size = DUMP_SPACE, offset = 0;
  
  /* get a proper reason string */
  if (reason == NULL) {
    reason_str = dmalloc_strerror(dmalloc_errno);
  }
  else {
    reason_str = reason;
  }
  
  /* dump the pointer information */
  if (pnt == NULL) {
    _dmalloc_message("%s: %s: from '%s'",
		     where, reason_str, _chunk_display_where(file, line));
  }
  else {
    _dmalloc_message("%s: %s: pointer '%#lx' from '%s'",
		     where, reason_str, (unsigned long)pnt,
		     _chunk_display_where(file, line));
  }
  
  /* if we are not displaying memory then quit */
  if (! (dump_b && BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_BAD_SPACE))) {
    return;
  }
  
  /* NOTE: display memroy like this has the potential for generating a core */
  if (dmalloc_errno == ERROR_UNDER_FENCE) {
    /* NOTE: only dump out the proper fence-post area once */
    if (! dump_bottom_b) {
      out_len = expand_chars(fence_bottom, fence_bottom_size, out,
			     sizeof(out));
      _dmalloc_message("Dump of proper fence-bottom bytes: '%.*s'",
		       out_len, out);
      dump_bottom_b = 1;
    }
    offset = -fence_bottom_size;
    dump_size = DUMP_SPACE + FENCE_BOTTOM_SIZE;
  }
  else if (dmalloc_errno == ERROR_OVER_FENCE) {
    if (size == 0) {
      _dmalloc_message("Could not dump upper fence area.  No size data.");
    }
    else {
      /* NOTE: only dump out the proper fence-post area once */
      if (! dump_top_b) {
	out_len = expand_chars(fence_top, FENCE_TOP_SIZE, out, sizeof(out));
	_dmalloc_message("Dump of proper fence-top bytes: '%.*s'",
			 out_len, out);
	dump_top_b = 1;
      }
      /*
       * The size includes the bottom fence post area.  We want it to
       * align with the start of the top fence post area.
       */
      offset = size - FENCE_BOTTOM_SIZE - FENCE_TOP_SIZE;
      if (offset < 0) {
	offset = 0;
      }
      dump_size = DUMP_SPACE + FENCE_TOP_SIZE;
    }
  }
  
  dump_pnt = (char *)pnt + offset;
  if (IS_IN_HEAP(dump_pnt)) {
    out_len = expand_chars(dump_pnt, dump_size, out, sizeof(out));
    _dmalloc_message("Dump of '%#lx'%+d: '%.*s'",
		     (unsigned long)pnt, offset, out_len, out);
  }
  else {
    _dmalloc_message("Dump of '%#lx'%+d failed: not in heap",
		     (unsigned long)pnt, offset);
  }
}

/************************* fence-post error functions ************************/

/*
 * Check PNT of SIZE for fence-post magic numbers.  Returns [NO]ERROR.
 */
static	int	fence_read(const char *file, const unsigned int line,
			   const void *pnt, const unsigned int size,
			   const char *where)
{
  /* check magic numbers in bottom of allocation block */
  if (memcmp(fence_bottom, (char *)pnt, FENCE_BOTTOM_SIZE) != 0) {
    dmalloc_errno = ERROR_UNDER_FENCE;
    log_error_info(file, line, CHUNK_TO_USER(pnt), size, NULL, where, TRUE);
    dmalloc_error("fence_read");
    return ERROR;
  }
  
  /* check numbers at top of allocation block */
  if (memcmp(fence_top, (char *)pnt + size - FENCE_TOP_SIZE,
	     FENCE_TOP_SIZE) != 0) {
    dmalloc_errno = ERROR_OVER_FENCE;
    log_error_info(file, line, CHUNK_TO_USER(pnt), size, NULL, where, TRUE);
    dmalloc_error("fence_read");
    return ERROR;
  }
  
  return NOERROR;
}

/************************** administration functions *************************/

/*
 * Set the information for BLOCK_N administrative block(s) at
 * BBLOCK_P.  returns [NO]ERROR
 */
static	int	set_bblock_admin(const int block_n, bblock_t *bblock_p,
				 const int flag, const unsigned short num,
				 const unsigned long info, const void *pnt)
{
  int		bblock_c;
  bblock_adm_t	*bblock_adm_p;
  
  bblock_adm_p = (bblock_adm_t *)BLOCK_NUM_TO_PNT(bblock_p);
  
  for (bblock_c = 0; bblock_c < block_n; bblock_c++, bblock_p++) {
    if (bblock_p == bblock_adm_p->ba_blocks + BB_PER_ADMIN) {
      bblock_adm_p = bblock_adm_p->ba_next;
      if (bblock_adm_p == NULL) {
	dmalloc_errno = ERROR_BAD_ADMIN_LIST;
	dmalloc_error("_set_bblock_admin");
	return FREE_ERROR;
      }
      
      bblock_p = bblock_adm_p->ba_blocks;
    }
    
    /* set bblock info */
    switch (flag) {
      
    case BBLOCK_START_USER:
    case BBLOCK_VALLOC:
      if (bblock_c == 0) {
	bblock_p->bb_flags = BBLOCK_START_USER;
      }
      else {
	bblock_p->bb_flags = BBLOCK_USER;
      }
      
      /* same as START_USER with the VALLOC flag added */ 
      if (flag == BBLOCK_VALLOC) {
	bblock_p->bb_flags |= BBLOCK_VALLOC;
      }
      
      bblock_p->bb_line = (unsigned short)num;
      bblock_p->bb_size = (unsigned int)info;
      bblock_p->bb_file = (char *)pnt;
#if FREED_POINTER_DELAY
      bblock_p->bb_reuse_iter = 0;
#endif
      break;
      
    case BBLOCK_FREE:
      bblock_p->bb_flags = BBLOCK_FREE;
      bblock_p->bb_bit_n = (unsigned short)num;
      bblock_p->bb_block_n = (unsigned int)block_n;
      if (bblock_c == 0) {
	bblock_p->bb_next = (struct bblock_st *)pnt;
      }
      else {
	bblock_p->bb_next = (struct bblock_st *)NULL;
      }
#if FREED_POINTER_DELAY
      bblock_p->bb_reuse_iter = _dmalloc_iter_c + FREED_POINTER_DELAY;
#endif
      break;
      
    default:
      dmalloc_errno = ERROR_BAD_FLAG;
      dmalloc_error("set_bblock_admin");
      return ERROR;
      /* NOTREACHED */
      break;
    }
  }
  
  return NOERROR;
}

/*
 * Parse the free lists looking for a slot of MANY bblocks returning
 * such a slot in RET_P or NULL if none.  Returns [NO]ERROR.
 */
static	int	find_free_bblocks(const unsigned int many, bblock_t **ret_p)
{
  bblock_t	*bblock_p, *prev_p;
  bblock_t	*best_p = NULL, *best_prev_p = NULL;
  int		bit_c, bit_n, block_n, pos, best = 0;
  bblock_adm_t	*adm_p;
  
  /*
   * NOTE: it is here were we can implement first/best/worst fit.
   * Depending on fragmentation, we may want to impose limits on the
   * level jump or do something to try and limit the number of chunks.
   */
  
  /* start at correct bit-size and work up till we find a match */
  NUM_BITS(many, bit_c);
  bit_c += BASIC_BLOCK;
  
  for (; bit_c < MAX_SLOTS; bit_c++) {
    
    for (bblock_p = free_bblock[bit_c], prev_p = NULL;
	 bblock_p != NULL;
	 prev_p = bblock_p, bblock_p = bblock_p->bb_next) {
      
#if FREED_POINTER_DELAY
      /* are we still waiting on this guy? */
      if (_dmalloc_iter_c < bblock_p->bb_reuse_iter) {
	continue;
      }
#endif
      
      if (bblock_p->bb_block_n >= many
#if BEST_FIT
	  && (best == 0 || bblock_p->bb_block_n < best)
#else
#if WORST_FIT
	  && (bblock_p->bb_block_n > best)
#else
#if FIRST_FIT
	  /* nothing more needs to be tested */
#endif /* FIRST_FIT */
#endif /* ! WORST_FIT */
#endif /* ! BEST_FIT */
	  ) {
	best = bblock_p->bb_block_n;
	best_p = bblock_p;
	best_prev_p = prev_p;
	
#if FIRST_FIT
	break;
#endif
      }
    }
    
    /* NOTE: we probably want to not quit here if WORST_FIT */
    if (best_p != NULL) {
      break;
    }
  }
  
  /* did we not find one? */
  if (best_p == NULL) {
    *ret_p = NULL;
    return NOERROR;
  }
  
  /* take it off the free list */
  if (best_prev_p == NULL) {
    free_bblock[bit_c] = best_p->bb_next;
  }
  else {
    best_prev_p->bb_next = best_p->bb_next;
  }
  
  if (best_p->bb_block_n == many) {
    *ret_p = best_p;
    return NOERROR;
  }
  
  /*
   * now we need to split the block.  we return the start of the
   * current free section and add the left-over chunk to another
   * free-list with an adjusted block-count
   */
  bblock_p = best_p;
  adm_p = (bblock_adm_t *)BLOCK_NUM_TO_PNT(bblock_p);
  pos = (bblock_p - adm_p->ba_blocks) + many;
  
  /* parse forward until we've found the correct split point */
  while (pos >= BB_PER_ADMIN) {
    pos -= BB_PER_ADMIN;
    adm_p = adm_p->ba_next;
    if (adm_p == NULL) {
      dmalloc_errno = ERROR_BAD_ADMIN_LIST;
      dmalloc_error("find_free_bblocks");
      return ERROR;
    }
  }
  
  bblock_p = adm_p->ba_blocks + pos;
  if (bblock_p->bb_flags != BBLOCK_FREE) {
    dmalloc_errno = ERROR_BAD_FREE_MEM;
    dmalloc_error("find_free_bblocks");
    return ERROR;
  }
  
  block_n = bblock_p->bb_block_n - many;
  NUM_BITS(block_n * BLOCK_SIZE, bit_n);
  
  set_bblock_admin(block_n, bblock_p, BBLOCK_FREE, bit_n, 0,
		   free_bblock[bit_n]);
  free_bblock[bit_n] = bblock_p;
  
  *ret_p = best_p;
  return NOERROR;
}

/*
 * Get MANY new bblock block(s) from the free list physically
 * allocation.  Return a pointer to the new blocks' memory in MEM_P.
 * returns the blocks or NULL on error.
 */
static	bblock_t	*get_bblocks(const int many, void **mem_p)
{
  static bblock_adm_t	*free_p = NULL;	/* pointer to block with free slots */
  static int		free_c = 0;	/* count of free slots */
  bblock_adm_t		*adm_p, *adm_store[MAX_ADMIN_STORE];
  bblock_t		*bblock_p, *ret_p = NULL;
  void			*mem = NULL, *extern_mem = NULL;
  int			bblock_c, count, adm_c = 0, extern_c = 0;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
    _dmalloc_message("need %d bblocks (%d bytes)", many, many * BLOCK_SIZE);
  }
  
  /* is there anything on the user-free list(s)? */
  if (find_free_bblocks(many, &bblock_p) != NOERROR) {
    return NULL;
  }
  
  /* did we find anything? */
  if (bblock_p != NULL) {
    free_space_count -= many * BLOCK_SIZE;
    
    /* space should be free */
    if (bblock_p->bb_flags != BBLOCK_FREE) {
      dmalloc_errno = ERROR_BAD_FREE_MEM;
      dmalloc_error("get_bblocks");
      return NULL;
    }
    
    adm_p = (bblock_adm_t *)BLOCK_NUM_TO_PNT(bblock_p);
    if (mem_p != NULL) {
      *mem_p = BLOCK_POINTER(adm_p->ba_pos_n +
			     (bblock_p - adm_p->ba_blocks));
    }
    return bblock_p;
  }
  
  /*
   * immediately allocate the memory necessary for the new blocks
   * because we need to know if external blocks we sbrk'd so we can
   * account for them in terms of admin slots
   */
  mem = _heap_alloc(many * BLOCK_SIZE, &extern_mem, &extern_c);
  if (mem == HEAP_ALLOC_ERROR) {
    return NULL;
  }
  
  /* account for allocated and any external blocks */
  bblock_count += many + extern_c;
  
  /*
   * do we have enough bblock-admin slots for the blocks we need, the
   * bblock-admin blocks themselves, and any external blocks found?
   */
  while (many + adm_c + extern_c > free_c) {
    
    /* get some more space for a bblock_admin structure */
    adm_p = (bblock_adm_t *)_heap_alloc(BLOCK_SIZE, NULL, &count);
    if (adm_p == (bblock_adm_t *)HEAP_ALLOC_ERROR) {
      return NULL;
    }
    
    bblock_count++;
    /* NOTE: bblock_adm_count handled below */
    
    /* this means that someone ran sbrk while we were in here */
    if (count > 0) {
      dmalloc_errno = ERROR_ALLOC_NONLINEAR;
      dmalloc_error("get_bblocks");
      return NULL;
    }
    
    /*
     * really we are taking it from mem since we want the admin blocks
     * to come ahead of the user allocation on the stack
     */
    adm_p = mem;
    mem = (char *)mem + BLOCK_SIZE;
    
    /*
     * Since we are just allocating some more slots here, we need to
     * account for the admin block space later.  We save the admin
     * block pointer in a little queue which cannot overflow.  If it
     * does, it means that someone sbrk+alloced some enormous chunk
     * equivalent to (BLOCK_SIZE * (BB_PER_ADMIN - 1) *
     * MAX_ADMIN_STORE) bytes.
     */
    if (adm_c == MAX_ADMIN_STORE) {
      dmalloc_errno = ERROR_EXTERNAL_HUGE;
      dmalloc_error("get_bblocks");
      return NULL;
    }
    
    /* store new admin block in queue */
    adm_store[adm_c] = adm_p;
    adm_c++;
    
    /* do we need to print admin info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
      _dmalloc_message("new bblock-admin alloced for %d more admin slots",
		       BB_PER_ADMIN);
    }
    
    /* initialize the new admin block and maintain the linked list */
    adm_p->ba_magic1 = CHUNK_MAGIC_BOTTOM;
    if (bblock_adm_tail == NULL) {
      adm_p->ba_pos_n = 0;
      bblock_adm_head = adm_p;
      bblock_adm_tail = adm_p;
    }
    else {
      adm_p->ba_pos_n = bblock_adm_tail->ba_pos_n + BB_PER_ADMIN;
      bblock_adm_tail->ba_next = adm_p;
      bblock_adm_tail = adm_p;
    }
    
    /* initialize the bblocks in the bblock_admin */
    for (bblock_p = adm_p->ba_blocks;
	 bblock_p < adm_p->ba_blocks + BB_PER_ADMIN;
	 bblock_p++) {
      bblock_p->bb_flags = 0;
#if STORE_SEEN_COUNT
      bblock_p->bb_overhead.ov_seen_c = 0;
#endif
    }
    
    adm_p->ba_next = NULL;
    adm_p->ba_magic2 = CHUNK_MAGIC_TOP;
    
    /* set counter to next free slot */
    bblock_p = adm_p->ba_blocks + (BB_PER_ADMIN - 1);
    bblock_p->bb_flags = BBLOCK_ADMIN_FREE;
    bblock_p->bb_free_n = 0;
    
    /* maybe we used them up the last time? */
    if (free_p == NULL) {
      free_p = adm_p;
    }
    
    /* we add more slots less the one we just allocated to hold them */
    free_c += BB_PER_ADMIN;
  }
  
  /* get the block pointer to the first free slot we have */
  bblock_p = free_p->ba_blocks + (BB_PER_ADMIN - 1);
  bblock_c = bblock_p->bb_free_n;
  bblock_p = free_p->ba_blocks + bblock_c;
  
  /* first off, handle external referenced blocks */
  for (count = 0; count < extern_c; count++) {
    bblock_p->bb_flags = BBLOCK_EXTERNAL;
    bblock_p->bb_mem = extern_mem;
    
    bblock_p++;
    bblock_c++;
    extern_count++;
    free_c--;
    
    if (bblock_p >= free_p->ba_blocks + BB_PER_ADMIN) {
      free_p = free_p->ba_next;
      bblock_p = free_p->ba_blocks + (BB_PER_ADMIN - 1);
      bblock_c = bblock_p->bb_free_n;
      bblock_p = free_p->ba_blocks + bblock_c;
    }
  }
  
  /* handle accounting for the admin-block(s) that we allocated above */
  for (count = 0; count < adm_c; count++) {
    adm_p = adm_store[count];
    bblock_p->bb_flags = BBLOCK_ADMIN;
    bblock_p->bb_admin_p = adm_p;
    bblock_p->bb_pos_n = adm_p->ba_pos_n;
    
    bblock_p++;
    bblock_c++;
    bblock_adm_count++;
    free_c--;
    
    if (bblock_p >= free_p->ba_blocks + BB_PER_ADMIN) {
      free_p = free_p->ba_next;
      bblock_p = free_p->ba_blocks + (BB_PER_ADMIN - 1);
      bblock_c = bblock_p->bb_free_n;
      bblock_p = free_p->ba_blocks + bblock_c;
    }
  }
  
  /*
   * finally, handle the admin slots for the needed blocks
   */
  
  /* set up return values */
  ret_p = free_p->ba_blocks + bblock_c;
  if (mem_p != NULL) {
    *mem_p = mem;
  }
  
  /* now skip over those slots, set_bblock_admin will be done after return */
  bblock_c += many;
  while (bblock_c >= BB_PER_ADMIN) {
    free_p = free_p->ba_next;
    bblock_c -= BB_PER_ADMIN;
  }
  free_c -= many;
  
  /*
   * do some error checking and write the last free count.  if free_p
   * is NULL then next time will have to allocate another bbadmin-block
   */
  if (free_p == NULL) {
    if (free_c != 0) {
      dmalloc_errno = ERROR_BAD_ADMIN_LIST;
      dmalloc_error("get_bblocks");
      return NULL;
    }
  }
  else {
    if (free_c <= 0 || free_c >= BB_PER_ADMIN) {
      dmalloc_errno = ERROR_BAD_ADMIN_LIST;
      dmalloc_error("get_bblocks");
      return NULL;
    }
    bblock_p = free_p->ba_blocks + (BB_PER_ADMIN - 1);
    bblock_p->bb_free_n = bblock_c;
  }
  
  return ret_p;
}

/*
 * Find the bblock entry for PNT, LAST_P and NEXT_P point to the last
 * and next blocks starting block
 */
static	bblock_t	*find_bblock(const void *pnt, bblock_t **last_p,
				     bblock_t **next_p)
{
  void		*tmp;
  unsigned int	bblock_c, bblock_n;
  bblock_t	*last = NULL, *this;
  bblock_adm_t	*bblock_adm_p;
  
  if (pnt == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    return NULL;
  }
  
  /*
   * check validity of the pointer
   */
  if (! IS_IN_HEAP(pnt)) {
    dmalloc_errno = ERROR_NOT_IN_HEAP;
    return NULL;
  }
  
  /* find right bblock admin */
  for (bblock_c = WHICH_BLOCK(pnt), bblock_adm_p = bblock_adm_head;
       bblock_c >= BB_PER_ADMIN && bblock_adm_p != NULL;
       bblock_c -= BB_PER_ADMIN, bblock_adm_p = bblock_adm_p->ba_next) {
    if (last_p != NULL) {
      last = bblock_adm_p->ba_blocks + (BB_PER_ADMIN - 1);
    }
  }
  
  if (bblock_adm_p == NULL) {
    dmalloc_errno = ERROR_NOT_FOUND;
    return NULL;
  }
  
  this = bblock_adm_p->ba_blocks + bblock_c;
  
  if (last_p != NULL) {
    if (bblock_c > 0) {
      last = bblock_adm_p->ba_blocks + (bblock_c - 1);
    }
    
    /* adjust the last pointer back to start of free block */
    if (last != NULL && BIT_IS_SET(last->bb_flags, BBLOCK_FREE)) {
      if (last->bb_block_n <= bblock_c) {
	last = bblock_adm_p->ba_blocks + (bblock_c - last->bb_block_n);
      }
      else {
	/* need to go recursive to go bblock_n back, check if at 1st block */
	tmp = (char *)pnt - last->bb_block_n * BLOCK_SIZE;
	if (! IS_IN_HEAP(tmp)) {
	  last = NULL;
	}
	else {
	  last = find_bblock(tmp, NULL, NULL);
	  if (last == NULL) {
	    dmalloc_error("find_bblock");
	    return NULL;
	  }
	}
      }
    }
    
    *last_p = last;
  }
  if (next_p != NULL) {
    /* next pointer should move past current allocation */
    if (BIT_IS_SET(this->bb_flags, BBLOCK_START_USER)) {
      bblock_n = NUM_BLOCKS(this->bb_size);
    }
    else {
      bblock_n = 1;
    }
    if (bblock_c + bblock_n < BB_PER_ADMIN) {
      *next_p = this + bblock_n;
    }
    else {
      /* need to go recursive to go bblock_n ahead, check if at last block */
      tmp = (char *)pnt + bblock_n * BLOCK_SIZE;
      if (! IS_IN_HEAP(tmp)) {
	*next_p = NULL;
      }
      else {
	*next_p = find_bblock(tmp, NULL, NULL);
	if (*next_p == NULL) {
	  dmalloc_error("find_bblock");
	  return NULL;
	}
      }
    }
  }
  
  return this;
}

/*
 * Get MANY of contiguous dblock administrative slots.
 */
static	dblock_t	*get_dblock_admin(const int many)
{
  static int		free_slots = 0;
  static dblock_adm_t	*dblock_adm_p = NULL;
  dblock_t		*dblock_p;
  bblock_t		*bblock_p;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
    _dmalloc_message("need %d dblock-admin slots", many);
  }
  
  /* do we have enough right now? */
  if (free_slots >= many) {
    dblock_p = dblock_adm_p->da_block + (DB_PER_ADMIN - free_slots);
    free_slots -= many;
    return dblock_p;
  }
  
  /*
   * allocate a new bblock of dblock admin slots, should use free list
   */
  bblock_p = get_bblocks(1, (void **)&dblock_adm_p);
  if (bblock_p == NULL) {
    return NULL;
  }
  
  dblock_adm_count++;
  free_slots = DB_PER_ADMIN;
  
  bblock_p->bb_flags = BBLOCK_DBLOCK_ADMIN;
  bblock_p->bb_slot_p = dblock_adm_p;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
    _dmalloc_message("opened another %d dblock-admin slots", DB_PER_ADMIN);
  }
  
  dblock_adm_p->da_magic1 = CHUNK_MAGIC_BOTTOM;
  
  /* initialize the db_slots */
  for (dblock_p = dblock_adm_p->da_block;
       dblock_p < dblock_adm_p->da_block + DB_PER_ADMIN;
       dblock_p++) {
    dblock_p->db_bblock = NULL;
    dblock_p->db_next = NULL;
  }
  
  dblock_adm_p->da_magic2 = CHUNK_MAGIC_TOP;
  
  free_slots -= many;
  
  return dblock_adm_p->da_block;
}

/*
 * Find the next available free dblock in the BIT_N bucket.
 */
static	dblock_t	*find_free_dblock(const int bit_n)
{
  dblock_t	*dblock_p;
#if FREED_POINTER_DELAY
  dblock_t	*last_p;
  
  /* find a value dblock entry */
  for (dblock_p = free_dblock[bit_n], last_p = NULL;
       dblock_p != NULL;
       last_p = dblock_p, dblock_p = dblock_p->db_next) {
    
    /* are we still waiting on this guy? */
    if (_dmalloc_iter_c < dblock_p->db_reuse_iter) {
      continue;
    }
    
    /* keep the linked lists */
    if (last_p == NULL) {
      free_dblock[bit_n] = dblock_p->db_next;
    }
    else {
      last_p->db_next = dblock_p->db_next;
    }
    break;
  }
  
#else /* FREED_POINTER_DELAY == 0 */
  dblock_p = free_dblock[bit_n];
  free_dblock[bit_n] = dblock_p->db_next;
#endif /* FREED_POINTER_DELAY == 0 */
  
  return dblock_p;
}

/*
 * Get a dblock of 1<<BIT_N sized chunks, also asked for the slot memory
 */
static	void	*get_dblock(const int bit_n, const unsigned short byte_n,
			    const char *file, const unsigned short line,
			    overhead_t **over_p)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p, *first_p, *free_p;
  void		*pnt;
  
  /* is there anything on the dblock free list? */
  dblock_p = find_free_dblock(bit_n);
  
  if (dblock_p != NULL) {
    free_space_count -= 1 << bit_n;
    
    /* find pointer to memory chunk */
    pnt = (char *)dblock_p->db_bblock->bb_mem +
      (dblock_p - dblock_p->db_bblock->bb_dblock) * (1 << bit_n);
    
    /* do we need to print admin info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
      _dmalloc_message("dblock entry for %d bytes found on free list",
		       1 << bit_n);
    }
  }
  else {
    
    /* do we need to print admin info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN)) {
      _dmalloc_message("need to create a dblock for %dx %d byte blocks",
		       1 << (BASIC_BLOCK - bit_n), 1 << bit_n);
    }
    
    /* get some dblock admin slots and the bblock space */
    dblock_p = get_dblock_admin(1 << (BASIC_BLOCK - bit_n));
    if (dblock_p == NULL) {
      return NULL;
    }
    
    dblock_count++;
    
    /* get a bblock from free list */
    bblock_p = get_bblocks(1, &pnt);
    if (bblock_p == NULL) {
      return NULL;
    }
    
    /* setup bblock information */
    bblock_p->bb_flags = BBLOCK_DBLOCK;
    bblock_p->bb_bit_n = bit_n;
    bblock_p->bb_dblock = dblock_p;
    bblock_p->bb_mem = pnt;
    
    /* add the rest to the free list (has to be at least 1 other dblock) */
    first_p = dblock_p;
    dblock_p++;
    free_p = free_dblock[bit_n];
    free_dblock[bit_n] = dblock_p;
    
    for (; dblock_p < first_p + (1 << (BASIC_BLOCK - bit_n)) - 1; dblock_p++) {
      dblock_p->db_bblock = bblock_p;
      dblock_p->db_next = dblock_p + 1;
#if FREED_POINTER_DELAY
      dblock_p->db_reuse_iter = 0;
#endif
#if STORE_SEEN_COUNT
      dblock_p->db_overhead.ov_seen_c = 0;
#endif
      free_space_count += 1 << bit_n;
    }
    
    /* last one points to the free list (probably NULL) */
    dblock_p->db_next = free_p;
    dblock_p->db_bblock = bblock_p;
#if FREED_POINTER_DELAY
    dblock_p->db_reuse_iter = 0;
#endif
    free_space_count += 1 << bit_n;
    
    /*
     * We return the 1st dblock chunk in the block.  Overwrite the
     * rest of the block.
     */ 
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)) {
      (void)memset((char *)pnt + (1 << bit_n), BLANK_CHAR,
		   BLOCK_SIZE - (1 << bit_n));
    }
    
#if STORE_SEEN_COUNT
    /* the first pointer in the block inherits the counter of the bblock */
    first_p->db_overhead.ov_seen_c = bblock_p->bb_overhead.ov_seen_c;
#endif
    
    dblock_p = first_p;
  }
  
  dblock_p->db_line = line;
  dblock_p->db_size = byte_n;
  dblock_p->db_file = file;
  
#if STORE_SEEN_COUNT
  dblock_p->db_overhead.ov_seen_c++;
#endif
#if STORE_ITERATION_COUNT
  dblock_p->db_overhead.ov_iteration = _dmalloc_iter_c;
#endif
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIME && HAVE_TIME
    dblock_p->db_overhead.ov_time = time(NULL);
#endif
#if STORE_TIMEVAL
    GET_TIMEVAL(dblock_p->db_overhead.ov_timeval);
#endif
  }
  
#if STORE_THREAD_ID
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_THREAD_ID)) {
    dblock_p->db_overhead.ov_thread_id = THREAD_GET_ID;
  }
#endif
  
  *over_p = &dblock_p->db_overhead;
  
  return pnt;
}

/******************************* heap checking *******************************/

/*
 * Run extensive tests on the entire heap
 */
int	_chunk_check(void)
{
  bblock_adm_t	*this_adm_p, *ahead_p;
  bblock_t	*bblock_p, *bblist_p, *last_bblock_p;
  dblock_t	*dblock_p;
  unsigned int	undef = 0, start = 0;
  char		*byte_p;
  void		*pnt;
  int		bit_c, dblock_c = 0, bblock_c = 0, free_c = 0;
  unsigned int	bb_c = 0, len;
  int		free_bblock_c[MAX_SLOTS];
  int		free_dblock_c[BASIC_BLOCK];
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("checking heap");
  }
  
  /* if the heap is empty then no need to check anything */
  if (bblock_adm_head == NULL) {
    return NOERROR;
  }
  
  check_count++;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
    
    /* count the bblock free lists */
    for (bit_c = 0; bit_c < MAX_SLOTS; bit_c++) {
      free_bblock_c[bit_c] = 0;
      
      /* parse bblock free list doing minimal pointer checking */
      for (bblock_p = free_bblock[bit_c];
	   bblock_p != NULL;
	   bblock_p = bblock_p->bb_next, free_bblock_c[bit_c]++) {
	/*
	 * NOTE: this should not present problems since the bb_next is
	 * NOT unioned with bb_file.
	 */
	if (! IS_IN_HEAP(bblock_p)) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
    }
    
    /* count the dblock free lists */
    for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
      free_dblock_c[bit_c] = 0;
      
      /* parse dblock free list doing minimal pointer checking */
      for (dblock_p = free_dblock[bit_c];
	   dblock_p != NULL;
	   dblock_p = dblock_p->db_next, free_dblock_c[bit_c]++) {
	/*
	 * NOTE: this might miss problems if the slot is allocated but
	 * db_file (unioned with db_next) points to a return address
	 * in the heap
	 */
	if (! IS_IN_HEAP(dblock_p)) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
    }
  }
  
  /* start pointers */
  this_adm_p = bblock_adm_head;
  ahead_p = this_adm_p;
  last_bblock_p = NULL;
  
  /* test admin pointer validity */
  if (! IS_IN_HEAP(this_adm_p)) {
    dmalloc_errno = ERROR_BAD_ADMIN_P;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  /* test structure validity */
  if (this_adm_p->ba_magic1 != CHUNK_MAGIC_BOTTOM
      || this_adm_p->ba_magic2 != CHUNK_MAGIC_TOP) {
    dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  /* verify count value */
  if (this_adm_p->ba_pos_n != bb_c) {
    dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  /* check out the basic blocks */
  for (bblock_p = this_adm_p->ba_blocks;; last_bblock_p = bblock_p++) {
    
    /* are we at the end of the bb_admin section */
    if (bblock_p >= this_adm_p->ba_blocks + BB_PER_ADMIN) {
      this_adm_p = this_adm_p->ba_next;
      bb_c += BB_PER_ADMIN;
      
      /* are we done? */
      if (this_adm_p == NULL) {
	break;
      }
      
      /* test admin pointer validity */
      if (! IS_IN_HEAP(this_adm_p)) {
	dmalloc_errno = ERROR_BAD_ADMIN_P;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* test structure validity */
      if (this_adm_p->ba_magic1 != CHUNK_MAGIC_BOTTOM
	  || this_adm_p->ba_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* verify count value */
      if (this_adm_p->ba_pos_n != bb_c) {
	dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      bblock_p = this_adm_p->ba_blocks;
    }
    
    /* check for no-allocation */
    if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ALLOCATED)) {
      undef++;
      continue;
    }
    
    /* we better not have seen a not-allocated block before */
    if (undef > 0 && bblock_p->bb_flags != BBLOCK_ADMIN_FREE) {
      dmalloc_errno = ERROR_BAD_BLOCK_ORDER;
      dmalloc_error("_chunk_check");
      return ERROR;
    }
    
    start = 0;
    
    /*
     * check for different types
     */
    switch (BBLOCK_FLAG_TYPE(bblock_p->bb_flags)) {
      
      /* check a starting user-block */
    case BBLOCK_START_USER:
      
      /* check X blocks in a row */
      if (bblock_c != 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* mark the size in bits */
      NUM_BITS(bblock_p->bb_size, bit_c);
      bblock_c = NUM_BLOCKS(bblock_p->bb_size);
      /* valloc basic blocks gets 1 extra block below for any fence info */
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)
	  && fence_bottom_size > 0) {
	bblock_c++;
      }
      start = 1;
      
      /* check fence-posts for memory chunk */
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
	pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			    (bblock_p - this_adm_p->ba_blocks));
	/* if we have valloc block and there is fence info then shift pnt up */
	if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)
	    && fence_bottom_size > 0) {
	  pnt = (char *)pnt + (BLOCK_SIZE - fence_bottom_size);
	}
	if (fence_read(bblock_p->bb_file, bblock_p->bb_line,
		       pnt, bblock_p->bb_size, "heap-check") != NOERROR) {
	  return ERROR;
	}
      }
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_USER:
      
      /* check line number */
      if (bblock_p->bb_line > MAX_LINE_NUMBER) {
	dmalloc_errno = ERROR_BAD_LINE;
	log_error_info(NULL, 0, NULL, 0, NULL, "heap-check", FALSE);
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /*
       * Check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take
       * over.  If we have a valloc then the size might be small.
       */
      if (((! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC))
	   && bblock_p->bb_size <= BLOCK_SIZE / 2)
	  || bblock_p->bb_size > (1 << LARGEST_BLOCK)) {
	dmalloc_errno = ERROR_BAD_SIZE;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check file pointer */
      if (bblock_p->bb_file != DMALLOC_DEFAULT_FILE
	  && bblock_p->bb_line != DMALLOC_DEFAULT_LINE) {
	len = strlen(bblock_p->bb_file);
	if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	  dmalloc_errno = ERROR_BAD_FILEP;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
      
      /* check X blocks in a row */
      if (bblock_c == 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      else {
	if (start == 0
	    && (last_bblock_p == NULL
		|| ((! BIT_IS_SET(last_bblock_p->bb_flags, BBLOCK_START_USER))
		    && (! BIT_IS_SET(last_bblock_p->bb_flags, BBLOCK_USER)))
		|| bblock_p->bb_file != last_bblock_p->bb_file
		|| bblock_p->bb_line != last_bblock_p->bb_line
		|| bblock_p->bb_size != last_bblock_p->bb_size)) {
	  dmalloc_errno = ERROR_USER_NON_CONTIG;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
      
      bblock_c--;
      /* NOTE: we should check above the allocated space if alloc_blank on */
      break;
      
    case BBLOCK_ADMIN:
      
      /* check the bblock_admin linked-list */
      if (bblock_p->bb_admin_p != ahead_p) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMIN_P;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check count against admin count */
      if (bblock_p->bb_pos_n != ahead_p->ba_pos_n) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMIN_C;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      ahead_p = ahead_p->ba_next;
      break;
      
    case BBLOCK_DBLOCK:
      
      /* check out bit_c */
      if (bblock_p->bb_bit_n >= BASIC_BLOCK) {
	dmalloc_errno = ERROR_BAD_DBLOCK_SIZE;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblock_p->bb_dblock)) {
	dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* verify mem pointer */
      if (bblock_p->bb_mem != BLOCK_POINTER(this_adm_p->ba_pos_n +
					    (bblock_p -
					     this_adm_p->ba_blocks))) {
	dmalloc_errno = ERROR_BAD_DBLOCK_MEM;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check dblock entry very closely if necessary */
      for (dblock_c = 0, dblock_p = bblock_p->bb_dblock;
	   dblock_p < bblock_p->bb_dblock +
	   (1 << (BASIC_BLOCK - bblock_p->bb_bit_n));
	   dblock_c++, dblock_p++) {
	
	/* check out dblock entry to see if it is not free */
	/*
	 * XXX: this test might think it was a free'd slot if db_file
	 * (unioned with db_next) points to a return address in the
	 * heap.
	 */
	if ((dblock_p->db_next == NULL || IS_IN_HEAP(dblock_p->db_next))
	    && IS_IN_HEAP(dblock_p->db_bblock)) {
	  
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	    dblock_t	*dblist_p;
	    
	    /* find the free block in the free list */
	    for (dblist_p = free_dblock[bblock_p->bb_bit_n];
		 dblist_p != NULL;
		 dblist_p = dblist_p->db_next) {
	      if (dblist_p == dblock_p) {
		break;
	      }
	    }
	    
	    /* did we find it? */
	    if (dblist_p == NULL) {
	      dmalloc_errno = ERROR_BAD_FREE_LIST;
	      dmalloc_error("_chunk_check");
	      return ERROR;
	    }
	    
	    free_dblock_c[bblock_p->bb_bit_n]--;
	  }
	  
	  continue;
	}
	
	/*
	 * check out size, better be less than BLOCK_SIZE / 2 I have to
	 * check this twice.  Yick.
	 */
	if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, 0, NULL, 0, NULL, "heap-check", FALSE);
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
	  pnt = (char *)bblock_p->bb_mem +
	    dblock_c * (1 << bblock_p->bb_bit_n);
	  if (fence_read(dblock_p->db_file, dblock_p->db_line,
			 pnt, dblock_p->db_size, "heap-check") != NOERROR) {
	    return ERROR;
	  }
	}
      }
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblock_p->bb_slot_p)) {
	dmalloc_errno = ERROR_BAD_DBADMIN_POINTER;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* verify magic numbers */
      if (bblock_p->bb_slot_p->da_magic1 != CHUNK_MAGIC_BOTTOM
	  || bblock_p->bb_slot_p->da_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_DBADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check out each dblock_admin struct? */
      for (dblock_p = bblock_p->bb_slot_p->da_block;
	   dblock_p < bblock_p->bb_slot_p->da_block + DB_PER_ADMIN;
	   dblock_p++) {
	
	/* see if we've used this slot before */
	if (dblock_p->db_bblock == NULL && dblock_p->db_next == NULL) {
	  continue;
	}
	
	/* check out dblock pointer and next pointer (if free) */
	/*
	 * XXX: this test might think it was a free'd slot if db_file
	 * (unioned with db_next) points to a return address in the
	 * heap.
	 */
	if ((dblock_p->db_next == NULL || IS_IN_HEAP(dblock_p->db_next))
	    && IS_IN_HEAP(dblock_p->db_bblock)) {
	  
	  /* find pointer to memory chunk */
	  pnt = (char *)dblock_p->db_bblock->bb_mem +
	    (dblock_p - dblock_p->db_bblock->bb_dblock) *
	      (1 << dblock_p->db_bblock->bb_bit_n);
	  
	  /* should we verify that we have a block of BLANK_CHAR? */
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	    for (byte_p = (char *)pnt;
		 byte_p < (char *)pnt + (1 << dblock_p->db_bblock->bb_bit_n);
		 byte_p++) {
	      if (*byte_p != BLANK_CHAR) {
		dmalloc_errno = ERROR_FREE_NON_BLANK;
		log_error_info(NULL, 0, byte_p, 0, NULL, "heap-check", TRUE);
		dmalloc_error("_chunk_check");
		return ERROR;
	      }
	    }
	  }
	  
	  continue;
	}
	
	/* check out size, better be less than BLOCK_SIZE / 2 */
	if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, 0, NULL, 0, NULL, "heap-check", FALSE);
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	
	/* check line number */
	if (dblock_p->db_line > MAX_LINE_NUMBER) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, 0, NULL, 0, NULL, "heap-check", FALSE);
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	
	if (dblock_p->db_file != DMALLOC_DEFAULT_FILE
	    && dblock_p->db_line != DMALLOC_DEFAULT_LINE) {
	  len = strlen(dblock_p->db_file);
	  if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	    dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	    /* should there be a log-error-info call here? */
	    dmalloc_error("_chunk_check");
	    return ERROR;
	  }
	}
      }
      break;
      
    case BBLOCK_FREE:
      
      /* NOTE: check out free_lists, depending on debug value? */
      
      /*
       * verify linked list pointer
       * NOTE: this should not present problems since the bb_next is
       * NOT unioned with bb_file.
       */
      if (bblock_p->bb_next != NULL && (! IS_IN_HEAP(bblock_p->bb_next))) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check X blocks in a row */
      if (free_c == 0) {
	free_c = bblock_p->bb_block_n;
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	  
	  /* find the free block in the free list */
	  for (bblist_p = free_bblock[bblock_p->bb_bit_n];
	       bblist_p != NULL;
	       bblist_p = bblist_p->bb_next) {
	    if (bblist_p == bblock_p)
	      break;
	  }
	  
	  /* did we find it? */
	  if (bblist_p == NULL) {
	    dmalloc_errno = ERROR_BAD_FREE_LIST;
	    dmalloc_error("_chunk_check");
	    return ERROR;
	  }
	  
	  free_bblock_c[bblock_p->bb_bit_n]--;
	}
      }
      else {
	if (last_bblock_p == NULL
	    || last_bblock_p->bb_flags != BBLOCK_FREE
	    || bblock_p->bb_bit_n != last_bblock_p->bb_bit_n) {
	  dmalloc_errno = ERROR_FREE_NON_CONTIG;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
      free_c--;
      
      /* should we verify that we have a block of BLANK_CHAR? */
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			    (bblock_p - this_adm_p->ba_blocks));
	for (byte_p = (char *)pnt;
	     byte_p < (char *)pnt + BLOCK_SIZE;
	     byte_p++) {
	  if (*byte_p != BLANK_CHAR) {
	    dmalloc_errno = ERROR_FREE_NON_BLANK;
	    log_error_info(NULL, 0, byte_p, 0, NULL, "heap-check", TRUE);
	    dmalloc_error("_chunk_check");
	    return ERROR;
	  }
	}
	break;
	
	/* externally used block */
      case BBLOCK_EXTERNAL:
	/* nothing much to check */
	break;
	
	/* pointer to first free slot */
      case BBLOCK_ADMIN_FREE:
	/* better be the last block and the count should match undef */
	if (bblock_p != this_adm_p->ba_blocks + (BB_PER_ADMIN - 1)
	    || bblock_p->bb_free_n != (BB_PER_ADMIN - 1) - undef) {
	  dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	break;
	
      default:
	dmalloc_errno = ERROR_BAD_FLAG;
	dmalloc_error("_chunk_check");
	return ERROR;
	/* NOTREACHED */
	break;
      }
    }
  }
  
  /*
   * any left over contiguous counters?
   */
  if (bblock_c > 0) {
    dmalloc_errno = ERROR_USER_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  if (free_c > 0) {
    dmalloc_errno = ERROR_FREE_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
    
    /* any free bblock entries not accounted for? */
    for (bit_c = 0; bit_c < MAX_SLOTS; bit_c++) {
      if (free_bblock_c[bit_c] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
    }
    
    /* any free dblock entries not accounted for? */
    for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
      if (free_dblock_c[bit_c] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
    }
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_HEAP_CHECK_MAP)) {
    _chunk_log_heap_map();
  }
  
  return NOERROR;
}

/*
 * Run extensive tests on PNT from FUNC. test PNT HOW_MUCH of MIN_SIZE
 * (or 0 if unknown).  CHECK is flags for types of checking (see
 * chunk.h).  returns [NO]ERROR
 */
int	_chunk_pnt_check(const char *func, const void *pnt,
			 const int check, const int min_size)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  int		diff;
  unsigned int	len, min = min_size;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("checking pointer '%#lx'", (unsigned long)pnt);
  }
  
  /* adjust the pointer down if fence-posting */
  pnt = USER_TO_CHUNK(pnt);
  if (min != 0) {
    min += fence_overhead_size;
  }
  
  /* find which block it is in */
  bblock_p = find_bblock(pnt, NULL, NULL);
  if (bblock_p == NULL) {
    if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
      /* the pointer might not be the heap or might be NULL */
      dmalloc_errno = ERROR_NONE;
      return NOERROR;
    }
    else {
      /* errno set in find_bblock */
      log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, "pointer-check",
		     FALSE);
      dmalloc_error(func);
      return ERROR;
    }
  }
  
  /* maybe watch out for '\0' character */
  if (BIT_IS_SET(check, CHUNK_PNT_NULL)) {
    if (min != 0) {
      len = strlen(pnt) + 1;
      if (len > min) {
	min = len;
      }
    }
  }
  
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    diff = ((char *)pnt -
	    (char *)bblock_p->bb_mem) % (1 << bblock_p->bb_bit_n);
    if (diff != 0) {
      if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
	if (min != 0) {
	  min += diff;
	}
	pnt = (char *)pnt - diff;
      }
      else {
	dmalloc_errno = ERROR_NOT_ON_BLOCK;
	log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, "pointer-check",
		       FALSE);
	dmalloc_error(func);
	return ERROR;
      }
    }
    
    /* find correct dblock_p */
    dblock_p = bblock_p->bb_dblock + ((char *)pnt - (char *)bblock_p->bb_mem) /
      (1 << bblock_p->bb_bit_n);
    
    if (dblock_p->db_bblock == bblock_p) {
      /* NOTE: we should run through free list here */
      dmalloc_errno = ERROR_IS_FREE;
      log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, "pointer-check",
		     FALSE);
      dmalloc_error(func);
      return ERROR;
    }
    
    /* check line number */
    if (dblock_p->db_line > MAX_LINE_NUMBER) {
      dmalloc_errno = ERROR_BAD_LINE;
      log_error_info(dblock_p->db_file, dblock_p->db_line, CHUNK_TO_USER(pnt),
		     0, NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
    
    /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
    if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
      dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
      log_error_info(dblock_p->db_file, dblock_p->db_line, CHUNK_TO_USER(pnt),
		     0, NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
    
    if (min != 0 && dblock_p->db_size < min) {
      dmalloc_errno = ERROR_WOULD_OVERWRITE;
      log_error_info(dblock_p->db_file, dblock_p->db_line, CHUNK_TO_USER(pnt),
		     0, NULL, "pointer-check", TRUE);
      dmalloc_error(func);
      return ERROR;
    }
    
    /* check file pointer */
    if (dblock_p->db_file != DMALLOC_DEFAULT_FILE
	&& dblock_p->db_line != DMALLOC_DEFAULT_LINE) {
      len = strlen(dblock_p->db_file);
      if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	dmalloc_errno = ERROR_BAD_FILEP;
	log_error_info(dblock_p->db_file, dblock_p->db_line,
		       CHUNK_TO_USER(pnt), 0, NULL, "pointer-check", FALSE);
	dmalloc_error(func);
	return ERROR;
      }
    }
    
    /* check out the fence-posts */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
      if (fence_read(dblock_p->db_file, dblock_p->db_line,
		     pnt, dblock_p->db_size, "pointer-check") != NOERROR) {
	return ERROR;
      }
    }
    
    return NOERROR;
  }
  
  /* on a block boundary? */
  if (! ON_BLOCK(pnt)) {
    if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
      /*
       * normalize size and pointer to nearest block.
       *
       * NOTE: we really need to back-track up the block list to find the
       * starting user block to test things.
       */
      diff = (char *)pnt - BLOCK_POINTER(WHICH_BLOCK(pnt));
      pnt = (char *)pnt - diff;
      if (min != 0) {
	min += diff;
      }
    }
    else {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, "pointer-check",
		     FALSE);
      dmalloc_error(func);
      return ERROR;
    }
  }
  
  /* are we on a normal block */
  if ((! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER))
      && (! (BIT_IS_SET(check, CHUNK_PNT_LOOSE))
	    && BIT_IS_SET(bblock_p->bb_flags, BBLOCK_USER))) {
    dmalloc_errno = ERROR_NOT_START_USER;
    log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, "pointer-check",
		   FALSE);
    dmalloc_error(func);
    return ERROR;
  }
  
  /* check line number */
  if (bblock_p->bb_line > MAX_LINE_NUMBER) {
    dmalloc_errno = ERROR_BAD_LINE;
    log_error_info(bblock_p->bb_file, bblock_p->bb_line, CHUNK_TO_USER(pnt),
		   0, NULL, "pointer-check", FALSE);
    dmalloc_error(func);
    return ERROR;
  }
  
  /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
  if (bblock_p->bb_size <= BLOCK_SIZE / 2
      || bblock_p->bb_size > (1 << LARGEST_BLOCK)) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(bblock_p->bb_file, bblock_p->bb_line, CHUNK_TO_USER(pnt),
		   0, NULL, "pointer-check", FALSE);
    dmalloc_error(func);
    return ERROR;
  }
  
  if (min != 0 && bblock_p->bb_size < min) {
    dmalloc_errno = ERROR_WOULD_OVERWRITE;
    log_error_info(bblock_p->bb_file, bblock_p->bb_line, CHUNK_TO_USER(pnt),
		   0, NULL, "pointer-check", TRUE);
    dmalloc_error(func);
    return ERROR;
  }
  
  /* check file pointer */
  if (bblock_p->bb_file != DMALLOC_DEFAULT_FILE
      && bblock_p->bb_line != DMALLOC_DEFAULT_LINE) {
    len = strlen(bblock_p->bb_file);
    if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
      dmalloc_errno = ERROR_BAD_FILEP;
      log_error_info(bblock_p->bb_file, bblock_p->bb_line, CHUNK_TO_USER(pnt),
		     0, NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
  }
  
  /* check out the fence-posts if we are at the start of a user-block */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)
      && BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    if (fence_read(bblock_p->bb_file, bblock_p->bb_line, pnt,
		   bblock_p->bb_size, "pointer-check") != NOERROR) {
      return ERROR;
    }
  }
  
  return NOERROR;
}

/**************************** information routines ***************************/

/*
 * return some information associated with PNT, returns [NO]ERROR
 */
int	_chunk_read_info(const void *pnt, unsigned int *size_p,
			 unsigned int *alloc_size_p, char **file_p,
			 unsigned int *line_p, void **ret_attr_p,
			 const char *where, unsigned long **seen_cp)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("reading info about pointer '%#lx'", (unsigned long)pnt);
  }
  
  if (seen_cp != NULL) {
    *seen_cp = NULL;
  }
  
  /* adjust the pointer down if fence-posting */
  pnt = USER_TO_CHUNK(pnt);
  
  /* find which block it is in */
  bblock_p = find_bblock(pnt, NULL, NULL);
  if (bblock_p == NULL) {
    /* errno set in find_bblock */
    log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
    dmalloc_error("_chunk_read_info");
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if (((char *)pnt - (char *)bblock_p->bb_mem) %
	(1 << bblock_p->bb_bit_n) != 0) {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
      dmalloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* find correct dblock_p */
    dblock_p = bblock_p->bb_dblock + ((char *)pnt - (char *)bblock_p->bb_mem) /
      (1 << bblock_p->bb_bit_n);
    
    if (dblock_p->db_bblock == bblock_p) {
      /* NOTE: we should run through free list here */
      dmalloc_errno = ERROR_IS_FREE;
      log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
      dmalloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size_p != NULL) {
      *size_p = dblock_p->db_size;
    }
    if (alloc_size_p != NULL) {
      *alloc_size_p = 1 << bblock_p->bb_bit_n;
    }
    if (file_p != NULL) {
      if (dblock_p->db_file == DMALLOC_DEFAULT_FILE) {
	*file_p = NULL;
      }
      else {
	*file_p = (char *)dblock_p->db_file;
      }
    }
    if (line_p != NULL) {
      *line_p = dblock_p->db_line;
    }
    if (ret_attr_p != NULL) {
      /* if the line is blank then the file will be 0 or the return address */
      if (dblock_p->db_line == DMALLOC_DEFAULT_LINE) {
	*ret_attr_p = (char *)dblock_p->db_file;
      }
      else {
	*ret_attr_p = NULL;
      }
#if STORE_SEEN_COUNT
      if (seen_cp != NULL) {
	*seen_cp = &dblock_p->db_overhead.ov_seen_c;
      }
#endif
    }
  }
  else {
    
    /* verify that the pointer is either dblock or user allocated */
    if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
      dmalloc_errno = ERROR_NOT_USER;
      log_error_info(NULL, 0, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
      dmalloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size_p != NULL) {
      *size_p = bblock_p->bb_size;
    }
    if (alloc_size_p != NULL) {
      /*
       * if we have a valloc block and there is fence info, then
       * another block was allocated
       */
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)
	  && fence_bottom_size > 0) {
	*alloc_size_p = (NUM_BLOCKS(bblock_p->bb_size) + 1) * BLOCK_SIZE;
      }
      else {
	*alloc_size_p = NUM_BLOCKS(bblock_p->bb_size) * BLOCK_SIZE;
      }
    }
    if (file_p != NULL) {
      if (bblock_p->bb_file == DMALLOC_DEFAULT_FILE) {
	*file_p = NULL;
      }
      else {
	*file_p = (char *)bblock_p->bb_file;
      }
    }
    if (line_p != NULL) {
      *line_p = bblock_p->bb_line;
    }
    if (ret_attr_p != NULL) {
      /* if the line is blank then the file will be 0 or the return address */
      if (bblock_p->bb_line == DMALLOC_DEFAULT_LINE) {
	*ret_attr_p = (char *)bblock_p->bb_file;
      }
      else {
	*ret_attr_p = NULL;
      }
    }
#if STORE_SEEN_COUNT
    if (seen_cp != NULL) {
      *seen_cp = &bblock_p->bb_overhead.ov_seen_c;
    }
#endif
  }
  
  return NOERROR;
}

/*
 * Write new FILE, LINE, SIZE info into PNT -- which is in chunk-space
 */
static	int	chunk_write_info(const char *file, const unsigned int line,
				 void *pnt, const unsigned int size,
				 const char *where)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  int		block_n;
  
  /* NOTE: pnt is already in chunk-space */
  
  /* find which block it is in */
  bblock_p = find_bblock(pnt, NULL, NULL);
  if (bblock_p == NULL) {
    /* errno set in find_bblock */
    log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
    dmalloc_error("chunk_write_info");
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if (((char *)pnt - (char *)bblock_p->bb_mem) %
	(1 << bblock_p->bb_bit_n) != 0) {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
      dmalloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* find correct dblock_p */
    dblock_p = bblock_p->bb_dblock + ((char *)pnt - (char *)bblock_p->bb_mem) /
      (1 << bblock_p->bb_bit_n);
    
    if (dblock_p->db_bblock == bblock_p) {
      /* NOTE: we should run through free list here */
      dmalloc_errno = ERROR_NOT_USER;
      log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
      dmalloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* write info to system space */
    dblock_p->db_size = size;
    dblock_p->db_file = (char *)file;
    dblock_p->db_line = (unsigned short)line;
  }
  else {
    
    /* verify that the pointer is user allocated */
    if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
      dmalloc_errno = ERROR_NOT_USER;
      log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, where, FALSE);
      dmalloc_error("chunk_write_info");
      return ERROR;
    }
    
    block_n = NUM_BLOCKS(size);
    
    /* reset values in the bblocks */
    if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
      /*
       * If the user is requesting a page-aligned block of data then
       * we will need another block below the allocation just for the
       * fence information.  Ugh.
       */
      if (fence_bottom_size > 0) {
	block_n++;
      }
      set_bblock_admin(block_n, bblock_p, BBLOCK_VALLOC, line, size, file);
    }
    else {
      set_bblock_admin(block_n, bblock_p, BBLOCK_START_USER, line, size, file);
    }
  }
  
  return NOERROR;
}

/*
 * Log the heap structure plus information on the blocks if necessary
 */
void	_chunk_log_heap_map(void)
{
  bblock_adm_t	*bblock_adm_p;
  bblock_t	*bblock_p;
  char		line[BB_PER_ADMIN + 10];
  int		char_c, bblock_c, tblock_c, bb_admin_c;
  int		undef_b = 0;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("logging heap map information");
  }
  
  _dmalloc_message("heap-base = %#lx, heap-end = %#lx, size = %ld bytes",
		   (unsigned long)_heap_base, (unsigned long)_heap_last,
		   (long)HEAP_SIZE);
  
  for (bb_admin_c = 0, bblock_adm_p = bblock_adm_head;
       bblock_adm_p != NULL;
       bb_admin_c++, bblock_adm_p = bblock_adm_p->ba_next) {
    char_c = 0;
    
    bblock_p = bblock_adm_p->ba_blocks;
    for (bblock_c = 0; bblock_c < BB_PER_ADMIN; bblock_c++, bblock_p++) {
      if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ALLOCATED)) {
	line[char_c++] = '_';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
	if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
	  line[char_c++] = 'V';
	}
	else {
	  line[char_c++] = 'S';
	}
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_USER)) {
	line[char_c++] = 'U';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ADMIN)) {
	line[char_c++] = 'A';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
	line[char_c++] = 'd';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK_ADMIN)) {
	line[char_c++] = 'a';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FREE)) {
	line[char_c++] = 'F';
	continue;
	
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_EXTERNAL)) {
	line[char_c++] = 'E';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ADMIN_FREE)) {
	line[char_c++] = 'P';
	continue;
      }
    }
    
    /* dumping a line to the logfile */
    if (char_c > 0) {
      line[char_c] = '\0';
      _dmalloc_message("S%d:%s", bb_admin_c, line);
    }
  }
  
  /* if we are not logging blocks then leave */
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_BLOCKS)) {
    return;
  }
  
  tblock_c = 0;
  for (bb_admin_c = 0, bblock_adm_p = bblock_adm_head;
       bblock_adm_p != NULL;
       bb_admin_c++, bblock_adm_p = bblock_adm_p->ba_next) {
    
    for (bblock_c = 0, bblock_p = bblock_adm_p->ba_blocks;
	 bblock_c < BB_PER_ADMIN;
	 bblock_c++, bblock_p++, tblock_c++) {
      
      if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ALLOCATED)) {
	if (! undef_b) {
	  _dmalloc_message("%d (%#lx): not-allocated block (till next)",
			   tblock_c, (unsigned long)BLOCK_POINTER(tblock_c));
	  undef_b = 1;
	}
	continue;
      }
      
      undef_b = 0;
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
	_dmalloc_message("%d (%#lx): start-of-user block: %ld bytes from '%s'",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 bblock_p->bb_size,
			 _chunk_display_where(bblock_p->bb_file,
					      bblock_p->bb_line));
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_USER)) {
	_dmalloc_message("%d (%#lx): user continuation block",
			tblock_c, (unsigned long)BLOCK_POINTER(tblock_c));
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ADMIN)) {
	_dmalloc_message("%d (%#lx): administration block, position = %ld",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 bblock_p->bb_free_n);
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
	_dmalloc_message("%d (%#lx): dblock block, bit_n = %d",
			tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			bblock_p->bb_bit_n);
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK_ADMIN)) {
	_dmalloc_message("%d (%#lx): dblock-admin block",
			tblock_c, (unsigned long)BLOCK_POINTER(tblock_c));
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FREE)) {
	_dmalloc_message("%d (%#lx): free block of %ld blocks, next at %#lx",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 bblock_p->bb_block_n,
			 (unsigned long)bblock_p->bb_mem);
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_EXTERNAL)) {
	_dmalloc_message("%d (%#lx): externally used block to %#lx",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 (unsigned long)bblock_p->bb_mem);
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_ADMIN_FREE)) {
	_dmalloc_message("%d (%#lx): admin free pointer to offset %ld",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 bblock_p->bb_free_n);
	continue;
      }
    }
  }
}

/************************** low-level user functions *************************/

/*
 * Get a SIZE chunk of memory for FILE at LINE.  If CALLOC_B then
 * count this as a calloc not a malloc call.  If REALLOC_B then don't
 * count it as a malloc call.  If ALIGNMENT is greater than 0 then try
 * to align the returned block.
 */
void	*_chunk_malloc(const char *file, const unsigned int line,
		       const unsigned int size, const int calloc_b,
		       const int realloc_b, const unsigned int alignment)
{
  unsigned int	bit_n, byte_n = size;
  int		valloc_b = 0, memalign_b = 0;
  bblock_t	*bblock_p;
  overhead_t	*over_p;
  const char	*trans_log;
  void		*pnt;
  
  /* counts calls to malloc */
  if (calloc_b) {
    calloc_count++;
  }
  else if (alignment == BLOCK_SIZE) {
    valloc_count++;
    valloc_b = 1;
  }
  else if (alignment > 0) {
    memalign_count++;
    memalign_b = 1;
  }
  else if (! realloc_b) {
    malloc_count++;
  }
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (byte_n == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, NULL, 0, "bad zero byte allocation request",
		   "malloc", FALSE);
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
#endif
  
  /* adjust the size */
  byte_n += fence_overhead_size;
  
  /* count the bits */
  NUM_BITS(byte_n, bit_n);
  
  /* have we exceeded the upper bounds */
  if (bit_n > LARGEST_BLOCK) {
    dmalloc_errno = ERROR_TOO_BIG;
    log_error_info(file, line, NULL, 0, NULL, "malloc", FALSE);
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* normalize to smallest_block.  No use spending 16 bytes to admin 1 byte */
  if (bit_n < smallest_block) {
    bit_n = smallest_block;
  }
  
  /* monitor current allocation level */
  alloc_current += byte_n;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_total += byte_n;
  alloc_one_max = MAX(alloc_one_max, byte_n);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  /* allocate divided block if small */
  if (bit_n < BASIC_BLOCK && (! valloc_b)) {
    pnt = get_dblock(bit_n, byte_n, file, line, &over_p);
    if (pnt == NULL) {
      return MALLOC_ERROR;
    }
    
    alloc_cur_given += 1 << bit_n;
    alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)) {
      (void)memset(pnt, BLANK_CHAR, 1 << bit_n);
    }
  }
  else {
    int		block_n, given;
    
    /*
     * allocate some bblock space
     */
    
    /* handle blocks */
    block_n = NUM_BLOCKS(byte_n);
    /*
     * If the user is requesting a page-aligned block of data then we
     * will need another block below the allocation just for the fence
     * information.  Ugh.
     */
    if (valloc_b && fence_bottom_size > 0) {
      block_n++;
    }
    bblock_p = get_bblocks(block_n, &pnt);
    if (bblock_p == NULL) {
      return MALLOC_ERROR;
    }
    
    /* initialize the bblocks */
    if (valloc_b) {
      set_bblock_admin(block_n, bblock_p, BBLOCK_VALLOC, line, byte_n, file);
    }
    else {
      set_bblock_admin(block_n, bblock_p, BBLOCK_START_USER, line, byte_n,
		       file);
    }
    
    given = block_n * BLOCK_SIZE;
    alloc_cur_given += given;
    alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)) {
      (void)memset(pnt, BLANK_CHAR, given);
    }
    
#if STORE_SEEN_COUNT
    bblock_p->bb_overhead.ov_seen_c++;
#endif
#if STORE_ITERATION_COUNT
    bblock_p->bb_overhead.ov_iteration = _dmalloc_iter_c;
#endif
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIME && HAVE_TIME
      bblock_p->bb_overhead.ov_time = time(NULL);
#endif
#if STORE_TIMEVAL
      GET_TIMEVAL(bblock_p->bb_overhead.ov_timeval);
#endif
    }
    
#if STORE_THREAD_ID
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_THREAD_ID)) {
      bblock_p->bb_overhead.ov_thread_id = THREAD_GET_ID;
    }
#endif
    
    over_p = &bblock_p->bb_overhead;
    
    /* we adjust the user pointer up to right below the 2nd block */
    if (valloc_b && fence_bottom_size > 0) {
      pnt = (char *)pnt + (BLOCK_SIZE - fence_bottom_size);
    }
  }
  
  /* write fence post info if needed */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    FENCE_WRITE(pnt, byte_n);
  }
  
  pnt = CHUNK_TO_USER(pnt);
  
  if (calloc_b) {
    (void)memset(pnt, 0, size);
  }
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    if (calloc_b) {
      trans_log = "calloc";
    }
    else if (memalign_b) {
      trans_log = "memalign";
    }
    else if (valloc_b) {
      trans_log = "valloc";
    }
    else {
      trans_log = "alloc";
    }
    _dmalloc_message("*** %s: at '%s' for %d bytes, got '%s'",
		     trans_log, _chunk_display_where(file, line), size,
		     display_pnt(pnt, over_p));
  }
  
  return pnt;
}

/*
 * Frees PNT from the heap.  REALLOC_B set if realloc is freeing a
 * pointer so doing count it as a free.  Returns FREE_ERROR or
 * FREE_NOERROR.
 *
 * NOTE: should be above _chunk_realloc which calls it.
 */
int	_chunk_free(const char *file, const unsigned int line, void *pnt,
		    const int realloc_b)
{
  unsigned int	bit_n, block_n, given;
  int		valloc_b = 0;
  bblock_t	*bblock_p, *prev_p, *next_p, *list_p, *this_p;
  dblock_t	*dblock_p;
  
  /* counts calls to free */
  if (! realloc_b) {
    free_count++;
  }
  
  if (pnt == NULL) {
#if ALLOW_FREE_NULL_MESSAGE
    _dmalloc_message("WARNING: tried to free(0) from '%s'",
		     _chunk_display_where(file, line));
#endif
    /*
     * NOTE: we have here both a default in the settings.h file and a
     * runtime token in case people want to turn it on or off at
     * runtime.
     */
#if ALLOW_FREE_NULL
    return FREE_NOERROR;
#else
    dmalloc_errno = ERROR_IS_NULL;
    if (! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_FREE_NULL)) {
      log_error_info(file, line, pnt, 0, "invalid pointer", "free", FALSE);
      dmalloc_error("_chunk_free");
    }
    return FREE_ERROR;
#endif
  }
  
  /* adjust the pointer down if fence-posting */
  pnt = USER_TO_CHUNK(pnt);
  
  /* find which block it is in */
  bblock_p = find_bblock(pnt, &prev_p, &next_p);
  if (bblock_p == NULL) {
    /* errno set in find_bblock */
    log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  alloc_cur_pnts--;
  
  /* are we free'ing a dblock entry? */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    
    /* on a mini-block boundary? */
    if (((char *)pnt - (char *)bblock_p->bb_mem) %
	(1 << bblock_p->bb_bit_n) != 0) {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, "free", FALSE);
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    /* find correct dblock_p */
    dblock_p = bblock_p->bb_dblock + ((char *)pnt - (char *)bblock_p->bb_mem) /
      (1 << bblock_p->bb_bit_n);
    
    if (dblock_p->db_bblock == bblock_p) {
      /* NOTE: we should run through free list here? */
      dmalloc_errno = ERROR_ALREADY_FREE;
      log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, "free", FALSE);
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
#if STORE_SEEN_COUNT
    dblock_p->db_overhead.ov_seen_c++;
#endif
    
    /* print transaction info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
      _dmalloc_message("*** free: at '%s' pnt '%s': size %d, alloced at '%s'",
		       _chunk_display_where(file, line),
		       display_pnt(CHUNK_TO_USER(pnt), &dblock_p->db_overhead),
		       dblock_p->db_size - fence_overhead_size,
		       chunk_display_where2(dblock_p->db_file,
					    dblock_p->db_line));
    }
    
    /* check fence-post, probably again */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
      if (fence_read(dblock_p->db_file, dblock_p->db_line, pnt,
		     dblock_p->db_size, "free") != NOERROR) {
	return FREE_ERROR;
      }
    }
    
    /* count the bits */
    bit_n = bblock_p->bb_bit_n;
    
    /* monitor current allocation level */
    alloc_current -= dblock_p->db_size;
    alloc_cur_given -= 1 << bit_n;
    free_space_count += 1 << bit_n;
    
    /* adjust the pointer info structure */
    dblock_p->db_bblock = bblock_p;
#if FREED_POINTER_DELAY
    dblock_p->db_reuse_iter = _dmalloc_iter_c + FREED_POINTER_DELAY;
#endif
    /* put pointer on the dblock free list if we are reusing memory */
    if (! BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)) {
      dblock_p->db_next = free_dblock[bit_n];
      free_dblock[bit_n] = dblock_p;
    }
    
    /* should we set free memory with BLANK_CHAR? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
      (void)memset(pnt, BLANK_CHAR, 1 << bit_n);
    }
    
    return FREE_NOERROR;
  }
  
  /* was it a valloc-d allocation? */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
    valloc_b = 1;
  }
  
  /*
   * Since we are in the basic-block section, the pointer should
   * either be on a block boundary or have the valloc bit set and be
   * right below.
   */
  if (((! valloc_b) && (! ON_BLOCK(pnt)))
      || (valloc_b && (! ON_BLOCK((char *)pnt + fence_bottom_size)))) {
    dmalloc_errno = ERROR_NOT_ON_BLOCK;
    log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* are we on a normal block */
  if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
    dmalloc_errno = ERROR_NOT_START_USER;
    log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
#if STORE_SEEN_COUNT
  bblock_p->bb_overhead.ov_seen_c++;
#endif
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("*** free: at '%s' pnt '%s': size %ld, alloced at '%s'",
		     _chunk_display_where(file, line),
		     display_pnt(CHUNK_TO_USER(pnt), &bblock_p->bb_overhead),
		     bblock_p->bb_size - fence_overhead_size,
		     chunk_display_where2(bblock_p->bb_file,
					  bblock_p->bb_line));
  }
  
  /* check fence-post, probably again */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    if (fence_read(bblock_p->bb_file, bblock_p->bb_line, pnt,
		   bblock_p->bb_size, "free") != NOERROR) {
      return FREE_ERROR;
    }
  }
  
  block_n = NUM_BLOCKS(bblock_p->bb_size);
  /*
   * If the user is requesting a page-aligned block of data then we
   * will need another block below the allocation just for the fence
   * information.  Ugh.
   */
  if (valloc_b && fence_bottom_size > 0) {
    block_n++;
  }
  given = block_n * BLOCK_SIZE;
  NUM_BITS(given, bit_n);
  
  /* if we are smaller than a basic block and not valloc then error */ 
  if (bit_n < BASIC_BLOCK && (! valloc_b)) {
    dmalloc_errno = ERROR_BAD_SIZE_INFO;
    log_error_info(file, line, CHUNK_TO_USER(pnt), 0, NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* monitor current allocation level */
  alloc_current -= bblock_p->bb_size;
  alloc_cur_given -= given;
  free_space_count += given;
  
  /*
   * should we set free memory with BLANK_CHAR?
   * NOTE: we do this hear because block_n might change below
   */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
    /*
     * if we have a valloc block with fence post info, then shift the
     * user pointer down to the start of the block
     */
    if (valloc_b && fence_bottom_size > 0) {
      pnt = (char *)pnt - (BLOCK_SIZE - fence_bottom_size);
    }
    (void)memset(pnt, BLANK_CHAR, block_n * BLOCK_SIZE);
  }
  
  /*
   * Check above and below the free bblock looking for neighbors that
   * are free so we can add them together and put them in a different
   * free slot.
   *
   * NOTE: all of these block's reuse-iter count will be moved ahead
   * because we are encorporating in this newly freed block.
   */
  
  if (prev_p != NULL && BIT_IS_SET(prev_p->bb_flags, BBLOCK_FREE)) {
    
    /* find prev in free list and remove it */
    for (this_p = free_bblock[prev_p->bb_bit_n], list_p = NULL;
	 this_p != NULL;
	 list_p = this_p, this_p = this_p->bb_next) {
      if (this_p == prev_p) {
	break;
      }
    }
    
    /* we better have found it */
    if (this_p == NULL) {
      dmalloc_errno = ERROR_BAD_FREE_LIST;
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    if (list_p == NULL) {
      free_bblock[prev_p->bb_bit_n] = prev_p->bb_next;
    }
    else {
      list_p->bb_next = prev_p->bb_next;
    }
    
    block_n += prev_p->bb_block_n;
    NUM_BITS(block_n * BLOCK_SIZE, bit_n);
    bblock_p = prev_p;
  }
  if (next_p != NULL && BIT_IS_SET(next_p->bb_flags, BBLOCK_FREE)) {
    /* find next in free list and remove it */
    for (this_p = free_bblock[next_p->bb_bit_n], list_p = NULL;
	 this_p != NULL;
	 list_p = this_p, this_p = this_p->bb_next) {
      if (this_p == next_p) {
	break;
      }
    }
    
    /* we better have found it */
    if (this_p == NULL) {
      dmalloc_errno = ERROR_BAD_FREE_LIST;
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    if (list_p == NULL) {
      free_bblock[next_p->bb_bit_n] = next_p->bb_next;
    }
    else {
      list_p->bb_next = next_p->bb_next;
    }
    
    block_n += next_p->bb_block_n;
    NUM_BITS(block_n * BLOCK_SIZE, bit_n);
  }
  
  /* set the information for the bblock(s) */
  set_bblock_admin(block_n, bblock_p, BBLOCK_FREE, bit_n, 0,
		   free_bblock[bit_n]);
  
  /* block goes at the start of the free list */
  free_bblock[bit_n] = bblock_p;
  
  return FREE_NOERROR;
}

/*
 * Reallocate a section of memory
 */
void	*_chunk_realloc(const char *file, const unsigned int line,
			void *old_p, unsigned int new_size,
			const int recalloc_b)
{
  void		*new_p, *ret_addr;
  char		*old_file;
  const char	*trans_log;
  unsigned long	*seen_cp;
  unsigned int	old_size, size, old_line, alloc_size;
  unsigned int	old_bit_n, new_bit_n;
  
  /* counts calls to realloc */
  if (recalloc_b) {
    recalloc_count++;
  }
  else {
    realloc_count++;
  }
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (new_size == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, NULL, 0, "bad zero byte allocation request",
		   "realloc", FALSE);
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
#endif
  
  /* by now malloc.c should have taken care of the realloc(NULL) case */
  if (old_p == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    log_error_info(file, line, old_p, 0, "invalid pointer", "realloc", FALSE);
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
  
  /*
   * TODO: for bblocks it would be nice to examine the above memory
   * looking for free blocks that we can absorb into this one.
   */
  
  /* get info about old pointer */
  if (_chunk_read_info(old_p, &old_size, &alloc_size, &old_file, &old_line,
		       &ret_addr, "realloc", &seen_cp) != NOERROR) {
    return REALLOC_ERROR;
  }
  
  if (ret_addr != NULL) {
    old_file = ret_addr;
  }
  
  /* adjust the pointer down if fence-posting */
  old_p = USER_TO_CHUNK(old_p);
  new_size += fence_overhead_size;
  
  /* check the fence-posting */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    if (fence_read(file, line, old_p, old_size, "realloc") != NOERROR) {
      return REALLOC_ERROR;
    }
  }
  
  /* get the old and new bit sizes */
  NUM_BITS(alloc_size, old_bit_n);
  NUM_BITS(new_size, new_bit_n);
  
  /* if we are not realloc copying and the size is the same */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_REALLOC_COPY)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)
      || old_bit_n != new_bit_n
      || NUM_BLOCKS(old_size) != NUM_BLOCKS(new_size)) {
    
    /* readjust info */
    old_p = CHUNK_TO_USER(old_p);
    old_size -= fence_overhead_size;
    new_size -= fence_overhead_size;
    
    /* allocate space for new chunk -- this will */
    new_p = _chunk_malloc(file, line, new_size, recalloc_b, 1, 0);
    if (new_p == MALLOC_ERROR) {
      return REALLOC_ERROR;
    }
    
    /*
     * NOTE: _chunk_malloc() already took care of the fence stuff and
     * an zeroing of memory.
     */
    
    /* copy stuff into new section of memory */
    size = MIN(new_size, old_size);
    if (size > 0) {
      memcpy((char *)new_p, (char *)old_p, size);
    }
    
    /* free old pointer */
    if (_chunk_free(file, line, old_p, 1) != FREE_NOERROR) {
      return REALLOC_ERROR;
    }
  }
  else {
    /*
     * monitor current allocation level
     *
     * NOTE: we do this here since the malloc/free used above take care
     * on if in that section
     */
    alloc_current += new_size - old_size;
    alloc_maximum = MAX(alloc_maximum, alloc_current);
    alloc_total += new_size;
    alloc_one_max = MAX(alloc_one_max, new_size);
    
    /* monitor pointer usage */
    alloc_tot_pnts++;
    
    /* reuse the old-pointer */
    new_p = old_p;
    
    /* rewrite size information */
    if (chunk_write_info(file, line, new_p, new_size, "realloc") != NOERROR) {
      return REALLOC_ERROR;
    }
    
    /* overwrite to-be-alloced or non-used portion of memory */
    size = MIN(new_size, old_size);
    
    /* NOTE: using same number of blocks so NUM_BLOCKS works with either */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)
	&& alloc_size > size) {
      (void)memset((char *)new_p + size, BLANK_CHAR, alloc_size - size);
    }
    
    /* write in fence-post info and adjust new pointer over fence info */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
      FENCE_WRITE(new_p, new_size);
    }
    
    new_p = CHUNK_TO_USER(new_p);
    old_p = CHUNK_TO_USER(old_p);
    old_size -= fence_overhead_size;
    new_size -= fence_overhead_size;
    
    if (recalloc_b && new_size > old_size) {
      (void)memset((char *)new_p + old_size, 0, new_size - old_size);
    }
    
#if STORE_SEEN_COUNT
    /* we see in inbound and outbound so we need to increment by 2 */
    *seen_cp += 2;
#endif
  }
  
  /* new_p is already user-level real */
  
  /*
   * do we need to print transaction info?
   *
   * NOTE: pointers and sizes here a user-level real
   */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    if (recalloc_b) {
      trans_log = "recalloc";
    }
    else {
      trans_log = "realloc";
    }
    _dmalloc_message("*** %s: at '%s' from '%#lx' (%u bytes) file '%s' to '%#lx' (%u bytes)",
		     trans_log, _chunk_display_where(file, line),
		     (unsigned long)old_p, old_size,
		     chunk_display_where2(old_file, old_line),
		     (unsigned long)new_p, new_size);
  }
  
  return new_p;
}

/***************************** diagnostic routines ***************************/

/*
 * Log present free and used lists
 */
void	_chunk_list_count(void)
{
  int		bit_c, block_c;
  char		info[256], tmp[80];
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  
  /* dump the free (and later used) list counts */
  info[0] = '\0';
  for (bit_c = smallest_block; bit_c < MAX_SLOTS; bit_c++) {
    if (bit_c < BASIC_BLOCK) {
      for (block_c = 0, dblock_p = free_dblock[bit_c];
	   dblock_p != NULL;
	   block_c++, dblock_p = dblock_p->db_next) {
      }
    }
    else {
      for (block_c = 0, bblock_p = free_bblock[bit_c];
	   bblock_p != NULL;
	   block_c++, bblock_p = bblock_p->bb_next) {
      }
    }
    
    if (block_c > 0) {
      (void)sprintf(tmp, " %d/%d", block_c, bit_c);
      (void)strcat(info, tmp);
    }
  }
  
  _dmalloc_message("free bucket count/bits: %s", info);
}

/*
 * Log statistics on the heap
 */
void	_chunk_stats(void)
{
  long		overhead, tot_space, wasted;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("dumping chunk statistics");
  }
  
  tot_space = alloc_current + free_space_count;
  overhead = (bblock_adm_count + dblock_adm_count) * BLOCK_SIZE;
  wasted = tot_space - alloc_max_given;
  
  /* version information */
  _dmalloc_message("basic-block %d bytes, alignment %d bytes, heap grows %s",
		   BLOCK_SIZE, ALLOCATION_ALIGNMENT,
		   (HEAP_GROWS_UP ? "up" : "down"));
  
  /* general heap information */
  _dmalloc_message("heap: %#lx to %#lx, size %ld bytes (%ld blocks), checked %ld",
		   (unsigned long)_heap_base, (unsigned long)_heap_last,
		   (long)HEAP_SIZE, bblock_count, check_count);
  
  /* log user allocation information */
  _dmalloc_message("alloc calls: malloc %ld, calloc %ld, realloc %ld, free %ld",
		   malloc_count, calloc_count, realloc_count, free_count);
  _dmalloc_message("alloc calls: recalloc %ld, memalign %ld, valloc %ld",
		   recalloc_count, memalign_count, valloc_count);
  _dmalloc_message(" total memory allocated: %ld bytes (%ld pnts)",
		  alloc_total, alloc_tot_pnts);
  
  /* maximum stats */
  _dmalloc_message(" max in use at one time: %ld bytes (%ld pnts)",
		  alloc_maximum, alloc_max_pnts);
  _dmalloc_message("max alloced with 1 call: %ld bytes",
		  alloc_one_max);
  _dmalloc_message("max alloc rounding loss: %ld bytes (%ld%%)",
		  alloc_max_given - alloc_maximum,
		  (alloc_max_given == 0 ? 0 :
		   ((alloc_max_given - alloc_maximum) * 100) /
		   alloc_max_given));
  /* wasted could be < 0 */
  if (wasted <= 0) {
    _dmalloc_message("max memory space wasted: 0 bytes (0%%)");
  }
  else {
    _dmalloc_message("max memory space wasted: %ld bytes (%ld%%)",
		    wasted,
		    (tot_space == 0 ? 0 : ((wasted * 100) / tot_space)));
  }
  
  /* final stats */
  _dmalloc_message("final user memory space: basic %ld, divided %ld, %ld bytes",
		   bblock_count - bblock_adm_count - dblock_count -
		   dblock_adm_count - extern_count, dblock_count,
		   tot_space);
  _dmalloc_message("   final admin overhead: basic %ld, divided %ld, %ld bytes (%ld%%)",
		   bblock_adm_count, dblock_adm_count, overhead,
		   (HEAP_SIZE == 0 ? 0 : (overhead * 100) / HEAP_SIZE));
  _dmalloc_message("   final external space: %ld bytes (%ld blocks)",
		   extern_count * BLOCK_SIZE, extern_count);
}

/*
 * Dump the unfreed memory, logs the unfreed information to logger
 */
void	_chunk_dump_unfreed(void)
{
  bblock_adm_t	*this_adm_p;
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  void		*pnt;
  int		unknown_b;
  char		out[DUMP_SPACE * 4];
  int		unknown_size_c = 0, unknown_block_c = 0, out_len;
  int		size_c = 0, block_c = 0;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("dumping the unfreed pointers");
  }
  
  /* has anything been alloced yet? */
  this_adm_p = bblock_adm_head;
  if (this_adm_p == NULL) {
    return;
  }
  
  /* check out the basic blocks */
  for (bblock_p = this_adm_p->ba_blocks;; bblock_p++) {
    
    /* are we at the end of the bb_admin section */
    if (bblock_p >= this_adm_p->ba_blocks + BB_PER_ADMIN) {
      this_adm_p = this_adm_p->ba_next;
      
      /* are we done? */
      if (this_adm_p == NULL) {
	break;
      }
      
      bblock_p = this_adm_p->ba_blocks;
    }
    
    /*
     * check for different types
     */
    switch (BBLOCK_FLAG_TYPE(bblock_p->bb_flags)) {
      
    case BBLOCK_START_USER:
      /* find pointer to memory chunk */
      pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			  (bblock_p - this_adm_p->ba_blocks));
      
      unknown_b = 0;
      
      /* unknown pointer? */
      if (bblock_p->bb_file == DMALLOC_DEFAULT_FILE
	  || bblock_p->bb_line == DMALLOC_DEFAULT_LINE) {
	unknown_block_c++;
	unknown_size_c += bblock_p->bb_size - fence_overhead_size;
	unknown_b = 1;
      }
      
      if ((! unknown_b) || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_UNKNOWN)) {
	_dmalloc_message("not freed: '%s' (%ld bytes) from '%s'",
			 display_pnt(CHUNK_TO_USER(pnt),
				     &bblock_p->bb_overhead),
			 bblock_p->bb_size - fence_overhead_size,
			 _chunk_display_where(bblock_p->bb_file,
					      bblock_p->bb_line));
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE)) {
	  out_len = expand_chars((char *)CHUNK_TO_USER(pnt), DUMP_SPACE,
				 out, sizeof(out));
	  _dmalloc_message("Dump of '%#lx': '%.*s'",
			   (unsigned long)CHUNK_TO_USER(pnt), out_len, out);
	}
      }
      
      size_c += bblock_p->bb_size - fence_overhead_size;
      block_c++;
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      for (dblock_p = bblock_p->bb_slot_p->da_block;
	   dblock_p < bblock_p->bb_slot_p->da_block + DB_PER_ADMIN;
	   dblock_p++) {
	
	/* see if this admin slot has ever been used */
	if (dblock_p->db_bblock == NULL && dblock_p->db_next == NULL) {
	  continue;
	}
	
	/*
	 * is this slot in a free list?
	 * NOTE: this may bypass a non-freed entry if the db_file pointer
	 * (unioned to db_next) points to return address in the heap
	 */
	if (dblock_p->db_next == NULL || IS_IN_HEAP(dblock_p->db_next)) {
	  continue;
	}
	
	{
	  bblock_adm_t	*bba_p;
	  bblock_t	*bb_p;
	  
	  bba_p = bblock_adm_head;
	  
	  /* check out the basic blocks */
	  for (bb_p = bba_p->ba_blocks;; bb_p++) {
	    
	    /* are we at the end of the bb_admin section */
	    if (bb_p >= bba_p->ba_blocks + BB_PER_ADMIN) {
	      bba_p = bba_p->ba_next;
	      
	      /* are we done? */
	      if (bba_p == NULL) {
		break;
	      }
	      
	      bb_p = bba_p->ba_blocks;
	    }
	    
	    if (bb_p->bb_flags != BBLOCK_DBLOCK) {
	      continue;
	    }
	    
	    if (dblock_p >= bb_p->bb_dblock
		&& dblock_p < bb_p->bb_dblock +
		(1 << (BASIC_BLOCK - bb_p->bb_bit_n))) {
	      break;
	    }
	  }
	  
	  if (bba_p == NULL) {
	    dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	    dmalloc_error("_chunk_dump_unfreed");
	    return;
	  }
	  
	  pnt = (char *)bb_p->bb_mem + (dblock_p - bb_p->bb_dblock) *
	    (1 << bb_p->bb_bit_n);
	}
	
	unknown_b = 0;
	
	/* unknown pointer? */
	if (dblock_p->db_file == DMALLOC_DEFAULT_FILE
	    || dblock_p->db_line == DMALLOC_DEFAULT_LINE) {
	  unknown_block_c++;
	  unknown_size_c += dblock_p->db_size - fence_overhead_size;
	  unknown_b = 1;
	}
	
	if ((! unknown_b) || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_UNKNOWN)) {
	  _dmalloc_message("not freed: '%s' (%d bytes) from '%s'",
			   display_pnt(CHUNK_TO_USER(pnt),
				       &dblock_p->db_overhead),
			   dblock_p->db_size - fence_overhead_size,
			   _chunk_display_where(dblock_p->db_file,
						dblock_p->db_line));
	  
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE)) {
	    out_len = expand_chars((char *)CHUNK_TO_USER(pnt), DUMP_SPACE,
				   out, sizeof(out));
	    _dmalloc_message("Dump of '%#lx': '%.*s'",
			     (unsigned long)CHUNK_TO_USER(pnt), out_len, out);
	  }
	}
	
	size_c += dblock_p->db_size - fence_overhead_size;
	block_c++;
      }
      break;
    }
  }
  
  /* copy out size of pointers */
  if (block_c > 0) {
    if (block_c - unknown_block_c > 0) {
      _dmalloc_message("  known memory not freed: %d pointer%s, %d bytes",
		       block_c - unknown_block_c,
		       (block_c - unknown_block_c == 1 ? "" : "s"),
		       size_c - unknown_size_c);
    }
    if (unknown_block_c > 0) {
      _dmalloc_message("unknown memory not freed: %d pointer%s, %d bytes",
		       unknown_block_c, (unknown_block_c == 1 ? "" : "s"),
		       unknown_size_c);
    }
  }
}
