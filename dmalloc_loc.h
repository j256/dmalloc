/*
 * local definitions for the user allocation level
 *
 * Copyright 1991 by the Antaire Corporation.
 * Please see the LICENSE file in this directory for license information
 *
 * Written by Gray Watson
 *
 * @(#)malloc_loc.h	1.2 GRAY@ANTAIRE.COM 11/26/91
 */

/* defaults if _alloc_line and _alloc_file could not be set */
#define DEFAULT_FILE		"unknown"
#define DEFAULT_LINE		0

/*
 * word boundaries for different machine types
 */
#if defined(i386)
#define WORD_BOUNDARY		4
#endif
#if defined(sparc)
#define WORD_BOUNDARY		8
#endif

/* fence post checking defines */
#define FENCE_BOTTOM		WORD_BOUNDARY
#define FENCE_TOP		sizeof(long)
#define FENCE_MAGIC_BASE	0x9EF1FE3
#define FENCE_MAGIC_TOP		0x127AFBE

/* error codes for fence checking */
#define FENCE_NOERROR		0
#define FENCE_OVER_ERROR	1
#define FENCE_UNDER_ERROR	(-1)
