/*
 * local definitions for the user allocation level
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * The author of the program may be contacted at gray.watson@antaire.com
 *
 * $Id: dmalloc_loc.h,v 1.10 1992/11/06 05:41:16 gray Exp $
 */

#ifndef __MALLOC_LOC_H__
#define __MALLOC_LOC_H__

/* should we NOT allow 0 length allocations? */
#define MALLOC_NO_ZERO_SIZE

/* defaults if _alloc_line and _alloc_file could not be set */
#define DEFAULT_FILE		"unknown"
#define DEFAULT_LINE		0

/* fence post checking defines */
#define FENCE_BOTTOM		WORD_BOUNDARY
#define FENCE_TOP		sizeof(long)
#define FENCE_OVERHEAD		(FENCE_BOTTOM + FENCE_TOP)
#define FENCE_MAGIC_BASE	0xC0C0AB1B
#define FENCE_MAGIC_TOP		0xFACADE69

/*
 * env variables
 */
#define ADDRESS_ENVIRON		"MALLOC_ADDRESS"
#define DEBUG_ENVIRON		"MALLOC_DEBUG"
#define INTERVAL_ENVIRON	"MALLOC_INTERVAL"
#define LOGFILE_ENVIRON		"MALLOC_LOGFILE"
#define START_ENVIRON		"MALLOC_START"

/******************************* useful defines ******************************/

/*
 * standard int return codes
 */
#ifndef ERROR

#undef	ERROR
#define	ERROR		(-1)

#undef	NOERROR
#define	NOERROR		0

#endif /* ! ERROR */

/*
 * generic constants
 */
#undef	NULL
#define NULL		0

#ifndef NULLC

#undef	NULLC
#define NULLC		'\0'

#undef	FALSE
#define FALSE		0

#undef	TRUE
#define TRUE		(! FALSE)

#endif /* ! NULLC */

/*
 * min/max macros
 *
 * WARNING: these use their arguments multiple times which may be bad
 */
#ifndef MAX

#undef MAX
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#undef MIN
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))

#endif /* ! MAX */

/*
 * bitflag tools for Variable and a Flag
 */
#ifndef BIT_FLAG

#undef BIT_FLAG
#define BIT_FLAG(x)		(1 << (x))
#undef BIT_SET
#define BIT_SET(v,f)		(v) |= (f)
#undef BIT_CLEAR
#define BIT_CLEAR(v,f)		(v) &= ~(f)
#undef BIT_IS_SET
#define BIT_IS_SET(v,f)		((v) & (f))

#endif /* ! BIT_FLAG */

#endif /* ! __MALLOC_LOC_H__ */
