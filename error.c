/*
 * error and message routines
 *
 * Copyright 1992 by Gray Watson and the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with this library (see COPYING-LIB); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 * The author of the program may be contacted at gray.watson@antaire.com
 */

#include <fcntl.h>				/* for O_WRONLY, etc. */
#include <signal.h>				/* for kill signals */
#include <stdarg.h>				/* for message vsprintf */

#define ERROR_MAIN

#include "malloc.h"
#include "malloc_loc.h"

#include "chunk.h"
#include "compat.h"
#include "conf.h"
#include "dbg_values.h"
#include "error.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: error.c,v 1.15 1992/12/22 18:01:34 gray Exp $";
#endif

/*
 * exported variables
 */
/* global debug flags that are set my MALLOC_DEBUG environ variable */
EXPORT	int		_malloc_debug = 0;

/*
 * message writer with printf like arguments
 */
EXPORT	void	_malloc_message(char * format, ...)
{
  static int	outfile = -1;
  int		len;
  char		str[1024];
  va_list	args;
  
  /* no logpath then no workie */
  if (malloc_logpath == NULL
      && ! BIT_IS_SET(_malloc_debug, DEBUG_PRINT_PERROR))
    return;
  
  /* write the format + info into str */
  va_start(args, format);
  (void)vsprintf(str, format, args);
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
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_PERROR) && malloc_logpath != NULL) {
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
	exit(1);
      }
    }
    
    /* write str to the outfile */
    (void)write(outfile, str, len);
  }
  
  /* do we need to print the message? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_PRINT_PERROR)) {
    (void)write(STDERR, str, strlen(str));
  }
}

/*
 * kill the program because of an internal malloc error
 */
EXPORT	void	_malloc_die(void)
{
  /* do I need to drop core? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_ERROR_ABORT))
    (void)kill(getpid(), SIGABRT);
  
  /*
   * NOTE: this should not be exit() because fclose will free, etc
   */
  _exit(1);
}

/*
 * malloc version of perror of an error in STR
 */
EXPORT	void	_malloc_perror(char * str)
{
  /* do we need to log or print the error? */
  if ((BIT_IS_SET(_malloc_debug, DEBUG_LOG_PERROR) && malloc_logpath != NULL)
      || BIT_IS_SET(_malloc_debug, DEBUG_PRINT_PERROR)) {
    
    /* default str value */
    if (str == NULL)
      str = "malloc_perror";
    
    /* print the malloc error message */
    _malloc_message("ERROR: %s: %s(%d)",
		    str, malloc_strerror(malloc_errno), malloc_errno);
  }
  
  /* do I need to abort? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_ERROR_ABORT))
    _malloc_die();
}
