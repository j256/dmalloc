/*
 * Token attributes for the dmalloc program and _dmalloc_flags
 *
 * Copyright 2000 by Gray Watson
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
 * The author may be contacted via http://dmalloc.com/
 *
 * $Id: debug_tok.h,v 1.33 2000/11/07 17:03:51 gray Exp $
 */

#ifndef __DEBUG_TOK_H__
#define __DEBUG_TOK_H__

/*
 * NOTE: see debug_val.h for instructions about changes here
 */

#include "dmalloc_loc.h"		/* for LOCAL */
#include "debug_val.h"			/* for debug codes */

typedef struct {
  char		*at_string;		/* attribute string */
  char		*at_short;		/* short string */
  unsigned long	at_value;		/* value for the item */
  char		*at_desc;		/* description string */
} attr_t;

static	attr_t		attributes[] = {
  { "none",		"nil",	0,
      "no functionality" },
  
  { "log-stats",	"lst",	DEBUG_LOG_STATS,
      "log general statistics" },
  { "log-non-free",	"lnf",	DEBUG_LOG_NONFREE,
      "log non-freed pointers" },
  { "log-known",	"lkn",	DEBUG_LOG_KNOWN,
      "log only known non-freed" },
  { "log-trans",	"ltr",	DEBUG_LOG_TRANS,
      "log memory transactions" },
  { "log-admin",	"lad",	DEBUG_LOG_ADMIN,
      "log administrative info" },
  { "log-blocks",	"lbl",	DEBUG_LOG_BLOCKS,
      "log blocks when heap-map" },
  /* this is the default now and log-known is the opposite */
  { "log-unknown",	"lun",	0,
      "Disabled token -- this is the default now" },
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
      "check mem overwritten by alloc-blank, free-blank" },
  { "check-funcs",	"cfu",	DEBUG_CHECK_FUNCS,
      "check functions" },
  
  { "force-linear",	"fli",	DEBUG_FORCE_LINEAR,
      "force heap space to be linear" },
  { "catch-signals",	"csi",	DEBUG_CATCH_SIGNALS,
      "shutdown program on SIGHUP, SIGINT, SIGTERM" },
  { "realloc-copy",	"rco",	DEBUG_REALLOC_COPY,
      "copy all re-allocations" },
  { "free-blank",	"fbl",	DEBUG_FREE_BLANK,
      "overwrite freed memory space with BLANK_CHAR" },
  { "error-abort",	"eab",	DEBUG_ERROR_ABORT,
      "abort immediately on error" },
  { "alloc-blank",	"abl",	DEBUG_ALLOC_BLANK,
      "overwrite newly alloced memory with BLANK_CHAR" },
  { "heap-check-map",	"hcm",	DEBUG_HEAP_CHECK_MAP,
      "log heap-map on heap-check" },
  { "print-messages",	"pme",	DEBUG_PRINT_MESSAGES,
      "write messages to stderr" },
  { "catch-null",	"cnu",	DEBUG_CATCH_NULL,
      "abort if no memory available" },
  { "never-reuse",	"nre",	DEBUG_NEVER_REUSE,
      "never re-use freed memory" },
  /* does nothing anymore since this is the default */
  { "allow-nonlinear",	"ano",	0,
      "allow non-linear heap space" },
  { "allow-free-null",	"afn",	DEBUG_ALLOW_FREE_NULL,
      "allow the frees of NULL pointers" },
  { "error-dump",	"edu",	DEBUG_ERROR_DUMP,
      "dump core on error and then continue" },
  
  { NULL }
};

#endif /* ! __DEBUG_TOK_H__ */
