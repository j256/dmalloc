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
 * $Id: dmalloc_loc.h,v 1.9 1992/11/06 01:14:05 gray Exp $
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

#endif /* ! __MALLOC_LOC_H__ */
