/*
 * local definitions for the user allocation level
 *
 * Copyright 1991 by the Antaire Corporation.
 *
 * $Id: dmalloc_loc.h,v 1.8 1992/10/21 07:34:28 gray Exp $
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
