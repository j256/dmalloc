/*
 * token attributes for the dmalloc program and _dmalloc_flags
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies, and that the name of Gray Watson not be used in advertising
 * or publicity pertaining to distribution of the document or software
 * without specific, written prior permission.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted at gray.watson@letters.com
 *
 * $Id: debug_tok.h,v 1.16 1995/05/16 01:58:12 gray Exp $
 */

#ifndef __DEBUG_TOK_H__
#define __DEBUG_TOK_H__

/*
 * NOTE: see debug_val.h for instructions about changes here
 */

#include "conf.h"			/* for DUMP_CONTINUE */
#include "dmalloc_loc.h"		/* for LOCAL */
#include "debug_val.h"			/* for debug codes */

typedef struct {
  char		*at_string;		/* attribute string */
  char		*at_short;		/* short string */
  unsigned long	at_value;		/* value for the item */
  char		*at_desc;		/* description string */
} attr_t;

LOCAL	attr_t		attributes[] = {
  { "none",		"nil",	0,
      "no functionality" },
  
  { "log-stats",	"lst",	DEBUG_LOG_STATS,
      "log general statistics" },
  { "log-non-free",	"lnf",	DEBUG_LOG_NONFREE,
      "log non-freed pointers" },
  { "log-thread-id",	"lti",	DEBUG_LOG_THREAD_ID,
      "log thread-id for allocated pointer" },
  { "log-trans",	"ltr",	DEBUG_LOG_TRANS,
      "log memory transactions" },
  { "log-stamp",	"lst",	DEBUG_LOG_STAMP,
      "add time-stamp to log" },
  { "log-admin",	"lad",	DEBUG_LOG_ADMIN,
      "log administrative info" },
  { "log-blocks",	"lbl",	DEBUG_LOG_BLOCKS,
      "log blocks when heap-map" },
  { "log-unknown",	"lun",	DEBUG_LOG_UNKNOWN,
      "log unknown non-freed" },
  { "log-bad-space",	"lbs",	DEBUG_LOG_BAD_SPACE,
      "dump space from bad pnt" },
  { "log-nonfree-space","lns",	DEBUG_LOG_NONFREE_SPACE,
      "dump space from non-freed pointers" },
  { "log-elapsed-time",	"let",	DEBUG_LOG_ELAPSED_TIME,
      "log elapsed-time for allocated pointer" },
  { "log-current-time",	"lct",	DEBUG_LOG_CURRENT_TIME,
      "log current-time for allocated pointer" },
  
  { "check-fence",	"cfe",	DEBUG_CHECK_FENCE,
      "check fence-post errors" },
  { "check-heap",	"che",	DEBUG_CHECK_HEAP,
      "check heap adm structs" },
  { "check-lists",	"cli",	DEBUG_CHECK_LISTS,
      "check free lists" },
  { "check-blank",	"cbl",	DEBUG_CHECK_BLANK,
      "check blanked memory" },
  /* NOTE: this should be after check-blank */
  { "check-free",	"cfr",	DEBUG_CHECK_BLANK,
      "PLEASE USE AND SEE CHECK-BLANK" },
  { "check-funcs",	"cfu",	DEBUG_CHECK_FUNCS,
      "check functions" },
  
  { "realloc-copy",	"rco",	DEBUG_REALLOC_COPY,
      "copy all re-allocations" },
  { "free-blank",	"fbl",	DEBUG_FREE_BLANK,
      "blank free'd memory" },
  { "error-abort",	"eab",	DEBUG_ERROR_ABORT,
      "abort immediately on error" },
  { "alloc-blank",	"abl",	DEBUG_ALLOC_BLANK,
      "blank newly alloced memory" },
  { "heap-check-map",	"hcm",	DEBUG_HEAP_CHECK_MAP,
      "log heap-map on heap-check" },
  { "print-error",	"per",	DEBUG_PRINT_ERROR,
      "print errors to stderr" },
  { "catch-null",	"cnu",	DEBUG_CATCH_NULL,
      "abort if no memory available" },
  { "never-reuse",	"nre",	DEBUG_NEVER_REUSE,
      "never re-use freed memory" },
  { "allow-nonlinear",	"ano",	DEBUG_ALLOW_NONLINEAR,
      "allow non-linear heap space"},
  { "allow-zero",	"aze",	DEBUG_ALLOW_ZERO,
      "allow allocs of 0 bytes, frees of NULL"},
#if DUMP_CONTINUE
  { "error-dump",	"edu",	DEBUG_ERROR_DUMP,
      "dump core on error and then continue"},
#endif
  
  { NULL }
};

#endif /* ! __DEBUG_TOK_H__ */
