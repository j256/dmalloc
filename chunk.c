/*
 * Memory chunk low-level allocation routines
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
 * $Id: chunk.c,v 1.173 2001/07/12 22:28:25 gray Exp $
 */

/*
 * This file contains algorithm level routine for the heap.  They handle the
 * manipulation and administration of chunks of memory.
 */

#include <ctype.h>

#if HAVE_STRING_H
# include <string.h>
#endif

#define DMALLOC_DISABLE

#include "conf.h"

#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
# include TIMEVAL_INCLUDE
#endif
#else
# if STORE_TIME
#  ifdef TIME_INCLUDE
#   include TIME_INCLUDE
#  endif
# endif
#endif

#include "dmalloc.h"

#include "chunk.h"
#include "chunk_loc.h"
#include "compat.h"
#include "debug_val.h"
#include "dmalloc_loc.h"
#include "dmalloc_tab.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "protect.h"

#if INCLUDE_RCS_IDS
#if IDENT_WORKS
#ident "@(#) $Id: chunk.c,v 1.173 2001/07/12 22:28:25 gray Exp $"
#else
static	char	*rcs_id =
  "@(#) $Id: chunk.c,v 1.173 2001/07/12 22:28:25 gray Exp $";
#endif
#endif

/*
 * Library Copyright and URL information for ident and what programs
 */
#if IDENT_WORKS
#ident "@(#) $Copyright: Dmalloc package Copyright 2000 by Gray Watson $"
#ident "@(#) $URL: Source for dmalloc available from http://dmalloc.com/ $"
#else
static	char	*copyright =
  "@(#) $Copyright: Dmalloc package Copyright 2000 by Gray Watson $";
static	char	*source_url =
  "@(#) $URL: Source for dmalloc available from http://dmalloc.com/ $";
#endif

#if LOCK_THREADS
#if IDENT_WORKS
#ident "@(#) $Information: lock-threads is enabled $"
#else
static char *information = "@(#) $Information: lock-threads is enabled $";
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

/* memory stats */
static	unsigned long	alloc_current = 0;	/* current memory usage */
static	unsigned long	alloc_maximum = 0;	/* maximum memory usage  */
static	unsigned long	alloc_cur_given = 0;	/* current mem given */
static	unsigned long	alloc_max_given = 0;	/* maximum mem given  */
static	unsigned long	alloc_total = 0;	/* total allocation */
static	unsigned long	alloc_one_max = 0;	/* maximum at once */
static	unsigned long	free_space_count = 0;	/* count the free bytes */

/* pointer stats */
static	unsigned long	alloc_cur_pnts = 0;	/* current pointers */
static	unsigned long	alloc_max_pnts = 0;	/* maximum pointers */
static	unsigned long	alloc_tot_pnts = 0;	/* current pointers */

/* admin counts */
static	unsigned long	bblock_adm_count = 0;	/* count of bblock_admin */
static	unsigned long	dblock_adm_count = 0;	/* count of dblock_admin */
static	unsigned long	bblock_count = 0;	/* count of basic-blocks */
static	unsigned long	dblock_count = 0;	/* count of divided-blocks */
static	unsigned long	extern_count = 0;	/* count of external blocks */
static	unsigned long	check_count = 0;	/* count of heap-checks */

/* alloc counts */
static	unsigned long	malloc_count = 0;	/* count the mallocs */
static	unsigned long	calloc_count = 0;	/* # callocs, done in alloc */
static	unsigned long	realloc_count = 0;	/* count the reallocs */
static	unsigned long	recalloc_count = 0;	/* count the reallocs */
static	unsigned long	memalign_count = 0;	/* count the memaligns */
static	unsigned long	valloc_count = 0;	/* count the veallocs */
static	unsigned long	free_count = 0;		/* count the frees */

/******************************* misc routines *******************************/

/*
 * int _chunk_startup
 * 
 * DESCRIPTION:
 *
 * Startup the low level malloc routines.
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
int	_chunk_startup(void)
{
  unsigned int	bin_c;
  unsigned long	num;
  
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
    return 0;
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
  
  return 1;
}

/*
 * static int expand_chars
 *
 * DESCRIPTION:
 *
 * Copies a buffer into a output buffer while translates
 * non-printables into %03o octal values.  If it can, it will also
 * translate certain \ characters (\r, \n, etc.) into \\%c.  The
 * routine is useful for printing out binary values.
 *
 * Note: It does _not_ add a \0 at the end of the output buffer.
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
  char	 		*out_p = out, *bounds_p;
  
  /* setup our max pointer */
  bounds_p = out + out_size;
  
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
      if (out_p + 2 >= bounds_p) {
	break;
      }
      out_p += loc_snprintf(out_p, bounds_p - out_p, "\\%c", *(spec_p - 1));
      continue;
    }
    
    /* print out any 7-bit printable characters */
    if (*buf_p < 128 && isprint(*buf_p)) {
      if (out_p + 1 >= bounds_p) {
	break;
      }
      *out_p = *(char *)buf_p;
      out_p += 1;
    }
    else {
      if (out_p + 4 >= bounds_p) {
	break;
      }
      out_p += loc_snprintf(out_p, bounds_p - out_p, "\\%03o", *buf_p);
    }
  }
  /* try to punch the null if we have space in case the %.*s doesn't work */
  if (out_p < bounds_p) {
    *out_p = '\0';
  }
  
  return out_p - out;
}

/*
 * Describe pnt from its FILE, LINE into BUF.  Returns BUF.
 */
char	*_chunk_desc_pnt(char *buf, const int buf_size,
			const char *file, const unsigned int line)
{
  if (file == DMALLOC_DEFAULT_FILE && line == DMALLOC_DEFAULT_LINE) {
    (void)loc_snprintf(buf, buf_size, "unknown");
  }
  else if (line == DMALLOC_DEFAULT_LINE) {
    (void)loc_snprintf(buf, buf_size, "ra=%#lx", (unsigned long)file);
  }
  else if (file == DMALLOC_DEFAULT_FILE) {
    (void)loc_snprintf(buf, buf_size, "ra=ERROR(line=%u)", line);
  }
  else {
    (void)loc_snprintf(buf, buf_size, "%.*s:%u", MAX_FILE_LENGTH, file, line);
  }
  
  return buf;
}

/*
 * Display a pointer PNT and information about it.
 */
static	char	*display_pnt(const void *user_pnt, const overhead_t *over_p,
			     char *buf, const int buf_size)
{
  char	*buf_p, *bounds_p;
  int	elapsed_b;
  
  buf_p = buf;
  bounds_p = buf_p + buf_size;
  
  buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "%#lx",
			(unsigned long)user_pnt);
  
#if STORE_SEEN_COUNT
  buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|s%lu", over_p->ov_seen_c);
#endif
  
#if STORE_ITERATION_COUNT
  buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|i%lu",
			over_p->ov_iteration);
#endif
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)) {
    elapsed_b = 1;
  }
  else {
    elapsed_b = 0;
  }
  if (elapsed_b || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
    {
      char	time_buf[64];
      buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|w%s",
			    _dmalloc_ptimeval(&over_p->ov_timeval, time_buf,
					      sizeof(time_buf), elapsed_b));
    }
#else
#if STORE_TIME
    {
      char	time_buf[64];
      buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|w%s",
			    _dmalloc_ptime(&over_p->ov_time, time_buf,
					   sizeof(time_buf), elapsed_b));
    }
#endif
#endif
  }
  
#if LOG_THREAD_ID
  {
    char	thread_id[256];
    
    buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "|t");
    THREAD_ID_TO_STRING(thread_id, sizeof(thread_id), over_p->ov_thread_id);
    buf_p += loc_snprintf(buf_p, bounds_p - buf_p, "%s", thread_id);
  }
#endif
  
  return buf;
}

/*
 * static void log_error_info
 *
 * DESCRIPTION:
 *
 * Logging information about a pointer, usually during an error
 * condition.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * now_file -> File from where we generated the error.
 *
 * now_line -> Line number from where we generated the error.
 *
 * prev_file -> File of a previous location.  For instance the
 * location of a previous free() if we tried to free something twice.
 *
 * prev_line -> Line number of a previous location.  For instance the
 * location of a previous free() if we tried to free something twice.
 *
 * user_pnt -> Pointer in question.
 *
 * size -> Size that the user requested including any fence space.
 *
 * reason -> Reason string why something happened.
 *
 * where -> What routine is calling log_error_info.  For instance
 * malloc or chunk_check.
 */
static	void	log_error_info(const char *now_file,
			       const unsigned int now_line,
			       const char *prev_file,
			       const unsigned int prev_line,
			       const void *user_pnt, const unsigned int size,
			       const char *reason, const char *where)
{
  static int	dump_bottom_b = 0, dump_top_b = 0;
  char		out[(DUMP_SPACE + FENCE_BOTTOM_SIZE + FENCE_TOP_SIZE) * 4];
  char		where_buf[MAX_FILE_LENGTH + 64];
  char		where_buf2[MAX_FILE_LENGTH + 64];
  const char	*reason_str;
  const void	*dump_pnt = user_pnt;
  int		out_len, dump_size, offset;
  
  /* get a proper reason string */
  if (reason == NULL) {
    reason_str = dmalloc_strerror(dmalloc_errno);
  }
  else {
    reason_str = reason;
  }
  
  /* dump the pointer information */
  if (user_pnt == NULL) {
    _dmalloc_message("%s: %s: from '%s' prev access '%s'",
		     where, reason_str,
		     _chunk_desc_pnt(where_buf, sizeof(where_buf),
				     now_file, now_line),
		     _chunk_desc_pnt(where_buf2, sizeof(where_buf2),
				     prev_file, prev_line));
  }
  else {
    _dmalloc_message("%s: %s: pointer '%#lx' from '%s' prev access '%s'",
		     where, reason_str, (unsigned long)user_pnt,
		     _chunk_desc_pnt(where_buf, sizeof(where_buf),
				     now_file, now_line),
		     _chunk_desc_pnt(where_buf2, sizeof(where_buf2),
				     prev_file, prev_line));
  }
  
  /*
   * If we aren't logging bad space or we didn't error with an
   * overwrite error then don't log the bad bytes.
   */
  if ((! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_BAD_SPACE))
      || (dmalloc_errno != ERROR_UNDER_FENCE
	  && dmalloc_errno != ERROR_OVER_FENCE
	  && dmalloc_errno != ERROR_FREE_NON_BLANK)) {
    return;
  }
  
  /* NOTE: display memroy like this has the potential for generating a core */
  if (dmalloc_errno == ERROR_UNDER_FENCE) {
    /* NOTE: only dump out the proper fence-post area once */
    if (! dump_bottom_b) {
      out_len = expand_chars(fence_bottom, FENCE_BOTTOM_SIZE, out,
			     sizeof(out));
      _dmalloc_message("Dump of proper fence-bottom bytes: '%.*s'",
		       out_len, out);
      dump_bottom_b = 1;
    }
    offset = -FENCE_BOTTOM_SIZE;
    dump_size = DUMP_SPACE + FENCE_BOTTOM_SIZE;
  }
  else if (dmalloc_errno == ERROR_OVER_FENCE && size > 0) {
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
  else {
    dump_size = DUMP_SPACE;
    offset = 0;
  }
  
  if (size > 0 && dump_size > size) {
    dump_size = size;
  }
  
  dump_pnt = (char *)user_pnt + offset;
  if (IS_IN_HEAP(dump_pnt)) {
    out_len = expand_chars(dump_pnt, dump_size, out, sizeof(out));
    _dmalloc_message("Dump of '%#lx'%+d: '%.*s'",
		     (unsigned long)user_pnt, offset, out_len, out);
  }
  else {
    _dmalloc_message("Dump of '%#lx'%+d failed: not in heap",
		     (unsigned long)user_pnt, offset);
  }
}

/************************* fence-post error functions ************************/

/*
 * static int fence_read
 *
 * DESCRIPTION
 *
 * Check a pointer for fence-post magic numbers.
 *
 * RETURNS:
 *
 * Success - 1 if the fence posts are good.
 *
 * Failure - 0 if they are not.
 *
 * ARGUMENTS:
 *
 * chunk_pnt -> Address we are checking.
 *
 * size -> Size of the block we are checking.
 */
static	int	fence_read(const void *chunk_pnt, const unsigned int size)
{
  /* check magic numbers in bottom of allocation block */
  if (memcmp(fence_bottom, (char *)chunk_pnt, FENCE_BOTTOM_SIZE) != 0) {
    dmalloc_errno = ERROR_UNDER_FENCE;
    return 0;
  }
  
  /* check numbers at top of allocation block */
  if (memcmp(fence_top, (char *)chunk_pnt + size - FENCE_TOP_SIZE,
	     FENCE_TOP_SIZE) != 0) {
    dmalloc_errno = ERROR_OVER_FENCE;
    return 0;
  }
  
  return 1;
}

/************************** administration functions *************************/

/*
 * static int set_bblock_admin
 *
 * DESCRIPTION:
 *
 * Set the information for BLOCK_N administrative block(s) at
 * BBLOCK_P.
 *
 * RETURNS:
 *
 * Success - 1
 *
 * Failure - 0
 *
 * ARGUMENTS:
 *
 * block_n -> Number of blocks we are setting.
 *
 * bblock_p -> Pointer to the 1st block we are setting.
 *
 * flags -> Set the block flags to this.
 *
 * num -> Information line number we are setting.
 *
 * info -> Information 
 */
static	int	set_bblock_admin(const int block_n, bblock_t *bblock_p,
				 const int flags, const char *file,
				 const unsigned int line,
				 const unsigned int size,
				 bblock_t *next_p,
				 const int bit_n)
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
	return 0;
      }
      
      bblock_p = bblock_adm_p->ba_blocks;
    }
    
    /* set bblock info */
    switch (BBLOCK_FLAG_TYPE(flags)) {
      
    case BBLOCK_START_USER:
    case BBLOCK_USER:
    case BBLOCK_VALLOC:
      if (bblock_c == 0) {
	bblock_p->bb_flags = BBLOCK_START_USER;
      }
      else {
	bblock_p->bb_flags = BBLOCK_USER;
      }
      
      /* same as START_USER with the VALLOC flag added */ 
      if (BIT_IS_SET(flags, BBLOCK_VALLOC)) {
	BIT_SET(bblock_p->bb_flags, BBLOCK_VALLOC);
      }
      if (BIT_IS_SET(flags, BBLOCK_FENCE)) {
	BIT_SET(bblock_p->bb_flags, BBLOCK_FENCE);
      }
      
      bblock_p->bb_line = line;
      bblock_p->bb_size = size;
      bblock_p->bb_file = file;
      bblock_p->bb_use_iter = _dmalloc_iter_c;
      break;
      
    case BBLOCK_START_FREE:
    case BBLOCK_FREE:
      if (bblock_c == 0) {
	bblock_p->bb_next = next_p;
	bblock_p->bb_flags = BBLOCK_START_FREE;
      }
      else {
	bblock_p->bb_next = NULL;
	bblock_p->bb_flags = BBLOCK_FREE;
      }
      bblock_p->bb_bit_n = bit_n;
      bblock_p->bb_block_n = (unsigned int)block_n;
      bblock_p->bb_use_iter = _dmalloc_iter_c;
      break;
      
    default:
      dmalloc_errno = ERROR_BAD_FLAG;
      dmalloc_error("set_bblock_admin");
      return 0;
      /* NOTREACHED */
      break;
    }
  }
  
  return 1;
}

/*
 * static int find_free_bblocks
 *
 * DESCRIPTION:
 *
 * Parse the free lists looking for a free slot of bblocks.
 *
 * RETURNS:
 *
 * Success - 1 that we did or didn't find a block  
 *
 * Failure - 0 indicating problems with the structures
 *
 * ARGUMENTS:
 *
 * many -> How many bblocks we need.
 *
 * bblock_pp <- Pointer to block pointer which will be set with
 * block we found or NULL.
 */
static	int	find_free_bblocks(const unsigned int many,
				  bblock_t **bblock_pp)
{
  bblock_t	*bblock_p, *prev_p;
  bblock_t	*best_p = NULL, *best_prev_p = NULL;
  int		bit_c, bit_n, block_n, pos, best = 0;
  bblock_adm_t	*adm_p;
  
  /* if we are never reusing then always say we don't have any */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)) {
    *bblock_pp = NULL;
    return 1;
  }
  
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
      if (bblock_p->bb_use_iter > 0
	  && _dmalloc_iter_c < bblock_p->bb_use_iter + FREED_POINTER_DELAY) {
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
    *bblock_pp = NULL;
    return 1;
  }
  
  /* take it off the free list */
  if (best_prev_p == NULL) {
    free_bblock[bit_c] = best_p->bb_next;
  }
  else {
    best_prev_p->bb_next = best_p->bb_next;
  }
  
  if (best_p->bb_block_n == many) {
    *bblock_pp = best_p;
    return 1;
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
      return 0;
    }
  }
  
  bblock_p = adm_p->ba_blocks + pos;
  /* we should not be at the start of a free section but in the middle */
  if (bblock_p->bb_flags != BBLOCK_FREE) {
    dmalloc_errno = ERROR_BAD_FREE_MEM;
    dmalloc_error("find_free_bblocks");
    return 0;
  }
  
  block_n = bblock_p->bb_block_n - many;
  NUM_BITS(block_n * BLOCK_SIZE, bit_n);
  
  set_bblock_admin(block_n, bblock_p, BBLOCK_START_FREE, NULL, 0, 0,
		   free_bblock[bit_n], bit_n);
  free_bblock[bit_n] = bblock_p;
  
  *bblock_pp = best_p;
  return 1;
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
  if (! find_free_bblocks(many, &bblock_p)) {
    return NULL;
  }
  
  /* did we find anything? */
  if (bblock_p != NULL) {
    free_space_count -= many * BLOCK_SIZE;
    
    /* space should be free */
    if (bblock_p->bb_flags != BBLOCK_START_FREE) {
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
  SET_POINTER(mem_p, mem);
  
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
 * static bblock_t *find_blocks
 *
 * Find a pointer's corresponding bblock_t entry.
 *
 * RETURNS:
 *
 * Success - Pointer to the bblock
 *
 * Failure - NULL
 *
 * ARGUMENTS:
 *
 * user_pnt -> Pointer we are tracking.
 *
 * start_b -> Whether the pointer must be at the start of an
 * allocation.
 *
 * prev_pp <- Pointer to a bblock_t which, if not NULL, will be set to
 * the bblock right before the one found.
 *
 * next_pp <- Pointer to a bblock_t which, if not NULL, will be set to
 * the bblock right after the one found.
 *
 * dblock_pp <- Pointer to a dblock_t which, if not NULL, will be set
 * to the corresponding dblock if the user_pnt is pointing to a
 * dblock.
 *
 * fence_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the user block has the fence-post checking enabled.
 *
 * valloc_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the user block was a valloc allocation.
 */
static	bblock_t	*find_blocks(const void *user_pnt, const int start_b,
				     bblock_t **prev_pp, bblock_t **next_pp,
				     dblock_t **dblock_pp, int *fence_bp,
				     int *valloc_bp)
{
  const void	*chunk_pnt, *tmp;
  int		fence_b, valloc_b, file_line_b = 0;
  const char	*file, *name_p, *bounds_p;
  unsigned int	line, bblock_c, bblock_n;
  bblock_t	*prev_bb_p = NULL, *bblock_p;
  bblock_adm_t	*bblock_adm_p;
  dblock_t	*dblock_p;
  
  if (user_pnt == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    return NULL;
  }
  
  /*
   * check validity of the pointer
   */
  if (! IS_IN_HEAP(user_pnt)) {
    dmalloc_errno = ERROR_NOT_IN_HEAP;
    return NULL;
  }
  
  /* find right bblock admin */
  for (bblock_c = WHICH_BLOCK(user_pnt), bblock_adm_p = bblock_adm_head;
       bblock_c >= BB_PER_ADMIN && bblock_adm_p != NULL;
       bblock_c -= BB_PER_ADMIN, bblock_adm_p = bblock_adm_p->ba_next) {
    prev_bb_p = bblock_adm_p->ba_blocks + (BB_PER_ADMIN - 1);
  }
  
  if (bblock_adm_p == NULL) {
    dmalloc_errno = ERROR_NOT_FOUND;
    return NULL;
  }
  
  bblock_p = bblock_adm_p->ba_blocks + bblock_c;
  
  /* verify that the pointer is either dblock or user allocated */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    
    /* find correct dblock_p */
    dblock_p = bblock_p->bb_dblock +
      ((char *)user_pnt - (char *)bblock_p->bb_mem) /
      (1 << bblock_p->bb_bit_n);
    
    SET_POINTER(dblock_pp, dblock_p);
    
    /* are fence posts on? */
    fence_b = BIT_IS_SET(dblock_p->db_flags, DBLOCK_FENCE);
    valloc_b = 0;
    chunk_pnt = USER_TO_CHUNK(user_pnt, fence_b);
    
    if (start_b) {
      /* allocated user space? */
      if (! BIT_IS_SET(dblock_p->db_flags, DBLOCK_USER)) {
	/* NOTE: we should run through free list here */
	dmalloc_errno = ERROR_NOT_USER;
	return NULL;
      }
      
      /* on a correct mini-block boundary? */
      if (((char *)chunk_pnt - (char *)bblock_p->bb_mem) %
	  (1 << bblock_p->bb_bit_n) != 0) {
	dmalloc_errno = ERROR_NOT_ON_BLOCK;
	return NULL;
      }
      
      /* check out the fence-posts */
      if (fence_b && (! fence_read(chunk_pnt, dblock_p->db_size))) {
	/* errno set in fence_read */
	return NULL;
      }
      
      file = dblock_p->db_file;
      line = dblock_p->db_line;
      file_line_b = 1;
    }
    
    /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
    if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
      dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
      return NULL;
    }
  }
  else {
    fence_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FENCE);
    valloc_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC);
    chunk_pnt = USER_TO_CHUNK(user_pnt, fence_b);
    
    if (start_b) {
      /*
       * If we have a valloc allocation with fence-post writing then
       * we should be in the next block, _not_ at the start block.
       * Otherwise, we should be in the starting block of a user
       * allocation.
       */
      if (valloc_b && fence_b) {
	if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_USER)) {
	  /* verify that we are at the start of an allocation */
	  dmalloc_errno = ERROR_NOT_START_USER;
	  return NULL;
	}
	
	/*
	 * Find right bblock admin now based on the chunk-pnt.  We
	 * can't just do a bblock_p-- in case it was at the start of a
	 * bblock_adm section.
	 */
	for (bblock_c = WHICH_BLOCK(chunk_pnt), bblock_adm_p = bblock_adm_head;
	     bblock_c >= BB_PER_ADMIN && bblock_adm_p != NULL;
	     bblock_c -= BB_PER_ADMIN, bblock_adm_p = bblock_adm_p->ba_next) {
	  prev_bb_p = bblock_adm_p->ba_blocks + (BB_PER_ADMIN - 1);
	}
	
	if (bblock_adm_p == NULL) {
	  dmalloc_errno = ERROR_NOT_FOUND;
	  return NULL;
	}
	
	bblock_p = bblock_adm_p->ba_blocks + bblock_c;
	/* this should now point at the start-user block */
      }
      if (! BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
	/* verify that we are at the start of an allocation */
	dmalloc_errno = ERROR_NOT_START_USER;
	return NULL;
      }
      /*
       * If we have a valloc allocation then the _user_ pnt should be
       * block aligned otherwise the chunk_pnt should be.
       */
      if ((valloc_b && (! ON_BLOCK(user_pnt)))
	  || ((! valloc_b) && (! ON_BLOCK(chunk_pnt)))) {
	dmalloc_errno = ERROR_NOT_ON_BLOCK;
	return NULL;
      }
      
      /*
       * Check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take
       * over unless we have a valloc call.
       */
      if (((! valloc_b) && bblock_p->bb_size <= BLOCK_SIZE / 2)
	  || bblock_p->bb_size > (1 << LARGEST_BLOCK)) {
	dmalloc_errno = ERROR_BAD_SIZE;
	return NULL;
      }
      
      /* check out the fence-posts if we are at the start of a user-block */
      if (fence_b
	  && BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
	if (! fence_read(chunk_pnt, bblock_p->bb_size)) {
	  /* errno set in fence_read */
	  return NULL;
	}
      }
      
      file = bblock_p->bb_file;
      line = bblock_p->bb_line;
      file_line_b = 1;
    }
  }
  
  if (prev_pp != NULL) {
    if (bblock_c > 0) {
      prev_bb_p = bblock_adm_p->ba_blocks + (bblock_c - 1);
    }
    
    /* adjust the last pointer back to start of free block */
    if (prev_bb_p != NULL
	&& BIT_IS_SET(prev_bb_p->bb_flags, BBLOCK_START_FREE)) {
      if (prev_bb_p->bb_block_n <= bblock_c) {
	prev_bb_p = bblock_adm_p->ba_blocks +
	  (bblock_c - prev_bb_p->bb_block_n);
      }
      else {
	/* need to go recursive to go bblock_n back, check if at 1st block */
	tmp = (char *)user_pnt - prev_bb_p->bb_block_n * BLOCK_SIZE;
	if (IS_IN_HEAP(tmp)) {
	  /* the prev block before may not be a user block */
	  prev_bb_p = find_blocks(tmp, 0, NULL, NULL, NULL, NULL, NULL);
	  if (prev_bb_p == NULL) {
	    dmalloc_error("find_blocks");
	    return NULL;
	  }
	}
	else {
	  prev_bb_p = NULL;
	}
      }
    }
    
    *prev_pp = prev_bb_p;
  }
  if (next_pp != NULL) {
    /* next pointer should move past current allocation */
    if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_USER)) {
      bblock_n = NUM_BLOCKS(bblock_p->bb_size);
    }
    else {
      bblock_n = 1;
    }
    if (bblock_c + bblock_n < BB_PER_ADMIN) {
      *next_pp = bblock_p + bblock_n;
    }
    else {
      /* need to go recursive to go bblock_n ahead, check if at prev block */
      tmp = (char *)user_pnt + bblock_n * BLOCK_SIZE;
      if (! IS_IN_HEAP(tmp)) {
	*next_pp = NULL;
      }
      else {
	/* the next block may not be a user block */
	*next_pp = find_blocks(tmp, 0, NULL, NULL, NULL, NULL, NULL);
	if (*next_pp == NULL) {
	  dmalloc_error("find_blocks");
	  return NULL;
	}
      }
    }
  }
  
  if (file_line_b) {
    /* check line number */
    if (line > MAX_LINE_NUMBER) {
      dmalloc_errno = ERROR_BAD_LINE;
      return NULL;
    }
    
    /*
     * Check file pointer only if file is not NULL and line is not 0
     * which implies that file is a return-addr.
     */
    if (file != DMALLOC_DEFAULT_FILE && line != DMALLOC_DEFAULT_LINE) {
      /* NOTE: we don't use strlen here because we might check too far */
      bounds_p = file + MAX_FILE_LENGTH;
      for (name_p = file; name_p < bounds_p && *name_p != '\0'; name_p++) {
      }
      if (name_p >= bounds_p
	  || name_p < file + MIN_FILE_LENGTH) {
	dmalloc_errno = ERROR_BAD_FILEP;
	return NULL;
      }
    }
  }
  
  SET_POINTER(fence_bp, fence_b);
  SET_POINTER(valloc_bp, valloc_b);
  return bblock_p;
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
    dblock_p->db_flags = DBLOCK_FREE;
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
  dblock_t	*prev_p;
#endif
  
  /* if we are never reusing then always say we don't have any */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)) {
    return NULL;
  }
  
#if FREED_POINTER_DELAY
  /* find a value dblock entry */
  for (dblock_p = free_dblock[bit_n], prev_p = NULL;
       dblock_p != NULL;
       prev_p = dblock_p, dblock_p = dblock_p->db_next) {
    
    /* are we still waiting on this guy? */
    if (dblock_p->db_use_iter > 0
	&& _dmalloc_iter_c < dblock_p->db_use_iter + FREED_POINTER_DELAY) {
      continue;
    }
    
    /* keep the linked lists */
    if (prev_p == NULL) {
      free_dblock[bit_n] = dblock_p->db_next;
    }
    else {
      prev_p->db_next = dblock_p->db_next;
    }
    break;
  }
#else /* FREED_POINTER_DELAY == 0 */
  dblock_p = free_dblock[bit_n];
  if (dblock_p != NULL) {
    free_dblock[bit_n] = dblock_p->db_next;
  }
#endif /* FREED_POINTER_DELAY == 0 */
  
  return dblock_p;
}

/*
 * Get a dblock of 1<<BIT_N sized chunks, also asked for the slot memory
 */
static	void	*get_dblock(const int bit_n, const unsigned short byte_n,
			    const char *file, const unsigned short line,
			    const int fence_b, overhead_t **over_p)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p, *first_p, *free_p;
  void		*chunk_pnt;
  
  /* is there anything on the dblock free list? */
  dblock_p = find_free_dblock(bit_n);
  
  if (dblock_p != NULL) {
    free_space_count -= 1 << bit_n;
    
    /* find pointer to memory chunk */
    chunk_pnt = (char *)dblock_p->db_bblock->bb_mem +
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
    bblock_p = get_bblocks(1, &chunk_pnt);
    if (bblock_p == NULL) {
      return NULL;
    }
    
    /* setup bblock information */
    bblock_p->bb_flags = BBLOCK_DBLOCK;
    bblock_p->bb_bit_n = bit_n;
    bblock_p->bb_dblock = dblock_p;
    bblock_p->bb_mem = chunk_pnt;
    
    /* add the rest to the free list (has to be at least 1 other dblock) */
    first_p = dblock_p;
    dblock_p->db_bblock = bblock_p;
    dblock_p++;
    free_p = free_dblock[bit_n];
    free_dblock[bit_n] = dblock_p;
    
    for (; dblock_p < first_p + (1 << (BASIC_BLOCK - bit_n)) - 1; dblock_p++) {
      dblock_p->db_flags = DBLOCK_FREE;
      dblock_p->db_bblock = bblock_p;
      dblock_p->db_next = dblock_p + 1;
      dblock_p->db_use_iter = 0;
#if STORE_SEEN_COUNT
      dblock_p->db_overhead.ov_seen_c = 0;
#endif
      free_space_count += 1 << bit_n;
    }
    
    /* prev one points to the free list (probably NULL) */
    dblock_p->db_flags = DBLOCK_FREE;
    dblock_p->db_next = free_p;
    dblock_p->db_bblock = bblock_p;
    dblock_p->db_use_iter = 0;
    free_space_count += 1 << bit_n;
    
    /*
     * We return the 1st dblock chunk in the block.  Overwrite the
     * rest of the block.
     */ 
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
      (void)memset((char *)chunk_pnt + (1 << bit_n), FREE_BLANK_CHAR,
		   BLOCK_SIZE - (1 << bit_n));
    }
    
#if STORE_SEEN_COUNT
    /* the first pointer in the block inherits the counter of the bblock */
    first_p->db_overhead.ov_seen_c = bblock_p->bb_overhead.ov_seen_c;
#endif
    
    dblock_p = first_p;
  }
  
  dblock_p->db_flags = DBLOCK_USER | (fence_b ? DBLOCK_FENCE : 0);
  dblock_p->db_line = line;
  dblock_p->db_size = byte_n;
  dblock_p->db_file = file;
  dblock_p->db_use_iter = _dmalloc_iter_c;
  
#if STORE_SEEN_COUNT
  dblock_p->db_overhead.ov_seen_c++;
#endif
#if STORE_ITERATION_COUNT
  dblock_p->db_overhead.ov_iteration = _dmalloc_iter_c;
#endif
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
    GET_TIMEVAL(dblock_p->db_overhead.ov_timeval);
#else
#if STORE_TIME
    dblock_p->db_overhead.ov_time = time(NULL);
#endif
#endif
  }
  
#if LOG_THREAD_ID
  dblock_p->db_overhead.ov_thread_id = THREAD_GET_ID();
#endif
  
  *over_p = &dblock_p->db_overhead;
  
  return chunk_pnt;
}

/******************************* heap checking *******************************/

/*
 * int _chunk_check
 *
 * DESCRIPTION:
 *
 * Run extensive tests on the entire heap.
 *
 * RETURNS:
 *
 * Success - 1 if the heap is okay
 *
 * Failure - 0 if a problem was detected
 *
 * ARGUMENTS:
 *
 * None.
 */
int	_chunk_check(void)
{
  bblock_adm_t	*this_adm_p, *ahead_p;
  bblock_t	*bblock_p, *bblist_p, *prev_bblock_p;
  dblock_t	*dblock_p;
  unsigned int	undef = 0, start = 0;
  char		*byte_p;
  void		*pnt;
  int		bit_c, dblock_c = 0, bblock_c = 0, free_c = 0, fence_b;
  unsigned int	bb_c = 0, len, block_type;
  int		free_bblock_c[MAX_SLOTS];
  int		free_dblock_c[BASIC_BLOCK];
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("checking heap");
  }
  
  /* if the heap is empty then no need to check anything */
  if (bblock_adm_head == NULL) {
    return 1;
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
	if (! IS_IN_HEAP(bblock_p)) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
    }
    
    /* count the dblock free lists */
    for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
      free_dblock_c[bit_c] = 0;
      
      /* parse dblock free list doing minimal pointer checking */
      for (dblock_p = free_dblock[bit_c];
	   dblock_p != NULL;
	   dblock_p = dblock_p->db_next) {
	if (! IS_IN_HEAP(dblock_p)) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	if (dblock_p->db_flags != DBLOCK_FREE) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	free_dblock_c[bit_c]++;
      }
    }
  }
  
  /* start pointers */
  this_adm_p = bblock_adm_head;
  ahead_p = this_adm_p;
  
  /* test admin pointer validity */
  if (! IS_IN_HEAP(this_adm_p)) {
    dmalloc_errno = ERROR_BAD_ADMIN_P;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  /* test structure validity */
  if (this_adm_p->ba_magic1 != CHUNK_MAGIC_BOTTOM
      || this_adm_p->ba_magic2 != CHUNK_MAGIC_TOP) {
    dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  /* verify count value */
  if (this_adm_p->ba_pos_n != bb_c) {
    dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  /* check out the basic blocks */
  prev_bblock_p = NULL;
  for (bblock_p = this_adm_p->ba_blocks;; prev_bblock_p = bblock_p++) {
    
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
	return 0;
      }
      
      /* test structure validity */
      if (this_adm_p->ba_magic1 != CHUNK_MAGIC_BOTTOM
	  || this_adm_p->ba_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* verify count value */
      if (this_adm_p->ba_pos_n != bb_c) {
	dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
	dmalloc_error("_chunk_check");
	return 0;
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
      return 0;
    }
    
    start = 0;
    
    /*
     * check for different types
     */
    block_type = BBLOCK_FLAG_TYPE(bblock_p->bb_flags);
    switch (block_type) {
      
      /* check a starting user-block */
    case BBLOCK_START_USER:
      
      /* check X blocks in a row */
      if (bblock_c != 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      fence_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FENCE);
      
      /* mark the size in bits */
      NUM_BITS(bblock_p->bb_size, bit_c);
      bblock_c = NUM_BLOCKS(bblock_p->bb_size);
      /* valloc basic blocks gets 1 extra block below for any fence info */
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC) && fence_b) {
	bblock_c++;
      }
      start = 1;
      
      /* check fence-posts for memory chunk */
      if (fence_b) {
	pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			    (bblock_p - this_adm_p->ba_blocks));
	/* if we have valloc block and there is fence info then shift pnt up */
	if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
	  pnt = (char *)pnt + (BLOCK_SIZE - FENCE_BOTTOM_SIZE);
	}
	if (! fence_read(pnt, bblock_p->bb_size)) {
	  log_error_info(NULL, NULL, bblock_p->bb_file, bblock_p->bb_line,
			 pnt, bblock_p->bb_size, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_USER:
      
      fence_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FENCE);
      
      /* check line number */
      if (bblock_p->bb_line > MAX_LINE_NUMBER) {
	dmalloc_errno = ERROR_BAD_LINE;
	log_error_info(NULL, NULL, bblock_p->bb_file, bblock_p->bb_line, NULL,
		       0, NULL, "heap-check");
	dmalloc_error("_chunk_check");
	return 0;
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
	return 0;
      }
      
      /* check file pointer */
      if (bblock_p->bb_file != DMALLOC_DEFAULT_FILE
	  && bblock_p->bb_line != DMALLOC_DEFAULT_LINE) {
	len = strlen(bblock_p->bb_file);
	if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	  dmalloc_errno = ERROR_BAD_FILEP;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
      
      /* check X blocks in a row */
      if (bblock_c == 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      if (start == 0
	  && (prev_bblock_p == NULL
	      || ((! BIT_IS_SET(prev_bblock_p->bb_flags, BBLOCK_START_USER))
		  && (! BIT_IS_SET(prev_bblock_p->bb_flags, BBLOCK_USER)))
	      || bblock_p->bb_file != prev_bblock_p->bb_file
	      || bblock_p->bb_line != prev_bblock_p->bb_line
	      || bblock_p->bb_size != prev_bblock_p->bb_size)) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      bblock_c--;
      /* NOTE: we should check above the allocated space if alloc_blank on */
      break;
      
    case BBLOCK_ADMIN:
      
      /* check the bblock_admin linked-list */
      if (bblock_p->bb_admin_p != ahead_p) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMIN_P;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check count against admin count */
      if (bblock_p->bb_pos_n != ahead_p->ba_pos_n) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMIN_C;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      ahead_p = ahead_p->ba_next;
      break;
      
    case BBLOCK_DBLOCK:
      
      /* check out bit_c */
      if (bblock_p->bb_bit_n >= BASIC_BLOCK) {
	dmalloc_errno = ERROR_BAD_DBLOCK_SIZE;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblock_p->bb_dblock)) {
	dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* verify mem pointer */
      if (bblock_p->bb_mem != BLOCK_POINTER(this_adm_p->ba_pos_n +
					    (bblock_p -
					     this_adm_p->ba_blocks))) {
	dmalloc_errno = ERROR_BAD_DBLOCK_MEM;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check dblock entry very closely if necessary */
      for (dblock_c = 0, dblock_p = bblock_p->bb_dblock;
	   dblock_p < bblock_p->bb_dblock +
	   (1 << (BASIC_BLOCK - bblock_p->bb_bit_n));
	   dblock_c++, dblock_p++) {
	
	/* check out dblock entry to see if it is not free */
	if (dblock_p->db_flags == DBLOCK_FREE) {
	  
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
	    
	    /* did we not find it and we are reusing pointers */
	    if (dblist_p == NULL) {
	      dmalloc_errno = ERROR_BAD_FREE_LIST;
	      dmalloc_error("_chunk_check");
	      return 0;
	    }
	    else {
	      free_dblock_c[bblock_p->bb_bit_n]--;
	    }
	  }
	  
	  continue;
	}
	
	/*
	 * check out size, better be less than BLOCK_SIZE / 2 I have to
	 * check this twice.  Yick.
	 */
	if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 NULL, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
	  pnt = (char *)bblock_p->bb_mem +
	    dblock_c * (1 << bblock_p->bb_bit_n);
	  if (! fence_read(pnt, dblock_p->db_size)) {
	    return 0;
	  }
	}
      }
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblock_p->bb_slot_p)) {
	dmalloc_errno = ERROR_BAD_DBADMIN_POINTER;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* verify magic numbers */
      if (bblock_p->bb_slot_p->da_magic1 != CHUNK_MAGIC_BOTTOM
	  || bblock_p->bb_slot_p->da_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_DBADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      /* check out each dblock_admin struct? */
      for (dblock_p = bblock_p->bb_slot_p->da_block;
	   dblock_p < bblock_p->bb_slot_p->da_block + DB_PER_ADMIN;
	   dblock_p++) {
	
	/* see if we've used this slot before */
	if (dblock_p->db_bblock == NULL && dblock_p->db_next == NULL) {
	  continue;
	}
	
	/* sanity check */
	if ((! IS_IN_HEAP(dblock_p->db_bblock))
	    || dblock_p->db_bblock->bb_flags != BBLOCK_DBLOCK) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 dblock_p->db_bblock, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  continue;
	}
	
	/* check out dblock pointer and next pointer (if free) */
	if (dblock_p->db_flags == DBLOCK_FREE) {
	  
	  /* should we verify that we have a block of free'd chars */
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	    
	    /* find pointer to memory chunk */
	    pnt = (char *)dblock_p->db_bblock->bb_mem +
	      (dblock_p - dblock_p->db_bblock->bb_dblock) *
	      (1 << dblock_p->db_bblock->bb_bit_n);
	    
	    for (byte_p = (char *)pnt;
		 byte_p < (char *)pnt + (1 << dblock_p->db_bblock->bb_bit_n);
		 byte_p++) {
	      if (*byte_p != FREE_BLANK_CHAR) {
		dmalloc_errno = ERROR_FREE_NON_BLANK;
		log_error_info(NULL, NULL,
			       dblock_p->db_file, dblock_p->db_line, byte_p, 0,
			       NULL, "heap-check");
		dmalloc_error("_chunk_check");
		return 0;
	      }
	    }
	  }
	  
	  continue;
	}
	
	/* check out size, better be less than BLOCK_SIZE / 2 */
	if ((int)dblock_p->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 NULL, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	
	/* check line number */
	if (dblock_p->db_line > MAX_LINE_NUMBER) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(NULL, NULL, dblock_p->db_file, dblock_p->db_line,
			 NULL, 0, NULL, "heap-check");
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	
	if (dblock_p->db_file != DMALLOC_DEFAULT_FILE
	    && dblock_p->db_line != DMALLOC_DEFAULT_LINE) {
	  len = strlen(dblock_p->db_file);
	  if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	    dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	    /* should there be a log-error-info call here? */
	    dmalloc_error("_chunk_check");
	    return 0;
	  }
	}
      }
      break;
      
    case BBLOCK_START_FREE:
      
      /* check X blocks in a row */
      if (free_c != 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return 0;
      }
      
      free_c = bblock_p->bb_block_n;
      
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	
	/* find the free block in the free list */
	for (bblist_p = free_bblock[bblock_p->bb_bit_n];
	     bblist_p != NULL;
	     bblist_p = bblist_p->bb_next) {
	  if (bblist_p == bblock_p) {
	    break;
	  }
	}
	
	/* did we find it? */
	if (bblist_p == NULL) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
	else {
	  free_bblock_c[bblock_p->bb_bit_n]--;
	}
      }
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_FREE:
      
      /* NOTE: check out free_lists, depending on debug value? */
      
      if (block_type == BBLOCK_FREE) {
	if (prev_bblock_p == NULL
	    || (prev_bblock_p->bb_flags != BBLOCK_FREE
		&& prev_bblock_p->bb_flags != BBLOCK_START_FREE)
	    || bblock_p->bb_bit_n != prev_bblock_p->bb_bit_n) {
	  dmalloc_errno = ERROR_FREE_NON_CONTIG;
	  dmalloc_error("_chunk_check");
	  return 0;
	}
      }
      free_c--;
      
      /* should we verify that we have a block of freed chars? */
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
			    (bblock_p - this_adm_p->ba_blocks));
	for (byte_p = (char *)pnt;
	     byte_p < (char *)pnt + BLOCK_SIZE;
	     byte_p++) {
	  if (*byte_p != FREE_BLANK_CHAR) {
	    dmalloc_errno = ERROR_FREE_NON_BLANK;
	    log_error_info(NULL, NULL, bblock_p->bb_file, bblock_p->bb_line,
			   byte_p, 0, NULL, "heap-check");
	    dmalloc_error("_chunk_check");
	    /* continue to check the rest of the free list */
	    break;
	  }
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
	return 0;
      }
      break;
      
    default:
      dmalloc_errno = ERROR_BAD_FLAG;
      dmalloc_error("_chunk_check");
      return 0;
      /* NOTREACHED */
      break;
    }
  }
  
  /*
   * any left over contiguous counters?
   */
  if (bblock_c > 0) {
    dmalloc_errno = ERROR_USER_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return 0;
  }
  if (free_c > 0) {
    dmalloc_errno = ERROR_FREE_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return 0;
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
    
    /* any free bblock entries not accounted for? */
    for (bit_c = 0; bit_c < MAX_SLOTS; bit_c++) {
      if (free_bblock_c[bit_c] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return 0;
      }
    }
    
    /* any free dblock entries not accounted for? */
    for (bit_c = 0; bit_c < BASIC_BLOCK; bit_c++) {
      if (free_dblock_c[bit_c] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return 0;
      }
    }
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_HEAP_CHECK_MAP)) {
    _chunk_log_heap_map();
  }
  
  return 1;
}

/*
 * int _chunk_pnt_check
 *
 * DESCRIPTION:
 *
 * Run extensive tests on a pointer.
 *
 * RETURNS:
 *
 * Success - 1 if the pointer is okay
 *
 * Failure - 0 if not
 *
 * ARGUMENTS:
 *
 * func -> Function string which is checking the pointer.
 *
 * user_pnt -> Pointer we are checking.
 *
 * check -> Type of checking (see chunk.h).
 *
 * min_size -> Make sure that pnt can hold at least that many bytes.
 * If -1 then do a strlen + 1 for the \0.
 */
int	_chunk_pnt_check(const char *func, const void *user_pnt,
			 const int check, const int min_size)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  int		diff, fence_b;
  const void	*chunk_pnt;
  unsigned int	min;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("checking pointer '%#lx'", (unsigned long)user_pnt);
  }
  
  /* find which block it is in */
  bblock_p = find_blocks(user_pnt, BIT_IS_SET(check, CHUNK_PNT_EXACT),
			 NULL, NULL, &dblock_p, &fence_b, NULL);
  if (bblock_p == NULL) {
    if (! BIT_IS_SET(check, CHUNK_PNT_EXACT)) {
      /* the pointer might not be the heap or might be NULL */
      dmalloc_errno = ERROR_NONE;
      return 1;
    }
    else {
      /* errno set in find_blocks */
      log_error_info(NULL, 0, NULL, 0, user_pnt, 0, NULL, "pointer-check");
      dmalloc_error(func);
      return 0;
    }
  }
  chunk_pnt = USER_TO_CHUNK(user_pnt, fence_b);
  
  /* if min_size is < 0 then do a strlen and take into account the \0 */
  if (min_size >= 0) {
    min = min_size;
  }
  else {
    min = strlen((char *)user_pnt) + 1;
  }
  
  if (dblock_p != NULL) {
    
    if (min != 0 && dblock_p->db_size < min) {
      dmalloc_errno = ERROR_WOULD_OVERWRITE;
      log_error_info(NULL, 0, dblock_p->db_file, dblock_p->db_line,
		     user_pnt, 0, NULL, "pointer-check");
      dmalloc_error(func);
      return 0;
    }
    
    return 1;
  }
  
  if (fence_b) {
    min += FENCE_OVERHEAD_SIZE;
  }
  
  /* on a block boundary? */
  if (! ON_BLOCK(chunk_pnt)) {
    if (! BIT_IS_SET(check, CHUNK_PNT_EXACT)) {
      /*
       * normalize size and pointer to nearest block.
       *
       * NOTE: we really need to back-track up the block list to find the
       * starting user block to test things.
       */
      diff = (char *)chunk_pnt - BLOCK_POINTER(WHICH_BLOCK(chunk_pnt));
      if (min != 0) {
	min += diff;
      }
    }
    else {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(NULL, 0, NULL, 0, user_pnt, 0, NULL, "pointer-check");
      dmalloc_error(func);
      return 0;
    }
  }
  
  if (min != 0 && bblock_p->bb_size < min) {
    dmalloc_errno = ERROR_WOULD_OVERWRITE;
    log_error_info(NULL, 0, bblock_p->bb_file, bblock_p->bb_line,
		   user_pnt, 0, NULL, "pointer-check");
    dmalloc_error(func);
    return 0;
  }
  
  return 1;
}

/**************************** information routines ***************************/

/*
 * int _chunk_read_info
 *
 * DESCRIPTION:
 *
 * Return some information associated with a pointer.
 *
 * RETURNS:
 *
 * Success - 1 pointer is okay
 *
 * Failure - 0 problem with pointer
 *
 * ARGUMENTS:
 *
 * user_pnt -> Pointer we are checking.
 *
 * where <- Where the check is being made from.
 *
 * size_p <- Pointer to an unsigned int which, if not NULL, will be
 * set to the size of bytes that the user requested.
 *
 * alloc_size_p <- Pointer to an unsigned int which, if not NULL, will
 * be set to the total given size of bytes including block overhead.
 *
 * file_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the file where the pointer was allocated.
 *
 * line_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the line-number where the pointer was allocated.
 *
 * ret_attr_p <- Pointer to a void pointer, if not NULL, will be set
 * to the return-address where the pointer was allocated.
 *
 * seen_cp <- Pointer to an unsigned long which, if not NULL, will be
 * set to the number of times the pointer has been "seen".
 *
 * valloc_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the pointer was allocated with valloc() otherwise 0.
 *
 * fence_bp <- Pointer to an integer which, if not NULL, will be set
 * to 1 if the pointer has the fence bit set otherwise 0.
 */
int	_chunk_read_info(const void *user_pnt, const char *where,
			 unsigned int *size_p,
			 unsigned int *alloc_size_p, char **file_p,
			 unsigned int *line_p, void **ret_attr_p,
			 unsigned long **seen_cp, int *valloc_bp,
			 int *fence_bp)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  const void	*chunk_pnt;
  int		fence_b;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("reading info about pointer '%#lx'",
		     (unsigned long)user_pnt);
  }
  
  SET_POINTER(seen_cp, NULL);
  
  /* find which block it is in */
  bblock_p = find_blocks(user_pnt, 1, NULL, NULL, &dblock_p, &fence_b, NULL);
  if (bblock_p == NULL) {
    /* errno set in find_block */
    log_error_info(NULL, 0, NULL, 0, user_pnt, 0, NULL, where);
    dmalloc_error("_chunk_read_info");
    return 0;
  }
  chunk_pnt = USER_TO_CHUNK(user_pnt, fence_b);
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    
    /* write info back to user space */
    SET_POINTER(size_p, dblock_p->db_size);
    SET_POINTER(alloc_size_p, 1 << bblock_p->bb_bit_n);
    if (dblock_p->db_file == DMALLOC_DEFAULT_FILE) {
      SET_POINTER(file_p, NULL);
    }
    else {
      SET_POINTER(file_p, (char *)dblock_p->db_file);
    }
    SET_POINTER(line_p, dblock_p->db_line);
    /* if the line is blank then the file will be 0 or the return address */
    if (dblock_p->db_line == DMALLOC_DEFAULT_LINE) {
      SET_POINTER(ret_attr_p, (char *)dblock_p->db_file);
    }
    else {
      SET_POINTER(ret_attr_p, NULL);
    }
#if STORE_SEEN_COUNT
    SET_POINTER(seen_cp, &dblock_p->db_overhead.ov_seen_c);
#endif
    SET_POINTER(valloc_bp, 0);
  }
  else {
    
    /* write info back to user space */
    SET_POINTER(size_p, bblock_p->bb_size);
    /*
     * if we have a valloc block and there is fence info, then
     * another block was allocated
     */
    if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC) && fence_b) {
      SET_POINTER(alloc_size_p,
		  (NUM_BLOCKS(bblock_p->bb_size) + 1) * BLOCK_SIZE);
    }
    else {
      SET_POINTER(alloc_size_p, NUM_BLOCKS(bblock_p->bb_size) * BLOCK_SIZE);
    }
    if (bblock_p->bb_file == DMALLOC_DEFAULT_FILE) {
      SET_POINTER(file_p, NULL);
    }
    else {
      SET_POINTER(file_p, (char *)bblock_p->bb_file);
    }
    SET_POINTER(line_p, bblock_p->bb_line);
    /* if the line is blank then the file will be 0 or the return address */
    if (bblock_p->bb_line == DMALLOC_DEFAULT_LINE) {
      SET_POINTER(ret_attr_p, (char *)bblock_p->bb_file);
    }
    else {
      SET_POINTER(ret_attr_p, NULL);
    }
#if STORE_SEEN_COUNT
    SET_POINTER(seen_cp, &bblock_p->bb_overhead.ov_seen_c);
#endif
    if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
      SET_POINTER(valloc_bp, 1);
    }
    else {
      SET_POINTER(valloc_bp, 0);
    }
  }
  
  SET_POINTER(fence_bp, fence_b);
  return 1;
}

/*
 * static int chunk_write_info
 *
 * DESCRIPTION:
 *
 * Write new FILE, LINE, SIZE info into PNT -- which is in chunk-space
 */
static	int	chunk_write_info(const char *file, const unsigned int line,
				 void *chunk_pnt, const unsigned int size,
				 const char *where, const int fence_b)
{
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  int		block_n;
  void		*user_pnt;
  
  /* NOTE: pnt is in chunk-space */
  
  user_pnt = CHUNK_TO_USER(chunk_pnt, fence_b);
  
  /* find which block it is in */
  bblock_p = find_blocks(user_pnt, 1, NULL, NULL, &dblock_p, NULL, NULL);
  if (bblock_p == NULL) {
    /* errno set in find_blocks */
    log_error_info(file, line, NULL, 0, user_pnt, 0, NULL, where);
    dmalloc_error("chunk_write_info");
    return 0;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    
    /* write info to system space */
    dblock_p->db_size = size;
    dblock_p->db_file = (char *)file;
    dblock_p->db_line = (unsigned short)line;
    dblock_p->db_use_iter = _dmalloc_iter_c;
  }
  else {
    
    block_n = NUM_BLOCKS(size);
    
    /* reset values in the bblocks */
    if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_VALLOC)) {
      /*
       * If the user is requesting a page-aligned block of data then
       * we will need another block below the allocation just for the
       * fence information.  Ugh.
       */
      if (fence_b) {
	block_n++;
      }
      set_bblock_admin(block_n, bblock_p,
		       BBLOCK_VALLOC | (fence_b ? BBLOCK_FENCE : 0),
		       file, line, size, NULL, 0);
    }
    else {
      set_bblock_admin(block_n, bblock_p,
		       BBLOCK_START_USER | (fence_b ? BBLOCK_FENCE : 0),
		       file, line, size, NULL, 0);
    }
  }
  
  return 1;
}

/*
 * Log the heap structure plus information on the blocks if necessary
 */
void	_chunk_log_heap_map(void)
{
  bblock_adm_t	*bblock_adm_p;
  bblock_t	*bblock_p;
  char		line[BB_PER_ADMIN + 10], where_buf[MAX_FILE_LENGTH + 64];
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
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_FREE)) {
	line[char_c++] = 'F';
	continue;
      }
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FREE)) {
	line[char_c++] = 'f';
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
	_dmalloc_message("%d (%#lx): start-of-user block: %lu bytes from '%s'",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 bblock_p->bb_size,
			 _chunk_desc_pnt(where_buf, sizeof(where_buf),
					 bblock_p->bb_file,
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
      
      if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_START_FREE)) {
	_dmalloc_message("%d (%#lx): start-of-free block of %ld blocks, next at %#lx",
			 tblock_c, (unsigned long)BLOCK_POINTER(tblock_c),
			 bblock_p->bb_block_n,
			 (unsigned long)bblock_p->bb_mem);
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
 * Get a SIZE chunk of memory for FILE at LINE.  FUNC_ID is the type
 * of function which generated this call.  If ALIGNMENT is greater
 * than 0 then try to align the returned block.
 */
void	*_chunk_malloc(const char *file, const unsigned int line,
		       const unsigned long size, const int func_id,
		       const unsigned int alignment)
{
  unsigned int	bit_n;
  unsigned long	byte_n = size;
  int		valloc_b = 0, memalign_b = 0, fence_b;
  char		where_buf[MAX_FILE_LENGTH + 64], disp_buf[64];
  bblock_t	*bblock_p;
  overhead_t	*over_p;
  const char	*trans_log;
  void		*chunk_pnt, *user_pnt;
  
  /* counts calls to malloc */
  if (func_id == DMALLOC_FUNC_CALLOC) {
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
  else if (func_id != DMALLOC_FUNC_REALLOC
	   && func_id != DMALLOC_FUNC_RECALLOC) {
    malloc_count++;
  }
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (byte_n == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, NULL, 0, NULL, 0,
		   "bad zero byte allocation request", "malloc");
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
#endif
  
  /* adjust the size */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    byte_n += FENCE_OVERHEAD_SIZE;
    fence_b = 1;
  }
  else {
    fence_b = 0;
  }
  
  /* count the bits */
  NUM_BITS(byte_n, bit_n);
  
  /* have we exceeded the upper bounds */
  if (bit_n > LARGEST_BLOCK) {
    dmalloc_errno = ERROR_TOO_BIG;
    log_error_info(file, line, NULL, 0, NULL, 0, NULL, "malloc");
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* normalize to smallest_block.  No use spending 16 bytes to admin 1 byte */
  if (bit_n < smallest_block) {
    bit_n = smallest_block;
  }
  
  /* monitor current allocation level */
  alloc_current += size;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_total += size;
  alloc_one_max = MAX(alloc_one_max, size);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  /* allocate divided block if small */
  if (bit_n < BASIC_BLOCK && (! valloc_b)) {
    
    /* get a dblock chunk */
    chunk_pnt = get_dblock(bit_n, byte_n, file, line, fence_b, &over_p);
    if (chunk_pnt == NULL) {
      return MALLOC_ERROR;
    }
    
    alloc_cur_given += 1 << bit_n;
    alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
      (void)memset(chunk_pnt, ALLOC_BLANK_CHAR, 1 << bit_n);
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
    if (valloc_b && fence_b) {
      block_n++;
    }
    bblock_p = get_bblocks(block_n, &chunk_pnt);
    if (bblock_p == NULL) {
      return MALLOC_ERROR;
    }
    
    /* initialize the bblocks */
    if (valloc_b) {
      set_bblock_admin(block_n, bblock_p,
		       BBLOCK_VALLOC | (fence_b ? BBLOCK_FENCE : 0),
		       file, line, byte_n, NULL, 0);
    }
    else {
      set_bblock_admin(block_n, bblock_p,
		       BBLOCK_START_USER | (fence_b ? BBLOCK_FENCE : 0),
		       file, line, byte_n, NULL, 0);
    }
    
    given = block_n * BLOCK_SIZE;
    alloc_cur_given += given;
    alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
      (void)memset(chunk_pnt, ALLOC_BLANK_CHAR, given);
    }
    
#if STORE_SEEN_COUNT
    bblock_p->bb_overhead.ov_seen_c++;
#endif
#if STORE_ITERATION_COUNT
    bblock_p->bb_overhead.ov_iteration = _dmalloc_iter_c;
#endif
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
      GET_TIMEVAL(bblock_p->bb_overhead.ov_timeval);
#else
#if STORE_TIME
      bblock_p->bb_overhead.ov_time = time(NULL);
#endif
#endif
    }
    
#if LOG_THREAD_ID
    bblock_p->bb_overhead.ov_thread_id = THREAD_GET_ID();
#endif
    
    over_p = &bblock_p->bb_overhead;
  }
  
  /* write fence post info if needed */
  if (fence_b) {
    if (valloc_b) {
      chunk_pnt = (char *)chunk_pnt + (BLOCK_SIZE - FENCE_BOTTOM_SIZE);
    }
    FENCE_WRITE(chunk_pnt, byte_n);
    user_pnt = CHUNK_TO_USER(chunk_pnt, 1);
  }
  
  if (func_id == DMALLOC_FUNC_CALLOC || func_id == DMALLOC_FUNC_RECALLOC) {
    (void)memset(user_pnt, 0, size);
  }
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    switch (func_id) {
    case DMALLOC_FUNC_CALLOC:
      trans_log = "calloc";
      break;
    case DMALLOC_FUNC_MEMALIGN:
      trans_log = "memalign";
      break;
    case DMALLOC_FUNC_VALLOC:
      trans_log = "valloc";
      break;
    default:
      trans_log = "alloc";
      break;
    }
    _dmalloc_message("*** %s: at '%s' for %ld bytes, got '%s'",
		     trans_log, _chunk_desc_pnt(where_buf, sizeof(where_buf),
						file, line),
		     size, display_pnt(user_pnt, over_p, disp_buf,
				       sizeof(disp_buf)));
  }
  
#if MEMORY_TABLE_LOG
  if (func_id != DMALLOC_FUNC_REALLOC && func_id != DMALLOC_FUNC_RECALLOC) {
    _table_alloc(file, line, size);
  }
#endif
  
  return user_pnt;
}

/*
 * Frees USER_PNT from the heap.  REALLOC_B set if realloc is freeing
 * a pointer so doing count it as a free.  Returns FREE_ERROR or
 * FREE_NOERROR.
 *
 * NOTE: should be above _chunk_realloc which calls it.
 */
int	_chunk_free(const char *file, const unsigned int line, void *user_pnt,
		    const int realloc_b)
{
  unsigned int	bit_n, block_n, given;
  unsigned long	user_size;
  char		where_buf[MAX_FILE_LENGTH + 64];
  char		where_buf2[MAX_FILE_LENGTH + 64], disp_buf[64];
  int		valloc_b = 0, fence_b;
  void		*chunk_pnt;
  bblock_t	*bblock_p, *prev_p, *next_p, *list_p, *this_p;
  dblock_t	*dblock_p;
  
  /* counts calls to free */
  if (! realloc_b) {
    free_count++;
  }
  
  if (user_pnt == NULL) {
#if ALLOW_FREE_NULL_MESSAGE
    _dmalloc_message("WARNING: tried to free(0) from '%s'",
		     _chunk_desc_pnt(where_buf, sizeof(where_buf),
				     file, line));
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
      log_error_info(file, line, NULL, 0, user_pnt, 0, "invalid pointer",
		     "free");
      dmalloc_error("_chunk_free");
    }
    return FREE_ERROR;
#endif
  }
  
  /* find which block it is in */
  bblock_p = find_blocks(user_pnt, 1, &prev_p, &next_p, &dblock_p, &fence_b,
			 &valloc_b);
  if (bblock_p == NULL) {
    /* errno set in find_block */
    log_error_info(file, line, NULL, 0, user_pnt, 0, NULL, "free");
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  chunk_pnt = USER_TO_CHUNK(user_pnt, fence_b);
  
  alloc_cur_pnts--;
  
  /* are we free'ing a dblock entry? */
  if (BIT_IS_SET(bblock_p->bb_flags, BBLOCK_DBLOCK)) {
    
#if STORE_SEEN_COUNT
    dblock_p->db_overhead.ov_seen_c++;
#endif
    
    if (fence_b) {
      user_size = dblock_p->db_size - FENCE_OVERHEAD_SIZE;
    }
    else {
      user_size = dblock_p->db_size;
    }
    
    /* print transaction info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
      _dmalloc_message("*** free: at '%s' pnt '%s': size %ld, alloced at '%s'",
		       _chunk_desc_pnt(where_buf, sizeof(where_buf),
				       file, line),
		       display_pnt(user_pnt, &dblock_p->db_overhead,
				   disp_buf, sizeof(disp_buf)),
		       user_size,
		       _chunk_desc_pnt(where_buf2, sizeof(where_buf2),
				       dblock_p->db_file,
				       dblock_p->db_line));
    }
    
#if MEMORY_TABLE_LOG
    if (! realloc_b) {
      _table_free(dblock_p->db_file, dblock_p->db_line, user_size);
    }
#endif
    
    /* count the bits */
    bit_n = bblock_p->bb_bit_n;
    
    /* monitor current allocation level */
    alloc_current -= user_size;
    alloc_cur_given -= 1 << bit_n;
    free_space_count += 1 << bit_n;
    
    /* adjust the pointer info structure */
    dblock_p->db_flags = DBLOCK_FREE;
    dblock_p->db_bblock = bblock_p;
    dblock_p->db_use_iter = _dmalloc_iter_c;
    /* put pointer on the dblock free list */
    dblock_p->db_next = free_dblock[bit_n];
    free_dblock[bit_n] = dblock_p;
    
    /* should we set free memory with freed chars? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
      (void)memset(chunk_pnt, FREE_BLANK_CHAR, 1 << bit_n);
    }
    
    return FREE_NOERROR;
  }
  
#if STORE_SEEN_COUNT
  bblock_p->bb_overhead.ov_seen_c++;
#endif
  
  if (fence_b) {
    user_size = bblock_p->bb_size - FENCE_OVERHEAD_SIZE;
  }
  else {
    user_size = bblock_p->bb_size;
  }
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("*** free: at '%s' pnt '%s': size %lu, alloced at '%s'",
		     _chunk_desc_pnt(where_buf, sizeof(where_buf), file, line),
		     display_pnt(user_pnt, &bblock_p->bb_overhead,
				 disp_buf, sizeof(disp_buf)),
		     user_size,
		     _chunk_desc_pnt(where_buf2, sizeof(where_buf2),
				     bblock_p->bb_file,
				     bblock_p->bb_line));
  }
  
#if MEMORY_TABLE_LOG
  if (! realloc_b) {
    _table_free(bblock_p->bb_file, bblock_p->bb_line, user_size);
  }
#endif
  
  block_n = NUM_BLOCKS(bblock_p->bb_size);
  /*
   * If the user is requesting a page-aligned block of data then we
   * will need another block below the allocation just for the fence
   * information.  Ugh.
   */
  if (valloc_b && fence_b) {
    block_n++;
  }
  given = block_n * BLOCK_SIZE;
  NUM_BITS(given, bit_n);
  
  /* if we are smaller than a basic block and not valloc then error */ 
  if (bit_n < BASIC_BLOCK && (! valloc_b)) {
    dmalloc_errno = ERROR_BAD_SIZE_INFO;
    log_error_info(file, line, bblock_p->bb_file, bblock_p->bb_line, user_pnt,
		   0, NULL, "free");
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* monitor current allocation level */
  alloc_current -= user_size;
  alloc_cur_given -= given;
  free_space_count += given;
  
  /*
   * should we set free memory with freed chars?
   *
   * NOTE: we do this here because block_n might change below
   */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
    /*
     * if we have a valloc block with fence post info, then shift the
     * pointer down to the start of the user space
     */
    if (valloc_b && fence_b) {
      chunk_pnt = (char *)chunk_pnt - BLOCK_SIZE;
    }
    (void)memset(chunk_pnt, FREE_BLANK_CHAR, block_n * BLOCK_SIZE);
  }
  
  /*
   * Check above and below the free bblock looking for neighbors that
   * are free so we can add them together and put them in a different
   * free slot.
   *
   * NOTE: all of these block's reuse-iter count will be moved ahead
   * because we are encorporating in this newly freed block.
   */
  
  if (prev_p != NULL
      && BIT_IS_SET(prev_p->bb_flags, BBLOCK_START_FREE)) {
    
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
    
    /* remove from the free linked list */
    if (list_p == NULL) {
      free_bblock[prev_p->bb_bit_n] = prev_p->bb_next;
    }
    else {
      list_p->bb_next = prev_p->bb_next;
    }
    
    /* now we add in the previous guys space and we are now freeing it again */
    block_n += prev_p->bb_block_n;
    NUM_BITS(block_n * BLOCK_SIZE, bit_n);
    bblock_p = prev_p;
  }
  if (next_p != NULL
      && (BIT_IS_SET(next_p->bb_flags, BBLOCK_START_FREE)
	  || BIT_IS_SET(next_p->bb_flags, BBLOCK_FREE))) {
    
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
    
    /* remove from the free linked list */
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
  set_bblock_admin(block_n, bblock_p, BBLOCK_FREE, NULL, 0, 0,
		   free_bblock[bit_n], bit_n);
  
  /* block goes at the start of the free list */
  free_bblock[bit_n] = bblock_p;
  
  return FREE_NOERROR;
}

/*
 * Reallocate a section of memory
 */
void	*_chunk_realloc(const char *file, const unsigned int line,
			void *old_user_p, unsigned long new_size,
			const int func_id)
{
  void		*old_chunk_p, *new_chunk_p, *new_user_p, *ret_addr;
  char		*old_file;
  char		where_buf[MAX_FILE_LENGTH + 64];
  char		where_buf2[MAX_FILE_LENGTH + 64];
  const char	*trans_log;
  unsigned long	*seen_cp;
  int		valloc_b, fence_b;
  unsigned int	old_size, size, old_line, alloc_size;
  unsigned int	old_bit_n, new_bit_n;
  
  /* counts calls to realloc */
  if (func_id == DMALLOC_FUNC_RECALLOC) {
    recalloc_count++;
  }
  else {
    realloc_count++;
  }
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (new_size == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, NULL, 0, NULL, 0,
		   "bad zero byte allocation request", "realloc");
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
#endif
  
  /* by now malloc.c should have taken care of the realloc(NULL) case */
  if (old_user_p == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    log_error_info(file, line, NULL, 0, old_user_p, 0, "invalid pointer",
		   "realloc");
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
  
  /*
   * TODO: for bblocks it would be nice to examine the above memory
   * looking for free blocks that we can absorb into this one.
   */
  
  /* get info about old pointer */
  if (! _chunk_read_info(old_user_p, "realloc", &old_size, &alloc_size,
			 &old_file, &old_line, &ret_addr, &seen_cp, &valloc_b,
			 &fence_b)) {
    return REALLOC_ERROR;
  }
  
  if (ret_addr != NULL) {
    old_file = ret_addr;
  }
  
  /* adjust the pointer down if fence-posting */
  if (fence_b) {
    old_chunk_p = USER_TO_CHUNK(old_user_p, 1);
    new_size += FENCE_OVERHEAD_SIZE;
  }
  else {
    old_chunk_p = old_user_p;
  }
  
  /* get the old and new bit sizes */
  NUM_BITS(alloc_size, old_bit_n);
  NUM_BITS(new_size, new_bit_n);
  
  /* if we are not realloc copying and the size is the same */
  if (valloc_b
      || BIT_IS_SET(_dmalloc_flags, DEBUG_REALLOC_COPY)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)
      || old_bit_n != new_bit_n
      || NUM_BLOCKS(old_size) != NUM_BLOCKS(new_size)) {
    
    /* readjust info */
    if (fence_b) {
      old_size -= FENCE_OVERHEAD_SIZE;
      new_size -= FENCE_OVERHEAD_SIZE;
    }
    
    /* allocate space for new chunk -- this will */
    new_user_p = _chunk_malloc(file, line, new_size, func_id, 0);
    if (new_user_p == MALLOC_ERROR) {
      return REALLOC_ERROR;
    }
    
    /*
     * NOTE: _chunk_malloc() already took care of the fence stuff and
     * an zeroing of memory.
     */
    
    /* copy stuff into new section of memory */
    size = MIN(new_size, old_size);
    if (size > 0) {
      memcpy((char *)new_user_p, (char *)old_chunk_p, size);
    }
    
    /* free old pointer */
    if (_chunk_free(file, line, old_user_p, 1) != FREE_NOERROR) {
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
    new_chunk_p = old_chunk_p;
    
    /* rewrite size information */
    if (! chunk_write_info(file, line, new_chunk_p, new_size, "realloc",
			   fence_b)) {
      return REALLOC_ERROR;
    }
    
    /* overwrite to-be-alloced or non-used portion of memory */
    size = MIN(new_size, old_size);
    
    /* if we are extending memory, write in our alloc chars */
    if ((BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)
	 || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK))
	&& alloc_size > size) {
      (void)memset((char *)new_chunk_p + size, ALLOC_BLANK_CHAR,
		   alloc_size - size);
    }
    
    /* write in fence-post info and adjust new pointer over fence info */
    if (fence_b) {
      FENCE_WRITE(new_chunk_p, new_size);
    }
    
    if (fence_b) {
      new_user_p = CHUNK_TO_USER(new_chunk_p, 1);
      old_size -= FENCE_OVERHEAD_SIZE;
      new_size -= FENCE_OVERHEAD_SIZE;
    }
    else {
      new_user_p = new_chunk_p;
    }
    
    if (func_id == DMALLOC_FUNC_RECALLOC && new_size > old_size) {
      (void)memset((char *)new_user_p + old_size, 0, new_size - old_size);
    }
    
#if STORE_SEEN_COUNT
    /* we see in inbound and outbound so we need to increment by 2 */
    *seen_cp += 2;
#endif
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    if (func_id == DMALLOC_FUNC_RECALLOC) {
      trans_log = "recalloc";
    }
    else {
      trans_log = "realloc";
    }
    _dmalloc_message("*** %s: at '%s' from '%#lx' (%u bytes) file '%s' to '%#lx' (%lu bytes)",
		     trans_log, _chunk_desc_pnt(where_buf, sizeof(where_buf),
						file, line),
		     (unsigned long)old_user_p, old_size,
		     _chunk_desc_pnt(where_buf2, sizeof(where_buf2),
				     old_file, old_line),
		     (unsigned long)new_user_p, new_size);
  }
  
#if MEMORY_TABLE_LOG
  _table_free(old_file, old_line, old_size);
  _table_alloc(file, line, new_size);
#endif
  
  return new_user_p;
}

/***************************** diagnostic routines ***************************/

/*
 * Log present free and used lists
 */
void	_chunk_list_count(void)
{
  int		bit_c, block_c;
  char		buf[256], *buf_p, *bounds_p;
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  
  buf_p = buf;
  bounds_p = buf + sizeof(buf);
  
  /* we have to punch the 1st \0 in case be add nothing to the buffer below */
  *buf_p = '\0';
  
  /* dump the free (and later used) list counts */
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
      buf_p += loc_snprintf(buf_p, bounds_p - buf_p, " %d/%d", block_c, bit_c);
    }
  }
  
  _dmalloc_message("free bucket count/bits: %s", buf);
}

/*
 * Log statistics on the heap
 */
void	_chunk_stats(void)
{
  unsigned long	overhead, tot_space, wasted;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("dumping chunk statistics");
  }
  
  tot_space = alloc_current + free_space_count;
  overhead = (bblock_adm_count + dblock_adm_count) * BLOCK_SIZE;
  if (alloc_max_given >= tot_space) {
    wasted = 0;
  }
  else {
    wasted = tot_space - alloc_max_given;
  }
  
  /* version information */
  _dmalloc_message("basic-block %d bytes, alignment %d bytes, heap grows %s",
		   BLOCK_SIZE, ALLOCATION_ALIGNMENT,
		   (HEAP_GROWS_UP ? "up" : "down"));
  
  /* general heap information */
  _dmalloc_message("heap: %#lx to %#lx, size %ld bytes (%ld blocks)",
		   (unsigned long)_heap_base, (unsigned long)_heap_last,
		   (long)HEAP_SIZE, bblock_count);
  _dmalloc_message("heap checked %ld", check_count);
  
  /* log user allocation information */
  _dmalloc_message("alloc calls: malloc %lu, calloc %lu, realloc %lu, free %lu",
		   malloc_count, calloc_count, realloc_count, free_count);
  _dmalloc_message("alloc calls: recalloc %lu, memalign %lu, valloc %lu",
		   recalloc_count, memalign_count, valloc_count);
  _dmalloc_message(" total memory allocated: %lu bytes (%lu pnts)",
		  alloc_total, alloc_tot_pnts);
  
  /* maximum stats */
  _dmalloc_message(" max in use at one time: %lu bytes (%lu pnts)",
		  alloc_maximum, alloc_max_pnts);
  _dmalloc_message("max alloced with 1 call: %lu bytes",
		  alloc_one_max);
  _dmalloc_message("max alloc rounding loss: %lu bytes (%lu%%)",
		  alloc_max_given - alloc_maximum,
		  (alloc_max_given == 0 ? 0 :
		   ((alloc_max_given - alloc_maximum) * 100) /
		   alloc_max_given));
  _dmalloc_message("max memory space wasted: %lu bytes (%lu%%)",
		   wasted,
		   (tot_space == 0 ? 0 : ((wasted * 100) / tot_space)));
  
  /* final stats */
  _dmalloc_message("final user memory space: basic %ld, divided %ld, %ld bytes",
		   bblock_count - bblock_adm_count - dblock_count -
		   dblock_adm_count - extern_count, dblock_count,
		   tot_space);
  _dmalloc_message(" final admin overhead: basic %ld, divided %ld, %ld bytes (%ld%%)",
		   bblock_adm_count, dblock_adm_count, overhead,
		   (HEAP_SIZE == 0 ? 0 : (overhead * 100) / HEAP_SIZE));
  _dmalloc_message(" final external space: %ld bytes (%ld blocks)",
		   extern_count * BLOCK_SIZE, extern_count);
  
#if MEMORY_TABLE_LOG
  _dmalloc_message("top %d allocations:", MEMORY_TABLE_LOG);
  _table_log_info(MEMORY_TABLE_LOG, 1);
#endif
}

/*
 * Dump the pointer information that has changed since mark.  If
 * non_freed_b is 1 then log the new not-freed (i.e. used) pointers.
 * If free_b is 1 then log the new freed pointers.  If details_b is 1
 * then dump the individual pointer entries instead of just the
 * summary.
 */
void	_chunk_log_changed(const unsigned long mark, const int not_freed_b,
			   const int freed_b, const int details_b)
{
  bblock_adm_t	*this_adm_p;
  bblock_t	*bblock_p;
  dblock_t	*dblock_p;
  void		*user_pnt, *chunk_pnt;
  unsigned long	user_size;
  unsigned int	block_type;
  int		known_b, fence_b;
  char		out[DUMP_SPACE * 4], *which_str;
  char		where_buf[MAX_FILE_LENGTH + 64], disp_buf[64];
  int		unknown_size_c = 0, unknown_block_c = 0, out_len;
  int		size_c = 0, block_c = 0;
  
  if (not_freed_b && freed_b) {
    which_str = "not-freed and freed";
  }
  else if (not_freed_b) {
    which_str = "not-freed";
  }
  else if (freed_b) {
    which_str = "freed";
  }
  else {
    return;
  }
  
  _dmalloc_message("dumping %s pointers changed since %lu:",
		   which_str, mark);
  
  /* clear out our memory table so we can fill it with pointer info */
  _table_clear();
  
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
    block_type = BBLOCK_FLAG_TYPE(bblock_p->bb_flags);
    switch (block_type) {
      
    case BBLOCK_START_USER:
    case BBLOCK_START_FREE:
      
      /* are we displaying the currently used pointers? */
      if (! ((not_freed_b && block_type == BBLOCK_START_USER)
	     || (freed_b && block_type == BBLOCK_START_FREE))) {
	continue;
      }
      /* has this changed since the mark */
      if (bblock_p->bb_use_iter <= mark) {
	continue;
      }
      
      /* find pointer to memory chunk */
      chunk_pnt = BLOCK_POINTER(this_adm_p->ba_pos_n +
				(bblock_p - this_adm_p->ba_blocks));
      fence_b = BIT_IS_SET(bblock_p->bb_flags, BBLOCK_FENCE);
      user_pnt = CHUNK_TO_USER(chunk_pnt, fence_b);
      
      if (fence_b) {
	user_size = bblock_p->bb_size - FENCE_OVERHEAD_SIZE;
      }
      else {
	user_size = bblock_p->bb_size;
      }
      
      /* unknown pointer? */
      if (bblock_p->bb_file == DMALLOC_DEFAULT_FILE
	  || bblock_p->bb_line == DMALLOC_DEFAULT_LINE) {
	unknown_block_c++;
	unknown_size_c += user_size;
	known_b = 0;
      }
      else {
	known_b = 1;
      }
      
      if (known_b || (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_KNOWN))) {
	if (details_b) {
	  _dmalloc_message(" not freed: '%s' (%ld bytes) from '%s'",
			   display_pnt(user_pnt, &bblock_p->bb_overhead,
				       disp_buf, sizeof(disp_buf)),
			   user_size,
			   _chunk_desc_pnt(where_buf, sizeof(where_buf),
					   bblock_p->bb_file,
					   bblock_p->bb_line));
	  
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE)) {
	    out_len = expand_chars((char *)user_pnt, DUMP_SPACE,
				   out, sizeof(out));
	    _dmalloc_message("  dump of '%#lx': '%.*s'",
			     (unsigned long)user_pnt, out_len, out);
	  }
	}
	_table_alloc(bblock_p->bb_file, bblock_p->bb_line, user_size);
      }
      
      size_c += user_size;
      block_c++;
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      for (dblock_p = bblock_p->bb_slot_p->da_block;
	   dblock_p < bblock_p->bb_slot_p->da_block + DB_PER_ADMIN;
	   dblock_p++) {
	bblock_t	*bb_p;
	
	/* see if this admin slot has ever been used */
	if (dblock_p->db_bblock == NULL && dblock_p->db_next == NULL) {
	  continue;
	}
	
	/* do we want this slot */
	if (! ((freed_b && dblock_p->db_flags == DBLOCK_FREE)
	       || (not_freed_b && dblock_p->db_flags != DBLOCK_FREE))) {
	  continue;
	}
	/* has this changed since the mark */
	if (dblock_p->db_use_iter <= mark) {
	  continue;
	}
	
	bb_p = dblock_p->db_bblock;
	if (bb_p == NULL) {
	  dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	  dmalloc_error("_chunk_dump_unfreed");
	  return;
	}
	
	chunk_pnt = (char *)bb_p->bb_mem + (dblock_p - bb_p->bb_dblock) *
	  (1 << bb_p->bb_bit_n);
	fence_b = BIT_IS_SET(dblock_p->db_flags, DBLOCK_FENCE);
	user_pnt = CHUNK_TO_USER(chunk_pnt, fence_b);
	
	if (fence_b) {
	  user_size = dblock_p->db_size - FENCE_OVERHEAD_SIZE;
	}
	else {
	  user_size = dblock_p->db_size;
	}
	
	/* unknown pointer? */
	if (dblock_p->db_file == DMALLOC_DEFAULT_FILE
	    || dblock_p->db_line == DMALLOC_DEFAULT_LINE) {
	  unknown_block_c++;
	  unknown_size_c += user_size;
	  known_b = 0;
	}
	else {
	  known_b = 1;
	}
	
	if (known_b || (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_KNOWN))) {
	  if (details_b) {
	    _dmalloc_message(" %s: '%s' (%ld bytes) from '%s'",
			     (dblock_p->db_flags == DBLOCK_FREE ?
			      "freed" : "not freed"),
			     display_pnt(user_pnt, &dblock_p->db_overhead,
					 disp_buf, sizeof(disp_buf)),
			     user_size,
			     _chunk_desc_pnt(where_buf, sizeof(where_buf),
					     dblock_p->db_file,
					     dblock_p->db_line));
	    
	    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE)) {
	      out_len = expand_chars((char *)user_pnt, DUMP_SPACE,
				     out, sizeof(out));
	      _dmalloc_message("  dump of '%#lx': '%.*s'",
			       (unsigned long)user_pnt, out_len, out);
	    }
	  }
	  _table_alloc(dblock_p->db_file, dblock_p->db_line, user_size);
	}
	
	size_c += user_size;
	block_c++;
      }
      break;
    }
  }
  
  /* dump the summary and clear the table */
  _table_log_info(0, 0);
  _table_clear();
  
  /* copy out size of pointers */
  if (block_c > 0) {
    if (block_c - unknown_block_c > 0) {
      _dmalloc_message(" known memory: %d pointer%s, %d bytes",
		       block_c - unknown_block_c,
		       (block_c - unknown_block_c == 1 ? "" : "s"),
		       size_c - unknown_size_c);
    }
    if (unknown_block_c > 0) {
      _dmalloc_message(" unknown memory: %d pointer%s, %d bytes",
		       unknown_block_c, (unknown_block_c == 1 ? "" : "s"),
		       unknown_size_c);
    }
  }
}
