/*
 * error routines for the malloc system.
 *
 * Copyright 1992 by the Antaire Corporation
 */

#include <fcntl.h>				/* for O_WRONLY */
#include <signal.h>				/* for kill signals */
#include <stdio.h>				/* for STDERR */
#include <stdarg.h>				/* for message vsprintf */

#define ERROR_MAIN

#if defined(ANTAIRE)
#include "useful.h"
#endif

#include "chunk.h"
#include "error.h"
#include "malloc.h"
#include "malloc_dbg.h"
#include "malloc_errors.h"

RCS_ID("$Id: error.c,v 1.2 1992/10/22 04:46:25 gray Exp $");

/*
 * message writer for passing in args lists, like vprintf
 */
EXPORT	void	_malloc_vmessage(char * format, va_list args)
{
  static int	outfile = -1;
  int		len;
  char		str[1024];
  
  /* no logpath then no workie */
  if (malloc_logpath == NULL)
    return;
  
  /* do we need to open the outfile? */
  if (outfile < 0 &&
      (outfile = open(malloc_logpath, O_WRONLY | O_CREAT | O_TRUNC, 0666)) <
      0) {
    (void)fprintf(stderr, "%s:%d: could not open '%s': ",
		  __FILE__, __LINE__, malloc_logpath);
    (void)perror("");
    exit(1);
  }
  
  /* write the format + info into str */
  (void)vsprintf(str, format, args);
  
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
 * message writter with printf like arguments
 */
EXPORT	void	_malloc_message(char * format, ...)
{
  va_list	args;
  
  /* write the format + info into str */
  va_start(args, format);
  _malloc_vmessage(format, args);
  va_end(args);
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
		    str, malloc_errlist[_malloc_errno], _malloc_errno);
  }
  
  /* do I need to abort? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_ERROR_ABORT))
    _malloc_die();
}
