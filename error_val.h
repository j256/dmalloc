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
 * $Id: error_val.h,v 1.30 2003/05/16 00:09:42 gray Exp $
 */

#ifndef __ERROR_VAL_H__
#define __ERROR_VAL_H__

/*
 * Dmalloc error codes.
 *
 * NOTE: these are here instead of error.h because the dmalloc utility
 * needs them as well as the dmalloc library.
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
#define ERROR_BAD_FILE			24	/* bad file-name */
#define ERROR_BAD_LINE			25	/* bad line-number */
#define ERROR_UNDER_FENCE		26	/* failed picket fence lower */
#define ERROR_OVER_FENCE		27	/* failed picket fence upper */
#define ERROR_WOULD_OVERWRITE		28	/* would overwrite fence */
#define ERROR_IS_FREE			29	/* pointer is already free */

/* allocation errors */
#define ERROR_BAD_SIZE			40	/* bad size value */
#define ERROR_TOO_BIG			41	/* allocation too large */
#define ERROR_USER_NON_CONTIG		42	/* user space not contiguous */
#define ERROR_ALLOC_FAILED		43	/* could not get more space */
#define ERROR_ALLOC_NONLINEAR		44	/* no linear address space */
#define ERROR_OVER_LIMIT		45	/* over allocation limit */

/* free errors */
#define ERROR_NOT_ON_BLOCK		60	/* not on block boundary */
#define ERROR_ALREADY_FREE		61	/* already in free list */
#define ERROR_NOT_USER			63	/* not user allocated */
#define ERROR_FREE_LIST			64	/* free-list mess-up */
#define ERROR_FREE_NON_CONTIG		65	/* free space not contiguous */
#define ERROR_FREE_MEM			66	/* bad memory pointer */
#define ERROR_FREE_NON_BLANK		67	/* free space should be 0's */

/* administrative errors */
#define ERROR_ADMIN_LIST		70	/* list pnt out of bounds */
#define ERROR_TABLE_CORRUPT		71	/* memory table corruption */
#define ERROR_ADDRESS_LIST		72	/* invalid address list */
#define ERROR_SLOT_CORRUPT		73	/* memory slot corruption */

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
  { ERROR_BAD_FILE,		"possibly bad .c filename pointer" },
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
  { ERROR_OVER_LIMIT,		"over user specified allocation limit" },
  
  /* free errors */
  { ERROR_NOT_ON_BLOCK,	 	"pointer is not on block boundary" },
  { ERROR_ALREADY_FREE,		"tried to free previously freed pointer" },
  { ERROR_NOT_USER,		"pointer does not point to user-alloc space" },
  { ERROR_FREE_LIST,		"inconsistency with free linked-list" },
  { ERROR_FREE_NON_CONTIG,	"free space contiguous block error" },
  { ERROR_FREE_MEM,		"bad basic-block mem pointer in free-list" },
  { ERROR_FREE_NON_BLANK,	"free space has been overwritten" },
  
  /* administrative errors */
  { ERROR_ADMIN_LIST,		"bad admin structure list" },
  { ERROR_TABLE_CORRUPT,	"internal memory table corruption" },
  { ERROR_ADDRESS_LIST,		"internal address list corruption" },
  { ERROR_SLOT_CORRUPT,		"internal memory slot corruption" },
  
  { 0 }
};

#endif /* ! __ERROR_VAL_H__ */
