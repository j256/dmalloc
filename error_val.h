/*
 * global error codes for chunk allocation problems
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the dmalloc package.
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
 *
 * $Id: error_val.h,v 1.11 1994/09/10 23:27:49 gray Exp $
 */

#ifndef __ERROR_VAL_H__
#define __ERROR_VAL_H__

/*
 * malloc error codes
 */
#define ERROR_NONE			0	/* no error */

/* administrative errors */
#define ERROR_BAD_SETUP			1	/* bad setup value */
#define ERROR_IN_TWICE			2	/* in malloc domain twice */
#define ERROR_BAD_ERRNO			3	/* bad errno value */

/* pointer verification errors */
#define ERROR_IS_NULL			4	/* pointer is NULL */
#define ERROR_NOT_IN_HEAP		5	/* pointer is not in heap */
#define ERROR_NOT_FOUND			6	/* pointer not-found */
#define ERROR_IS_FOUND			7	/* found special pointer */
#define ERROR_BAD_FILEP			8	/* bad bblock file-name */
#define ERROR_BAD_LINE			9	/* bad bblock line-number */
#define ERROR_UNDER_FENCE		10	/* failed picket fence lower */
#define ERROR_OVER_FENCE		11	/* failed picket fence upper */
#define ERROR_WOULD_OVERWRITE		12	/* would overwrite fence */
#define ERROR_IS_FREE			13	/* free space should be 0's */

/* allocation errors */
#define ERROR_BAD_SIZE			14	/* bad bblock size value */
#define ERROR_TOO_BIG			15	/* allocation too large */
#define ERROR_USER_NON_CONTIG		16	/* user space not contiguous */
#define ERROR_ALLOC_FAILED		17	/* could not get more space */
#define ERROR_ALLOC_NONLINEAR		18	/* no linear address space */
#define ERROR_BAD_SIZE_INFO		19	/* info doesn't match size */

/* free errors */
#define ERROR_NOT_ON_BLOCK		20	/* not on block boundary */
#define ERROR_ALREADY_FREE		21	/* already in free list */
#define ERROR_NOT_START_USER		22	/* not start of user alloc */
#define ERROR_NOT_USER			23	/* not user allocated */
#define ERROR_BAD_FREE_LIST		24	/* free-list mess-up */
#define ERROR_FREE_NON_CONTIG		25	/* free space not contiguous */
#define ERROR_BAD_FREE_MEM		26	/* bad memory pointer */
#define ERROR_FREE_NON_BLANK		27	/* free space should be 0's */

/* dblock errors */
#define ERROR_BAD_DBLOCK_SIZE		28	/* dblock bad size */
#define ERROR_BAD_DBLOCK_POINTER	29	/* bad dblock pointer */
#define ERROR_BAD_DBLOCK_MEM		30	/* bad memory pointer */
#define ERROR_BAD_DBADMIN_POINTER	31	/* bad dblock admin pointer */
#define ERROR_BAD_DBADMIN_MAGIC		32	/* bad dblock admin pointer */
#define ERROR_BAD_DBADMIN_SLOT		33	/* bad dblock slot info */

/* administrative errors */
#define ERROR_BAD_ADMINP		34	/* admin value out of bounds */
#define ERROR_BAD_ADMIN_LIST		35	/* list pnt out of bounds */
#define ERROR_BAD_ADMIN_MAGIC		36	/* bad magic numbers */
#define ERROR_BAD_ADMIN_COUNT		37	/* bad count number */
#define ERROR_BAD_BLOCK_ADMINP		38	/* bblock adminp bad */
#define ERROR_BAD_BLOCK_ADMINC		39	/* bblock adminp->count bad */

/* heap check verification errors */
#define ERROR_BAD_BLOCK_ORDER		40	/* block allocation bad */
#define ERROR_BAD_FLAG			41	/* bad basic-block flag */

#define IS_MALLOC_ERRNO(e)	((e) >= ERROR_NONE && \
				 (e) <= ERROR_BAD_FLAG)

#endif /* ! __ERROR_VAL_H__ */
