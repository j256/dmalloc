/*
 * Local defines for the low level memory routines
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
 * $Id: chunk_loc.h,v 1.61 2000/03/21 18:19:07 gray Exp $
 */

#ifndef __CHUNK_LOC_H__
#define __CHUNK_LOC_H__

#include "conf.h"				/* up here for _INCLUDE */
#include "dmalloc_loc.h"			/* for DMALLOC_SIZE */

/* for thread-id types -- see conf.h */
#if LOG_THREAD_ID
#ifdef THREAD_INCLUDE
#include THREAD_INCLUDE
#endif
#endif

/* for time type -- see settings.h */
#if STORE_TIMEVAL
# ifdef TIMEVAL_INCLUDE
#  include TIMEVAL_INCLUDE
# endif
#else
# if STORE_TIME
#  ifdef TIME_INCLUDE
#   include TIME_INCLUDE
#  endif
# endif
#endif

/* log-bad-space info */
#define SPECIAL_CHARS		"\"\"''\\\\n\nr\rt\tb\bf\fa\007"

/*
 * the default smallest allowable allocations in bits.  this is
 * adjusted at runtime to conform to other settings.
 */
#define DEFAULT_SMALLEST_BLOCK	4

/*
 * definition of admin overhead during race condition clearing with
 * admin, needed, and external block handling.
 */
#define MAX_ADMIN_STORE		100

#define BITS(type)		(sizeof(type) * 8)	/* # bits in TYPE */
#define MAX_SLOTS		(int)(BITS(long) - 1)	/* # of bit slots */

/* pointer to the start of the block which holds PNT */
#define BLOCK_NUM_TO_PNT(pnt)	(((long)(pnt) / BLOCK_SIZE) * BLOCK_SIZE)

/* adjust internal PNT to user-space */
#define CHUNK_TO_USER(pnt)	((char *)(pnt) + fence_bottom_size)
#define USER_TO_CHUNK(pnt)	((char *)(pnt) - fence_bottom_size)

/* get the number of blocks to hold SIZE */
#define NUM_BLOCKS(size)	(((size) + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

/*
 * number of ba_block entries is a bblock_adm_t which must fit in a
 * basic block
 */
#define BB_PER_ADMIN	(int)((BLOCK_SIZE - \
			       (sizeof(long) + \
				sizeof(int) + \
				sizeof(struct bblock_adm_st *) + \
				sizeof(long))) \
			      / sizeof(bblock_t))

/*
 * number of da_block entries in a dblock_adm_t which must fit in a
 * basic block
 */
#define DB_PER_ADMIN	(int)((BLOCK_SIZE - \
			       (sizeof(long) + sizeof(long))) \
			      / sizeof(dblock_t))

/*
 * calculate the number of bits in SIZE and put in ANS
 */
#define NUM_BITS(size, ans)	do { \
				  unsigned int	_amt = (size), _b; \
				   \
				  for (_b = 0; _b <= LARGEST_BLOCK; _b++) \
				    if (_amt <= bits[_b]) \
				      break; \
				   \
				  (ans) = _b; \
				} while (0)
  
/* NOTE: FENCE_BOTTOM_SIZE and TOP_SIZE defined in settings.h */
#define FENCE_OVERHEAD_SIZE	(FENCE_BOTTOM_SIZE + FENCE_TOP_SIZE)
#define FENCE_MAGIC_BOTTOM	0xC0C0AB1B
#define FENCE_MAGIC_TOP		0xFACADE69
/* type of the fence magic values */
#define FENCE_MAGIC_TYPE	int

#define FENCE_WRITE(pnt, size)	do { \
				  memcpy((char *)(pnt), fence_bottom, \
					FENCE_BOTTOM_SIZE); \
				  memcpy((char *)(pnt) + (size) \
					 - FENCE_TOP_SIZE, \
					 fence_top, FENCE_TOP_SIZE); \
				} while (0)

#define CHUNK_MAGIC_BOTTOM	0xDEA007	/* bottom magic number */
#define CHUNK_MAGIC_TOP		0x976DEAD	/* top magic number */

/* bb_flags values (unsigned short) */
#define BBLOCK_ALLOCATED	0x00FF		/* block has been allocated */
#define BBLOCK_START_USER	0x0001		/* start of some user space */
#define BBLOCK_USER		0x0002		/* allocated by user space */
#define BBLOCK_ADMIN		0x0004		/* pointing to bblock admin */
#define BBLOCK_DBLOCK		0x0008		/* pointing to divided block */
#define BBLOCK_DBLOCK_ADMIN	0x0010		/* pointing to dblock admin */
#define BBLOCK_FREE		0x0020		/* block is free */
#define BBLOCK_EXTERNAL		0x0040		/* externally used block */
#define BBLOCK_ADMIN_FREE	0x0080		/* ba_free_n pnt to free slot*/
#define BBLOCK_STRING		0x0100		/* block is a string (unused)*/
#define BBLOCK_VALLOC		0x0200		/* block is aligned alloc */

#define BBLOCK_FLAG_TYPE(f)	((f) & BBLOCK_ALLOCATED)

/*
 * some overhead fields for recording information about the pointer.
 * this structure is included in both the basic-block and
 * divided-block admin structures and helps track specific information
 * about the pointer.
 */
typedef struct {
#if STORE_SEEN_COUNT
  unsigned long	ov_seen_c;		/* times pointer was seen */
#endif
#if STORE_ITERATION_COUNT
  unsigned long	ov_iteration;		/* interation when pointer alloced */
#endif
#if STORE_TIMEVAL
  TIMEVAL_TYPE  ov_timeval;		/* time when pointer alloced */
#else
#if STORE_TIME
  TIME_TYPE	ov_time;		/* time when pointer alloced */
#endif
#endif
#if LOG_THREAD_ID
  THREAD_TYPE	ov_thread_id;		/* thread id which allocaed pnt */
#endif
  
#if STORE_SEEN_COUNT == 0 && STORE_ITERATION_COUNT == 0 && STORE_TIME == 0 && STORE_TIMEVAL == 0 && LOG_THREAD_ID == 0
  int		ov_junk;		/* for compilers that hate 0 structs */
#endif
} overhead_t;

/*
 * Below defines a divided-block structure.  This structure is used to
 * track allocations that are less than half the size of a
 * basic-block.  If you have a basic-block size of 4k then 2 2k
 * allocations will be put into 1 basic-block, each getting one of
 * these admin structures.  Or 4 1k allocations, or 8 512 byte
 * allocations, etc.
 */
typedef struct dblock_st {
  unsigned short	db_flags;		/* what it is */
  
  unsigned short	db_size;		/* size of allocated area */
  unsigned short	db_line;		/* line where it was alloced */
  struct bblock_st	*db_bblock;		/* pointer to free bblock */
  
  struct dblock_st	*db_next;		/* next in the free list */
  const char		*db_file;		/* .c filename where alloced */
  
#if FREED_POINTER_DELAY
  unsigned long		db_reuse_iter;		/* when avail for reuse */
#endif
  
  overhead_t		db_overhead;		/* configured overhead adds */
} dblock_t;

/* db_flags values (unsigned short) */
#define DBLOCK_USER	0x0001			/* block is free */
#define DBLOCK_FREE	0x0002			/* block is free */

/*
 * Below defines a basic-block structure.  This structure is used to
 * track allocations that fit in one or many basic-blocks.  If you
 * have a basic-block size of 4k then a 16k allocation will take up 4
 * basic-blocks and 4 of these admin structures.
 */
typedef struct bblock_st {
  unsigned short	bb_flags;		/* what it is */
  
  unsigned short	bb_bit_n;		/* free chunk bit size */
  unsigned short	bb_line;		/* line where it was alloced */
  
  unsigned long		bb_free_n;		/* admin free number */
  unsigned long		bb_pos_n;		/* admin block position */
  unsigned long		bb_block_n;		/* number of free blocks */
  unsigned long		bb_size;		/* size of user allocation */
  dblock_t		*bb_dblock;		/* pointer to dblock info */
  
  struct dblock_adm_st	*bb_slot_p;		/* pointer to db_admin block */
  struct bblock_adm_st	*bb_admin_p;		/* pointer to bb_admin block */
  void			*bb_mem;		/* extern user dblock mem */
  struct bblock_st	*bb_next;		/* next in free list */
  const char		*bb_file;		/* .c filename where alloced */
  
#if FREED_POINTER_DELAY
  unsigned long	bb_reuse_iter;			/* when avail for reuse */
#endif
  
  overhead_t	bb_overhead;			/* configured overhead adds */
} bblock_t;

/*
 * Below is a definition of a bblock (basic-block) administration
 * structure.  This structure which fills a basic-block of space in
 * memory and holds a number of bblock structures.
 */
typedef struct bblock_adm_st {
  long			ba_magic1;		/* bottom magic number */
  unsigned int		ba_pos_n;		/* position in bblock array */
  bblock_t		ba_blocks[BB_PER_ADMIN]; /* bblock admin info */
  struct bblock_adm_st	*ba_next;		/* next bblock adm struct */
  long			ba_magic2;		/* top magic number */
} bblock_adm_t;

/*
 * Below is a definition of a dblock (divided-block) administration
 * structure.  This structure fills a basic-block of space in memory
 * and holds a number of dblock structures.
 */
typedef struct dblock_adm_st {
  long			da_magic1;		/* bottom magic number */
  dblock_t		da_block[DB_PER_ADMIN];	/* dblock admin info */
  long			da_magic2;		/* top magic number */
} dblock_adm_t;

#endif /* ! __CHUNK_LOC_H__ */
