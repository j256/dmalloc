/*
 * token attributes for the dmalloc program and _dmalloc_flags
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the dmalloc package.
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
 * $Id: debug_tok.h,v 1.10 1994/09/26 16:20:08 gray Exp $
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

LOCAL	attr_t		attributes[] = {
  { "none",		"nil",	0,
      "no functionality" },
  
  { "log-stats",	"lst",	DEBUG_LOG_STATS,
      "generally log statistics" },
  { "log-non-free",	"lnf",	DEBUG_LOG_NONFREE,
      "report non-freed pointers" },
  { "log-error",	"ler",	DEBUG_LOG_ERROR,
      "log error messages" },
  { "log-trans",	"ltr",	DEBUG_LOG_TRANS,
      "log memory transactions" },
  { "log-stamp",	"lst",	DEBUG_LOG_STAMP,
      "add time stamp to log" },
  { "log-admin",	"lad",	DEBUG_LOG_ADMIN,
      "log background admin info" },
  { "log-blocks",	"lbl",	DEBUG_LOG_BLOCKS,
      "log blocks when heap-map" },
  { "log-unknown",	"lun",	DEBUG_LOG_UNKNOWN,
      "report unknown non-freed" },
  { "log-bad-space",	"lbs",	DEBUG_LOG_BAD_SPACE,
      "dump space from bad pnt" },
  { "log-nonfree-space","lns",	DEBUG_LOG_NONFREE_SPACE,
      "dump space from non-freed pointers" },
  
  { "check-fence",	"cfe",	DEBUG_CHECK_FENCE,
      "check fence-post errors" },
  { "check-heap",	"che",	DEBUG_CHECK_HEAP,
      "examine heap adm structs" },
  { "check-lists",	"cli",	DEBUG_CHECK_LISTS,
      "check free lists" },
  { "check-free",	"cfr",	DEBUG_CHECK_BLANK,
      "check blanked memory" },
  { "check-funcs",	"cfu",	DEBUG_CHECK_FUNCS,
      "check functions" },
  
  { "realloc-copy",	"rco",	DEBUG_REALLOC_COPY,
      "copy all re-allocations" },
  { "free-blank",	"fbl",	DEBUG_FREE_BLANK,
      "write over free'd memory" },
  { "error-abort",	"eab",	DEBUG_ERROR_ABORT,
      "abort immediately on error" },
  { "alloc-blank",	"abl",	DEBUG_ALLOC_BLANK,
      "blank newly alloced memory" },
  { "heap-check-map",	"hcm",	DEBUG_HEAP_CHECK_MAP,
      "heap-map on heap-check" },
  { "print-error",	"per",	DEBUG_PRINT_ERROR,
      "print errors to stderr" },
  { "catch-null",	"cnu",	DEBUG_CATCH_NULL,
      "abort before return null" },
  { "never-reuse",	"nre",	DEBUG_NEVER_REUSE,
      "never re-use freed memory" },
  { "allow-nonlinear",	"ano",	DEBUG_ALLOW_NONLINEAR,
      "allow non-linear heap space"},
  
  { NULL }
};

#endif /* ! __DEBUG_TOK_H__ */
