/*
 * global error codes for chunk allocation problems
 *
 * Copyright 1992 by Gray Watson and the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (see COPYING-LIB); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * The author of the program may be contacted at gray.watson@antaire.com
 *
 * $Id: error_val.h,v 1.3 1993/04/15 21:58:14 gray Exp $
 */

#ifndef __ERROR_VAL_H__
#define __ERROR_VAL_H__

/*
 * malloc error codes
 */
#define MALLOC_NO_ERROR			0	/* no error */

/* administrative errors */
#define MALLOC_BAD_SETUP		1	/* bad setup value */
#define MALLOC_IN_TWICE			2	/* in malloc domain twice */
#define MALLOC_BAD_ERRNO		3	/* bad errno value */

/* pointer verification errors */
#define MALLOC_POINTER_NULL		4	/* pointer is not in heap */
#define MALLOC_POINTER_NOT_IN_HEAP	5	/* pointer is not in heap */
#define MALLOC_POINTER_NOT_FOUND	6	/* pointer not-found */
#define MALLOC_POINTER_FOUND		7	/* found special pointer */
#define MALLOC_BAD_FILEP		8	/* bad bblock file-name */
#define MALLOC_BAD_LINE			9	/* bad bblock line-number */
#define MALLOC_UNDER_FENCE		10	/* failed picket fence lower */
#define MALLOC_OVER_FENCE		11	/* failed picket fence upper */
#define MALLOC_WOULD_OVERWRITE		12	/* would overwrite fence */

/* allocation errors */
#define MALLOC_BAD_SIZE			13	/* bad bblock size value */
#define MALLOC_TOO_BIG			14	/* allocation too large */
#define MALLOC_USER_NON_CONTIG		15	/* user space contig error */
#define MALLOC_ALLOC_FAILED		16	/* could not get more space */
#define MALLOC_ALLOC_NONLINEAR		17	/* no linear address space */
#define MALLOC_BAD_SIZE_INFO		18	/* info doesn't match size */

/* free errors */
#define MALLOC_NOT_ON_BLOCK		19	/* not on block boundary */
#define MALLOC_ALREADY_FREE		20	/* already in free list */
#define MALLOC_NOT_START_USER		21	/* not start of user alloc */
#define MALLOC_NOT_USER			22	/* not user allocated */
#define MALLOC_BAD_FREE_LIST		23	/* free-list mess-up */
#define MALLOC_FREE_NON_CONTIG		24	/* free space contig error */
#define MALLOC_BAD_FREE_MEM		25	/* bad memory pointer */
#define MALLOC_FREE_NON_BLANK		26	/* free space should be 0's */

/* dblock errors */
#define MALLOC_BAD_DBLOCK_SIZE		27	/* dblock bad size */
#define MALLOC_BAD_DBLOCK_POINTER	28	/* bad dblock pointer */
#define MALLOC_BAD_DBLOCK_MEM		29	/* bad memory pointer */
#define MALLOC_BAD_DBADMIN_POINTER	30	/* bad dblock admin pointer */
#define MALLOC_BAD_DBADMIN_MAGIC	31	/* bad dblock admin pointer */
#define MALLOC_BAD_DBADMIN_SLOT		32	/* bad dblock slot info */

/* administrative errors */
#define MALLOC_BAD_ADMINP		33	/* out of bounds */
#define MALLOC_BAD_ADMIN_LIST		34	/* out of bounds */
#define MALLOC_BAD_ADMIN_MAGIC		35	/* bad magic numbers */
#define MALLOC_BAD_ADMIN_COUNT		36	/* bad count number */
#define MALLOC_BAD_BLOCK_ADMINP		37	/* bblock adminp bad */
#define MALLOC_BAD_BLOCK_ADMINC		38	/* bblock adminp->count bad */

/* heap check verification */
#define MALLOC_BAD_BLOCK_ORDER		39	/* block allocation bad */
#define MALLOC_BAD_FLAG			40	/* bad basic-block flag */

#define IS_MALLOC_ERRNO(e)	((e) >= MALLOC_NO_ERROR && \
				 (e) <= MALLOC_BAD_FLAG)

#endif /* ! __ERROR_VAL_H__ */
