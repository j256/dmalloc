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

#include <fcntl.h>				/* for O_WRONLY */
#include <signal.h>				/* for kill signals */
#include <stdio.h>				/* for STDERR */
#include <stdarg.h>				/* for message vsprintf */

#define ERROR_MAIN

#include "malloc.h"
#include "malloc_loc.h"

#include "chunk.h"
#include "compat.h"
#include "conf.h"
#include "debug_values.h"
#include "error.h"

LOCAL	char	*rcs_id =
  "$Id: error.c,v 1.8 1992/11/11 23:14:50 gray Exp $";

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
  if (malloc_logpath == NULL)
    return;
  
  /*
   * do we need to open the outfile?
   * this will be closed by exit(0).  yeach.
   */
  if (outfile < 0
      && (outfile = open(malloc_logpath, O_WRONLY | O_CREAT | O_TRUNC, 0666)) <
      0) {
    (void)fprintf(stderr, "%s:%d: could not open '%s': ",
		  __FILE__, __LINE__, malloc_logpath);
    (void)perror("");
    exit(1);
  }
  
  /* write the format + info into str */
  va_start(args, format);
  (void)vsprintf(str, format, args);
  va_end(args);
  
  /* find the length of str, if empty then return */
  if ((len = strlen(str)) == 0)
    return;
  
  /* tack on a '\n' if necessary */
  if (str[len - 1] != '\n') {
    str[len] = '\n';
    str[++len] = '\0';
  }
  
  /* write str to the outfile */
  (void)write(outfile, str, len);
}

/*
 * kill the program because of an internal malloc error
 */
EXPORT	void	_malloc_die(void)
{
  /* do I need to drop core? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_ERROR_ABORT))
    (void)kill(getpid(), SIGABRT);
  
  /* oh well, just die */
  exit(1);
}

/*
 * malloc version of perror of an error in STR
 */
EXPORT	void	_malloc_perror(char * str)
{
  /* if debug level is not high enough, return */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_PERROR)) {
    
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
