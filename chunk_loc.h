/*
 * local defines for the low level memory routines
 *
 * Copyright 1995 by Gray Watson
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
 * The author may be contacted at gray.watson@letters.com
 *
 * $Id: chunk_loc.h,v 1.35 1995/05/30 15:09:35 gray Exp $
 */

#ifndef __CHUNK_LOC_H__
#define __CHUNK_LOC_H__

#include "conf.h"				/* up here for _INCLUDE */

/* for thread-id types -- see conf.h */
#if STORE_THREAD_ID
#ifdef THREAD_INCLUDE
#include THREAD_INCLUDE
#endif
#endif

/* for timeval type -- see conf.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
#endif

/* defines for the malloc subsystem */

/* checking information */
#define MIN_FILE_LENGTH		    3		/* min "[a-zA-Z].c" length */
#define MAX_FILE_LENGTH		   40		/* max __FILE__ length */
#define MAX_LINE_NUMBER		10000		/* max __LINE__ value */

/* free info */
#define BLANK_CHAR		'\305'		/* to blank free space with */

/* log-bad-space info */
#define SPECIAL_CHARS		"e\033^^\"\"''\\\\n\nr\rt\tb\bf\fa\007"
#define DUMP_SPACE		20		/* number of bytes to dump */
#define DUMP_SPACE_BUF		128		/* space for memory dump */

/*
 * the default smallest allowable allocations in bits.  this is
 * adjusted at runtime to conform to other settings.
 */
#define DEFAULT_SMALLEST_BLOCK	4

#define BITS(type)		(sizeof(type) * 8)	/* # bits in TYPE */
#define MAX_SLOTS		(BITS(long) - 1)	/* # of bit slots */

/* pointer to the start of the block which holds PNT */
#define BLOCK_NUM_TO_PNT(pnt)	(((long)(pnt) / BLOCK_SIZE) * BLOCK_SIZE)

/* adjust internal PNT to user-space */
#define CHUNK_TO_USER(pnt)	((char *)(pnt) + pnt_below_adm)
#define USER_TO_CHUNK(pnt)	((char *)(pnt) - pnt_below_adm)

/* get the number of blocks to hold SIZE */
#define NUM_BLOCKS(size)	(((size) + (BLOCK_SIZE - 1)) / BLOCK_SIZE)

/*
 * number of ba_block entries is a bblock_adm_t which must fit in a
 * basic block
 */
#define BB_PER_ADMIN	((BLOCK_SIZE - \
			  (sizeof(long) + sizeof(int) + \
			   sizeof(struct bblock_adm_st *) + \
			   sizeof(long))) / sizeof(bblock_t))

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
  
/* fence post checking defines */
#define FENCE_BOTTOM		ALLOCATION_ALIGNMENT
/* NOTE: FENCE_TOP defined in conf.h */
#define FENCE_OVERHEAD		(FENCE_BOTTOM + FENCE_TOP)
#define FENCE_MAGIC_BOTTOM	0xC0C0AB1B
#define FENCE_MAGIC_TOP		0xFACADE69

#define FENCE_WRITE(pnt, size)	do { \
				  bcopy(fence_bottom, (char *)(pnt), \
					FENCE_BOTTOM); \
				  bcopy(fence_top, (char *)(pnt) + (size) - \
					FENCE_TOP, FENCE_TOP); \
				} while (0)

/*
 * number of da_block entries in a dblock_adm_t which must fit in a
 * basic block
 */
#define DB_PER_ADMIN	((BLOCK_SIZE - (sizeof(long) + sizeof(long))) \
			 / sizeof(dblock_t))

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
#define BBLOCK_ADMIN_FREE	0x0080		/* ba_freen pnt to free slot */
#define BBLOCK_STRING		0x0100		/* block is a string (unused)*/

/*
 * some overhead fields for recording information about the pointer
 */
typedef struct {
#if STORE_SEEN_COUNT
  int		ov_seenc;		/* times pointer was seen */
#endif
#if STORE_ITERATION_COUNT
  long		ov_iteration;		/* interation when pointer alloced */
#endif
#if STORE_TIME
  long		ov_time;		/* time when pointer alloced */
#endif
#if STORE_TIMEVAL
  struct timeval  ov_timeval;		/* time when pointer alloced */
#endif
#if STORE_THREAD_ID
  THREAD_TYPE	ov_thread_id;		/* thread id which allocaed pnt */
#endif
  
#if STORE_SEEN_COUNT == 0 && STORE_ITERATION_COUNT == 0 && STORE_TIME == 0 && STORE_TIMEVAL == 0 && STORE_THREAD_ID == 0
  int		ov_junk;		/* for compilers that hate 0 structs */
#endif
} overhead_t;

/*
 * single divided-block administrative structure
 */
struct dblock_st {
  union {
    struct {
      unsigned short	nu_size;		/* size of contiguous area */
      unsigned short	nu_line;		/* line where it was alloced */
    } in_nums;
    
    struct bblock_st	*in_bblock;		/* pointer to the bblock */
  } db_info;
  
  /* to reference union and struct elements as db elements */
#define db_size		db_info.in_nums.nu_size	/* Used */
#define db_line		db_info.in_nums.nu_line	/* Used */
#define db_bblock	db_info.in_bblock	/* Free */
  
  union {
    struct dblock_st	*pn_next;		/* next in the free list */
    const char		*pn_file;		/* .c filename where alloced */
  } db_pnt;
  
  /* to reference union elements as db elements */
#define db_next		db_pnt.pn_next		/* Free */
#define db_file		db_pnt.pn_file		/* Used */
  
  overhead_t	db_overhead;			/* configured overhead adds */
};
typedef struct dblock_st	dblock_t;

/*
 * single basic-block administrative structure
 */
struct bblock_st {
  unsigned short	bb_flags;		/* what it is */
  
  union {
    unsigned short	nu_bitn;		/* chunk bit size */
    unsigned short	nu_line;		/* line where it was alloced */
  } bb_nums;
  
  /* to reference union elements as bb elements */
#define bb_bitn		bb_nums.nu_bitn		/* User-dblock, Free */
#define bb_line		bb_nums.nu_line		/* User-bblock */
  
  union {
    unsigned int	in_freen;		/* admin count number */
    dblock_t		*in_dblock;		/* pointer to dblock info */
    unsigned int	in_blockn;		/* number of blocks */
    unsigned int	in_size;		/* size of allocation */
  } bb_info;
  
  /* to reference union elements as bb elements */
#define bb_freen	bb_info.in_freen	/* BBlock-admin */
#define	bb_dblock	bb_info.in_dblock	/* User-dblock */
#define	bb_blockn	bb_info.in_blockn	/* Free */
#define	bb_size		bb_info.in_size		/* User-bblock */
  
  union {
    struct dblock_adm_st	*pn_slotp;	/* pointer to db_admin block */
    struct bblock_adm_st	*pn_adminp;	/* pointer to bb_admin block */
    void			*pn_mem;	/* memory associated to it */
    struct bblock_st		*pn_next;	/* next in free list */
    const char			*pn_file;	/* .c filename where alloced */
  } bb_pnt;
  
  /* to reference union elements as bb elements */
#define	bb_slotp	bb_pnt.pn_slotp		/* DBlock-admin */
#define	bb_adminp	bb_pnt.pn_adminp	/* BBlock-admin */
#define	bb_mem		bb_pnt.pn_mem		/* User-dblock, Extern (+tmp)*/
#define	bb_next		bb_pnt.pn_next		/* Free */
#define	bb_file		bb_pnt.pn_file		/* User-bblock */
  
  overhead_t	bb_overhead;			/* configured overhead adds */
};
typedef struct bblock_st	bblock_t;

/*
 * collection of bblock admin structures
 */
struct bblock_adm_st {
  long			ba_magic1;		/* bottom magic number */
  int			ba_posn;		/* position in bblock array */
  bblock_t		ba_blocks[BB_PER_ADMIN]; /* bblock admin info */
  struct bblock_adm_st	*ba_next;		/* next bblock adm struct */
  long			ba_magic2;		/* top magic number */
};
typedef struct bblock_adm_st	bblock_adm_t;

/*
 * collection of dblock admin structures
 */
struct dblock_adm_st {
  long			da_magic1;		/* bottom magic number */
  dblock_t		da_block[DB_PER_ADMIN];	/* dblock admin info */
  long			da_magic2;		/* top magic number */
};
typedef struct dblock_adm_st	dblock_adm_t;

#endif /* ! __CHUNK_LOC_H__ */
