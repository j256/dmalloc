/*
 * local definitions for the user allocation level
 *
 * Copyright 1991 by the Antaire Corporation.
 *
 * Written by Gray Watson
 *
 * $Id: dmalloc_loc.h,v 1.4 1992/09/04 20:50:39 gray Exp $
 */

/* should we NOT allow 0 length allocations? */
#define MALLOC_NO_ZERO_SIZE

/* defaults if _alloc_line and _alloc_file could not be set */
#define DEFAULT_FILE		"unknown"
#define DEFAULT_LINE		0

/* fence post checking defines */
#define FENCE_BOTTOM		WORD_BOUNDARY
#define FENCE_TOP		sizeof(long)
#define FENCE_OVERHEAD		(FENCE_BOTTOM + FENCE_TOP)
#define FENCE_MAGIC_BASE	0x9EF1FE3
#define FENCE_MAGIC_TOP		0x127AFBE
