/*
 * local definitions for the user allocation level
 *
 * Copyright 1991 by the Antaire Corporation.
 *
 * Written by Gray Watson
 *
 * $Id: dmalloc_loc.h,v 1.3 1992/09/04 20:46:47 gray Exp $
 */

/* should we NOT allow 0 length allocations? */
#define MALLOC_NO_ZERO_SIZE

/* defaults if _alloc_line and _alloc_file could not be set */
#define DEFAULT_FILE		"unknown"
#define DEFAULT_LINE		0

/* fence post checking defines */
#define FENCE_BOTTOM		WORD_BOUNDARY
#define FENCE_TOP		sizeof(long)
#define FENCE_MAGIC_BASE	0x9EF1FE3
#define FENCE_MAGIC_TOP		0x127AFBE

/* debug level boundaries */
#define MALLOC_DEBUG_OFF	0		/* no debugging */
#define MALLOC_DEBUG_HIGHEST	6		/* most debugging */
