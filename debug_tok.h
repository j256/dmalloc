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
 * $Id: debug_tok.h,v 1.35 2003/05/13 16:37:44 gray Exp $
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
  unsigned long	at_value;		/* value for the item */
  char		*at_desc;		/* description string */
} attr_t;

static	attr_t		attributes[] = {
  { "none",		0,
      "no functionality" },
  
  { "log-stats",	DEBUG_LOG_STATS,	"log general statistics" },
  { "log-non-free",	DEBUG_LOG_NONFREE,	"log non-freed pointers" },
  { "log-known",	DEBUG_LOG_KNOWN,	"log only known non-freed" },
  { "log-trans",	DEBUG_LOG_TRANS,	"log memory transactions" },
  { "log-admin",	DEBUG_LOG_ADMIN,	"log administrative info" },
  { "log-blocks",	0,
    "Disabled because of new heap organization" },
  { "log-bad-space",	DEBUG_LOG_BAD_SPACE,	"dump space from bad pnt" },
  { "log-nonfree-space",DEBUG_LOG_NONFREE_SPACE,
    "dump space from non-freed pointers" },
  
  { "log-elapsed-time",	DEBUG_LOG_ELAPSED_TIME,
    "log elapsed-time for allocated pointer" },
  { "log-current-time",	DEBUG_LOG_CURRENT_TIME,
    "log current-time for allocated pointer" },
  
  { "check-fence",	DEBUG_CHECK_FENCE,	"check fence-post errors" },
  { "check-heap",	DEBUG_CHECK_HEAP,	"check heap adm structs" },
  { "check-lists",	DEBUG_CHECK_LISTS,	"check free lists" },
  { "check-blank",	DEBUG_CHECK_BLANK,
    "check mem overwritten by alloc-blank, free-blank" },
  { "check-funcs",	DEBUG_CHECK_FUNCS,	"check functions" },
  
  { "force-linear",	DEBUG_FORCE_LINEAR,
    "force heap space to be linear" },
  { "catch-signals",	DEBUG_CATCH_SIGNALS,
    "shutdown program on SIGHUP, SIGINT, SIGTERM" },
  { "realloc-copy",	DEBUG_REALLOC_COPY,	"copy all re-allocations" },
  { "free-blank",	DEBUG_FREE_BLANK,
    "overwrite freed memory space with BLANK_CHAR" },
  { "error-abort",	DEBUG_ERROR_ABORT,	"abort immediately on error" },
  { "alloc-blank",	DEBUG_ALLOC_BLANK,
    "overwrite newly alloced memory with BLANK_CHAR" },
  { "heap-check-map",	0,
    "Disbaled because of new heap organization" },
  { "print-messages",	DEBUG_PRINT_MESSAGES,	"write messages to stderr" },
  { "catch-null",	DEBUG_CATCH_NULL,      "abort if no memory available"},
  { "never-reuse",	DEBUG_NEVER_REUSE,	"never re-use freed memory" },
  { "error-dump",	DEBUG_ERROR_DUMP,
    "dump core on error, then continue" },
  { "error-free-null",	DEBUG_ERROR_FREE_NULL,
    "generate error when a 0L pointer is freed" },
  
  /*
   * The following are here for backwards compatibility
   */
  
  /* this is the default now and log-known is the opposite */
  { "log-unknown",	0,
    "Disabled -- this is the default now, log-known is opposite" },
  /* this is the default and force-linear is opposite */
  { "allow-nonlinear",	0,
    "Disabled -- this is the default now, force-linear is opposite" },
  /* this is the default and error-free-null opposite */
  { "allow-free-null",	0,
    "Disabled -- this is the default now, error-free-null is opposite" },

  { NULL }
};

#endif /* ! __DEBUG_TOK_H__ */
