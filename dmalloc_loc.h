/*
 * local definitions for the user allocation level
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
 * $Id: dmalloc_loc.h,v 1.33 1995/03/03 17:38:06 gray Exp $
 */

#ifndef __DMALLOC_LOC_H__
#define __DMALLOC_LOC_H__

#include "conf.h"			/* for HAVE_BCMP and BASIC_BLOCK */

/*
 * env variable(s)
 */
#define OPTIONS_ENVIRON		"DMALLOC_OPTIONS"

/******************************* useful defines ******************************/

/*
 * global variable and procedure scoping for code readability
 */
#undef	EXPORT
#define	EXPORT

#undef	IMPORT
#define	IMPORT		extern

#undef	LOCAL
#define	LOCAL		static

/*
 * standard int return codes
 */
#undef	ERROR
#define	ERROR		(-1)

#undef	NOERROR
#define	NOERROR		0

/*
 * generic constants
 */
/* special consideration for NULL.  some compilers bitch if I redefine it */
#ifndef NULL
#define NULL		0L
#endif

#undef	NULLC
#define NULLC		'\0'

#undef	FALSE
#define FALSE		0

#undef	TRUE
#define TRUE		(! FALSE)

/*
 * standard i/o file descriptors
 */
#undef	STDIN
#define	STDIN		0		/* fileno(stdin) */

#undef	STDOUT
#define	STDOUT		1		/* fileno(stdout) */

#undef	STDERR
#define	STDERR		2		/* fileno(stderr) */

/*
 * min/max macros
 *
 * WARNING: these use their arguments multiple times which may be bad
 */
#undef MAX
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#undef MIN
#define MIN(a,b)	(((a) < (b)) ? (a) : (b))

/*
 * bitflag tools for Variable and a Flag
 */
#undef BIT_FLAG
#define BIT_FLAG(x)		(1 << (x))
#undef BIT_SET
#define BIT_SET(v,f)		(v) |= (f)
#undef BIT_CLEAR
#define BIT_CLEAR(v,f)		(v) &= ~(f)
#undef BIT_IS_SET
#define BIT_IS_SET(v,f)		((v) & (f))

/*
 * global malloc defines
 */
#define BLOCK_SIZE		(1 << BASIC_BLOCK)	/* size of a block */

#endif /* ! __DMALLOC_LOC_H__ */
