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
 * $Id: error_val.h,v 1.1 1993/03/31 00:35:48 gray Exp $
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

/* allocation errors */
#define MALLOC_BAD_SIZE			12	/* bad bblock size value */
#define MALLOC_TOO_BIG			13	/* allocation too large */
#define MALLOC_USER_NON_CONTIG		14	/* user space contig error */
#define MALLOC_ALLOC_FAILED		15	/* could not get more space */
#define MALLOC_BAD_SIZE_INFO		16	/* info doesn't match size */

/* free errors */
#define MALLOC_NOT_ON_BLOCK		17	/* not on block boundary */
#define MALLOC_ALREADY_FREE		18	/* already in free list */
#define MALLOC_NOT_START_USER		19	/* not start of user alloc */
#define MALLOC_NOT_USER			20	/* not user allocated */
#define MALLOC_BAD_FREE_LIST		21	/* free-list mess-up */
#define MALLOC_FREE_NON_CONTIG		22	/* free space contig error */
#define MALLOC_BAD_FREE_MEM		23	/* bad memory pointer */
#define MALLOC_FREE_NON_BLANK		24	/* free space should be 0's */

/* dblock errors */
#define MALLOC_BAD_DBLOCK_SIZE		25	/* dblock bad size */
#define MALLOC_BAD_DBLOCK_POINTER	26	/* bad dblock pointer */
#define MALLOC_BAD_DBLOCK_MEM		27	/* bad memory pointer */
#define MALLOC_BAD_DBADMIN_POINTER	28	/* bad dblock admin pointer */
#define MALLOC_BAD_DBADMIN_MAGIC	29	/* bad dblock admin pointer */
#define MALLOC_BAD_DBADMIN_SLOT		30	/* bad dblock slot info */

/* administrative errors */
#define MALLOC_BAD_ADMINP		31	/* out of bounds */
#define MALLOC_BAD_ADMIN_LIST		32	/* out of bounds */
#define MALLOC_BAD_ADMIN_MAGIC		33	/* bad magic numbers */
#define MALLOC_BAD_ADMIN_COUNT		34	/* bad count number */
#define MALLOC_BAD_BLOCK_ADMINP		35	/* bblock adminp bad */
#define MALLOC_BAD_BLOCK_ADMINC		36	/* bblock adminp->count bad */

/* heap check verification */
#define MALLOC_BAD_BLOCK_ORDER		37	/* block allocation bad */
#define MALLOC_BAD_FLAG			38	/* bad basic-block flag */

#define IS_MALLOC_ERRNO(e)	((e) >= MALLOC_NO_ERROR && \
				 (e) <= MALLOC_BAD_FLAG)

#endif /* ! __ERROR_VAL_H__ */
