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
#include "dbg_values.h"
#include "error.h"
#include "malloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: error.c,v 1.33 1993/11/23 09:04:03 gray Exp $";
#endif

/*
 * exported variables
 */
/* global debug flags that are set my MALLOC_DEBUG environ variable */
EXPORT	int		_malloc_flags = 0;

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
      && ! BIT_IS_SET(_malloc_flags, DEBUG_PRINT_PERROR))
    return;
  
  /* maybe dump a time stamp */
  if (BIT_IS_SET(_malloc_flags, DEBUG_LOG_STAMP)) {
    (void)sprintf(strp, "%ld: ", time(NULL));
    strp += strlen(strp);
  }
  
  /* write the format + info into str */
  va_start(args, format);
  (void)vsprintf(strp, format, args);
  va_end(args);
  
  /* find the length of str, if empty then return */
  len = strlen(str);
  if (len == 0)
    return;
  
  /* tack on a '\n' if necessary */
  if (str[len - 1] != '\n') {
    str[len++] = '\n';
    str[len] = NULLC;
  }
  
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
  if (BIT_IS_SET(_malloc_flags, DEBUG_PRINT_PERROR))
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
 * malloc version of perror of an error in STR
 */
EXPORT	void	_malloc_error(const char * func)
{
  /* do we need to log or print the error? */
  if ((BIT_IS_SET(_malloc_flags, DEBUG_LOG_PERROR) && malloc_logpath != NULL)
      || BIT_IS_SET(_malloc_flags, DEBUG_PRINT_PERROR)) {
    
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
