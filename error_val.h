/*
 * global error codes for chunk allocation problems
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
 * The author may be contacted via http://dmalloc.com/
 *
 * $Id: error_val.h,v 1.24 2000/03/21 01:34:56 gray Exp $
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
#define ERROR_BAD_DBLOCK_SIZE		80	/* dblock bad size */
#define ERROR_BAD_DBLOCK_POINTER	81	/* bad dblock pointer */
#define ERROR_BAD_DBLOCK_MEM		82	/* bad memory pointer */
#define ERROR_BAD_DBADMIN_POINTER	83	/* bad dblock admin pointer */
#define ERROR_BAD_DBADMIN_MAGIC		84	/* bad dblock admin pointer */
#define ERROR_BAD_DBADMIN_SLOT		85	/* bad dblock slot info */

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

/*
 * NOTE: you must update the IS_MALLOC_ERRNO below if you add errors
 */
#define IS_MALLOC_ERRNO(e)	((e) >= ERROR_NONE && \
				 (e) <= ERROR_TABLE_CORRUPT)

#endif /* ! __ERROR_VAL_H__ */
