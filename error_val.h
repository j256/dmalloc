/*
 * global error codes for chunk allocation problems
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
 * $Id: error_val.h,v 1.28 2003/05/13 18:16:49 gray Exp $
 */

#ifndef __ERROR_VAL_H__
#define __ERROR_VAL_H__

/*
 * malloc error codes
 */
#define ERROR_NONE			1	/* no error */

/* administrative errors */
#define ERROR_BAD_SETUP			10	/* bad setup value */
#define ERROR_IN_TWICE			11	/* in malloc domain twice */
#define ERROR_BAD_ERRNO			12	/* bad errno value */
#define ERROR_LOCK_NOT_CONFIG		13	/* thread locking not config */

/* pointer verification errors */
#define ERROR_IS_NULL			20	/* pointer is NULL */
#define ERROR_NOT_IN_HEAP		21	/* pointer is not in heap */
#define ERROR_NOT_FOUND			22	/* pointer not-found */
#define ERROR_IS_FOUND			23	/* found special pointer */
#define ERROR_BAD_FILEP			24	/* bad bblock file-name */
#define ERROR_BAD_LINE			25	/* bad bblock line-number */
#define ERROR_UNDER_FENCE		26	/* failed picket fence lower */
#define ERROR_OVER_FENCE		27	/* failed picket fence upper */
#define ERROR_WOULD_OVERWRITE		28	/* would overwrite fence */
#define ERROR_IS_FREE			29	/* pointer is already free */

/* allocation errors */
#define ERROR_BAD_SIZE			40	/* bad bblock size value */
#define ERROR_TOO_BIG			41	/* allocation too large */
#define ERROR_USER_NON_CONTIG		42	/* user space not contiguous */
#define ERROR_ALLOC_FAILED		43	/* could not get more space */
#define ERROR_ALLOC_NONLINEAR		44	/* no linear address space */
#define ERROR_BAD_SIZE_INFO		45	/* info doesn't match size */
#define ERROR_EXTERNAL_HUGE		46	/* external allocation big */
#define ERROR_OVER_LIMIT		47	/* over allocation limit */

/* free errors */
#define ERROR_NOT_ON_BLOCK		60	/* not on block boundary */
#define ERROR_ALREADY_FREE		61	/* already in free list */
#define ERROR_NOT_START_USER		62	/* not start of user alloc */
#define ERROR_NOT_USER			63	/* not user allocated */
#define ERROR_BAD_FREE_LIST		64	/* free-list mess-up */
#define ERROR_FREE_NON_CONTIG		65	/* free space not contiguous */
#define ERROR_BAD_FREE_MEM		66	/* bad memory pointer */
#define ERROR_FREE_NON_BLANK		67	/* free space should be 0's */

/* dblock errors */
#if 0
#define ERROR_BAD_DBLOCK_SIZE		80	/* dblock bad size */
#define ERROR_BAD_DBLOCK_POINTER	81	/* bad dblock pointer */
#define ERROR_BAD_DBLOCK_MEM		82	/* bad memory pointer */
#define ERROR_BAD_DBADMIN_POINTER	83	/* bad dblock admin pointer */
#define ERROR_BAD_DBADMIN_MAGIC		84	/* bad dblock admin pointer */
#define ERROR_BAD_DBADMIN_SLOT		85	/* bad dblock slot info */
#endif

/* administrative errors */
#define ERROR_BAD_ADMIN_P		90	/* admin value out of bounds */
#define ERROR_BAD_ADMIN_LIST		91	/* list pnt out of bounds */
#define ERROR_BAD_ADMIN_MAGIC		92	/* bad magic numbers */
#define ERROR_BAD_ADMIN_COUNT		93	/* bad count number */
#define ERROR_BAD_BLOCK_ADMIN_P		94	/* bblock adminp bad */
#define ERROR_BAD_BLOCK_ADMIN_C		95	/* bblock adminp->count bad */

/* heap check verification errors */
#define ERROR_BAD_BLOCK_ORDER		100	/* block allocation bad */
#define ERROR_BAD_FLAG			101	/* bad basic-block flag */

/* memory table errors */
#define ERROR_TABLE_CORRUPT		102	/* memory table corruption */
#define ERROR_ADDRESS_LIST		103	/* invalid address list */

#define INVALID_ERROR		"errno value is not valid"

typedef struct {
  int		es_error;		/* error number */
  char		*es_string;		/* assocaited string */
} error_str_t;

/* string error codes which apply to error codes in error_val.h */
static	error_str_t	error_list[]
#ifdef __GNUC__
__attribute__ ((unused))
#endif
= {
  { ERROR_NONE,			"no error" },
  
  /* administrative errors */
  { ERROR_BAD_SETUP,		"initialization and setup failed" },
  { ERROR_IN_TWICE,		"malloc library has gone recursive" },
  { ERROR_BAD_ERRNO,		"errno value from user is out-of-bounds" },
  { ERROR_LOCK_NOT_CONFIG,	"thread locking has not been configured" },
  
  /* pointer verification errors */
  { ERROR_IS_NULL,		"pointer is null" },
  { ERROR_NOT_IN_HEAP,		"pointer is not pointing to heap data space" },
  { ERROR_NOT_FOUND,		"cannot locate pointer in heap" },
  { ERROR_IS_FOUND,		"found pointer the user was looking for" },
  { ERROR_BAD_FILEP,		"possibly bad .c filename pointer" },
  { ERROR_BAD_LINE,		"possibly bad .c file line-number" },
  { ERROR_UNDER_FENCE,	       "failed UNDER picket-fence magic-number check"},
  { ERROR_OVER_FENCE,		"failed OVER picket-fence magic-number check"},
  { ERROR_WOULD_OVERWRITE,	"use of pointer would exceed allocation" },
  { ERROR_IS_FREE,		"pointer is on free list" },
  
  /* allocation errors */
  { ERROR_BAD_SIZE,		"invalid allocation size" },
  { ERROR_TOO_BIG,		"largest maximum allocation size exceeded" },
  { ERROR_USER_NON_CONTIG,	"user allocated space contiguous block error"},
  { ERROR_ALLOC_FAILED,		"could not grow heap by allocating memory" },
  { ERROR_ALLOC_NONLINEAR,	"heap failed to produce linear address space"},
  { ERROR_BAD_SIZE_INFO,	"bad size in information structure" },
  { ERROR_EXTERNAL_HUGE,	"external sbrk too large, cannot be handled" },
  { ERROR_OVER_LIMIT,		"over user specified allocation limit" },
  
  /* free errors */
  { ERROR_NOT_ON_BLOCK,	 	"pointer is not on block boundary" },
  { ERROR_ALREADY_FREE,		"tried to free previously freed pointer" },
  { ERROR_NOT_START_USER,	"pointer does not start at user-alloc space" },
  { ERROR_NOT_USER,		"pointer does not point to user-alloc space" },
  { ERROR_BAD_FREE_LIST,	"inconsistency with free linked-list" },
  { ERROR_FREE_NON_CONTIG,	"free space contiguous block error" },
  { ERROR_BAD_FREE_MEM,		"bad basic-block mem pointer in free-list" },
  { ERROR_FREE_NON_BLANK,	"free space has been overwritten" },
  
  /* dblock errors */
#if 0
  { ERROR_BAD_DBLOCK_SIZE,	"bad divided-block chunk size" },
  { ERROR_BAD_DBLOCK_POINTER,	"bad divided-block pointer" },
  { ERROR_BAD_DBLOCK_MEM,     "bad basic-block mem pointer in dblock struct" },
  { ERROR_BAD_DBADMIN_POINTER,	"bad divided-block admin pointer" },
  { ERROR_BAD_DBADMIN_MAGIC,	"bad divided-block admin magic numbers" },
  { ERROR_BAD_DBADMIN_SLOT,	"bad divided-block chunk admin info struct" },
#endif
  
  /* administrative errors */
  { ERROR_BAD_ADMIN_P,		"admin structure pointer out of bounds" },
  { ERROR_BAD_ADMIN_LIST,	"bad admin structure list" },
  { ERROR_BAD_ADMIN_MAGIC,	"bad magic number in admin structure" },
  { ERROR_BAD_ADMIN_COUNT,	"bad basic-block count value in admin struct"},
  { ERROR_BAD_BLOCK_ADMIN_P,	"bad basic-block administration pointer" },
  { ERROR_BAD_BLOCK_ADMIN_C,	"bad basic-block administration counter" },
  
  /* heap check verification */
  { ERROR_BAD_BLOCK_ORDER,	"bad basic-block allocation order" },
  { ERROR_BAD_FLAG,		"basic-block has bad flag value" },
  
  /* memory table errors */
  { ERROR_TABLE_CORRUPT,	"internal memory table corruption" },
  { ERROR_ADDRESS_LIST,		"internal address list corruption" }
};

#endif /* ! __ERROR_VAL_H__ */
