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
 * The author may be contacted at gray.watson@letters.com
 */

/*
 * This file contains algorithm level routine for the heap.  They handle the
 * manipulation and administration of chunks of memory.
 */

#include <ctype.h>

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "chunk.h"
#include "chunk_loc.h"
#include "compat.h"
#include "debug_val.h"
#include "error.h"
#include "error_str.h"
#include "error_val.h"
#include "heap.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: chunk.c,v 1.107 1997/03/21 21:22:49 gray Exp $";
#endif

/*
 * the unknown file pointer.  did not use DMALLOC_UNKNOWN_FILE everywhere else
 * the pointers would be different.
 */
EXPORT	char		*_dmalloc_unknown_file = DMALLOC_UNKNOWN_FILE;

/* local routines */
EXPORT	void		_chunk_log_heap_map(void);

/* local variables */

/* free lists of bblocks and dblocks */
LOCAL	bblock_t	*free_bblock[MAX_SLOTS];
LOCAL	dblock_t	*free_dblock[BASIC_BLOCK];

/* administrative structures */
LOCAL	bblock_adm_t	*bblock_adm_head = NULL; /* pointer to 1st bb_admin */
LOCAL	bblock_adm_t	*bblock_adm_tail = NULL; /* pointer to last bb_admin */
LOCAL	unsigned int	smallest_block	= 0;	/* smallest size in bits */
LOCAL	unsigned int	bits[MAX_SLOTS];
LOCAL	char		fence_bottom[FENCE_BOTTOM], fence_top[FENCE_TOP];

/* user information shifts for display purposes */
LOCAL	int		pnt_below_adm	= 0;	/* add to pnt for display */
LOCAL	int		pnt_total_adm	= 0;	/* total adm per pointer */

/* memory stats */
LOCAL	long		alloc_current	= 0;	/* current memory usage */
LOCAL	long		alloc_maximum	= 0;	/* maximum memory usage  */
LOCAL	long		alloc_cur_given	= 0;	/* current mem given */
LOCAL	long		alloc_max_given	= 0;	/* maximum mem given  */
LOCAL	long		alloc_total	= 0;	/* total allocation */
LOCAL	unsigned long	alloc_one_max	= 0;	/* maximum at once */
LOCAL	int		free_space_count = 0;	/* count the free bytes */

/* pointer stats */
LOCAL	long		alloc_cur_pnts	= 0;	/* current pointers */
LOCAL	long		alloc_max_pnts	= 0;	/* maximum pointers */
LOCAL	long		alloc_tot_pnts	= 0;	/* current pointers */

/* admin counts */
LOCAL	long		bblock_adm_count = 0;	/* count of bblock_admin */
LOCAL	long		dblock_adm_count = 0;	/* count of dblock_admin */
LOCAL	long		bblock_count	 = 0;	/* count of basic-blocks */
LOCAL	long		dblock_count	 = 0;	/* count of divided-blocks */
LOCAL	long		extern_count	 = 0;	/* count of external blocks */
LOCAL	long		check_count	 = 0;	/* count of heap-checks */

/* alloc counts */
EXPORT	long		_calloc_count	= 0;	/* # callocs, done in alloc */
LOCAL	long		free_count	= 0;	/* count the frees */
LOCAL	long		malloc_count	= 0;	/* count the mallocs */
LOCAL	long		realloc_count	= 0;	/* count the reallocs */

/******************************* misc routines *******************************/

/*
 * startup the low level malloc routines
 */
EXPORT	int	_chunk_startup(void)
{
  unsigned int	binc, num;
  
  /* calculate the smallest possible block */
  for (smallest_block = DEFAULT_SMALLEST_BLOCK;
       DB_PER_ADMIN < BLOCK_SIZE / (1 << smallest_block);
       smallest_block++);
  
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
  
  /* initialize free bins */
  for (binc = 0; binc < MAX_SLOTS; binc++)
    free_bblock[binc] = NULL;
  for (binc = 0; binc < BASIC_BLOCK; binc++)
    free_dblock[binc] = NULL;
  
  /* make array for NUM_BITS calculation */
  bits[0] = 1;
  for (binc = 1, num = 2; binc < MAX_SLOTS; binc++, num *= 2)
    bits[binc] = num;
  
  /* assign value to add to pointers when displaying */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
    pnt_below_adm = FENCE_BOTTOM;
    pnt_total_adm = FENCE_OVERHEAD;
  }
  else {
    pnt_below_adm = 0;
    pnt_total_adm = 0;
  }
  
  {
    unsigned FENCE_MAGIC_TYPE	value;
    char			*posp, *maxp;
    
    value = FENCE_MAGIC_BOTTOM;
    maxp = fence_bottom + FENCE_BOTTOM;
    for (posp = fence_bottom; posp < maxp; posp += sizeof(FENCE_MAGIC_TYPE)) {
      if (posp + sizeof(FENCE_MAGIC_TYPE) <= maxp)
	memcpy(posp, (char *)&value, sizeof(FENCE_MAGIC_TYPE));
      else
	memcpy(posp, (char *)&value, maxp - posp);
    }
    
    value = FENCE_MAGIC_TOP;
    maxp = fence_top + FENCE_TOP;
    for (posp = fence_top; posp < maxp; posp += sizeof(FENCE_MAGIC_TYPE)) {
      if (posp + sizeof(FENCE_MAGIC_TYPE) <= maxp)
	memcpy(posp, (char *)&value, sizeof(FENCE_MAGIC_TYPE));
      else
	memcpy(posp, (char *)&value, maxp - posp);
    }
  }
  
  return NOERROR;
}

/*
 * display printable chars from BUF of SIZE, non-printables as \%03o
 */
LOCAL	char	*expand_buf(const void * buf, const int size)
{
  static char	out[DUMP_SPACE_BUF];
  int		sizec;
  const void	*bufp;
  char	 	*outp;
  
  for (sizec = 0, outp = out, bufp = buf; sizec < size;
       sizec++, bufp = (char *)bufp + 1) {
    char	*specp;
    
    /* handle special chars */
    if (outp + 2 >= out + sizeof(out))
      break;
    
    /* search for special characters */
    for (specp = SPECIAL_CHARS + 1; *(specp - 1) != NULLC; specp += 2)
      if (*specp == *(char *)bufp)
	break;
    
    /* did we find one? */
    if (*(specp - 1) != NULLC) {
      if (outp + 2 >= out + sizeof(out))
	break;
      (void)sprintf(outp, "\\%c", *(specp - 1));
      outp += 2;
      continue;
    }
    
    if (*(unsigned char *)bufp < 128 && isprint(*(char *)bufp)) {
      if (outp + 1 >= out + sizeof(out))
	break;
      *outp = *(char *)bufp;
      outp += 1;
    }
    else {
      if (outp + 4 >= out + sizeof(out))
	break;
      (void)sprintf(outp, "\\%03o", *(unsigned char *)bufp);
      outp += 4;
    }
  }
  
  *outp = NULLC;
  return out;
}

/*
 * describe pnt from its FILE, LINE into BUF
 */
LOCAL	void	desc_pnt(char * buf, const char * file,
			 const unsigned int line)
{
  if ((file == DMALLOC_DEFAULT_FILE || file == _dmalloc_unknown_file)
      && line == DMALLOC_DEFAULT_LINE)
    (void)sprintf(buf, "%s", _dmalloc_unknown_file);
  else if (line == DMALLOC_DEFAULT_LINE)
    (void)sprintf(buf, "ra=%#lx", (long)file);
  else if (file == DMALLOC_DEFAULT_FILE)
    (void)sprintf(buf, "ra=ERROR(line=%u)", line);
  else
    (void)sprintf(buf, "%s:%u", file, line);
}

/*
 * display a bad pointer with FILE and LINE information
 */
EXPORT	char	*_chunk_display_where(const char * file,
				      const unsigned int line)
{
  static char	buf[256];
  desc_pnt(buf, file, line);
  return buf;
}

/*
 * small hack here.  same as above but different buffer space so we
 * can use 2 calls in 1 printf.
 */
LOCAL	char	*chunk_display_where2(const char * file,
				      const unsigned int line)
{
  static char	buf[256];
  desc_pnt(buf, file, line);
  return buf;
}

/*
 * display a pointer PNT and information about it.
 */
LOCAL	char	*display_pnt(const void * pnt, const overhead_t * overp)
{
  static char	buf[256];
  char		buf2[64];
  
  (void)sprintf(buf, "%#lx", (long)pnt);
  
#if STORE_SEEN_COUNT
  (void)sprintf(buf2, "|s%d", overp->ov_seenc);
  (void)strcat(buf, buf2);
#endif
  
#if STORE_ITERATION_COUNT
  (void)sprintf(buf2, "|i%ld", overp->ov_iteration);
  (void)strcat(buf, buf2);
#endif
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIMEVAL
    (void)sprintf(buf2, "|w%s", _dmalloc_ptimeval(&overp->ov_timeval, FALSE));
    (void)strcat(buf, buf2);
#else
#if STORE_TIME
    (void)sprintf(buf2, "|w%s", _dmalloc_ptime(&overp->ov_time, FALSE));
    (void)strcat(buf, buf2);
#endif
#endif
  }
  
#if STORE_THREAD_ID
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_THREAD_ID)) {
    (void)strcat(buf, "|t");
    THREAD_ID_TO_STRING(buf2, overp->ov_thread_id);
    (void)strcat(buf, buf2);
  }
#endif
  
  return buf;
}

/*
 * log information about bad PNT (if PNT_KNOWN) from FILE, LINE.  Bad
 * because of REASON (if NULL then use error-code), from WHERE.
 */
LOCAL	void	log_error_info(const char * file, const unsigned int line,
			       const char pnt_known, const void * pnt,
			       const char * reason, const char * where,
			       const char dump)
{
  static char	dump_bottom = FALSE;
  const char	*reason_str;
  
  if (reason == NULL)
    reason_str = errlist[dmalloc_errno];
  else
    reason_str = reason;
  
  /* dump the pointer information */
  if (pnt_known)
    _dmalloc_message("%s: %s: pointer '%#lx' from '%s'",
		     where, reason_str, pnt, _chunk_display_where(file, line));
  else
    _dmalloc_message("%s: %s: from '%s'",
		     where, reason_str, _chunk_display_where(file, line));
  
  /* NOTE: this has the potential for generating a seg-fault */
  if (dump && pnt_known && BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_BAD_SPACE)) {
    if (! dump_bottom) {
      _dmalloc_message("Dump of proper fence-bottom bytes: '%s'",
		       expand_buf(fence_bottom, FENCE_BOTTOM));
      dump_bottom = TRUE;
    }
    if (IS_IN_HEAP((char *)pnt - pnt_below_adm))
      _dmalloc_message("Dump of '%#lx'-%d: '%s'",
		       pnt, pnt_below_adm,
		       expand_buf((char *)pnt - pnt_below_adm, DUMP_SPACE));
    else
      _dmalloc_message("Dump of '%#lx'-%d failed: not in heap",
		       pnt, pnt_below_adm);
  }
}

/************************* fence-post error functions ************************/

/*
 * check PNT of SIZE for fence-post magic numbers, returns ERROR or NOERROR
 */
LOCAL	int	fence_read(const char * file, const unsigned int line,
			   const void * pnt, const unsigned int size,
			   const char * where)
{
  /* check magic numbers in bottom of allocation block */
  if (memcmp(fence_bottom, (char *)pnt, FENCE_BOTTOM) != 0) {
    dmalloc_errno = ERROR_UNDER_FENCE;
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt),
		   NULL, where, TRUE);
    dmalloc_error("fence_read");
    return ERROR;
  }
  
  /* check numbers at top of allocation block */
  if (memcmp(fence_top, (char *)pnt + size - FENCE_TOP, FENCE_TOP) != 0) {
    dmalloc_errno = ERROR_OVER_FENCE;
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt),
		   NULL, where, TRUE);
    dmalloc_error("fence_read");
    return ERROR;
  }
  
  return NOERROR;
}

/************************** administration functions *************************/

/*
 * set the information for BLOCKN administrative block(s) at BBLOCKP.
 * returns [NO]ERROR
 */
LOCAL	int	set_bblock_admin(const int blockn, bblock_t * bblockp,
				 const int flag, const unsigned short num,
				 const unsigned long info, const void * pnt)
{
  int		bblockc;
  bblock_adm_t	*bblock_admp;
  
  bblock_admp = (bblock_adm_t *)BLOCK_NUM_TO_PNT(bblockp);
  
  for (bblockc = 0; bblockc < blockn; bblockc++, bblockp++) {
    if (bblockp == bblock_admp->ba_blocks + BB_PER_ADMIN) {
      bblock_admp = bblock_admp->ba_next;
      if (bblock_admp == NULL) {
	dmalloc_errno = ERROR_BAD_ADMIN_LIST;
	dmalloc_error("_set_bblock_admin");
	return FREE_ERROR;
      }
      
      bblockp = bblock_admp->ba_blocks;
    }
    
    /* set bblock info */
    switch (flag) {
      
    case BBLOCK_START_USER:
      if (bblockc == 0)
	bblockp->bb_flags = BBLOCK_START_USER;
      else
	bblockp->bb_flags = BBLOCK_USER;
      
      bblockp->bb_line	= (unsigned short)num;
      bblockp->bb_size	= (unsigned int)info;
      bblockp->bb_file	= (char *)pnt;
      break;
      
    case BBLOCK_FREE:
      bblockp->bb_flags		= BBLOCK_FREE;
      bblockp->bb_bitn		= (unsigned short)num;
      bblockp->bb_blockn	= (unsigned int)blockn;
      if (bblockc == 0)
	bblockp->bb_next	= (struct bblock_st *)pnt;
      else
	bblockp->bb_next	= (struct bblock_st *)NULL;
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
 * parse the free lists looking for a slot of MANY bblocks returning
 * such a slot in RETP or NULL if none.
 * returns [NO]ERROR
 */
LOCAL	int	find_free_bblocks(const unsigned int many, bblock_t ** retp)
{
  bblock_t	*bblockp, *prevp;
  bblock_t	*bestp = NULL, *best_prevp = NULL;
  int		bitc, bitn, blockn, pos, best = 0;
  bblock_adm_t	*admp;
  
  /*
   * NOTE: it is here were we can implement first/best/worst fit.
   * Depending on fragmentation, we may want to impose limits on the
   * level jump or do something to try and limit the number of chunks.
   */
  
  /* start at correct bit-size and work up till we find a match */
  NUM_BITS(many, bitc);
  bitc += BASIC_BLOCK;
  
  for (; bitc < MAX_SLOTS; bitc++) {
    for (bblockp = free_bblock[bitc], prevp = NULL; bblockp != NULL;
	 prevp = bblockp, bblockp = bblockp->bb_next) {
      
      if (bblockp->bb_blockn >= many
#if BEST_FIT
	  && (best == 0 || bblockp->bb_blockn < best)
#else
#if WORST_FIT
	  && (bblockp->bb_blockn > best)
#else
#if FIRST_FIT
	  /* nothing more needs to be tested */
#endif /* FIRST_FIT */
#endif /* ! WORST_FIT */
#endif /* ! BEST_FIT */
	  ) {
	best = bblockp->bb_blockn;
	bestp = bblockp;
	best_prevp = prevp;
	
#if FIRST_FIT
	break;
#endif
      }
    }
    
    /* NOTE: we probably want to not quit here if WORST_FIT */
    if (bestp != NULL)
      break;
  }
  
  /* did we not find one? */
  if (bestp == NULL) {
    *retp = NULL;
    return NOERROR;
  }
  
  /* take it off the free list */
  if (best_prevp == NULL)
    free_bblock[bitc] = bestp->bb_next;
  else
    best_prevp->bb_next = bestp->bb_next;
  
  if (bestp->bb_blockn == many) {
    *retp = bestp;
    return NOERROR;
  }
  
  /*
   * now we need to split the block.  we return the start of the
   * current free section and add the left-over chunk to another
   * free-list with an adjusted block-count
   */
  bblockp = bestp;
  admp = (bblock_adm_t *)BLOCK_NUM_TO_PNT(bblockp);
  pos = (bblockp - admp->ba_blocks) + many;
  
  /* parse forward until we've found the correct split point */
  while (pos >= BB_PER_ADMIN) {
    pos -= BB_PER_ADMIN;
    admp = admp->ba_next;
    if (admp == NULL) {
      dmalloc_errno = ERROR_BAD_ADMIN_LIST;
      dmalloc_error("find_free_bblocks");
      return ERROR;
    }
  }
  
  bblockp = admp->ba_blocks + pos;
  if (bblockp->bb_flags != BBLOCK_FREE) {
    dmalloc_errno = ERROR_BAD_FREE_MEM;
    dmalloc_error("find_free_bblocks");
    return ERROR;
  }
  
  blockn = bblockp->bb_blockn - many;
  NUM_BITS(blockn * BLOCK_SIZE, bitn);
  
  set_bblock_admin(blockn, bblockp, BBLOCK_FREE, bitn, 0, free_bblock[bitn]);
  free_bblock[bitn] = bblockp;
  
  *retp = bestp;
  return NOERROR;
}

/*
 * get MANY new bblock block(s) from the free list physically
 * allocation.  return a pointer to the new blocks' memory in MEMP.
 * returns the blocks or NULL on error.
 */
LOCAL	bblock_t	*get_bblocks(const int many, void ** memp)
{
  static bblock_adm_t	*freep = NULL;	/* pointer to block with free slots */
  static int		freec = 0;	/* count of free slots */
  bblock_adm_t		*admp, *adm_store[MAX_ADMIN_STORE];
  bblock_t		*bblockp, *retp = NULL;
  void			*mem = NULL, *extern_mem = NULL;
  int			bblockc, count, admc = 0, externc = 0;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN))
    _dmalloc_message("need %d bblocks (%d bytes)", many, many * BLOCK_SIZE);
  
  /* is there anything on the user-free list(s)? */
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)) {
    
    if (find_free_bblocks(many, &bblockp) != NOERROR)
      return NULL;
    
    /* did we find anything? */
    if (bblockp != NULL) {
      free_space_count -= many * BLOCK_SIZE;
      
      /* space should be free */
      if (bblockp->bb_flags != BBLOCK_FREE) {
	dmalloc_errno = ERROR_BAD_FREE_MEM;
	dmalloc_error("get_bblocks");
	return NULL;
      }
      
      admp = (bblock_adm_t *)BLOCK_NUM_TO_PNT(bblockp);
      if (memp != NULL)
	*memp = BLOCK_POINTER(admp->ba_posn + (bblockp - admp->ba_blocks));
      return bblockp;
    }
  }
  
  /*
   * immediately allocate the memory necessary for the new blocks
   * because we need to know if external blocks we sbrk'd so we can
   * account for them in terms of admin slots
   */
  mem = _heap_alloc(many * BLOCK_SIZE, &extern_mem, &externc);
  if (mem == HEAP_ALLOC_ERROR)
    return NULL;
  
  /* account for allocated and any external blocks */
  bblock_count += many + externc;
  
  /*
   * do we have enough bblock-admin slots for the blocks we need, the
   * bblock-admin blocks themselves, and any external blocks found?
   */
  while (many + admc + externc > freec) {
    
    /* get some more space for a bblock_admin structure */
    admp = (bblock_adm_t *)_heap_alloc(BLOCK_SIZE, NULL, &count);
    if (admp == (bblock_adm_t *)HEAP_ALLOC_ERROR)
      return NULL;
    
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
    admp = mem;
    mem = (char *)mem + BLOCK_SIZE;
    
    /*
     * Since we are just allocating some more slots here, we need to
     * account for the admin block space later.  We save the admin
     * block pointer in a little queue which cannot overflow.  If it
     * does, it means that someone sbrk+alloced some enormous chunk
     * equivalent to (BLOCK_SIZE * (BB_PER_ADMIN - 1) *
     * MAX_ADMIN_STORE) bytes.
     */
    if (admc == MAX_ADMIN_STORE) {
      dmalloc_errno = ERROR_EXTERNAL_HUGE;
      dmalloc_error("get_bblocks");
      return NULL;
    }
    
    /* store new admin block in queue */
    adm_store[admc] = admp;
    admc++;
    
    /* do we need to print admin info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN))
      _dmalloc_message("new bblock-admin alloced for %d more admin slots",
		       BB_PER_ADMIN);
    
    /* initialize the new admin block and maintain the linked list */
    admp->ba_magic1 = CHUNK_MAGIC_BOTTOM;
    if (bblock_adm_tail == NULL) {
      admp->ba_posn = 0;
      bblock_adm_head = admp;
      bblock_adm_tail = admp;
    }
    else {
      admp->ba_posn = bblock_adm_tail->ba_posn + BB_PER_ADMIN;
      bblock_adm_tail->ba_next = admp;
      bblock_adm_tail = admp;
    }
    
    /* initialize the bblocks in the bblock_admin */
    for (bblockp = admp->ba_blocks; bblockp < admp->ba_blocks + BB_PER_ADMIN;
	 bblockp++) {
      bblockp->bb_flags = 0;
#if STORE_SEEN_COUNT
      bblockp->bb_overhead.ov_seenc = 0;
#endif
    }
    
    admp->ba_next = NULL;
    admp->ba_magic2 = CHUNK_MAGIC_TOP;
    
    /* set counter to next free slot */
    bblockp = admp->ba_blocks + (BB_PER_ADMIN - 1);
    bblockp->bb_flags = BBLOCK_ADMIN_FREE;
    bblockp->bb_freen = 0;
    
    /* maybe we used them up the last time? */
    if (freep == NULL)
      freep = admp;
    
    /* we add more slots less the one we just allocated to hold them */
    freec += BB_PER_ADMIN;
  }
  
  /* get the block pointer to the first free slot we have */
  bblockp = freep->ba_blocks + (BB_PER_ADMIN - 1);
  bblockc = bblockp->bb_freen;
  bblockp = freep->ba_blocks + bblockc;
  
  /* first off, handle external referenced blocks */
  for (count = 0; count < externc; count++) {
    bblockp->bb_flags = BBLOCK_EXTERNAL;
    bblockp->bb_mem = extern_mem;
    
    bblockp++;
    bblockc++;
    extern_count++;
    freec--;
    
    if (bblockp >= freep->ba_blocks + BB_PER_ADMIN) {
      freep = freep->ba_next;
      bblockp = freep->ba_blocks + (BB_PER_ADMIN - 1);
      bblockc = bblockp->bb_freen;
      bblockp = freep->ba_blocks + bblockc;
    }
  }
  
  /* handle accounting for the admin-block(s) that we allocated above */
  for (count = 0; count < admc; count++) {
    admp = adm_store[count];
    bblockp->bb_flags = BBLOCK_ADMIN;
    bblockp->bb_adminp = admp;
    bblockp->bb_posn = admp->ba_posn;
    
    bblockp++;
    bblockc++;
    bblock_adm_count++;
    freec--;
    
    if (bblockp >= freep->ba_blocks + BB_PER_ADMIN) {
      freep = freep->ba_next;
      bblockp = freep->ba_blocks + (BB_PER_ADMIN - 1);
      bblockc = bblockp->bb_freen;
      bblockp = freep->ba_blocks + bblockc;
    }
  }
  
  /*
   * finally, handle the admin slots for the needed blocks
   */
  
  /* set up return values */
  retp = freep->ba_blocks + bblockc;
  if (memp != NULL)
    *memp = mem;
  
  /* now skip over those slots, set_bblock_admin will be done after return */
  bblockc += many;
  while (bblockc >= BB_PER_ADMIN) {
    freep = freep->ba_next;
    bblockc -= BB_PER_ADMIN;
  }
  freec -= many;
  
  /*
   * do some error checking and write the last free count.  if freep
   * is NULL then next time will have to allocate another bbadmin-block
   */
  if (freep == NULL) {
    if (freec != 0) {
      dmalloc_errno = ERROR_BAD_ADMIN_LIST;
      dmalloc_error("get_bblocks");
      return NULL;
    }
  }
  else {
    if (freec <= 0 || freec >= BB_PER_ADMIN) {
      dmalloc_errno = ERROR_BAD_ADMIN_LIST;
      dmalloc_error("get_bblocks");
      return NULL;
    }
    bblockp = freep->ba_blocks + (BB_PER_ADMIN - 1);
    bblockp->bb_freen = bblockc;
  }
  
  return retp;
}

/*
 * find the bblock entry for PNT, LASTP and NEXTP point to the last
 * and next blocks starting block
 */
LOCAL	bblock_t	*find_bblock(const void * pnt, bblock_t ** lastp,
				     bblock_t ** nextp)
{
  void		*tmp;
  unsigned int	bblockc, bblockn;
  bblock_t	*last = NULL, *this;
  bblock_adm_t	*bblock_admp;
  
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
  for (bblockc = WHICH_BLOCK(pnt), bblock_admp = bblock_adm_head;
       bblockc >= BB_PER_ADMIN && bblock_admp != NULL;
       bblockc -= BB_PER_ADMIN, bblock_admp = bblock_admp->ba_next) {
    if (lastp != NULL)
      last = bblock_admp->ba_blocks + (BB_PER_ADMIN - 1);
  }
  
  if (bblock_admp == NULL) {
    dmalloc_errno = ERROR_NOT_FOUND;
    return NULL;
  }
  
  this = bblock_admp->ba_blocks + bblockc;
  
  if (lastp != NULL) {
    if (bblockc > 0)
      last = bblock_admp->ba_blocks + (bblockc - 1);
    
    /* adjust the last pointer back to start of free block */
    if (last != NULL && BIT_IS_SET(last->bb_flags, BBLOCK_FREE)) {
      if (last->bb_blockn <= bblockc)
	last = bblock_admp->ba_blocks + (bblockc - last->bb_blockn);
      else {
	/* we need to go recursive to go bblockn back, check if at 1st block */
	tmp = (char *)pnt - last->bb_blockn * BLOCK_SIZE;
	if (! IS_IN_HEAP(tmp))
	  last = NULL;
	else {
	  last = find_bblock(tmp, NULL, NULL);
	  if (last == NULL) {
	    dmalloc_error("find_bblock");
	    return NULL;
	  }
	}
      }
    }
    
    *lastp = last;
  }
  if (nextp != NULL) {
    /* next pointer should move past current allocation */
    if (BIT_IS_SET(this->bb_flags, BBLOCK_START_USER))
      bblockn = NUM_BLOCKS(this->bb_size);
    else
      bblockn = 1;
    if (bblockc + bblockn < BB_PER_ADMIN)
      *nextp = this + bblockn;
    else {
      /* we need to go recursive to go bblockn ahead, check if at last block */
      tmp = (char *)pnt + bblockn * BLOCK_SIZE;
      if (! IS_IN_HEAP(tmp))
	*nextp = NULL;
      else {
	*nextp = find_bblock(tmp, NULL, NULL);
	if (*nextp == NULL) {
	  dmalloc_error("find_bblock");
	  return NULL;
	}
      }
    }
  }
  
  return this;
}

/*
 * get MANY of contiguous dblock administrative slots.
 */
LOCAL	dblock_t	*get_dblock_admin(const int many)
{
  static int		free_slots = 0;
  static dblock_adm_t	*dblock_admp = NULL;
  dblock_t		*dblockp;
  bblock_t		*bblockp;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN))
    _dmalloc_message("need %d dblock-admin slots", many);
  
  /* do we have enough right now? */
  if (free_slots >= many) {
    dblockp = dblock_admp->da_block + (DB_PER_ADMIN - free_slots);
    free_slots -= many;
    return dblockp;
  }
  
  /*
   * allocate a new bblock of dblock admin slots, should use free list
   */
  bblockp = get_bblocks(1, (void **)&dblock_admp);
  if (bblockp == NULL)
    return NULL;
  
  dblock_adm_count++;
  free_slots = DB_PER_ADMIN;
  
  bblockp->bb_flags = BBLOCK_DBLOCK_ADMIN;
  bblockp->bb_slotp = dblock_admp;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN))
    _dmalloc_message("opened another %d dblock-admin slots", DB_PER_ADMIN);
  
  dblock_admp->da_magic1 = CHUNK_MAGIC_BOTTOM;
  
  /* initialize the db_slots */
  for (dblockp = dblock_admp->da_block;
       dblockp < dblock_admp->da_block + DB_PER_ADMIN; dblockp++) {
    dblockp->db_bblock = NULL;
    dblockp->db_next = NULL;
  }
  
  dblock_admp->da_magic2 = CHUNK_MAGIC_TOP;
  
  free_slots -= many;
  
  return dblock_admp->da_block;
}

/*
 * get a dblock of 1<<BITN sized chunks, also asked for the slot memory
 */
LOCAL	void	*get_dblock(const int bitn, const unsigned short byten,
			    const char * file, const unsigned short line,
			    overhead_t ** overp)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp, *firstp, *freep;
  void		*pnt;
  
  /* is there anything on the dblock free list? */
  dblockp = free_dblock[bitn];
  if (dblockp != NULL && ! BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)) {
    free_dblock[bitn] = dblockp->db_next;
    free_space_count -= 1 << bitn;
    
    /* find pointer to memory chunk */
    pnt = (char *)dblockp->db_bblock->bb_mem +
      (dblockp - dblockp->db_bblock->bb_dblock) * (1 << bitn);
    
    /* do we need to print admin info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN))
      _dmalloc_message("dblock entry for %d bytes found on free list",
		       1 << bitn);
  }
  else {
    /* do we need to print admin info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ADMIN))
      _dmalloc_message("need to create a dblock for %dx %d byte blocks",
		       1 << (BASIC_BLOCK - bitn), 1 << bitn);
    
    /* get some dblock admin slots and the bblock space */
    dblockp = get_dblock_admin(1 << (BASIC_BLOCK - bitn));
    if (dblockp == NULL)
      return NULL;
    
    dblock_count++;
    
    /* get a bblock from free list */
    bblockp = get_bblocks(1, &pnt);
    if (bblockp == NULL)
      return NULL;
    
    /* setup bblock information */
    bblockp->bb_flags	= BBLOCK_DBLOCK;
    bblockp->bb_bitn	= bitn;
    bblockp->bb_dblock	= dblockp;
    bblockp->bb_mem	= pnt;
    
    /* add the rest to the free list (has to be at least 1 other dblock) */
    firstp = dblockp;
    dblockp++;
    freep = free_dblock[bitn];
    free_dblock[bitn] = dblockp;
    
    for (; dblockp < firstp + (1 << (BASIC_BLOCK - bitn)) - 1; dblockp++) {
      dblockp->db_next = dblockp + 1;
      dblockp->db_bblock = bblockp;
#if STORE_SEEN_COUNT
      dblockp->db_overhead.ov_seenc = 0;
#endif
      free_space_count	+= 1 << bitn;
    }
    
    /* last one points to the free list (probably NULL) */
    dblockp->db_next	= freep;
    dblockp->db_bblock	= bblockp;
    free_space_count += 1 << bitn;
    
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK))
      (void)memset((char *)pnt + (1 << bitn), BLANK_CHAR,
		   BLOCK_SIZE - (1 << bitn));
    
#if STORE_SEEN_COUNT
    /* the first pointer in the block inherits the counter of the bblock */
    firstp->db_overhead.ov_seenc = bblockp->bb_overhead.ov_seenc;
#endif
    
    dblockp = firstp;
  }
  
  dblockp->db_line = line;
  dblockp->db_size = byten;
  dblockp->db_file = file;
  
#if STORE_SEEN_COUNT
  dblockp->db_overhead.ov_seenc++;
#endif
#if STORE_ITERATION_COUNT
  dblockp->db_overhead.ov_iteration = _dmalloc_iterc;
#endif
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIME && HAVE_TIME
    dblockp->db_overhead.ov_time = time(NULL);
#endif
#if STORE_TIMEVAL
    GET_TIMEVAL(dblockp->db_overhead.ov_timeval);
#endif
  }

#if STORE_THREAD_ID
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_THREAD_ID)) {
    dblockp->db_overhead.ov_thread_id = THREAD_GET_ID;
  }
#endif
  
  *overp = &dblockp->db_overhead;
  
  return pnt;
}

/******************************* heap checking *******************************/

/*
 * run extensive tests on the entire heap
 */
EXPORT	int	_chunk_check(void)
{
  bblock_adm_t	*this_admp, *aheadp;
  bblock_t	*bblockp, *bblistp, *last_bblockp;
  dblock_t	*dblockp;
  unsigned int	undef = 0, start = 0;
  char		*bytep;
  void		*pnt;
  int		bitc, dblockc = 0, bblockc = 0, freec = 0;
  unsigned int	bbc = 0, len;
  int		free_bblockc[MAX_SLOTS];
  int		free_dblockc[BASIC_BLOCK];
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("checking heap");
  
  /* if the heap is empty then no need to check anything */
  if (bblock_adm_head == NULL)
    return NOERROR;
  
  check_count++;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
    
    /* count the bblock free lists */
    for (bitc = 0; bitc < MAX_SLOTS; bitc++) {
      free_bblockc[bitc] = 0;
      
      /* parse bblock free list doing minimal pointer checking */
      for (bblockp = free_bblock[bitc]; bblockp != NULL;
	   bblockp = bblockp->bb_next, free_bblockc[bitc]++) {
	/*
	 * NOTE: this should not present problems since the bb_next is
	 * NOT unioned with bb_file.
	 */
	if (! IS_IN_HEAP(bblockp)) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
    }
    
    /* count the dblock free lists */
    for (bitc = 0; bitc < BASIC_BLOCK; bitc++) {
      free_dblockc[bitc] = 0;
      
      /* parse dblock free list doing minimal pointer checking */
      for (dblockp = free_dblock[bitc]; dblockp != NULL;
	   dblockp = dblockp->db_next, free_dblockc[bitc]++) {
	/*
	 * NOTE: this might miss problems if the slot is allocated but
	 * db_file (unioned with db_next) points to a return address
	 * in the heap
	 */
	if (! IS_IN_HEAP(dblockp)) {
	  dmalloc_errno = ERROR_BAD_FREE_LIST;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
    }
  }
  
  /* start pointers */
  this_admp = bblock_adm_head;
  aheadp = this_admp;
  last_bblockp = NULL;
  
  /* test admin pointer validity */
  if (! IS_IN_HEAP(this_admp)) {
    dmalloc_errno = ERROR_BAD_ADMINP;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  /* test structure validity */
  if (this_admp->ba_magic1 != CHUNK_MAGIC_BOTTOM
      || this_admp->ba_magic2 != CHUNK_MAGIC_TOP) {
    dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  /* verify count value */
  if (this_admp->ba_posn != bbc) {
    dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  /* check out the basic blocks */
  for (bblockp = this_admp->ba_blocks;; last_bblockp = bblockp++) {
    
    /* are we at the end of the bb_admin section */
    if (bblockp >= this_admp->ba_blocks + BB_PER_ADMIN) {
      this_admp = this_admp->ba_next;
      bbc += BB_PER_ADMIN;
      
      /* are we done? */
      if (this_admp == NULL)
	break;
      
      /* test admin pointer validity */
      if (! IS_IN_HEAP(this_admp)) {
	dmalloc_errno = ERROR_BAD_ADMINP;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* test structure validity */
      if (this_admp->ba_magic1 != CHUNK_MAGIC_BOTTOM
	  || this_admp->ba_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_ADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* verify count value */
      if (this_admp->ba_posn != bbc) {
	dmalloc_errno = ERROR_BAD_ADMIN_COUNT;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      bblockp = this_admp->ba_blocks;
    }
    
    /* check for no-allocation */
    if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
      undef++;
      continue;
    }
    
    /* we better not have seen a not-allocated block before */
    if (undef > 0 && bblockp->bb_flags != BBLOCK_ADMIN_FREE) {
      dmalloc_errno = ERROR_BAD_BLOCK_ORDER;
      dmalloc_error("_chunk_check");
      return ERROR;
    }
    
    start = 0;
    
    /*
     * check for different types
     */
    switch (bblockp->bb_flags) {
      
      /* check a starting user-block */
    case BBLOCK_START_USER:
      
      /* check X blocks in a row */
      if (bblockc != 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* mark the size in bits */
      NUM_BITS(bblockp->bb_size, bitc);
      bblockc = NUM_BLOCKS(bblockp->bb_size);
      start = 1;
      
      /* check fence-posts for memory chunk */
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
	pnt = BLOCK_POINTER(this_admp->ba_posn +
			    (bblockp - this_admp->ba_blocks));
	if (fence_read(bblockp->bb_file, bblockp->bb_line,
		       pnt, bblockp->bb_size, "heap-check") != NOERROR)
	  return ERROR;
      }
      
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_USER:
      
      /* check line number */
      if (bblockp->bb_line > MAX_LINE_NUMBER) {
	dmalloc_errno = ERROR_BAD_LINE;
	log_error_info(bblockp->bb_file, bblockp->bb_line, FALSE, NULL,
		       NULL, "heap-check", FALSE);
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
      if (bblockp->bb_size <= BLOCK_SIZE / 2
	  || bblockp->bb_size > (1 << LARGEST_BLOCK)) {
	dmalloc_errno = ERROR_BAD_SIZE;
	log_error_info(bblockp->bb_file, bblockp->bb_line, FALSE, NULL,
		       NULL, "heap-check", FALSE);
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check file pointer */
      if (bblockp->bb_file != NULL
	  && bblockp->bb_line != DMALLOC_DEFAULT_LINE) {
	len = strlen(bblockp->bb_file);
	if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	  dmalloc_errno = ERROR_BAD_FILEP;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      }
      
      /* check X blocks in a row */
      if (bblockc == 0) {
	dmalloc_errno = ERROR_USER_NON_CONTIG;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      else
	if (start == 0
	    && (last_bblockp == NULL
		|| (last_bblockp->bb_flags != BBLOCK_START_USER &&
		    last_bblockp->bb_flags != BBLOCK_USER)
		|| bblockp->bb_file != last_bblockp->bb_file
		|| bblockp->bb_line != last_bblockp->bb_line
		|| bblockp->bb_size != last_bblockp->bb_size)) {
	  dmalloc_errno = ERROR_USER_NON_CONTIG;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      
      bblockc--;
      
      /* NOTE: we should check above the allocated space if alloc_blank on */
      
      break;
      
    case BBLOCK_ADMIN:
      
      /* check the bblock_admin linked-list */
      if (bblockp->bb_adminp != aheadp) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMINP;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check count against admin count */
      if (bblockp->bb_posn != aheadp->ba_posn) {
	dmalloc_errno = ERROR_BAD_BLOCK_ADMINC;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      aheadp = aheadp->ba_next;
      break;
      
    case BBLOCK_DBLOCK:
      
      /* check out bitc */
      if (bblockp->bb_bitn >= BASIC_BLOCK) {
	dmalloc_errno = ERROR_BAD_DBLOCK_SIZE;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblockp->bb_dblock)) {
	dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* verify mem pointer */
      if (bblockp->bb_mem != BLOCK_POINTER(this_admp->ba_posn +
					   (bblockp - this_admp->ba_blocks))) {
	dmalloc_errno = ERROR_BAD_DBLOCK_MEM;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check dblock entry very closely if necessary */
      for (dblockc = 0, dblockp = bblockp->bb_dblock;
	   dblockp < bblockp->bb_dblock +
	   (1 << (BASIC_BLOCK - bblockp->bb_bitn));
	   dblockc++, dblockp++) {
	
	/* check out dblock entry to see if it is not free */
	/*
	 * XXX: this test might think it was a free'd slot if db_file
	 * (unioned with db_next) points to a return address in the
	 * heap.
	 */
	if ((dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next))
	    && IS_IN_HEAP(dblockp->db_bblock)) {
	  
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	    dblock_t	*dblistp;
	    
	    /* find the free block in the free list */
	    for (dblistp = free_dblock[bblockp->bb_bitn]; dblistp != NULL;
		 dblistp = dblistp->db_next)
	      if (dblistp == dblockp)
		break;
	    
	    /* did we find it? */
	    if (dblistp == NULL) {
	      dmalloc_errno = ERROR_BAD_FREE_LIST;
	      dmalloc_error("_chunk_check");
	      return ERROR;
	    }
	    
	    free_dblockc[bblockp->bb_bitn]--;
	  }
	  
	  continue;
	}
	
	/*
	 * check out size, better be less than BLOCK_SIZE / 2
	 * I have to check this twice :-(
	 */
	if ((int)dblockp->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(dblockp->db_file, dblockp->db_line, FALSE, NULL,
			 NULL, "heap-check", FALSE);
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE)) {
	  pnt = (char *)bblockp->bb_mem + dblockc * (1 << bblockp->bb_bitn);
	  if (fence_read(dblockp->db_file, dblockp->db_line,
			 pnt, dblockp->db_size, "heap-check") != NOERROR)
	    return ERROR;
	}
      }
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblockp->bb_slotp)) {
	dmalloc_errno = ERROR_BAD_DBADMIN_POINTER;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* verify magic numbers */
      if (bblockp->bb_slotp->da_magic1 != CHUNK_MAGIC_BOTTOM
	  || bblockp->bb_slotp->da_magic2 != CHUNK_MAGIC_TOP) {
	dmalloc_errno = ERROR_BAD_DBADMIN_MAGIC;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check out each dblock_admin struct? */
      for (dblockp = bblockp->bb_slotp->da_block;
	   dblockp < bblockp->bb_slotp->da_block + DB_PER_ADMIN; dblockp++) {
	
	/* see if we've used this slot before */
	if (dblockp->db_bblock == NULL && dblockp->db_next == NULL)
	  continue;
	
	/* check out dblock pointer and next pointer (if free) */
	/*
	 * XXX: this test might think it was a free'd slot if db_file
	 * (unioned with db_next) points to a return address in the
	 * heap.
	 */
	if ((dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next))
	    && IS_IN_HEAP(dblockp->db_bblock)) {
	  
	  /* find pointer to memory chunk */
	  pnt = (char *)dblockp->db_bblock->bb_mem +
	    (dblockp - dblockp->db_bblock->bb_dblock) *
	      (1 << dblockp->db_bblock->bb_bitn);
	  
	  /* should we verify that we have a block of BLANK_CHAR? */
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	    for (bytep = (char *)pnt;
		 bytep < (char *)pnt + (1 << dblockp->db_bblock->bb_bitn);
		 bytep++)
	      if (*bytep != BLANK_CHAR) {
		dmalloc_errno = ERROR_FREE_NON_BLANK;
		log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE,
			       TRUE, bytep, NULL, "heap-check", TRUE);
		dmalloc_error("_chunk_check");
		return ERROR;
	      }
	  }
	  
	  continue;
	}
	
	/* check out size, better be less than BLOCK_SIZE / 2 */
	if ((int)dblockp->db_size > BLOCK_SIZE / 2) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(dblockp->db_file, dblockp->db_line, FALSE, NULL,
			 NULL, "heap-check", FALSE);
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	
	/* check line number */
	if (dblockp->db_line > MAX_LINE_NUMBER) {
	  dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  log_error_info(dblockp->db_file, dblockp->db_line, FALSE, NULL,
			 NULL, "heap-check", FALSE);
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
	
	if (dblockp->db_file != NULL
	    && dblockp->db_line != DMALLOC_DEFAULT_LINE) {
	  len = strlen(dblockp->db_file);
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
      if (bblockp->bb_next != NULL && ! IS_IN_HEAP(bblockp->bb_next)) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
      
      /* check X blocks in a row */
      if (freec == 0) {
	freec = bblockp->bb_blockn;
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
	  
	  /* find the free block in the free list */
	  for (bblistp = free_bblock[bblockp->bb_bitn]; bblistp != NULL;
	       bblistp = bblistp->bb_next)
	    if (bblistp == bblockp)
	      break;
	  
	  /* did we find it? */
	  if (bblistp == NULL) {
	    dmalloc_errno = ERROR_BAD_FREE_LIST;
	    dmalloc_error("_chunk_check");
	    return ERROR;
	  }
	  
	  free_bblockc[bblockp->bb_bitn]--;
	}
      }
      else
	if (last_bblockp == NULL
	    || last_bblockp->bb_flags != BBLOCK_FREE
	    || bblockp->bb_bitn != last_bblockp->bb_bitn) {
	  dmalloc_errno = ERROR_FREE_NON_CONTIG;
	  dmalloc_error("_chunk_check");
	  return ERROR;
	}
      freec--;
      
      /* should we verify that we have a block of BLANK_CHAR? */
      if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
	pnt = BLOCK_POINTER(this_admp->ba_posn +
			    (bblockp - this_admp->ba_blocks));
	for (bytep = (char *)pnt; bytep < (char *)pnt + BLOCK_SIZE; bytep++)
	  if (*bytep != BLANK_CHAR) {
	    dmalloc_errno = ERROR_FREE_NON_BLANK;
	    log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
			   bytep, NULL, "heap-check", TRUE);
	    dmalloc_error("_chunk_check");
	    return ERROR;
	  }
	break;
	
	/* externally used block */
      case BBLOCK_EXTERNAL:
	/* nothing much to check */
	break;
	
	/* pointer to first free slot */
      case BBLOCK_ADMIN_FREE:
	/* better be the last block and the count should match undef */
	if (bblockp != this_admp->ba_blocks + (BB_PER_ADMIN - 1)
	    || bblockp->bb_freen != (BB_PER_ADMIN - 1) - undef) {
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
  if (bblockc > 0) {
    dmalloc_errno = ERROR_USER_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  if (freec > 0) {
    dmalloc_errno = ERROR_FREE_NON_CONTIG;
    dmalloc_error("_chunk_check");
    return ERROR;
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_LISTS)) {
    
    /* any free bblock entries not accounted for? */
    for (bitc = 0; bitc < MAX_SLOTS; bitc++)
      if (free_bblockc[bitc] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
    
    /* any free dblock entries not accounted for? */
    for (bitc = 0; bitc < BASIC_BLOCK; bitc++) {
      if (free_dblockc[bitc] != 0) {
	dmalloc_errno = ERROR_BAD_FREE_LIST;
	dmalloc_error("_chunk_check");
	return ERROR;
      }
    }
  }
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_HEAP_CHECK_MAP))
    _chunk_log_heap_map();
  
  return NOERROR;
}

/*
 * run extensive tests on PNT from FUNC. test PNT HOW_MUCH of MIN_SIZE
 * (or 0 if unknown).  CHECK is flags for types of checking (see chunk.h).
 * returns [NO]ERROR
 */
EXPORT	int	_chunk_pnt_check(const char * func, const void * pnt,
				 const int check, const int min_size)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  int		diff;
  unsigned int	len, min = min_size;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("checking pointer '%#lx'", pnt);
  
  /* adjust the pointer down if fence-posting */
  pnt = USER_TO_CHUNK(pnt);
  if (min != 0)
    min += pnt_total_adm;
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, NULL);
  if (bblockp == NULL) {
    if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
      /* the pointer might not be the heap or might be NULL */
      dmalloc_errno = 0;
      return NOERROR;
    }
    else {
      /* errno set in find_bblock */
      log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
  }
  
  /* maybe watch out for nullc character */
  if (BIT_IS_SET(check, CHUNK_PNT_NULL)) {
    if (min != 0) {
      len = strlen(pnt) + 1;
      if (len > min)
	min = len;
    }
  }
  
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    diff = ((char *)pnt - (char *)bblockp->bb_mem) % (1 << bblockp->bb_bitn);
    if (diff != 0) {
      if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
	if (min != 0)
	  min += diff;
	pnt = (char *)pnt - diff;
      }
      else {
	dmalloc_errno = ERROR_NOT_ON_BLOCK;
	log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		       CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
	dmalloc_error(func);
	return ERROR;
      }
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - (char *)bblockp->bb_mem) /
      (1 << bblockp->bb_bitn);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      dmalloc_errno = ERROR_IS_FREE;
      log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
    
    /* check line number */
    if (dblockp->db_line > MAX_LINE_NUMBER) {
      dmalloc_errno = ERROR_BAD_LINE;
      log_error_info(dblockp->db_file, dblockp->db_line, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
    
    /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
    if ((int)dblockp->db_size > BLOCK_SIZE / 2) {
      dmalloc_errno = ERROR_BAD_DBADMIN_SLOT;
      log_error_info(dblockp->db_file, dblockp->db_line, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
    
    if (min != 0 && dblockp->db_size < min) {
      dmalloc_errno = ERROR_WOULD_OVERWRITE;
      log_error_info(dblockp->db_file, dblockp->db_line, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", TRUE);
      dmalloc_error(func);
      return ERROR;
    }
    
    /* check file pointer */
    if (dblockp->db_file != NULL
	&& dblockp->db_line != DMALLOC_DEFAULT_LINE) {
      len = strlen(dblockp->db_file);
      if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	dmalloc_errno = ERROR_BAD_FILEP;
	log_error_info(dblockp->db_file, dblockp->db_line, TRUE,
		       CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
	dmalloc_error(func);
	return ERROR;
      }
    }
    
    /* check out the fence-posts */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
      if (fence_read(dblockp->db_file, dblockp->db_line,
		     pnt, dblockp->db_size, "pointer-check") != NOERROR)
	return ERROR;
    
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
      if (min != 0)
	min += diff;
    }
    else {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
  }
  
  /* are we on a normal block */
  if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)
      && ! (BIT_IS_SET(check, CHUNK_PNT_LOOSE)
	    && BIT_IS_SET(bblockp->bb_flags, BBLOCK_USER))) {
    dmalloc_errno = ERROR_NOT_START_USER;
    log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		   CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
    dmalloc_error(func);
    return ERROR;
  }
  
  /* check line number */
  if (bblockp->bb_line > MAX_LINE_NUMBER) {
    dmalloc_errno = ERROR_BAD_LINE;
    log_error_info(bblockp->bb_file, bblockp->bb_line, TRUE,
		   CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
    dmalloc_error(func);
    return ERROR;
  }
  
  /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
  if (bblockp->bb_size <= BLOCK_SIZE / 2
      || bblockp->bb_size > (1 << LARGEST_BLOCK)) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(bblockp->bb_file, bblockp->bb_line, TRUE,
		   CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
    dmalloc_error(func);
    return ERROR;
  }
  
  if (min != 0 && bblockp->bb_size < min) {
    dmalloc_errno = ERROR_WOULD_OVERWRITE;
    log_error_info(bblockp->bb_file, bblockp->bb_line, TRUE,
		   CHUNK_TO_USER(pnt), NULL, "pointer-check", TRUE);
    dmalloc_error(func);
    return ERROR;
  }
  
  /* check file pointer */
  if (bblockp->bb_file != NULL
      && bblockp->bb_line != DMALLOC_DEFAULT_LINE) {
    len = strlen(bblockp->bb_file);
    if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
      dmalloc_errno = ERROR_BAD_FILEP;
      log_error_info(bblockp->bb_file, bblockp->bb_line, TRUE,
		     CHUNK_TO_USER(pnt), NULL, "pointer-check", FALSE);
      dmalloc_error(func);
      return ERROR;
    }
  }
  
  /* check out the fence-posts if we are at the start of a user-block */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)
      && BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
    if (fence_read(bblockp->bb_file, bblockp->bb_line, pnt, bblockp->bb_size,
		   "pointer-check") != NOERROR)
      return ERROR;
  
  return NOERROR;
}

/**************************** information routines ***************************/

/*
 * return some information associated with PNT, returns [NO]ERROR
 */
EXPORT	int	_chunk_read_info(const void * pnt, unsigned int * size,
				 unsigned int * alloc_size, char ** file,
				 unsigned int * line, void ** ret_attr,
				 const char * where, int ** seencp)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("reading info about pointer '%#lx'", pnt);
  
  if (seencp != NULL)
    *seencp = NULL;
  
  /* adjust the pointer down if fence-posting */
  pnt = USER_TO_CHUNK(pnt);
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, NULL);
  if (bblockp == NULL) {
    /* errno set in find_bblock */
    log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		   CHUNK_TO_USER(pnt), NULL, where, FALSE);
    dmalloc_error("_chunk_read_info");
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if (((char *)pnt - (char *)bblockp->bb_mem) %
	(1 << bblockp->bb_bitn) != 0) {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		     CHUNK_TO_USER(pnt), NULL, where, FALSE);
      dmalloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - (char *)bblockp->bb_mem) /
      (1 << bblockp->bb_bitn);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      dmalloc_errno = ERROR_IS_FREE;
      log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		     CHUNK_TO_USER(pnt), NULL, where, FALSE);
      dmalloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      *size = dblockp->db_size;
    if (alloc_size != NULL)
      *alloc_size = 1 << bblockp->bb_bitn;
    if (file != NULL) {
      if (dblockp->db_line == DMALLOC_DEFAULT_LINE)
	*file = NULL;
      else
	*file = (char *)dblockp->db_file;
    }
    if (line != NULL)
      *line = dblockp->db_line;
    if (ret_attr != NULL) {
      if (dblockp->db_line == DMALLOC_DEFAULT_LINE)
	*ret_attr = (char *)dblockp->db_file;
      else
	*ret_attr = NULL;
#if STORE_SEEN_COUNT
      if (seencp != NULL)
	*seencp = &dblockp->db_overhead.ov_seenc;
#endif
    }
  }
  else {
    
    /* verify that the pointer is either dblock or user allocated */
    if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
      dmalloc_errno = ERROR_NOT_USER;
      log_error_info(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE, TRUE,
		     CHUNK_TO_USER(pnt), NULL, where, FALSE);
      dmalloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      *size = bblockp->bb_size;
    if (alloc_size != NULL)
      *alloc_size = NUM_BLOCKS(bblockp->bb_size) * BLOCK_SIZE;
    if (file != NULL) {
      if (bblockp->bb_line == DMALLOC_DEFAULT_LINE)
	*file = NULL;
      else
	*file = (char *)bblockp->bb_file;
    }
    if (line != NULL)
      *line = bblockp->bb_line;
    if (ret_attr != NULL) {
      if (bblockp->bb_line == DMALLOC_DEFAULT_LINE)
	*ret_attr = (char *)bblockp->bb_file;
      else
	*ret_attr = NULL;
    }
#if STORE_SEEN_COUNT
    if (seencp != NULL)
      *seencp = &bblockp->bb_overhead.ov_seenc;
#endif
  }
  
  return NOERROR;
}

/*
 * write new FILE, LINE, SIZE info into PNT (which is in chunk-space)
 */
LOCAL	int	chunk_write_info(const char * file, const unsigned int line,
				 void * pnt, const unsigned int size,
				 const char * where)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  /* NOTE: pnt is already in chunk-space */
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, NULL);
  if (bblockp == NULL) {
    /* errno set in find_bblock */
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, where, FALSE);
    dmalloc_error("chunk_write_info");
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if (((char *)pnt - (char *)bblockp->bb_mem) %
	(1 << bblockp->bb_bitn) != 0) {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, where, FALSE);
      dmalloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - (char *)bblockp->bb_mem) /
      (1 << bblockp->bb_bitn);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      dmalloc_errno = ERROR_NOT_USER;
      log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, where, FALSE);
      dmalloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* write info to system space */
    dblockp->db_size = size;
    dblockp->db_file = (char *)file;
    dblockp->db_line = (unsigned short)line;
  }
  else {
    
    /* verify that the pointer is user allocated */
    if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
      dmalloc_errno = ERROR_NOT_USER;
      log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, where, FALSE);
      dmalloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* reset values in the bblocks */
    set_bblock_admin(NUM_BLOCKS(size), bblockp, BBLOCK_START_USER, line,
		     size, file);
  }
  
  return NOERROR;
}

/*
 * log the heap structure plus information on the blocks if necessary
 */
EXPORT	void	_chunk_log_heap_map(void)
{
  bblock_adm_t	*bblock_admp;
  bblock_t	*bblockp;
  char		line[BB_PER_ADMIN + 10];
  int		charc, bblockc, tblockc, bb_adminc, undef = 0;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("logging heap map information");
  
  _dmalloc_message("heap-base = %#lx, heap-end = %#lx, size = %ld bytes",
		  _heap_base, _heap_last, HEAP_SIZE);
  
  for (bb_adminc = 0, bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bb_adminc++, bblock_admp = bblock_admp->ba_next) {
    charc = 0;
    
    bblockp = bblock_admp->ba_blocks;
    for (bblockc = 0; bblockc < BB_PER_ADMIN; bblockc++, bblockp++) {
      if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
	line[charc++] = '_';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
	line[charc++] = 'S';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_USER)) {
	line[charc++] = 'U';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ADMIN)) {
	line[charc++] = 'A';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
	line[charc++] = 'd';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK_ADMIN)) {
	line[charc++] = 'a';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_FREE)) {
	line[charc++] = 'F';
	continue;
	
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_EXTERNAL)) {
	line[charc++] = 'E';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ADMIN_FREE)) {
	line[charc++] = 'P';
	continue;
      }
    }
    
    /* dumping a line to the logfile */
    if (charc > 0) {
      line[charc] = NULLC;
      _dmalloc_message("S%d:%s", bb_adminc, line);
    }
  }
  
  /* if we are not logging blocks then leave */
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_BLOCKS))
    return;
  
  tblockc = 0;
  for (bb_adminc = 0, bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bb_adminc++, bblock_admp = bblock_admp->ba_next) {
    
    for (bblockc = 0, bblockp = bblock_admp->ba_blocks; bblockc < BB_PER_ADMIN;
	 bblockc++, bblockp++, tblockc++) {
      
      if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
	if (undef == 0) {
	  _dmalloc_message("%d (%#lx): not-allocated block (till next)",
			  tblockc, BLOCK_POINTER(tblockc));
	  undef = 1;
	}
	continue;
      }
      
      undef = 0;
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
	_dmalloc_message("%d (%#lx): start-of-user block: %d bytes from '%s'",
			 tblockc, BLOCK_POINTER(tblockc),
			 bblockp->bb_size,
			 _chunk_display_where(bblockp->bb_file,
					      bblockp->bb_line));
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_USER)) {
	_dmalloc_message("%d (%#lx): user continuation block",
			tblockc, BLOCK_POINTER(tblockc));
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ADMIN)) {
	_dmalloc_message("%d (%#lx): administration block, position = %d",
			tblockc, BLOCK_POINTER(tblockc),
			bblockp->bb_freen);
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
	_dmalloc_message("%d (%#lx): dblock block, bitn = %d",
			tblockc, BLOCK_POINTER(tblockc),
			bblockp->bb_bitn);
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK_ADMIN)) {
	_dmalloc_message("%d (%#lx): dblock-admin block",
			tblockc, BLOCK_POINTER(tblockc));
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_FREE)) {
	_dmalloc_message("%d (%#lx): free block of %d blocks, next at %#lx",
			tblockc, BLOCK_POINTER(tblockc),
			bblockp->bb_blockn, bblockp->bb_mem);
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_EXTERNAL)) {
	_dmalloc_message("%d (%#lx): externally used block to %#lx",
			tblockc, BLOCK_POINTER(tblockc), bblockp->bb_mem);
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ADMIN_FREE)) {
	_dmalloc_message("%d (%#lx): admin free pointer to offset %d",
			tblockc, BLOCK_POINTER(tblockc), bblockp->bb_freen);
	continue;
      }
    }
  }
}

/************************** low-level user functions *************************/

/*
 * get a SIZE chunk of memory for FILE at LINE
 */
EXPORT	void	*_chunk_malloc(const char * file, const unsigned int line,
			       const unsigned int size)
{
  unsigned int	bitn, byten = size;
  bblock_t	*bblockp;
  overhead_t	*overp;
  void		*pnt;
  
  /* counts calls to malloc */
  malloc_count++;
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (byten == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, FALSE, NULL, "bad zero byte allocation request",
		   "malloc", FALSE);
    if (! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_ZERO))
      dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
#endif
  
  if (file == NULL)
    file = _dmalloc_unknown_file;
  
  /* adjust the size */
  byten += pnt_total_adm;
  
  /* count the bits */
  NUM_BITS(byten, bitn);
  
  /* have we exceeded the upper bounds */
  if (bitn > LARGEST_BLOCK) {
    dmalloc_errno = ERROR_TOO_BIG;
    log_error_info(file, line, FALSE, NULL, NULL, "malloc", FALSE);
    dmalloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* normalize to smallest_block.  No use spending 16 bytes to admin 1 byte */
  if (bitn < smallest_block)
    bitn = smallest_block;
  
  /* monitor current allocation level */
  alloc_current += byten;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_total += byten;
  alloc_one_max = MAX(alloc_one_max, byten);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  /* allocate divided block if small */
  if (bitn < BASIC_BLOCK) {
    pnt = get_dblock(bitn, byten, file, line, &overp);
    if (pnt == NULL)
      return MALLOC_ERROR;
    
    alloc_cur_given += 1 << bitn;
    alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK))
      (void)memset(pnt, BLANK_CHAR, 1 << bitn);
  }
  else {
    int		blockn, given;
    
    /*
     * allocate some bblock space
     */
    
    /* handle blocks */
    blockn = NUM_BLOCKS(byten);
    bblockp = get_bblocks(blockn, &pnt);
    if (bblockp == NULL)
      return MALLOC_ERROR;
    
    /* initialize the bblocks */
    set_bblock_admin(blockn, bblockp, BBLOCK_START_USER, line, byten, file);
    
    given = blockn * BLOCK_SIZE;
    alloc_cur_given += given;
    alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK))
      (void)memset(pnt, BLANK_CHAR, given);
    
#if STORE_SEEN_COUNT
    bblockp->bb_overhead.ov_seenc++;
#endif
#if STORE_ITERATION_COUNT
    bblockp->bb_overhead.ov_iteration = _dmalloc_iterc;
#endif
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_CURRENT_TIME)) {
#if STORE_TIME && HAVE_TIME
      bblockp->bb_overhead.ov_time = time(NULL);
#endif
#if STORE_TIMEVAL
      GET_TIMEVAL(bblockp->bb_overhead.ov_timeval);
#endif
    }
    
#if STORE_THREAD_ID
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_THREAD_ID)) {
      bblockp->bb_overhead.ov_thread_id = THREAD_GET_ID;
    }
#endif
    
    overp = &bblockp->bb_overhead;
  }
  
  /* write fence post info if needed */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
    FENCE_WRITE(pnt, byten);
  
  pnt = CHUNK_TO_USER(pnt);
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("*** alloc: at '%s' for %d bytes, got '%s'",
		     _chunk_display_where(file, line), byten - pnt_total_adm,
		     display_pnt(pnt, overp));
  
  return pnt;
}

/*
 * frees PNT from the heap, returns FREE_ERROR or FREE_NOERROR
 */
EXPORT	int	_chunk_free(const char * file, const unsigned int line,
			    void * pnt)
{
  unsigned int	bitn, blockn, given;
  bblock_t	*bblockp, *last, *next, *prevp, *tmp;
  dblock_t	*dblockp;
  
  /* counts calls to free */
  free_count++;
  
  if (pnt == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    log_error_info(file, line, TRUE, pnt, "invalid pointer", "free", FALSE);
    if (! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_ZERO))
      dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* adjust the pointer down if fence-posting */
  pnt = USER_TO_CHUNK(pnt);
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, &last, &next);
  if (bblockp == NULL) {
    /* errno set in find_bblock */
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  alloc_cur_pnts--;
  
  /* are we free'ing a dblock entry? */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    
    /* on a mini-block boundary? */
    if (((char *)pnt - (char *)bblockp->bb_mem) %
	(1 << bblockp->bb_bitn) != 0) {
      dmalloc_errno = ERROR_NOT_ON_BLOCK;
      log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, "free",
		     FALSE);
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - (char *)bblockp->bb_mem) /
      (1 << bblockp->bb_bitn);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here? */
      dmalloc_errno = ERROR_ALREADY_FREE;
      log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, "free",
		     FALSE);
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
#if STORE_SEEN_COUNT
    dblockp->db_overhead.ov_seenc++;
#endif
    
    /* print transaction info? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
      _dmalloc_message("*** free: at '%s' pnt '%s': size %d, alloced at '%s'",
		       _chunk_display_where(file, line),
		       display_pnt(CHUNK_TO_USER(pnt), &dblockp->db_overhead),
		       dblockp->db_size - pnt_total_adm,
		       chunk_display_where2(dblockp->db_file,
					    dblockp->db_line));
    
    /* check fence-post, probably again */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
      if (fence_read(dblockp->db_file, dblockp->db_line, pnt,
		     dblockp->db_size, "free") != NOERROR)
	return FREE_ERROR;
    
    /* count the bits */
    bitn = bblockp->bb_bitn;
    
    /* monitor current allocation level */
    alloc_current -= dblockp->db_size;
    alloc_cur_given -= 1 << bitn;
    
    /* rearrange info */
    dblockp->db_bblock	= bblockp;
    dblockp->db_next	= free_dblock[bitn];
    free_dblock[bitn]	= dblockp;
    free_space_count	+= 1 << bitn;
    
    /* should we set free memory with BLANK_CHAR? */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK))
      (void)memset(pnt, BLANK_CHAR, 1 << bitn);
    
    return FREE_NOERROR;
  }
  
  /* on a block boundary? */
  if (! ON_BLOCK(pnt)) {
    dmalloc_errno = ERROR_NOT_ON_BLOCK;
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* are we on a normal block */
  if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
    dmalloc_errno = ERROR_NOT_START_USER;
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
#if STORE_SEEN_COUNT
  bblockp->bb_overhead.ov_seenc++;
#endif
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("*** free: at '%s' pnt '%s': size %d, alloced at '%s'",
		     _chunk_display_where(file, line),
		     display_pnt(CHUNK_TO_USER(pnt), &bblockp->bb_overhead),
		     bblockp->bb_size - pnt_total_adm,
		     chunk_display_where2(bblockp->bb_file, bblockp->bb_line));
  
  /* check fence-post, probably again */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
    if (fence_read(bblockp->bb_file, bblockp->bb_line, pnt, bblockp->bb_size,
		   "free") != NOERROR)
      return FREE_ERROR;
  
  blockn = NUM_BLOCKS(bblockp->bb_size);
  given = blockn * BLOCK_SIZE;
  NUM_BITS(given, bitn);
  
  if (bitn < BASIC_BLOCK) {
    dmalloc_errno = ERROR_BAD_SIZE_INFO;
    log_error_info(file, line, TRUE, CHUNK_TO_USER(pnt), NULL, "free", FALSE);
    dmalloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* monitor current allocation level */
  alloc_current -= bblockp->bb_size;
  alloc_cur_given -= given;
  free_space_count += given;
  
  /*
   * should we set free memory with BLANK_CHAR?
   * NOTE: we do this hear because blockn might change below
   */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_FREE_BLANK)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK))
    (void)memset(pnt, BLANK_CHAR, blockn * BLOCK_SIZE);
  
  /*
   * check above and below the free bblock looking for neighbors that
   * are free so we can add them together and put them in a different
   * free slot.
   */
  
  if (last != NULL && BIT_IS_SET(last->bb_flags, BBLOCK_FREE)) {
    /* find last in free list and remove it */
    for (tmp = free_bblock[last->bb_bitn], prevp = NULL;
	 tmp != NULL;
	 prevp = tmp, tmp = tmp->bb_next) {
      if (tmp == last)
	break;
    }
    
    /* we better have found it */
    if (tmp == NULL) {
      dmalloc_errno = ERROR_BAD_FREE_LIST;
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    if (prevp == NULL)
      free_bblock[last->bb_bitn] = last->bb_next;
    else
      prevp->bb_next = last->bb_next;
    
    blockn += last->bb_blockn;
    NUM_BITS(blockn * BLOCK_SIZE, bitn);
    bblockp = last;
  }
  if (next != NULL && BIT_IS_SET(next->bb_flags, BBLOCK_FREE)) {
    /* find last in free list and remove it */
    for (tmp = free_bblock[next->bb_bitn], prevp = NULL;
	 tmp != NULL;
	 prevp = tmp, tmp = tmp->bb_next) {
      if (tmp == next)
	break;
    }
    
    /* we better have found it */
    if (tmp == NULL) {
      dmalloc_errno = ERROR_BAD_FREE_LIST;
      dmalloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    if (prevp == NULL)
      free_bblock[next->bb_bitn] = next->bb_next;
    else
      prevp->bb_next = next->bb_next;
    
    blockn += next->bb_blockn;
    NUM_BITS(blockn * BLOCK_SIZE, bitn);
  }
  
  /* set the information for the bblock(s) */
  set_bblock_admin(blockn, bblockp, BBLOCK_FREE, bitn, 0, free_bblock[bitn]);
  
  /* block goes at the start of the free list */
  free_bblock[bitn] = bblockp;
  
  return FREE_NOERROR;
}

/*
 * reallocate a section of memory
 */
EXPORT	void	*_chunk_realloc(const char * file, const unsigned int line,
				void * oldp, unsigned int new_size)
{
  void		*newp, *ret_addr;
  char		*old_file;
  int		*seencp;
  unsigned int	old_size, size, old_line, alloc_size;
  unsigned int	old_bitn, new_bitn;
  
  /* counts calls to realloc */
  realloc_count++;
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (new_size == 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    log_error_info(file, line, FALSE, NULL, "bad zero byte allocation request",
		   "realloc", FALSE);
    if (! BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOW_ZERO))
      dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
#endif
  
  /* by now malloc.c should have taken care of the realloc(NULL) case */
  if (oldp == NULL) {
    dmalloc_errno = ERROR_IS_NULL;
    log_error_info(file, line, TRUE, oldp, "invalid pointer", "realloc",
		   FALSE);
    dmalloc_error("_chunk_realloc");
    return REALLOC_ERROR;
  }
  
  /*
   * TODO: for bblocks it would be nice to examine the above memory
   * looking for free blocks that we can absorb into this one.
   */
  
  /* get info about old pointer */
  if (_chunk_read_info(oldp, &old_size, &alloc_size, &old_file, &old_line,
		       &ret_addr, "realloc", &seencp) != NOERROR)
    return REALLOC_ERROR;
  
  if (ret_addr != NULL)
    old_file = ret_addr;
  
  /* adjust the pointer down if fence-posting */
  oldp = USER_TO_CHUNK(oldp);
  new_size += pnt_total_adm;
  
  /* check the fence-posting */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
    if (fence_read(file, line, oldp, old_size, "realloc") != NOERROR)
      return REALLOC_ERROR;
  
  /* get the old and new bit sizes */
  NUM_BITS(old_size, old_bitn);
  NUM_BITS(new_size, new_bitn);
  
  /* if we are not realloc copying and the size is the same */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_REALLOC_COPY)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_NEVER_REUSE)
      || old_bitn != new_bitn
      || NUM_BLOCKS(old_size) != NUM_BLOCKS(new_size)) {
    
    /* readjust info */
    oldp = CHUNK_TO_USER(oldp);
    old_size -= pnt_total_adm;
    new_size -= pnt_total_adm;
    
    /* allocate space for new chunk */
    newp = _chunk_malloc(file, line, new_size);
    if (newp == MALLOC_ERROR)
      return REALLOC_ERROR;
    
    malloc_count--;
    
    /*
     * NOTE: _chunk_malloc() already took care of the fence stuff...
     */
    
    /* copy stuff into new section of memory */
    size = MIN(new_size, old_size);
    if (size > 0)
      memcpy((char *)newp, (char *)oldp, size);
    
    /* free old pointer */
    if (_chunk_free(file, line, oldp) != FREE_NOERROR)
      return REALLOC_ERROR;
    
    free_count--;
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
    newp = oldp;
    
    /* rewrite size information */
    if (chunk_write_info(file, line, newp, new_size, "realloc") != NOERROR)
      return REALLOC_ERROR;
    
    /* overwrite to-be-alloced or non-used portion of memory */
    size = MIN(new_size, old_size);
    
    /* NOTE: using same number of blocks so NUM_BLOCKS works with either */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ALLOC_BLANK)
	&& alloc_size - size > 0)
      (void)memset((char *)newp + size, BLANK_CHAR, alloc_size - size);
    
    /* write in fence-post info and adjust new pointer over fence info */
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FENCE))
      FENCE_WRITE(newp, new_size);
    
    newp = CHUNK_TO_USER(newp);
    oldp = CHUNK_TO_USER(oldp);
    old_size -= pnt_total_adm;
    new_size -= pnt_total_adm;
    
#if STORE_SEEN_COUNT
    /* we see in inbound and outbound so we need to increment by 2 */
    *seencp += 2;
#endif
  }
  
  /*
   * do we need to print transaction info?
   *
   * NOTE: pointers and sizes here a user-level real
   */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("*** realloc: at '%s' from '%#lx' (%u bytes) file '%s' to '%#lx' (%u bytes)",
		    _chunk_display_where(file, line),
		    oldp, old_size, chunk_display_where2(old_file, old_line),
		    newp, new_size);
  
  /* newp is already user-level real */
  return newp;
}

/***************************** diagnostic routines ***************************/

/*
 * log present free and used lists
 */
EXPORT	void	_chunk_list_count(void)
{
  int		bitc, blockc;
  char		info[256], tmp[80];
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  /* dump the free (and later used) list counts */
  info[0] = NULLC;
  for (bitc = smallest_block; bitc < MAX_SLOTS; bitc++) {
    if (bitc < BASIC_BLOCK)
      for (blockc = 0, dblockp = free_dblock[bitc]; dblockp != NULL;
	   blockc++, dblockp = dblockp->db_next);
    else
      for (blockc = 0, bblockp = free_bblock[bitc]; bblockp != NULL;
	   blockc++, bblockp = bblockp->bb_next);
    
    if (blockc > 0) {
      (void)sprintf(tmp, " %d/%d", blockc, bitc);
      (void)strcat(info, tmp);
    }
  }
  
  _dmalloc_message("free count/bits: %s", info);
}

/*
 * log statistics on the heap
 */
EXPORT	void	_chunk_stats(void)
{
  long		overhead, tot_space, wasted;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("dumping chunk statistics");
  
  tot_space = alloc_current + free_space_count;
  overhead = (bblock_adm_count + dblock_adm_count) * BLOCK_SIZE;
  wasted = tot_space - alloc_max_given;
  
  /* version information */
  _dmalloc_message("basic-block %d bytes, alignment %d bytes, heap grows %s",
		   BLOCK_SIZE, ALLOCATION_ALIGNMENT,
		   (HEAP_GROWS_UP ? "up" : "down"));
  
  /* general heap information */
  _dmalloc_message("heap: %#lx to %#lx, size %ld bytes (%ld blocks), checked %ld",
		  _heap_base, _heap_last, HEAP_SIZE, bblock_count,
		   check_count);
  
  /* log user allocation information */
  _dmalloc_message("alloc calls: malloc %ld, realloc %ld, calloc %ld, free %ld"
		   , malloc_count - _calloc_count, realloc_count,
		   _calloc_count, free_count);
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
  if (wasted <= 0)
    _dmalloc_message("max memory space wasted: 0 bytes (0%%)");
  else
    _dmalloc_message("max memory space wasted: %ld bytes (%ld%%)",
		    wasted,
		    (tot_space == 0 ? 0 : ((wasted * 100) / tot_space)));
  
  /* final stats */
  _dmalloc_message("final user memory space: basic %ld, divided %ld, %ld bytes",
		   bblock_count - bblock_adm_count - dblock_count -
		   dblock_adm_count - extern_count, dblock_count,
		   tot_space);
  _dmalloc_message("   final admin overhead: basic %ld, divided %ld, %ld bytes (%d%%)",
		   bblock_adm_count, dblock_adm_count, overhead,
		   (HEAP_SIZE == 0 ? 0 : (overhead * 100) / HEAP_SIZE));
  _dmalloc_message("   final external space: %ld bytes (%ld blocks)",
		   extern_count * BLOCK_SIZE, extern_count);
}

/*
 * dump the unfreed memory, logs the unfreed information to logger
 */
EXPORT	void	_chunk_dump_unfreed(void)
{
  bblock_adm_t	*this_admp;
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  void		*pnt;
  char		unknown;
  int		unknown_sizec = 0, unknown_blockc = 0;
  int		sizec = 0, blockc = 0;
  
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("dumping the unfreed pointers");
  
  /* has anything been alloced yet? */
  this_admp = bblock_adm_head;
  if (this_admp == NULL)
    return;
  
  /* check out the basic blocks */
  for (bblockp = this_admp->ba_blocks;; bblockp++) {
    
    /* are we at the end of the bb_admin section */
    if (bblockp >= this_admp->ba_blocks + BB_PER_ADMIN) {
      this_admp = this_admp->ba_next;
      
      /* are we done? */
      if (this_admp == NULL)
	break;
      
      bblockp = this_admp->ba_blocks;
    }
    
    /*
     * check for different types
     */
    switch (bblockp->bb_flags) {
      
    case BBLOCK_START_USER:
      /* find pointer to memory chunk */
      pnt = BLOCK_POINTER(this_admp->ba_posn +
			  (bblockp - this_admp->ba_blocks));
      
      unknown = 0;
      
      /* unknown pointer? */
      if (bblockp->bb_file == _dmalloc_unknown_file
	  || bblockp->bb_file == NULL
	  || bblockp->bb_line == DMALLOC_DEFAULT_LINE) {
	unknown_blockc++;
	unknown_sizec += bblockp->bb_size - pnt_total_adm;
	unknown = 1;
      }
      
      if (! unknown || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_UNKNOWN)) {
	_dmalloc_message("not freed: '%s' (%d bytes) from '%s'",
			 display_pnt(CHUNK_TO_USER(pnt),
				     &bblockp->bb_overhead),
			 bblockp->bb_size - pnt_total_adm,
			 _chunk_display_where(bblockp->bb_file,
					      bblockp->bb_line));
	
	if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE))
	  _dmalloc_message("Dump of '%#lx': '%s'",
			  CHUNK_TO_USER(pnt),
			  expand_buf((char *)CHUNK_TO_USER(pnt), DUMP_SPACE));
      }
      
      sizec += bblockp->bb_size - pnt_total_adm;
      blockc++;
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      for (dblockp = bblockp->bb_slotp->da_block;
	   dblockp < bblockp->bb_slotp->da_block + DB_PER_ADMIN; dblockp++) {
	
	/* see if this admin slot has ever been used */
	if (dblockp->db_bblock == NULL && dblockp->db_next == NULL)
	  continue;
	
	/*
	 * is this slot in a free list?
	 * NOTE: this may bypass a non-freed entry if the db_file pointer
	 * (unioned to db_next) points to return address in the heap
	 */
	if (dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next))
	  continue;
	
	{
	  bblock_adm_t	*bbap;
	  bblock_t	*bbp;
	  
	  bbap = bblock_adm_head;
	  
	  /* check out the basic blocks */
	  for (bbp = bbap->ba_blocks;; bbp++) {
	    
	    /* are we at the end of the bb_admin section */
	    if (bbp >= bbap->ba_blocks + BB_PER_ADMIN) {
	      bbap = bbap->ba_next;
	      
	      /* are we done? */
	      if (bbap == NULL)
		break;
	      
	      bbp = bbap->ba_blocks;
	    }
	    
	    if (bbp->bb_flags != BBLOCK_DBLOCK)
	      continue;
	    
	    if (dblockp >= bbp->bb_dblock
		&& dblockp < bbp->bb_dblock +
		(1 << (BASIC_BLOCK - bbp->bb_bitn)))
	      break;
	  }
	  
	  if (bbap == NULL) {
	    dmalloc_errno = ERROR_BAD_DBLOCK_POINTER;
	    dmalloc_error("_chunk_dump_unfreed");
	    return;
	  }
	  
	  pnt = (char *)bbp->bb_mem + (dblockp - bbp->bb_dblock) *
	    (1 << bbp->bb_bitn);
	}
	
	unknown = 0;
	
	/* unknown pointer? */
	if (dblockp->db_file == _dmalloc_unknown_file
	    || dblockp->db_file == NULL
	    || dblockp->db_line == DMALLOC_DEFAULT_LINE) {
	  unknown_blockc++;
	  unknown_sizec += dblockp->db_size - pnt_total_adm;
	  unknown = 1;
	}
	
	if (! unknown || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_UNKNOWN)) {
	  _dmalloc_message("not freed: '%s' (%d bytes) from '%s'",
			   display_pnt(CHUNK_TO_USER(pnt),
				       &dblockp->db_overhead),
			   dblockp->db_size - pnt_total_adm,
			   _chunk_display_where(dblockp->db_file,
						dblockp->db_line));
	  
	  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE_SPACE))
	    _dmalloc_message("Dump of '%#lx': '%s'",
			     CHUNK_TO_USER(pnt),
			     expand_buf((char *)CHUNK_TO_USER(pnt),
					DUMP_SPACE));
	}
	
	sizec += dblockp->db_size - pnt_total_adm;
	blockc++;
      }
      break;
    }
  }
  
  /* copy out size of pointers */
  if (blockc > 0) {
    if (blockc - unknown_blockc > 0)
      _dmalloc_message("  known memory not freed: %d pointer%s, %d bytes",
		       blockc - unknown_blockc,
		       (blockc - unknown_blockc == 1 ? "" : "s"),
		       sizec - unknown_sizec);
    if (unknown_blockc > 0)
      _dmalloc_message("unknown memory not freed: %d pointer%s, %d bytes",
		       unknown_blockc, (unknown_blockc == 1 ? "" : "s"),
		       unknown_sizec);
  }
}
