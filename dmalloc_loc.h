/*
 * local definitions for the user allocation level
 *
 * Copyright 1991 by the Antaire Corporation.
 *
 * Written by Gray Watson
 *
 * $Id: dmalloc_loc.h,v 1.6 1992/09/26 19:18:10 gray Exp $
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

#endif /* ! __MALLOC_LOC_H__ */
