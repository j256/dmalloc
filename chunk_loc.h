/*
 * Local defines for the low level memory routines
 *
 * Copyright 1999 by Gray Watson
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
 * The author may be contacted via http://www.dmalloc.com/
 *
 * $Id: chunk_loc.h,v 1.57 1999/03/09 15:43:21 gray Exp $
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
#if STORE_TIME
#ifdef TIME_INCLUDE
#include TIME_INCLUDE
#endif
#endif

/* for timeval type -- see settings.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
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

/* bb_flags values (short) */
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
#if STORE_TIME
  TIME_TYPE	ov_time;		/* time when pointer alloced */
#endif
#if STORE_TIMEVAL
  TIMEVAL_TYPE  ov_timeval;		/* time when pointer alloced */
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
 *
 * In an attempt to keep per-pointer overhead as low as possible I use a
 * lot of unions in this structure.  This tends to complicate the code
 * a bit, so to simplify I use define statements.  This in turn
 * complicates debugging :-) -- a trade-off.  The comments at the end
 * of the define statements show when the field in the union is used.
 * For instance, db_size (really db_info.in_nums.nu_size) is in use
 * when the pointer is Alloc[at]ed.  The db_next field is used when
 * the admin structure is on the free list.
 */
typedef struct dblock_st {
  union {
    struct {
      unsigned short	nu_size;		/* size of contiguous area */
      unsigned short	nu_line;		/* line where it was alloced */
    } in_nums;
    
    struct bblock_st	*in_bblock;		/* pointer to the bblock */
  } db_info;
  
  /* to reference union and struct elements as db elements */
#define db_size		db_info.in_nums.nu_size	/* Alloced */
#define db_line		db_info.in_nums.nu_line	/* Alloced */
#define db_bblock	db_info.in_bblock	/* Free */
  
  union {
    struct dblock_st	*pn_next;		/* next in the free list */
    const char		*pn_file;		/* .c filename where alloced */
  } db_pnt;
  
  /* to reference union elements as db elements */
#define db_next		db_pnt.pn_next		/* Free */
#define db_file		db_pnt.pn_file		/* Alloced */
  
#if FREED_POINTER_DELAY
  unsigned long	db_reuse_iter;			/* when avail for reuse */
#endif
  
  overhead_t	db_overhead;			/* configured overhead adds */
} dblock_t;

/*
 * Below defines a basic-block structure.  This structure is used to
 * track allocations that fit in one or many basic-blocks.  If you
 * have a basic-block size of 4k then a 16k allocation will take up 4
 * basic-blocks and 4 of these admin structures.
 *
 * Like the above divided-block structure, I use unions here to to
 * simplify the code and the comments at the end show when the field
 * is used.  For instance, bb_bit_n (really bb_nums.nu_bit_n) is in use
 * when the pointer is tracking user divided-block allocations or when
 * the pointer is free.  The bb_free_n field is used when the block is
 * full of admin structures such as this.  Yes, the library uses this
 * structure to track another block full of these structures.  Yikes!
 */
typedef struct bblock_st {
  unsigned short	bb_flags;		/* what it is */
  
  union {
    unsigned short	nu_bit_n;		/* chunk bit size */
    unsigned short	nu_line;		/* line where it was alloced */
  } bb_nums;
  
  /* to reference union elements as bb elements */
#define bb_bit_n	bb_nums.nu_bit_n	/* User-dblock, Free */
#define bb_line		bb_nums.nu_line		/* User-bblock */
  
  union {
    unsigned long	in_free_n;		/* admin free number */
    unsigned long	in_pos_n;		/* admin block position */
    unsigned long	in_block_n;		/* number of blocks */
    unsigned long	in_size;		/* size of allocation */
    /* NOTE: this pointer and the longs may be of a different type */
    dblock_t		*in_dblock;		/* pointer to dblock info */
  } bb_info;
  
  /* to reference union elements as bb elements */
#define bb_free_n	bb_info.in_free_n	/* BBlock-admin-free */
#define	bb_pos_n	bb_info.in_pos_n	/* BBlock-admin */
#define	bb_block_n	bb_info.in_block_n	/* Free */
#define	bb_size		bb_info.in_size		/* User-bblock */
#define	bb_dblock	bb_info.in_dblock	/* User-dblock */
  
  union {
    struct dblock_adm_st	*pn_slot_p;	/* pointer to db_admin block */
    struct bblock_adm_st	*pn_admin_p;	/* pointer to bb_admin block */
    void			*pn_mem;	/* memory associated to it */
    struct bblock_st		*pn_next;	/* next in free list */
    const char			*pn_file;	/* .c filename where alloced */
  } bb_pnt;
  
  /* to reference union elements as bb elements */
#define	bb_slot_p	bb_pnt.pn_slot_p	/* DBlock-admin */
#define	bb_admin_p	bb_pnt.pn_admin_p	/* BBlock-admin */
#define	bb_mem		bb_pnt.pn_mem		/* User-dblock, External */
#define	bb_next		bb_pnt.pn_next		/* Free */
#define	bb_file		bb_pnt.pn_file		/* User-bblock */
  
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
