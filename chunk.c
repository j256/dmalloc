/*
 * memory chunk low-level allocation routines
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Antaire not be used in advertising or publicity pertaining to
 * distribution of the document or software without specific, written prior
 * permission.
 *
 * The Antaire Corporation makes no representations about the suitability of
 * the software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted at gray.watson@antaire.com
 */

/*
 * This file contains algorithm level routine for the heap.  They handle the
 * manipulation and administration of chunks of memory.
 */

#include <ctype.h>

#define MALLOC_DEBUG_DISABLE

#include "malloc_dbg.h"
#include "conf.h"

#include "chunk.h"
#include "chunk_loc.h"
#include "compat.h"
#include "dbg_values.h"
#include "error.h"
#include "error_val.h"
#include "heap.h"
#include "malloc_loc.h"
#include "version.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: chunk.c,v 1.51 1993/09/22 05:02:12 gray Exp $";
#endif

/* checking information */
#define MIN_FILE_LENGTH		    3		/* min "[a-zA-Z].c" length */
#define MAX_FILE_LENGTH		   40		/* max __FILE__ length */
#define MAX_LINE_NUMBER		10000		/* max __LINE__ value */

/* free info */
#define FREE_COLUMN		    5		/* for dump_free formatting */
#define FREE_CHAR		'\305'		/* to blank free space with */

/* log-bad-space info */
#define SPECIAL_CHARS		"e\033^^\"\"''\\\\n\nr\rt\tb\bf\fa\007"
#define DUMP_OFFSET		2		/* offset back to dump */
#define DUMP_SPACE		15		/* number of bytes to dump */
#define DUMP_SPACE_BUF		128		/* space for memory dump */

/*
 * the unknown file pointer.  did not use MALLOC_UNKNOWN_FILE everywhere else
 * the pointers would be different.
 */
EXPORT	char		*_malloc_unknown_file = MALLOC_UNKNOWN_FILE;

/* free lists of bblocks and dblocks */
LOCAL	bblock_t	*free_bblock[LARGEST_BLOCK + 1];
LOCAL	dblock_t	*free_dblock[BASIC_BLOCK];

/* administrative structures */
LOCAL	bblock_adm_t	*bblock_adm_head = NULL; /* pointer to 1st bb_admin */
LOCAL	bblock_adm_t	*bblock_adm_tail = NULL; /* pointer to last bb_admin */
LOCAL	int		smallest_block	= 0;	/* smallest size in bits */

/* user information shifts for display purposes */
LOCAL	int		pnt_below_adm	= 0;	/* add to pnt for display */
LOCAL	int		pnt_total_adm	= 0;	/* total adm per pointer */

/* memory stats */
LOCAL	long		alloc_current	= 0;	/* current memory usage */
LOCAL	long		alloc_cur_given	= 0;	/* current mem given */
LOCAL	long		alloc_maximum	= 0;	/* maximum memory usage  */
LOCAL	long		alloc_max_given	= 0;	/* maximum mem given  */
LOCAL	long		alloc_total	= 0;	/* total allocation */
LOCAL	long		alloc_one_max	= 0;	/* maximum at once */
LOCAL	int		free_space_count = 0;	/* count the free bytes */

/* pointer stats */
LOCAL	long		alloc_cur_pnts	= 0;	/* current pointers */
LOCAL	long		alloc_max_pnts	= 0;	/* maximum pointers */
LOCAL	long		alloc_tot_pnts	= 0;	/* current pointers */

/* admin counts */
LOCAL	int		bblock_adm_count = 0;	/* count of bblock_admin */
LOCAL	int		dblock_adm_count = 0;	/* count of dblock_admin */
LOCAL	int		bblock_count	 = 0;	/* count of basic-blocks */
LOCAL	int		dblock_count	 = 0;	/* count of divided-blocks */

/* alloc counts */
EXPORT	int		_calloc_count	= 0;	/* # callocs, done in alloc */
LOCAL	int		free_count	= 0;	/* count the frees */
LOCAL	int		malloc_count	= 0;	/* count the mallocs */
LOCAL	int		realloc_count	= 0;	/* count the reallocs */

/******************************* misc routines *******************************/

/*
 * display printable chars from BUF of SIZE, non-printables as \%03o
 */
LOCAL	char	*expand_buf(const void * buf, const int size)
{
  static char	out[DUMP_SPACE_BUF];
  int		sizec;
  void		*bufp;
  char	 	*outp;
  
  for (sizec = 0, outp = out, bufp = (void *)buf; sizec < size;
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
 * display a bad pointer with FILE and LINE information
 */
EXPORT	char	*_chunk_display_pnt(const char * file, const unsigned int line)
{
  static char	buf[256];
  
  if ((file == MALLOC_DEFAULT_FILE || file == _malloc_unknown_file)
      && line == MALLOC_DEFAULT_LINE)
    (void)sprintf(buf, "%s", _malloc_unknown_file);
  else if (line == MALLOC_DEFAULT_LINE)
    (void)sprintf(buf, "ra=%#lx", (long)file);
  else
    (void)sprintf(buf, "%s:%u", file, line);
  
  return buf;
}

/*
 * small hack here.  same as above but different buffer space so we
 * can use 2 calls in 1 printf.
 */
LOCAL	char	*chunk_display_pnt2(const char * file, const unsigned int line)
{
  static char	buf[256];
  
  if ((file == MALLOC_DEFAULT_FILE || file == _malloc_unknown_file)
      && line == MALLOC_DEFAULT_LINE)
    (void)sprintf(buf, "%s", _malloc_unknown_file);
  else if (line == MALLOC_DEFAULT_LINE)
    (void)sprintf(buf, "ra=%#lx", (long)file);
  else
    (void)sprintf(buf, "%s:%u", file, line);
  
  return buf;
}

/*
 * log information (if wanted) about bad PNT from FILE, LINE.  Bad because of
 * REASON.  If PNT is 0 then it is unknown.
 */
LOCAL	void	log_bad_pnt(const char * file, const unsigned int line,
			    const void * pnt, const char * reason,
			    const char dump)
{
  /* dump the pointer information */
  _malloc_message("%s: pointer '%#lx' from '%s'",
		  reason, pnt, _chunk_display_pnt(file, line));
  
  /* NOTE: this has the potential for generating a seg-fault */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_BAD_SPACE) && pnt != NULL && dump)
    _malloc_message("Dump of '%#lx'-%d: '%s'",
		    pnt, DUMP_OFFSET,
		    expand_buf((char *)pnt - DUMP_OFFSET, DUMP_SPACE));
}

/************************* fence-post error functions ************************/

/*
 * check PNT of SIZE for fence-post magic numbers, returns ERROR or NOERROR
 */
LOCAL	int	fence_read(const char * file, const unsigned int line,
			   void * pnt, unsigned int size)
{
  unsigned long	top;
  long		*longp;
  
  /*
   * write magic numbers into block in bottom of allocation
   *
   * WARNING: assuming a word-boundary here
   */
  for (longp = (long *)pnt; longp < (long *)((char *)pnt + FENCE_BOTTOM);
       longp++)
    if (*longp != FENCE_MAGIC_BASE) {
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "overwrote lower fencepost",
		  TRUE);
      malloc_errno = ERROR_UNDER_FENCE;
      _malloc_error("fence_read");
      return ERROR;
    }
  
  /*
   * write magic numbers into block in top of allocation
   *
   * WARNING: not guaranteed a word-boundary here
   */
  for (longp = (long *)((char *)pnt + size - FENCE_TOP);
       longp < (long *)((char *)pnt + size);
       longp++) {
    bcopy(longp, &top, sizeof(long));
    if (top != FENCE_MAGIC_TOP) {
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "overwrote upper fencepost",
		  TRUE);
      malloc_errno = ERROR_OVER_FENCE;
      _malloc_error("fence_read");
      return ERROR;
    }
  }
  
  return NOERROR;
}

/*
 * load PNT of SIZE bytes with fence-post magic numbers
 */
LOCAL	void	fence_write(void * pnt, unsigned int size)
{
  unsigned long	top = FENCE_MAGIC_TOP;
  long		*longp;
  
  /* write magic numbers into block in bottom of allocation */
  for (longp = (long *)pnt; longp < (long *)((char *)pnt + FENCE_BOTTOM);
       longp++)
    *longp = FENCE_MAGIC_BASE;
  
  /* write magic numbers into block in top of allocation */
  /* WARNING: not guaranteed a word-boundary here */
  for (longp = (long *)((char *)pnt + size - FENCE_TOP);
       longp < (long *)((char *)pnt + size);
       longp++)
    bcopy(&top, longp, sizeof(long));
}

/************************** administration functions *************************/

/*
 * startup the low level malloc routines
 */
EXPORT	int	_chunk_startup(void)
{
  int	binc;
  
  /* calculate the smallest possible block */
  for (smallest_block = DEFAULT_SMALLEST_BLOCK;
       DB_PER_ADMIN < BLOCK_SIZE / (1 << smallest_block);
       smallest_block++);
  
  /* verify that some conditions are not true */
  if (BB_PER_ADMIN <= 2
      || sizeof(bblock_adm_t) > BLOCK_SIZE
      || DB_PER_ADMIN < (BLOCK_SIZE / (1 << smallest_block))
      || sizeof(dblock_adm_t) > BLOCK_SIZE
      || smallest_block < ALLOCATION_ALIGNMENT) {
    malloc_errno = ERROR_BAD_SETUP;
    _malloc_error("_chunk_startup");
    return ERROR;
  }
  
  /* align the base pointer */
  if (_heap_align_base(BLOCK_SIZE) == MALLOC_ERROR) {
    malloc_errno = ERROR_BAD_SETUP;
    _malloc_error("_chunk_startup");
    return ERROR;
  }
  
  /* initialize free bins */
  for (binc = 0; binc <= LARGEST_BLOCK; binc++)
    free_bblock[binc] = NULL;
  for (binc = 0; binc < BASIC_BLOCK; binc++)
    free_dblock[binc] = NULL;
  
  /* assign value to add to pointers when displaying */
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE)) {
    pnt_below_adm = FENCE_BOTTOM;
    pnt_total_adm = FENCE_OVERHEAD;
  }
  else {
    pnt_below_adm = 0;
    pnt_total_adm = 0;
  }
  
  return NOERROR;
}

/*
 * return the number of bits in number SIZE
 */
LOCAL	int	num_bits(const unsigned int size)
{
  unsigned int	tmp = size;
  int		bitc;
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (size == 0) {
    malloc_errno = ERROR_BAD_SIZE;
    _malloc_error("num_bits");
    return ERROR;
  }
#endif
  
  /* shift right until 0, 2 ^ 0 == 1 */
  for (bitc = -1; tmp > 0; bitc++)
    tmp >>= 1;
  
  /* are we not a base 2 number? */
  if (size > (1 << bitc))
    bitc++;
  
  return bitc;
}

/*
 * "allocate" another bblock administrative block
 * NOTE: some logistic problems with getting from free list
 */
LOCAL	bblock_adm_t	*get_bblock_admin(void)
{
  bblock_adm_t	*new;
  bblock_t	*bblockp;
  
  bblock_adm_count++;
  bblock_count++;
  
  /* get some more space for a bblock_admin structure */
  new = (bblock_adm_t *)_heap_alloc(BLOCK_SIZE);
  if (new == (bblock_adm_t *)HEAP_ALLOC_ERROR)
    return NULL;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("new bblock admin alloced for another %d admin slots",
		    BB_PER_ADMIN);
  
  /*
   * initialize the new bblock_admin block
   */
  new->ba_magic1 = CHUNK_MAGIC_BASE;
  if (bblock_adm_tail == NULL)
    new->ba_count = 0;
  else
    new->ba_count = bblock_adm_tail->ba_count + BB_PER_ADMIN;
  
  /* initialize the bblocks in the bblock_admin */
  for (bblockp = new->ba_block; bblockp < new->ba_block + BB_PER_ADMIN;
       bblockp++)
    bblockp->bb_flags = 0;
  
  new->ba_next = NULL;
  new->ba_magic2 = CHUNK_MAGIC_TOP;
  
  /*
   * continue bblock_admin linked-list
   */
  if (bblock_adm_tail == NULL)
    bblock_adm_head = new;
  else
    bblock_adm_tail->ba_next = new;
  bblock_adm_tail = new;
  
  return new;
}

/*
 * get MANY of bblocks, return a pointer to the first one
 */
LOCAL	bblock_t	*get_bblocks(const int bitc)
{
  static int	free_slots = 0;		/* free slots in last bb_admin */
  int		newc, mark;
  bblock_adm_t	*bblock_admp;
  bblock_t	*bblockp;
  
  if (bitc < BASIC_BLOCK) {
    malloc_errno = ERROR_BAD_SIZE;
    _malloc_error("get_bblocks");
    return NULL;
  }
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("need bblocks for %d bytes or %d bits", 1 << bitc, bitc);
  
  /* is there anything on the free list? */
  bblockp = free_bblock[bitc];
  if (bblockp != NULL) {
    free_bblock[bitc] = bblockp->bb_next;
    free_space_count -= 1 << bitc;
    return bblockp;
  }
  
  bblock_count += 1 << (bitc - BASIC_BLOCK);
  
  /* point at first free bblock entry (may be out of range) */
  bblock_admp = bblock_adm_tail;
  mark = BB_PER_ADMIN - free_slots;
  
  /* loop until we have enough slots */
  for (newc = 0; free_slots < 1 << (bitc - BASIC_BLOCK); newc++) {
    if (get_bblock_admin() == NULL)
      return NULL;
    
    /* do we need to move to the next bblock admin? */
    if (mark == BB_PER_ADMIN) {
      if (bblock_admp == NULL)
	bblock_admp = bblock_adm_tail;
      else
	bblock_admp = bblock_admp->ba_next;
      mark = 0;
    }
    
    /* write a bblock entry for the bblock admin */
    bblockp = &bblock_admp->ba_block[mark];
    
    bblockp->bb_flags	= BBLOCK_ADMIN;
    bblockp->bb_count	= bblock_adm_tail->ba_count;
    bblockp->bb_adminp	= bblock_adm_tail;
    
    /* add one to mark to pass by bblock admin header */
    mark++;
    
    /* adjust free_slot count for addition of new bblock admin */
    free_slots += BB_PER_ADMIN - 1;
  }
  
  /* do we need to move to the next bblock admin? */
  if (mark == BB_PER_ADMIN) {
    bblock_admp = bblock_admp->ba_next;
    mark = 0;
  }
  
  /* adjust free_slot count for the allocation */
  free_slots -= 1 << (bitc - BASIC_BLOCK);
  
  return &bblock_admp->ba_block[mark];
}

/*
 * get MANY of contiguous dblock administrative slots
 */
LOCAL	dblock_t	*get_dblock_admin(const int many)
{
  static int		free_slots = 0;
  static dblock_adm_t	*dblock_admp = NULL;
  dblock_t		*dblockp;
  bblock_t		*bblockp;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("need %d dblock admin slots", many);
  
  /* do we have enough right now */
  if (free_slots >= many) {
    dblockp = dblock_admp->da_block + (DB_PER_ADMIN - free_slots);
    free_slots -= many;
    return dblockp;
  }
  
  /*
   * allocate a new bblock of dblock admin slots, should use free list
   */
  bblockp = get_bblocks(BASIC_BLOCK);
  if (bblockp == NULL)
    return NULL;
  
  dblock_adm_count++;
  
  /* allocate the block if needed */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED))
    dblock_admp = (dblock_adm_t *)bblockp->bb_mem;
  else
    dblock_admp = (dblock_adm_t *)_heap_alloc(BLOCK_SIZE);
  if (dblock_admp == (dblock_adm_t *)HEAP_ALLOC_ERROR)
    return NULL;
  
  bblockp->bb_flags = BBLOCK_DBLOCK_ADMIN;
  bblockp->bb_slotp = dblock_admp;
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("opened another %d dblock admin slots", DB_PER_ADMIN);
  
  dblock_admp->da_magic1 = CHUNK_MAGIC_BASE;
  
  /* initialize the db_slots */
  for (dblockp = dblock_admp->da_block;
       dblockp < dblock_admp->da_block + DB_PER_ADMIN; dblockp++) {
    dblockp->db_bblock = NULL;
    dblockp->db_next = NULL;
  }
  
  dblock_admp->da_magic2 = CHUNK_MAGIC_TOP;
  
  free_slots = DB_PER_ADMIN - many;
  
  return dblock_admp->da_block;
}

/*
 * get a dblock of 1<<BITC sized chunks, also asked for the slot memory
 */
LOCAL	void	*get_dblock(const int bitc, dblock_t ** admp)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  void		*pnt;
  
  /* is there anything on the dblock free list? */
  dblockp = free_dblock[bitc];
  if (dblockp != NULL) {
    free_dblock[bitc] = dblockp->db_next;
    free_space_count -= 1 << bitc;
    
    *admp = dblockp;
    
    /* find pointer to memory chunk */
    pnt = dblockp->db_bblock->bb_mem +
      (dblockp - dblockp->db_bblock->bb_dblock) * (1 << bitc);
    
    /* do we need to print admin info? */
    if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
      _malloc_message("found a %d byte dblock entry", 1 << bitc);
    
    return pnt;
  }
  
  /* do we need to print admin info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("need to create a dblock for %dx %d byte blocks",
		    1 << (BASIC_BLOCK - bitc), 1 << bitc);
  
  /* get some dblock admin slots */
  dblockp = get_dblock_admin(1 << (BASIC_BLOCK - bitc));
  if (dblockp == NULL)
    return NULL;
  *admp = dblockp;
  
  dblock_count++;
  
  /* get a bblock from free list */
  bblockp = get_bblocks(BASIC_BLOCK);
  if (bblockp == NULL)
    return NULL;
  
  /* allocate the block if needed */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED))
    pnt = bblockp->bb_mem;
  else {
    pnt = _heap_alloc(BLOCK_SIZE);
    if (pnt == HEAP_ALLOC_ERROR)
      return NULL;
    bblockp->bb_mem = pnt;
  }
  
  /* setup bblock information */
  bblockp->bb_flags	= BBLOCK_DBLOCK;
  bblockp->bb_bitc	= bitc;
  bblockp->bb_dblock	= dblockp;
  
  /* add the rest to the free list (has to be at least 1 other dblock) */
  free_dblock[bitc] = ++dblockp;
  free_space_count += 1 << bitc;
  
  for (; dblockp < *admp + (1 << (BASIC_BLOCK - bitc)) - 1; dblockp++) {
    dblockp->db_next	= dblockp + 1;
    dblockp->db_bblock	= bblockp;
    free_space_count	+= 1 << bitc;
  }
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_FREE_BLANK)
      || BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FREE))
    (void)memset((char *)pnt + (1 << bitc), FREE_CHAR,
		 BLOCK_SIZE - (1 << bitc));
  
  /* last one points to NULL */
  dblockp->db_next	= NULL;
  dblockp->db_bblock	= bblockp;
  
  return pnt;
}

/*
 * find the bblock entry for PNT, return bblock admin in *BB_ADMIN (if != NULL)
 * and return the block number in BLOCK_NUM (if non NULL)
 */
LOCAL	bblock_t	*find_bblock(void * pnt, int * block_num,
				     bblock_adm_t ** bb_admin)
{
  int		bblockc;
  bblock_adm_t	*bblock_admp;
  
  if (pnt == NULL) {
    malloc_errno = ERROR_IS_NULL;
    return NULL;
  }
  
  /*
   * check validity of the pointer
   */
  if (! IS_IN_HEAP(pnt)) {
    malloc_errno = ERROR_NOT_IN_HEAP;
    return NULL;
  }
  
  /* find which block it is in */
  bblockc = WHICH_BLOCK(pnt);
  
  /* do we need to return the block number */
  if (block_num != NULL)
    *block_num = bblockc;
  
  /* find right bblock admin */
  for (bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bblock_admp = bblock_admp->ba_next) {
    if (bblockc < BB_PER_ADMIN)
      break;
    
    bblockc -= BB_PER_ADMIN;
  }
  
  if (bblock_admp == NULL) {
    malloc_errno = ERROR_NOT_FOUND;
    return NULL;
  }
  
  /* should we return bblock admin info? */
  if (bb_admin != NULL)
    *bb_admin = bblock_admp;
  
  return &bblock_admp->ba_block[bblockc];
}

/*
 * log the heap structure plus information on the blocks if necessary
 */
LOCAL	void	log_heap_map(void)
{
  bblock_adm_t	*bblock_admp;
  char		line[BB_PER_ADMIN + 10];
  int		linec, bblockc, bb_adminc;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("logging heap map information");
  
  _malloc_message("heap-base = %#lx, heap-end = %#lx, size = %ld bytes",
		  _heap_base, _heap_last, HEAP_SIZE);
  
  for (bb_adminc = 0, bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bb_adminc++, bblock_admp = bblock_admp->ba_next) {
    linec = 0;
    
    for (bblockc = 0; bblockc < BB_PER_ADMIN; bblockc++) {
      bblock_t	*bblockp = &bblock_admp->ba_block[bblockc];
      
      if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
	line[linec++] = '_';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
	line[linec++] = 'S';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_USER)) {
	line[linec++] = 'U';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_ADMIN)) {
	line[linec++] = 'A';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
	line[linec++] = 'd';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK_ADMIN)) {
	line[linec++] = 'a';
	continue;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_FREE)) {
	line[linec++] = 'F';
	continue;
      }
    }
    
    /* dumping a line to the logfile */
    if (linec > 0) {
      line[linec] = NULLC;
      _malloc_message("S%d:%s", bb_adminc, line);
    }
  }
  
  /* if we are not logging blocks then leave */
  if (! BIT_IS_SET(_malloc_debug, DEBUG_LOG_BLOCKS))
    return;
  
  for (bb_adminc = 0, bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bb_adminc++, bblock_admp = bblock_admp->ba_next) {
    
    for (bblockc = 0; bblockc < BB_PER_ADMIN; bblockc++) {
      bblock_t	*bblockp = &bblock_admp->ba_block[bblockc];
      
      if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
	_malloc_message("%d: not-allocated block",
			bb_adminc * BB_PER_ADMIN + bblockc);
	/* quit after the first non-allocated is found */
	break;
      }
      
      if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
	_malloc_message("%d: start-of-user block: %d bytes from '%s'",
			bb_adminc * BB_PER_ADMIN + bblockc,
			bblockp->bb_size,
			_chunk_display_pnt(bblockp->bb_file,
					   bblockp->bb_line));
	continue;
      }
      
      if (BIT_IS_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_USER)) {
	_malloc_message("%d: user continuation block",
			bb_adminc * BB_PER_ADMIN + bblockc);
	continue;
      }
      
      if (BIT_IS_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_ADMIN)) {
	_malloc_message("%d: administration block",
			bb_adminc * BB_PER_ADMIN + bblockc);
	continue;
      }
      
      if (BIT_IS_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_DBLOCK)) {
	_malloc_message("%d: dblock block",
			bb_adminc * BB_PER_ADMIN + bblockc);
	continue;
      }
      
      if (BIT_IS_SET(bblock_admp->ba_block[bblockc].bb_flags,
		     BBLOCK_DBLOCK_ADMIN)) {
	_malloc_message("%d: dblock-admin block",
			bb_adminc * BB_PER_ADMIN + bblockc);
	continue;
      }
      
      if (BIT_IS_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_FREE)) {
	_malloc_message("%d: free block",
			bb_adminc * BB_PER_ADMIN + bblockc);
	continue;
      }
    }
  }
}

/*
 * run extensive tests on the entire heap depending on TYPE
 */
EXPORT	int	_chunk_heap_check(void)
{
  bblock_adm_t	*this, *last_admp;
  bblock_t	*bblockp, *bblistp, *last_bblockp;
  dblock_t	*dblockp;
  int		undef = 0, start = 0;
  char		*bytep;
  void		*pnt;
  int		bitc, dblockc = 0, bblockc = 0, freec = 0;
  int		bbc = 0, len;
  int		free_bblockc[LARGEST_BLOCK + 1];
  int		free_dblockc[BASIC_BLOCK];
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("checking heap");
  
  /* if the heap is empty then no need to check anything */
  if (bblock_adm_head == NULL)
    return NOERROR;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_LISTS)) {
    
    /* count the bblock free lists */
    for (bitc = 0; bitc < LARGEST_BLOCK + 1; bitc++) {
      free_bblockc[bitc] = 0;
      
      /* parse bblock free list doing minimal pointer checking */
      for (bblockp = free_bblock[bitc]; bblockp != NULL;
	   bblockp = bblockp->bb_next, free_bblockc[bitc]++) {
	/*
	 * NOTE: this should not present problems since the bb_next is
	 * NOT unioned with bb_file.
	 */
	if (bblockp->bb_next != NULL && ! IS_IN_HEAP(bblockp->bb_next)) {
	  malloc_errno = ERROR_BAD_FREE_LIST;
	  _malloc_error("_chunk_heap_check");
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
	if (dblockp->db_next != NULL && ! IS_IN_HEAP(dblockp->db_next)) {
	  malloc_errno = ERROR_BAD_FREE_LIST;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
      }
    }
  }
  
  /* start pointers */
  this = bblock_adm_head;
  last_admp = NULL;
  last_bblockp = NULL;
  
  /* test admin pointer validity */
  if (! IS_IN_HEAP(this)) {
    malloc_errno = ERROR_BAD_ADMINP;
    _malloc_error("_chunk_heap_check");
    return ERROR;
  }
  
  /* test structure validity */
  if (this->ba_magic1 != CHUNK_MAGIC_BASE
      || this->ba_magic2 != CHUNK_MAGIC_TOP) {
    malloc_errno = ERROR_BAD_ADMIN_MAGIC;
    _malloc_error("_chunk_heap_check");
    return ERROR;
  }
  
  /* verify count value */
  if (this->ba_count != bbc) {
    malloc_errno = ERROR_BAD_ADMIN_COUNT;
    _malloc_error("_chunk_heap_check");
    return ERROR;
  }
  
  /* check out the basic blocks */
  for (bblockp = this->ba_block;; last_bblockp = bblockp++) {
    
    /* are we at the end of the bb_admin section */
    if (bblockp >= this->ba_block + BB_PER_ADMIN) {
      this = this->ba_next;
      bbc += BB_PER_ADMIN;
      
      /* are we done? */
      if (this == NULL)
	break;
      
      /* test admin pointer validity */
      if (! IS_IN_HEAP(this)) {
	malloc_errno = ERROR_BAD_ADMINP;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* test structure validity */
      if (this->ba_magic1 != CHUNK_MAGIC_BASE
	  || this->ba_magic2 != CHUNK_MAGIC_TOP) {
	malloc_errno = ERROR_BAD_ADMIN_MAGIC;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify count value */
      if (this->ba_count != bbc) {
	malloc_errno = ERROR_BAD_ADMIN_COUNT;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      bblockp = this->ba_block;
    }
    
    /* check for no-allocation */
    if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
      undef = 1;
      continue;
    }
    
    /* we better not have seen a not-allocated block before */
    if (undef == 1) {
      malloc_errno = ERROR_BAD_BLOCK_ORDER;
      _malloc_error("_chunk_heap_check");
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
	malloc_errno = ERROR_USER_NON_CONTIG;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* mark the size in bits */
      bitc = num_bits(bblockp->bb_size);
      if (bitc == ERROR)
	return ERROR;
      bblockc = 1 << (bitc - BASIC_BLOCK);
      start = 1;
      
      /* check fence-posts for memory chunk */
      if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE)) {
	pnt = BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block));
	if (fence_read(bblockp->bb_file, bblockp->bb_line,
		       pnt, bblockp->bb_size) != NOERROR)
	  return ERROR;
      }
      
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_USER:
      
      /* check line number */
      if (bblockp->bb_line > MAX_LINE_NUMBER) {
	log_bad_pnt(bblockp->bb_file, bblockp->bb_line, NULL,
		    "bad line number", FALSE);
	malloc_errno = ERROR_BAD_LINE;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
      if (bblockp->bb_size <= BLOCK_SIZE / 2
	  || bblockp->bb_size > (1 << LARGEST_BLOCK)) {
	log_bad_pnt(bblockp->bb_file, bblockp->bb_line, NULL, "bad size",
		    FALSE);
	malloc_errno = ERROR_BAD_SIZE;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* check file pointer */
      if (bblockp->bb_file != NULL
	  && bblockp->bb_line != MALLOC_DEFAULT_LINE) {
	len = strlen(bblockp->bb_file);
	if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	  malloc_errno = ERROR_BAD_FILEP;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
      }
      
      /* check X blocks in a row */
      if (bblockc == 0) {
	malloc_errno = ERROR_USER_NON_CONTIG;
	_malloc_error("_chunk_heap_check");
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
	  malloc_errno = ERROR_USER_NON_CONTIG;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
      
      bblockc--;
      
      /* NOTE: we should check above the allocated space if alloc_blank on */
      
      break;
      
    case BBLOCK_ADMIN:
      
      /* check the bblock_admin linked-list */
      if ((last_admp == NULL && bblockp->bb_adminp != bblock_adm_head)
	  || (last_admp != NULL && last_admp->ba_next != bblockp->bb_adminp)) {
	malloc_errno = ERROR_BAD_BLOCK_ADMINP;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      last_admp = bblockp->bb_adminp;
      
      /* check count against admin count */
      if (bblockp->bb_count != bblockp->bb_adminp->ba_count) {
	malloc_errno = ERROR_BAD_BLOCK_ADMINC;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      break;
      
    case BBLOCK_DBLOCK:
      
      /* check out bitc */
      if (bblockp->bb_bitc >= BASIC_BLOCK) {
	malloc_errno = ERROR_BAD_DBLOCK_SIZE;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblockp->bb_dblock)) {
	malloc_errno = ERROR_BAD_DBLOCK_POINTER;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify mem pointer */
      if (bblockp->bb_mem !=
	  BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block))) {
	malloc_errno = ERROR_BAD_DBLOCK_MEM;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* check dblock entry very closely if necessary */
      for (dblockc = 0, dblockp = bblockp->bb_dblock;
	   dblockp < bblockp->bb_dblock +
	   (1 << (BASIC_BLOCK - bblockp->bb_bitc));
	   dblockc++, dblockp++) {
	
	/* check out dblock entry to see if it is not free */
	/*
	 * XXX: this test might think it was a free'd slot if db_file
	 * (unioned with db_next) points to a return address in the
	 * heap.
	 */
	if ((dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next))
	    && IS_IN_HEAP(dblockp->db_bblock)) {
	  
	  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_LISTS)) {
	    dblock_t	*dblistp;
	    
	    /* find the free block in the free list */
	    for (dblistp = free_dblock[bblockp->bb_bitc]; dblistp != NULL;
		 dblistp = dblistp->db_next)
	      if (dblistp == dblockp)
		break;
	    
	    /* did we find it? */
	    if (dblistp == NULL) {
	      malloc_errno = ERROR_BAD_FREE_LIST;
	      _malloc_error("_chunk_heap_check");
	      return ERROR;
	    }
	    
	    free_dblockc[bblockp->bb_bitc]--;
	  }
	  
	  continue;
	}
	
	/*
	 * check out size, better be less than BLOCK_SIZE / 2
	 * I have to check this twice :-(
	 */
	if (dblockp->db_size > BLOCK_SIZE / 2) {
	  log_bad_pnt(dblockp->db_file, dblockp->db_line, NULL, "bad size",
		      FALSE);
	  malloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
	
	if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE)) {
	  pnt = bblockp->bb_mem + dblockc * (1 << bblockp->bb_bitc);
	  if (fence_read(dblockp->db_file, dblockp->db_line,
			 pnt, dblockp->db_size) != NOERROR)
	    return ERROR;
	}
      }
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblockp->bb_slotp)) {
	malloc_errno = ERROR_BAD_DBADMIN_POINTER;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify magic numbers */
      if (bblockp->bb_slotp->da_magic1 != CHUNK_MAGIC_BASE
	  || bblockp->bb_slotp->da_magic2 != CHUNK_MAGIC_TOP) {
	malloc_errno = ERROR_BAD_DBADMIN_MAGIC;
	_malloc_error("_chunk_heap_check");
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
	  pnt = dblockp->db_bblock->bb_mem +
	    (dblockp - dblockp->db_bblock->bb_dblock) *
	      (1 << dblockp->db_bblock->bb_bitc);
	  
	  /* should we verify that we have a block of FREE_CHAR? */
	  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FREE)) {
	    for (bytep = (char *)pnt;
		 bytep < (char *)pnt + (1 << dblockp->db_bblock->bb_bitc);
		 bytep++)
	      if (*bytep != FREE_CHAR) {
		log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, bytep,
			    "overwrote free space", TRUE);
		malloc_errno = ERROR_FREE_NON_BLANK;
		_malloc_error("_chunk_heap_check");
		return ERROR;
	      }
	  }
	  
	  continue;
	}
	
	/* check out size, better be less than BLOCK_SIZE / 2 */
	if (dblockp->db_size > BLOCK_SIZE / 2) {
	  log_bad_pnt(dblockp->db_file, dblockp->db_line, NULL, "bad size",
		      FALSE);
	  malloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
	
	/* check line number */
	if (dblockp->db_line > MAX_LINE_NUMBER) {
	  log_bad_pnt(dblockp->db_file, dblockp->db_line, NULL,
		      "bad line number", FALSE);
	  malloc_errno = ERROR_BAD_DBADMIN_SLOT;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
	
	if (dblockp->db_file != NULL
	    && dblockp->db_line != MALLOC_DEFAULT_LINE) {
	  len = strlen(dblockp->db_file);
	  if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	    malloc_errno = ERROR_BAD_DBADMIN_SLOT;
	    _malloc_error("_chunk_heap_check");
	    return ERROR;
	  }
	}
      }
      break;
      
    case BBLOCK_FREE:
      
      /* NOTE: check out free_lists, depending on debug value? */
      
      /* check out bitc which is the free-lists size */
      if (bblockp->bb_bitc < BASIC_BLOCK
	  || bblockp->bb_bitc > LARGEST_BLOCK) {
	malloc_errno = ERROR_BAD_SIZE;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /*
       * verify linked list pointer
       * NOTE: this should not present problems since the bb_next is
       * NOT unioned with bb_file.
       */
      if (bblockp->bb_next != NULL && ! IS_IN_HEAP(bblockp->bb_next)) {
	malloc_errno = ERROR_BAD_FREE_LIST;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify mem pointer */
      if (bblockp->bb_mem !=
	  BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block))) {
	malloc_errno = ERROR_BAD_FREE_MEM;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
      
      /* check X blocks in a row */
      if (freec == 0) {
	freec = 1 << (bblockp->bb_bitc - BASIC_BLOCK);
	
	if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_LISTS)) {
	  
	  /* find the free block in the free list */
	  for (bblistp = free_bblock[bblockp->bb_bitc]; bblistp != NULL;
	       bblistp = bblistp->bb_next)
	    if (bblistp == bblockp)
	      break;
	  
	  /* did we find it? */
	  if (bblistp == NULL) {
	    malloc_errno = ERROR_BAD_FREE_LIST;
	    _malloc_error("_chunk_heap_check");
	    return ERROR;
	  }
	  
	  free_bblockc[bblockp->bb_bitc]--;
	}
      }
      else
	if (last_bblockp == NULL || last_bblockp->bb_flags != BBLOCK_FREE
	    || bblockp->bb_bitc != last_bblockp->bb_bitc) {
	  malloc_errno = ERROR_FREE_NON_CONTIG;
	  _malloc_error("_chunk_heap_check");
	  return ERROR;
	}
      freec--;
      
      /* should we verify that we have a block of FREE_CHAR? */
      if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FREE)) {
	for (bytep = (char *)bblockp->bb_mem;
	     bytep < (char *)bblockp->bb_mem + BLOCK_SIZE; bytep++)
	  if (*bytep != FREE_CHAR) {
	    log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, bytep,
			"overwrote free space", TRUE);
	    malloc_errno = ERROR_FREE_NON_BLANK;
	    _malloc_error("_chunk_heap_check");
	    return ERROR;
	  }
	break;
	
      default:
	malloc_errno = ERROR_BAD_FLAG;
	_malloc_error("_chunk_heap_check");
	return ERROR;
	break;
      }
    }
  }
  
  /*
   * any left over contiguous counters?
   */
  if (bblockc > 0) {
    malloc_errno = ERROR_USER_NON_CONTIG;
    _malloc_error("_chunk_heap_check");
    return ERROR;
  }
  if (freec > 0) {
    malloc_errno = ERROR_FREE_NON_CONTIG;
    _malloc_error("_chunk_heap_check");
    return ERROR;
  }
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_LISTS)) {
    
    /* any free bblock entries not accounted for? */
    for (bitc = 0; bitc < LARGEST_BLOCK + 1; bitc++)
      if (free_bblockc[bitc] != 0) {
	malloc_errno = ERROR_BAD_FREE_LIST;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
    
    /* any free dblock entries not accounted for? */
    for (bitc = 0; bitc < BASIC_BLOCK; bitc++) {
      if (free_dblockc[bitc] != 0) {
	malloc_errno = ERROR_BAD_FREE_LIST;
	_malloc_error("_chunk_heap_check");
	return ERROR;
      }
    }
  }
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_HEAP_CHECK_MAP))
    log_heap_map();
  
  return NOERROR;
}

/*
 * run extensive tests on PNT from FUNC. test PNT HOW_MUCH of MIN_SIZE
 * (or 0 if unknown).  CHECK is flags for types of checking (see chunk.h).
 * returns [NO]ERROR
 */
EXPORT	int	_chunk_pnt_check(const char * func, void * pnt,
				 const int check, const int min_size)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  int		len, diff, min = min_size;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("checking pointer '%#lx'", pnt);
  
  /* adjust the pointer down if fence-posting */
  pnt = (char *)pnt - pnt_below_adm;
  if (min != 0)
    min += pnt_total_adm;
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, NULL);
  if (bblockp == NULL) {
    if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
      /* the pointer might not be the heap or might be NULL */
      malloc_errno = 0;
      return NOERROR;
    }
    else {
      log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		  "not in heap", FALSE);
      _malloc_error("find_bblock");
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
    diff = ((char *)pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc);
    if (diff != 0) {
      if (BIT_IS_SET(check, CHUNK_PNT_LOOSE)) {
	if (min != 0)
	  min += diff;
	pnt = (char *)pnt - diff;
      }
      else {
	log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE,
		    CHUNK_TO_USER(pnt), "not on block boundary", FALSE);
	malloc_errno = ERROR_NOT_ON_BLOCK;
	_malloc_error(func);
	return ERROR;
      }
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		  "bad administration info", FALSE);
      malloc_errno = ERROR_IS_FREE;
      _malloc_error(func);
      return ERROR;
    }
    
    /* check line number */
    if (dblockp->db_line > MAX_LINE_NUMBER) {
      log_bad_pnt(dblockp->db_file, dblockp->db_line, CHUNK_TO_USER(pnt),
		  "bad line number", FALSE);
      malloc_errno = ERROR_BAD_LINE;
      _malloc_error(func);
      return ERROR;
    }
    
    /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
    if (dblockp->db_size > BLOCK_SIZE / 2) {
      log_bad_pnt(dblockp->db_file, dblockp->db_line, CHUNK_TO_USER(pnt),
		  "bad size", FALSE);
      malloc_errno = ERROR_BAD_DBADMIN_SLOT;
      _malloc_error(func);
      return ERROR;
    }
    
    if (min != 0 && dblockp->db_size < min) {
      log_bad_pnt(dblockp->db_file, dblockp->db_line, CHUNK_TO_USER(pnt),
		  "not enough space", TRUE);
      malloc_errno = ERROR_WOULD_OVERWRITE;
      _malloc_error(func);
      return ERROR;
    }
    
    /* check file pointer */
    if (dblockp->db_file != NULL
	&& dblockp->db_line != MALLOC_DEFAULT_LINE) {
      len = strlen(dblockp->db_file);
      if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	log_bad_pnt(dblockp->db_file, dblockp->db_line, CHUNK_TO_USER(pnt),
		    "bad file-name", FALSE);
	malloc_errno = ERROR_BAD_FILEP;
	_malloc_error(func);
	return ERROR;
      }
    }
    
    /* check out the fence-posts */
    if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
      if (fence_read(dblockp->db_file, dblockp->db_line,
		     pnt, dblockp->db_size) != NOERROR)
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
      log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		  "not on block boundary", FALSE);
      malloc_errno = ERROR_NOT_ON_BLOCK;
      _malloc_error(func);
      return ERROR;
    }
  }
  
  /* are we on a normal block */
  if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)
      && ! (BIT_IS_SET(check, CHUNK_PNT_LOOSE)
	    && BIT_IS_SET(bblockp->bb_flags, BBLOCK_USER))) {
    log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		"not at start of user space", FALSE);
    malloc_errno = ERROR_NOT_START_USER;
    _malloc_error(func);
    return ERROR;
  }
  
  /* check line number */
  if (bblockp->bb_line > MAX_LINE_NUMBER) {
    log_bad_pnt(bblockp->bb_file, bblockp->bb_line, CHUNK_TO_USER(pnt),
		"bad line number", FALSE);
    malloc_errno = ERROR_BAD_LINE;
    _malloc_error(func);
    return ERROR;
  }
  
  /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
  if (bblockp->bb_size <= BLOCK_SIZE / 2
      || bblockp->bb_size > (1 << LARGEST_BLOCK)) {
    log_bad_pnt(bblockp->bb_file, bblockp->bb_line, CHUNK_TO_USER(pnt),
		"bad size", FALSE);
    malloc_errno = ERROR_BAD_SIZE;
    _malloc_error(func);
    return ERROR;
  }
  
  if (min != 0 && bblockp->bb_size < min) {
    log_bad_pnt(bblockp->bb_file, bblockp->bb_line, CHUNK_TO_USER(pnt),
		"not enough space", TRUE);
    malloc_errno = ERROR_WOULD_OVERWRITE;
    _malloc_error(func);
    return ERROR;
  }
  
  /* check file pointer */
  if (bblockp->bb_file != NULL
      && bblockp->bb_line != MALLOC_DEFAULT_LINE) {
    len = strlen(bblockp->bb_file);
    if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
      log_bad_pnt(bblockp->bb_file, bblockp->bb_line, CHUNK_TO_USER(pnt),
		  "bad file-name", FALSE);
      malloc_errno = ERROR_BAD_FILEP;
      _malloc_error(func);
      return ERROR;
    }
  }
  
  /* check out the fence-posts if we are at the start of a user-block */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)
      && BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
    if (fence_read(bblockp->bb_file, bblockp->bb_line,
		   pnt, bblockp->bb_size) != NOERROR)
      return ERROR;
  
  return NOERROR;
}

/**************************** information routines ***************************/

/*
 * return some information associated with PNT, returns [NO]ERROR
 */
EXPORT	int	_chunk_read_info(void * pnt, unsigned int * size,
				 char ** file, unsigned int * line,
				 void ** ret_attr)
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("reading info about pointer '%#lx'", pnt);
  
  /* adjust the pointer down if fence-posting */
  pnt = (char *)pnt - pnt_below_adm;
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, NULL);
  if (bblockp == NULL) {
    log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, pnt,
		"not in heap", FALSE);
    _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_error("find_bblock");
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if (((char *)pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		  "not on block boundary", FALSE);
      malloc_errno = ERROR_NOT_ON_BLOCK;
      _malloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		  "bad administration info", FALSE);
      malloc_errno = ERROR_IS_FREE;
      _malloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      *size = dblockp->db_size;
    if (file != NULL) {
      if (dblockp->db_line == MALLOC_DEFAULT_LINE)
	*file = NULL;
      else
	*file = dblockp->db_file;
    }
    if (line != NULL)
      *line = dblockp->db_line;
    if (ret_attr != NULL) {
      if (dblockp->db_line == MALLOC_DEFAULT_LINE)
	*ret_attr = dblockp->db_file;
      else
	*ret_attr = NULL;
    }
  }
  else {
    
    /* verify that the pointer is either dblock or user allocated */
    if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
      log_bad_pnt(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE, CHUNK_TO_USER(pnt),
		  "not at start of user space", FALSE);
      malloc_errno = ERROR_NOT_USER;
      _malloc_error("_chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      *size = bblockp->bb_size;
    if (file != NULL) {
      if (bblockp->bb_line == MALLOC_DEFAULT_LINE)
	*file = NULL;
      else
	*file = bblockp->bb_file;
    }
    if (line != NULL)
      *line = bblockp->bb_line;
    if (ret_attr != NULL) {
      if (bblockp->bb_line == MALLOC_DEFAULT_LINE)
	*ret_attr = bblockp->bb_file;
      else
	*ret_attr = NULL;
    }
  }
  
  return NOERROR;
}

/*
 * write new FILE, LINE, SIZE info into PNT
 */
LOCAL	int	chunk_write_info(const char * file, const unsigned int line,
				 void * pnt, const unsigned int size)
{
  int		bitc, bblockc;
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  bblock_adm_t	*bblock_admp;
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, &bblock_admp);
  if (bblockp == NULL) {
    log_bad_pnt(file, line, pnt, "not in heap", FALSE);
    _malloc_error("find_bblock");
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if (((char *)pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "not on block boundary",
		  FALSE);
      malloc_errno = ERROR_NOT_ON_BLOCK;
      _malloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "bad administration info",
		  FALSE);
      malloc_errno = ERROR_NOT_USER;
      _malloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      dblockp->db_size = size;
    if (file != NULL)
      dblockp->db_file = (char *)file;
    if (line != NULL)
      dblockp->db_line = (unsigned short)line;
  }
  else {
    
    /* verify that the pointer is user allocated */
    if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt),
		  "not at start of user space", FALSE);
      malloc_errno = ERROR_NOT_USER;
      _malloc_error("chunk_write_info");
      return ERROR;
    }
    
    /* count the bits */
    bitc = num_bits(bblockp->bb_size);
    if (bitc == ERROR)
      return ERROR;
    if (bitc < BASIC_BLOCK) {
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "bad size", FALSE);
      malloc_errno = ERROR_BAD_SIZE_INFO;
      _malloc_error("chunk_write_info");
      return ERROR;
    }
    
    /*
     * reset values in the bblocks
     */
    for (bblockc = 0; bblockc < (1 << (bitc - BASIC_BLOCK));
	 bblockc++, bblockp++) {
      
      /* do we need to hop to a new bblock_admp header? */
      if (bblockp == &bblock_admp->ba_block[BB_PER_ADMIN]) {
	bblock_admp = bblock_admp->ba_next;
	if (bblock_admp == NULL) {
	  log_bad_pnt(file, line, CHUNK_TO_USER(pnt),
		      "bad administration info", FALSE);
	  malloc_errno = ERROR_BAD_ADMIN_LIST;
	  _malloc_error("chunk_write_info");
	  return ERROR;
	}
	bblockp = bblock_admp->ba_block;
      }
      
      /* set bblock info */
      bblockp->bb_size = size;
      bblockp->bb_file = (char *)file;
      bblockp->bb_line = (unsigned short)line;
    }
  }
  
  return NOERROR;
}

/************************** low-level user functions *************************/

/*
 * get a SIZE chunk of memory for FILE at LINE
 */
EXPORT	void	*_chunk_malloc(const char * file, const unsigned int line,
			       const unsigned int size)
{
  int		bitc, bblockc, byten = size;
  bblock_t	*bblockp;
  bblock_adm_t	*bblock_admp;
  dblock_t	*dblockp;
  void		*pnt;
  
  /* counts calls to malloc */
  malloc_count++;
  
#if ALLOW_ALLOC_ZERO_SIZE == 0
  if (byten == 0) {
    log_bad_pnt(file, line, NULL, "bad zero byte allocation request",
		FALSE);
    malloc_errno = ERROR_BAD_SIZE;
    _malloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
#endif
  
  if (file == NULL)
    file = _malloc_unknown_file;
  
  /* adjust the size */
  byten += pnt_total_adm;
  
  /* count the bits */
  bitc = num_bits(byten);
  if (bitc == ERROR)
    return MALLOC_ERROR;
  
  /* monitor current allocation level */
  alloc_current += byten;
  alloc_cur_given += 1 << bitc;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_max_given = MAX(alloc_max_given, alloc_cur_given);
  alloc_total += byten;
  alloc_one_max = MAX(alloc_one_max, byten);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  /* have we exceeded the upper bounds */
  if (bitc > LARGEST_BLOCK) {
    log_bad_pnt(file, line, NULL, "too large allocation request", FALSE);
    malloc_errno = ERROR_TOO_BIG;
    _malloc_error("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* normalize to smallest_block.  No use spending 16 bytes to admin 1 byte */
  if (bitc < smallest_block)
    bitc = smallest_block;
  
  /* allocate divided block if small */
  if (bitc < BASIC_BLOCK) {
    pnt = get_dblock(bitc, &dblockp);
    if (pnt == NULL)
      return MALLOC_ERROR;
    
    dblockp->db_line = (unsigned short)line;
    dblockp->db_size = (unsigned short)byten;
    dblockp->db_file = (char *)file;
  }
  else {
    
    /*
     * allocate some bblock space
     */
    
    /* handle blocks */
    bblockp = get_bblocks(bitc);
    if (bblockp == NULL)
      return MALLOC_ERROR;
    
    /* calculate current bblock admin pointer and memory pointer */
    bblock_admp = (bblock_adm_t *)WHAT_BLOCK(bblockp);
    pnt = BLOCK_POINTER(bblock_admp->ba_count +
			(bblockp - bblock_admp->ba_block));
    
    /*
     * initialize the bblocks
     */
    for (bblockc = 0; bblockc < (1 << (bitc - BASIC_BLOCK));
	 bblockc++, bblockp++) {
      
      /* do we need to hop to a new bblock admin header? */
      if (bblockp == &bblock_admp->ba_block[BB_PER_ADMIN]) {
	bblock_admp = bblock_admp->ba_next;
	if (bblock_admp == NULL)
	  return MALLOC_ERROR;
	
	bblockp = bblock_admp->ba_block;
      }
      
      /* allocate the block if needed */
      if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_ALLOCATED))
	if (_heap_alloc(BLOCK_SIZE) == HEAP_ALLOC_ERROR)
	  return MALLOC_ERROR;
      
      if (bblockc == 0)
	bblockp->bb_flags = BBLOCK_START_USER;
      else
	bblockp->bb_flags = BBLOCK_USER;
      
      bblockp->bb_line	= (unsigned short)line;
      bblockp->bb_size	= (unsigned int)byten;
      bblockp->bb_file	= (char *)file;
    }
  }
  
  /* overwrite to-be-alloced or non-used portion of memory */
  if (BIT_IS_SET(_malloc_debug, DEBUG_ALLOC_BLANK))
    (void)memset(pnt, FREE_CHAR, 1 << bitc);
  
  /* write fence post info if needed */
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
    fence_write(pnt, byten);
  
  pnt = (char *)pnt + pnt_below_adm;
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("*** alloc: at '%s' for %u bytes (%d bits), got '%#lx'",
		    _chunk_display_pnt(file, line), byten - pnt_total_adm,
		    bitc, pnt);
  
  return pnt;
}

/*
 * frees PNT from the heap, returns FREE_ERROR or FREE_NOERROR
 */
EXPORT	int	_chunk_free(const char * file, const unsigned int line,
			    void * pnt)
{
  int		bblockc, bitc;
  bblock_t	*bblockp, *first;
  bblock_adm_t	*bblock_admp;
  dblock_t	*dblockp;
  
  /* counts calls to free */
  free_count++;
  
  alloc_cur_pnts--;
  
  /* adjust the pointer down if fence-posting */
  pnt = (char *)pnt - pnt_below_adm;
  
  /* find which block it is in */
  bblockp = find_bblock(pnt, NULL, &bblock_admp);
  if (bblockp == NULL) {
    log_bad_pnt(file, line, pnt, "not in heap", FALSE);
    _malloc_error("find_bblock");
    return FREE_ERROR;
  }
  
  /* are we free'ing a dblock entry? */
  if (BIT_IS_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    
    /* on a mini-block boundary? */
    if (((char *)pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "not on block boundary",
		  FALSE);
      malloc_errno = ERROR_NOT_ON_BLOCK;
      _malloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + ((char *)pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "bad administration info",
		  FALSE);
      malloc_errno = ERROR_ALREADY_FREE;
      _malloc_error("_chunk_free");
      return FREE_ERROR;
    }
    
    /* do we need to print transaction info? */
    if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
      _malloc_message("*** free: at '%s' pnt '%#lx': size %u, alloced at '%s'",
		      _chunk_display_pnt(file, line), CHUNK_TO_USER(pnt),
		      dblockp->db_size - pnt_total_adm,
		      chunk_display_pnt2(dblockp->db_file, dblockp->db_line));
    
    /* check fence-post, probably again */
    if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
      if (fence_read(dblockp->db_file, dblockp->db_line,
		     pnt, dblockp->db_size) != NOERROR)
	return FREE_ERROR;
    
    /* count the bits */
    bitc = bblockp->bb_bitc;
    
    /* monitor current allocation level */
    alloc_current -= dblockp->db_size;
    alloc_cur_given -= 1 << bitc;
    
    /* rearrange info */
    dblockp->db_bblock	= bblockp;
    dblockp->db_next	= free_dblock[bitc];
    free_dblock[bitc]	= dblockp;
    free_space_count	+= 1 << bitc;
    
    /* should we set free memory with FREE_CHAR? */
    if (BIT_IS_SET(_malloc_debug, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FREE))
      (void)memset(pnt, FREE_CHAR, 1 << bitc);
    
    return FREE_NOERROR;
  }
  
  /* on a block boundary? */
  if (! ON_BLOCK(pnt)) {
    log_bad_pnt(file, line, CHUNK_TO_USER(pnt),	"not on block boundary",
		FALSE);
    malloc_errno = ERROR_NOT_ON_BLOCK;
    _malloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* are we on a normal block */
  if (! BIT_IS_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
    log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "not at start of user space",
		FALSE);
    malloc_errno = ERROR_NOT_START_USER;
    _malloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* do we need to print transaction info? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("*** free: at '%s' pnt '%#lx': size %u, alloced at '%s'",
		    _chunk_display_pnt(file, line), CHUNK_TO_USER(pnt),
		    bblockp->bb_size - pnt_total_adm,
		    chunk_display_pnt2(bblockp->bb_file, bblockp->bb_line));
  
  /* check fence-post, probably again */
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
    if (fence_read(bblockp->bb_file, bblockp->bb_line,
		   pnt, bblockp->bb_size) != NOERROR)
      return FREE_ERROR;
  
  /* count the bits */
  bitc = num_bits(bblockp->bb_size);
  if (bitc == ERROR)
    return FREE_ERROR;
  
  /* monitor current allocation level */
  alloc_current -= bblockp->bb_size;
  alloc_cur_given -= 1 << bitc;
  
  if (bitc < BASIC_BLOCK) {
    log_bad_pnt(file, line, CHUNK_TO_USER(pnt), "bad administration info",
		FALSE);
    malloc_errno = ERROR_BAD_SIZE_INFO;
    _malloc_error("_chunk_free");
    return FREE_ERROR;
  }
  
  /* setup free linked-list */
  first = free_bblock[bitc];
  free_bblock[bitc] = bblockp;
  free_space_count += 1 << bitc;
  
  bblockp->bb_next = first;
  
  /*
   * initialize the bblocks
   */
  for (bblockc = 0; bblockc < (1 << (bitc - BASIC_BLOCK));
       bblockc++, bblockp++, pnt = ((char *)pnt + BLOCK_SIZE)) {
    
    /* do we need to hop to a new bblock_admp header? */
    if (bblockp == &bblock_admp->ba_block[BB_PER_ADMIN]) {
      bblock_admp = bblock_admp->ba_next;
      if (bblock_admp == NULL) {
	malloc_errno = ERROR_BAD_ADMIN_LIST;
	_malloc_error("_chunk_free");
	return FREE_ERROR;
      }
      bblockp = bblock_admp->ba_block;
    }
    
    /* set bblock info */
    bblockp->bb_flags	= BBLOCK_FREE;
    bblockp->bb_bitc	= bitc;			/* num bblocks in this chunk */
    bblockp->bb_mem	= pnt;			/* pointer to real memory */
    bblockp->bb_next	= first;
    
    /* should we set free memory with FREE_CHAR? */
    if (BIT_IS_SET(_malloc_debug, DEBUG_FREE_BLANK)
	|| BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FREE))
      (void)memset(pnt, FREE_CHAR, BLOCK_SIZE);
  }
  
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
  unsigned int	old_size, size, old_line;
  int		old_bitc, new_bitc;
  
  /* counts calls to realloc */
  realloc_count++;
  
  /* get info about old pointer */
  if (_chunk_read_info(oldp, &old_size, &old_file, &old_line, &ret_addr) !=
      NOERROR)
    return REALLOC_ERROR;
  
  if (ret_addr != NULL)
    old_file = ret_addr;
  
  /* adjust the pointer down if fence-posting */
  oldp = (char *)oldp - pnt_below_adm;
  new_size += pnt_total_adm;
  
  /* check the fence-posting */
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
    if (fence_read(file, line, oldp, old_size) != NOERROR)
      return REALLOC_ERROR;
  
  /* get the old and new bit sizes */
  old_bitc = num_bits(old_size);
  if (old_bitc == ERROR)
    return REALLOC_ERROR;
  
  new_bitc = num_bits(new_size);
  if (new_bitc == ERROR)
    return REALLOC_ERROR;
  
  /* if we are not realloc copying and the size is the same */
  if (BIT_IS_SET(_malloc_debug, DEBUG_REALLOC_COPY) || old_bitc != new_bitc) {
    
    /* readjust info */
    oldp = (char *)oldp + pnt_below_adm;
    old_size -= pnt_total_adm;
    new_size -= pnt_total_adm;
    
    /* allocate space for new chunk */
    newp = _chunk_malloc(file, line, new_size);
    if (newp == MALLOC_ERROR)
      return REALLOC_ERROR;
    
    /*
     * NOTE: _chunk_malloc() already took care of the fence stuff...
     */
    
    /* copy stuff into new section of memory */
    size = MIN(new_size, old_size);
    if (size > 0)
      bcopy(oldp, newp, size);
    
    /* free old pointer */
    if (_chunk_free(file, line, oldp) != FREE_NOERROR)
      return REALLOC_ERROR;
  }
  else {
    /* monitor current allocation level */
    alloc_current += new_size - old_size;
    alloc_maximum = MAX(alloc_maximum, alloc_current);
    alloc_total += new_size;
    alloc_one_max = MAX(alloc_one_max, new_size);
    
    /* monitor pointer usage */
    alloc_tot_pnts++;
    
    /* reuse the old-pointer */
    newp = oldp;
    
    /* rewrite size information */
    if (chunk_write_info(file, line, newp, new_size) != NOERROR)
      return REALLOC_ERROR;
    
    /* overwrite to-be-alloced or non-used portion of memory */
    if (BIT_IS_SET(_malloc_debug, DEBUG_ALLOC_BLANK)
	&& (1 << new_bitc) - old_size > 0)
      (void)memset((char *)newp + old_size, FREE_CHAR,
		   (1 << new_bitc) - old_size);
    
    /* write in fence-post info and adjust new pointer over fence info */
    if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_FENCE))
      fence_write(newp, new_size);
    
    newp = (char *)newp + pnt_below_adm;
    oldp = (char *)oldp + pnt_below_adm;
    old_size -= pnt_total_adm;
    new_size -= pnt_total_adm;
  }
  
  /*
   * do we need to print transaction info?
   *
   * NOTE: pointers and sizes here a user-level real
   */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_TRANS))
    _malloc_message("*** realloc: at '%s' from '%#lx' (%u bytes) file '%s' to '%#lx' (%u bytes)",
		    _chunk_display_pnt(file, line),
		    oldp, old_size, chunk_display_pnt2(old_file, old_line),
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
  int		bitc, count;
  char		info[256], tmp[80];
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  /* print header bit-counts */
  info[0] = NULLC;
  for (bitc = smallest_block; bitc <= LARGEST_BLOCK; bitc++) {
    (void)sprintf(tmp, "%*d", FREE_COLUMN, bitc);
    (void)strcat(info, tmp);
  }
  
  _malloc_message("bits: %s", info);
  
  /* dump the free (and later used) list counts */
  info[0] = NULLC;
  for (bitc = smallest_block; bitc <= LARGEST_BLOCK; bitc++) {
    if (bitc < BASIC_BLOCK)
      for (count = 0, dblockp = free_dblock[bitc]; dblockp != NULL;
	   count++, dblockp = dblockp->db_next);
    else
      for (count = 0, bblockp = free_bblock[bitc]; bblockp != NULL;
	   count++, bblockp = bblockp->bb_next);
    
    (void)sprintf(tmp, "%*d", FREE_COLUMN, count);
    (void)strcat(info, tmp);
  }
  
  _malloc_message("free: %s", info);
}

/*
 * log statistics on the heap
 */
EXPORT	void	_chunk_stats(void)
{
  long		overhead;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("dumping chunk statistics");
  
  /* version information */
  _malloc_message("malloc version '%s'", malloc_version);
  
  /* general heap information */
  _malloc_message("heap start %#lx, heap end %#lx, heap size %ld bytes",
		  _heap_base, _heap_last, HEAP_SIZE);
  
  /* log user allocation information */
  _malloc_message("alloc calls: malloc %d, realloc %d, calloc %d, free %d",
		  malloc_count - _calloc_count, realloc_count, _calloc_count,
		  free_count);
  
  _malloc_message("total amount of memory allocated:   %ld bytes (%ld pnts)",
		  alloc_total, alloc_tot_pnts);
  
  _malloc_message("maximum memory in use at one time:  %ld bytes (%ld pnts)",
		  alloc_maximum, alloc_max_pnts);
  
  _malloc_message("maximum memory alloced with 1 call: %ld bytes",
		  alloc_one_max);
  
  _malloc_message("max base 2 allocation loss: %ld bytes (%d%%)",
		  alloc_max_given - alloc_maximum,
		  (alloc_max_given == 0 ? 0 :
		   100 - ((alloc_maximum * 100) / alloc_max_given)));
  
  _malloc_message("final user memory space: %ld bytes",
		  alloc_current + free_space_count);
  
  /* log administration information */
  _malloc_message("final admin: basic-blocks %d, divided blocks %d",
		  bblock_count, dblock_count);
  
  overhead = HEAP_SIZE - (alloc_current + free_space_count);
  
  _malloc_message("final heap admin overhead: %ld bytes (%d%%)",
		  overhead,
		  (HEAP_SIZE == 0 ? 0 : (overhead * 100) / HEAP_SIZE));
}

/*
 * dump the unfreed memory, logs the unfreed information to logger
 */
EXPORT	void	_chunk_dump_not_freed(void)
{
  bblock_adm_t	*this;
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  void		*pnt;
  char		unknown, ra;
  int		unknown_sizec = 0, unknown_dblockc = 0, unknown_bblockc = 0;
  int		ra_sizec = 0, ra_dblockc = 0, ra_bblockc = 0;
  int		sizec = 0, dblockc = 0, bblockc = 0;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_ADMIN))
    _malloc_message("dumping the unfreed pointers");
  
  /* has anything been alloced yet? */
  this = bblock_adm_head;
  if (this == NULL)
    return;
  
  /* check out the basic blocks */
  for (bblockp = this->ba_block;; bblockp++) {
    
    /* are we at the end of the bb_admin section */
    if (bblockp >= this->ba_block + BB_PER_ADMIN) {
      this = this->ba_next;
      
      /* are we done? */
      if (this == NULL)
	break;
      
      bblockp = this->ba_block;
    }
    
    /*
     * check for different types
     */
    switch (bblockp->bb_flags) {
      
    case BBLOCK_ADMIN:
    case BBLOCK_DBLOCK:
    case BBLOCK_FREE:
    case BBLOCK_USER:
      break;
      
    case BBLOCK_START_USER:
      /* find pointer to memory chunk */
      pnt = BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block));
      
      unknown = 0;
      ra = 0;
      
      /* unknown pointer? */
      if ((bblockp->bb_file == _malloc_unknown_file
	   || bblockp->bb_file == NULL)
	  && bblockp->bb_line == MALLOC_DEFAULT_LINE) {
	unknown_bblockc++;
	unknown_sizec += bblockp->bb_size - pnt_total_adm;
	unknown = 1;
      }
      else if (bblockp->bb_line == MALLOC_DEFAULT_LINE) {
	ra_bblockc++;
	ra_sizec += bblockp->bb_size - pnt_total_adm;
	ra = 1;
      }
      
      if ((! unknown || BIT_IS_SET(_malloc_debug, DEBUG_LOG_UNKNOWN))
	  && (! ra || BIT_IS_SET(_malloc_debug, DEBUG_LOG_RA)))
	_malloc_message("not freed: %#lx (%8d bytes) from '%s'",
			(char *)pnt + pnt_below_adm,
			bblockp->bb_size - pnt_total_adm,
			_chunk_display_pnt(bblockp->bb_file,
					   bblockp->bb_line));
      
      sizec += bblockp->bb_size - pnt_total_adm;
      bblockc++;
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
	  for (bbp = bbap->ba_block;; bbp++) {
	    
	    /* are we at the end of the bb_admin section */
	    if (bbp >= bbap->ba_block + BB_PER_ADMIN) {
	      bbap = bbap->ba_next;
	      
	      /* are we done? */
	      if (bbap == NULL)
		break;
	      
	      bbp = bbap->ba_block;
	    }
	    
	    if (bbp->bb_flags != BBLOCK_DBLOCK)
	      continue;
	    
	    if (dblockp >= bbp->bb_dblock
		&& dblockp < bbp->bb_dblock +
		(1 << (BASIC_BLOCK - bbp->bb_bitc)))
	      break;
	  }
	  
	  if (bbap == NULL) {
	    malloc_errno = ERROR_BAD_DBLOCK_POINTER;
	    _malloc_error("_chunk_dump_not_freed");
	    return;
	  }
	  
	  pnt = bbp->bb_mem + (dblockp - bbp->bb_dblock) * (1 << bbp->bb_bitc);
	}
	
	unknown = 0;
	ra = 0;
	
	/* unknown pointer? */
	if ((dblockp->db_file == _malloc_unknown_file
	     || dblockp->db_file == NULL)
	    && dblockp->db_line == MALLOC_DEFAULT_LINE) {
	  unknown_dblockc++;
	  unknown_sizec += dblockp->db_size - pnt_total_adm;
	  unknown = 1;
	}
	else if (dblockp->db_line == MALLOC_DEFAULT_LINE) {
	  ra_dblockc++;
	  ra_sizec += dblockp->db_size - pnt_total_adm;
	  ra = 1;
	}
	
	if ((! unknown || BIT_IS_SET(_malloc_debug, DEBUG_LOG_UNKNOWN))
	    && (! ra || BIT_IS_SET(_malloc_debug, DEBUG_LOG_RA)))
	  _malloc_message("not freed: %#lx (%8d bytes) from '%s'",
			  (char *)pnt + pnt_below_adm,
			  dblockp->db_size - pnt_total_adm,
			  _chunk_display_pnt(dblockp->db_file,
					     dblockp->db_line));
	
	sizec += dblockp->db_size - pnt_total_adm;
	dblockc++;
      }
      break;
    }
  }
  
  /* copy out size of pointers */
  if (bblockc + dblockc > 0) {
    _malloc_message("known memory not freed: %d bblock, %d dblock, %d byte%s",
		    bblockc - unknown_bblockc, dblockc - unknown_dblockc,
		    sizec - unknown_sizec, (sizec == 1 ? "" : "s"));
    
    _malloc_message("unknown memory not freed: %d pointers, %d byte%s",
		    unknown_bblockc + unknown_dblockc, unknown_sizec,
		    (unknown_sizec == 1 ? "" : "s"));
    
    _malloc_message("ret-addr memory not freed: %d pointers, %d byte%s",
		    ra_bblockc + ra_dblockc, ra_sizec,
		    (ra_sizec == 1 ? "" : "s"));
  }
}

/*
 * log the heap structure plus information on the blocks if necessary
 */
EXPORT	void	_chunk_log_heap_map(void)
{
  /* did we map the heap second ago? */
  if (! BIT_IS_SET(_malloc_debug, DEBUG_CHECK_HEAP) ||
      ! BIT_IS_SET(_malloc_debug, DEBUG_HEAP_CHECK_MAP))
    log_heap_map();
}
