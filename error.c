/*
 * error and message routines
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
 */

/*
 * This file contains the routines needed for processing error codes
 * produced by the library.
 */

#include <fcntl.h>				/* for O_WRONLY, etc. */
#include <signal.h>				/* for kill signals */
#include <stdarg.h>				/* for message vsprintf */

#define MALLOC_DEBUG_DISABLE

#include "malloc_dbg.h"
#include "conf.h"

#include "compat.h"
#include "debug_val.h"
#include "error.h"
#include "malloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: error.c,v 1.38 1994/05/30 21:36:34 gray Exp $";
#endif

/*
 * exported variables
 */
/* global debug flags that are set my MALLOC_DEBUG environ variable */
EXPORT	long		_malloc_flags = 0;

/* global iteration counter for activities */
EXPORT	unsigned long	_malloc_iterc = 0;

/*
 * message writer with printf like arguments
 */
EXPORT	void	_malloc_message(const char * format, ...)
{
  static int	outfile = -1;
  static char	str[1024];
  char		*strp = str;
  int		len;
  va_list	args;
  
  /* no logpath and no print then no workie */
  if (malloc_logpath == NULL
      && ! BIT_IS_SET(_malloc_flags, DEBUG_PRINT_ERROR))
    return;
  
  /* maybe dump a time stamp */
  if (BIT_IS_SET(_malloc_flags, DEBUG_LOG_STAMP)) {
    (void)sprintf(strp, "%ld: ", time(NULL));
    for (; *strp != NULLC; strp++);
  }
  
#if LOG_ITERATION_COUNT
  /* add the iteration number */
  (void)sprintf(strp, "%lu: ", _malloc_iterc);
  for (; *strp != NULLC; strp++);
#endif
  
  /*
   * NOTE: the following code, as well as the function definition
   * above, would need to be altered to conform to non-ANSI-C
   * specifications if necessary.
   */
  
  /* write the format + info into str */
  va_start(args, format);
  (void)vsprintf(strp, format, args);
  va_end(args);
  
  /* was it an empty format? */
  if (*strp == NULLC)
    return;
  
  /* find the length of str */
  for (; *strp != NULLC; strp++);
  
  /* tack on a '\n' if necessary */
  if (*(strp - 1) != '\n') {
    *strp++ = '\n';
    *strp = NULLC;
  }
  len = strp - str;
  
  /* do we need to log the message? */
  if (malloc_logpath != NULL) {
    /*
     * do we need to open the outfile?
     * it will be closed by _exit().  yeach.
     */
    if (outfile < 0) {
      outfile = open(malloc_logpath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (outfile < 0) {
	(void)sprintf(str, "%s:%d: could not open '%s'\n",
		      __FILE__, __LINE__, malloc_logpath);
	(void)write(STDERR, str, strlen(str));
	/* disable log_path */
	malloc_logpath = NULL;
	return;
      }
    }
    
    /* write str to the outfile */
    (void)write(outfile, str, len);
  }
  
  /* do we need to print the message? */
  if (BIT_IS_SET(_malloc_flags, DEBUG_PRINT_ERROR))
    (void)write(STDERR, str, len);
}

/*
 * kill the program because of an internal malloc error
 */
EXPORT	void	_malloc_die(void)
{
  /* do I need to drop core? */
  if (BIT_IS_SET(_malloc_flags, DEBUG_ERROR_ABORT))
    (void)kill(0, ERROR_SIGNAL);
  
  /*
   * NOTE: this should not be exit() because fclose will free, etc
   */
  _exit(1);
}

/*
 * handler of error codes from procedure FUNC.  the procedure should
 * have set the errno already.
 */
EXPORT	void	_malloc_error(const char * func)
{
  /* do we need to log or print the error? */
  if ((BIT_IS_SET(_malloc_flags, DEBUG_LOG_ERROR) && malloc_logpath != NULL)
      || BIT_IS_SET(_malloc_flags, DEBUG_PRINT_ERROR)) {
    
    /* default str value */
    if (func == NULL)
      func = "_malloc_error";
    
    /* print the malloc error message */
    _malloc_message("ERROR: %s: %s(%d)",
		    func, _malloc_strerror(malloc_errno), malloc_errno);
  }
  
  /* do I need to abort? */
  if (BIT_IS_SET(_malloc_flags, DEBUG_ERROR_ABORT))
    _malloc_die();
}
