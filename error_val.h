/*
 * global error codes for chunk allocation problems
 *
 * Copyright 2020 by Gray Watson
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
 */

#ifndef __ERROR_VAL_H__
#define __ERROR_VAL_H__

/*
 * Dmalloc error codes.
 *
 * NOTE: these are here instead of error.h because the dmalloc utility
 * needs them as well as the dmalloc library.
 */
#define DMALLOC_ERROR_NONE		1	/* no error */
/* 2 is reserved for invalid error */

/* administrative errors */
#define DMALLOC_ERROR_BAD_SETUP		10	/* bad setup value */
#define DMALLOC_ERROR_IN_TWICE		11	/* in malloc domain twice */
/* 12 unused */
#define DMALLOC_ERROR_LOCK_NOT_CONFIG	13	/* thread locking not config */

/* pointer verification errors */
#define DMALLOC_ERROR_IS_NULL		20	/* pointer is NULL */
#define DMALLOC_ERROR_NOT_IN_HEAP	21	/* pointer is not in heap */
#define DMALLOC_ERROR_NOT_FOUND		22	/* pointer not-found */
#define DMALLOC_ERROR_IS_FOUND		23	/* found special pointer */
#define DMALLOC_ERROR_BAD_FILE		24	/* bad file-name */
#define DMALLOC_ERROR_BAD_LINE		25	/* bad line-number */
#define DMALLOC_ERROR_UNDER_FENCE	26	/* failed picket fence lower */
#define DMALLOC_ERROR_OVER_FENCE	27	/* failed picket fence upper */
#define DMALLOC_ERROR_WOULD_OVERWRITE	28	/* would overwrite fence */
/* 29 unused */
#define DMALLOC_ERROR_NOT_START_BLOCK	30	/* pointer not to start mem */

/* allocation errors */
#define DMALLOC_ERROR_BAD_SIZE		40	/* bad size value */
#define DMALLOC_ERROR_TOO_BIG		41	/* allocation too large */
/* 42 unused */
#define DMALLOC_ERROR_ALLOC_FAILED	43	/* could not get more space */
/* 44 unused */
#define DMALLOC_ERROR_OVER_LIMIT	45	/* over allocation limit */

/* free errors */
#define DMALLOC_ERROR_NOT_ON_BLOCK	60	/* not on block boundary */
#define DMALLOC_ERROR_ALREADY_FREE	61	/* already in free list */
/* 62-66 unused */
#define DMALLOC_ERROR_FREE_OVERWRITTEN	67	/* free space overwritten */

/* administrative errors */
#define DMALLOC_ERROR_ADMIN_LIST	70	/* list pnt out of bounds */
/* 71 unused */
#define DMALLOC_ERROR_ADDRESS_LIST	72	/* invalid address list */
#define DMALLOC_ERROR_SLOT_CORRUPT	73	/* memory slot corruption */

#define INVALID_ERROR		"errno value is not valid"

typedef struct {
  int		es_error;		/* error number */
  char		*es_string;		/* associated string */
} error_str_t;

/* string error codes which apply to error codes in error_val.h */
static	error_str_t	error_list[]
#ifdef __GNUC__
__attribute__ ((unused))
#endif
= {
  { DMALLOC_ERROR_NONE,			"no error" },
  
  /* administrative errors */
  { DMALLOC_ERROR_BAD_SETUP,		"dmalloc initialization and setup failed" },
  { DMALLOC_ERROR_IN_TWICE,		"dmalloc library has gone recursive" },
  { DMALLOC_ERROR_LOCK_NOT_CONFIG,	"dmalloc thread locking has not been configured" },
  
  /* pointer verification errors */
  { DMALLOC_ERROR_IS_NULL,		"pointer is null" },
  { DMALLOC_ERROR_NOT_IN_HEAP,		"pointer is not pointing to heap data space" },
  { DMALLOC_ERROR_NOT_FOUND,		"cannot locate pointer in heap" },
  { DMALLOC_ERROR_IS_FOUND,		"found pointer the user was looking for" },
  { DMALLOC_ERROR_BAD_FILE,		"possibly bad .c filename pointer" },
  { DMALLOC_ERROR_BAD_LINE,		"possibly bad .c file line-number" },
  { DMALLOC_ERROR_UNDER_FENCE,		"failed UNDER picket-fence magic-number check"},
  { DMALLOC_ERROR_OVER_FENCE,		"failed OVER picket-fence magic-number check"},
  { DMALLOC_ERROR_WOULD_OVERWRITE,	"use of pointer would exceed allocation" },
  { DMALLOC_ERROR_NOT_START_BLOCK,	"pointer is not to start of memory block" },
  
  /* allocation errors */
  { DMALLOC_ERROR_BAD_SIZE,		"invalid allocation size" },
  { DMALLOC_ERROR_TOO_BIG,		"largest maximum allocation size exceeded" },
  { DMALLOC_ERROR_ALLOC_FAILED,		"could not grow heap by allocating memory" },
  { DMALLOC_ERROR_OVER_LIMIT,		"over user specified allocation limit" },
  
  /* free errors */
  { DMALLOC_ERROR_NOT_ON_BLOCK,		"pointer is not on block boundary" },
  { DMALLOC_ERROR_ALREADY_FREE,		"tried to free previously freed pointer" },
  { DMALLOC_ERROR_FREE_OVERWRITTEN,	"free space has been overwritten" },
  
  /* administrative errors */
  { DMALLOC_ERROR_ADMIN_LIST,		"dmalloc bad admin structure list" },
  { DMALLOC_ERROR_ADDRESS_LIST,		"dmalloc internal address list corruption" },
  { DMALLOC_ERROR_SLOT_CORRUPT,		"dmalloc internal memory slot corruption" },
  
  { 0 }
};

#endif /* ! __DMALLOC_ERROR_VAL_H__ */
