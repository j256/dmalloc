/*
 * token attributes for the malloc_dbg program and _malloc_flags
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
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
 * $Id: debug_tok.h,v 1.5 1994/04/08 21:13:54 gray Exp $
 */

#ifndef __DEBUG_TOK_H__
#define __DEBUG_TOK_H__

/*
 * NOTE: see debug_val.h for instructions about changes here
 */

#include "malloc_loc.h"			/* for LOCAL */
#include "debug_val.h"			/* for debug codes */

typedef struct {
  char		*at_string;		/* attribute string */
  int		at_value;		/* value for the item*/
  char		*at_desc;		/* description string */
} attr_t;

LOCAL	attr_t		attributes[] = {
  { "none",		0,			"no functionality" },
  
  { "log-stats",	DEBUG_LOG_STATS,	"generally log statistics" },
  { "log-non-free",	DEBUG_LOG_NONFREE,	"report non-freed pointers" },
  { "log-error",	DEBUG_LOG_ERROR,	"log error messages" },
  { "log-trans",	DEBUG_LOG_TRANS,	"log memory transactions" },
  { "log-stamp",	DEBUG_LOG_STAMP,	"add time stamp to log" },
  { "log-admin",	DEBUG_LOG_ADMIN,	"log background admin info" },
  { "log-blocks",	DEBUG_LOG_BLOCKS,	"log blocks when heap-map" },
  { "log-unknown",	DEBUG_LOG_UNKNOWN,	"report unknown non-freed" },
  { "log-bad-space",	DEBUG_LOG_BAD_SPACE,	"dump space from bad pnt" },
  { "log-nonfree-space",DEBUG_LOG_NONFREE_SPACE,"dump space from non-freed pointers" },
  
  { "check-fence",	DEBUG_CHECK_FENCE,	"check fence-post errors" },
  { "check-heap",	DEBUG_CHECK_HEAP,	"examine heap adm structs" },
  { "check-lists",	DEBUG_CHECK_LISTS,	"check free lists" },
  { "check-free",	DEBUG_CHECK_BLANK,	"check blanked memory" },
  { "check-funcs",	DEBUG_CHECK_FUNCS,	"check functions" },
  
  { "realloc-copy",	DEBUG_REALLOC_COPY,	"copy all re-allocations" },
  { "free-blank",	DEBUG_FREE_BLANK,	"write over free'd memory" },
  { "error-abort",	DEBUG_ERROR_ABORT,	"abort immediately on error" },
  { "alloc-blank",	DEBUG_ALLOC_BLANK,	"blank newly alloced memory" },
  { "heap-check-map",	DEBUG_HEAP_CHECK_MAP,	"heap-map on heap-check" },
  { "print-error",	DEBUG_PRINT_ERROR,	"print errors to stderr" },
  { "catch-null",	DEBUG_CATCH_NULL,	"abort before return null" },
  { "never-reuse",	DEBUG_NEVER_REUSE,	"never re-use freed memory" },
  { "allow-nonlinear",	DEBUG_ALLOW_NONLINEAR,	"allow non-linear heap space"},
  
  { NULL }
};

#endif /* ! __DEBUG_TOK_H__ */
