/*
 * error and message routines
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
 */

/*
 * This file contains the routines needed for processing error codes
 * produced by the library.
 */

#include <fcntl.h>				/* for O_WRONLY, etc. */
#include <stdarg.h>				/* for message vsprintf */

/* for KILL_PROCESS define */
#if USE_ABORT == 0
#ifdef SIGNAL_INCLUDE
#include SIGNAL_INCLUDE				/* for kill signals */
#endif
#endif

/* for timeval type -- see conf.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
#endif

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "compat.h"
#include "debug_val.h"
#include "env.h"				/* for LOGPATH_INIT */
#include "error.h"
#include "error_val.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: error.c,v 1.58 1995/05/16 02:17:20 gray Exp $";
#endif

#define SECS_IN_HOUR	(60 * SECS_IN_MIN)
#define SECS_IN_MIN	60

/* external routines */
IMPORT	char		*_dmalloc_strerror(const int errnum);

/*
 * exported variables
 */
/* global debug flags that are set my DMALLOC_DEBUG environ variable */
EXPORT	long		_dmalloc_flags = 0;

/* global iteration counter for activities */
EXPORT	unsigned long	_dmalloc_iterc = 0;

/* overhead information storing when the library started up for elapsed time */
#if STORE_TIMEVAL
EXPORT	struct timeval	_dmalloc_start;
#else
EXPORT	long		_dmalloc_start = 0;
#endif
/*
 * print the time into local buffer which is returned
 */
EXPORT	char	*_dmalloc_ptime(
#if STORE_TIMEVAL
				const struct timeval * timevalp,
#else
				const long * timep,
#endif
				const char elapsed
				)
{
  static char	buf[64];
  long		hrs, mins, secs;
#if STORE_TIMEVAL
  long		usecs;
#endif
  
  buf[0] = NULLC;
  
#if STORE_TIMEVAL
  secs = timevalp->tv_sec;
  usecs = timevalp->tv_usec;
#else
  secs = *timep;
#endif
  
  if (elapsed || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)) {
#if STORE_TIMEVAL
    usecs -= _dmalloc_start.tv_usec;
    if (usecs < 0) {
      secs--;
      usecs = - usecs;
    }
    secs -= _dmalloc_start.tv_sec;
#else
    secs -= _dmalloc_start;
#endif
  }
  
  hrs = secs / SECS_IN_HOUR;
  mins = (secs / SECS_IN_MIN) % SECS_IN_HOUR;
  secs %= SECS_IN_MIN;
  
#if STORE_TIMEVAL
  (void)sprintf(buf, "%ld:%ld:%ld.%ld", hrs, mins, secs, usecs);
#else
  (void)sprintf(buf, "%ld:%ld:%ld", hrs, mins, secs);
#endif
  
  return buf;
}

/*
 * message writer with printf like arguments
 */
EXPORT	void	_dmalloc_message(const char * format, ...)
{
  static int	outfile = -1;
  char		str[1024], *strp = str;
  int		len;
  va_list	args;
  
  /* no logpath and no print then no workie */
  if (dmalloc_logpath == LOGPATH_INIT
      && ! BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_ERROR))
    return;
  
#if HAVE_TIME
  /* maybe dump a time stamp */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_STAMP)) {
    (void)sprintf(strp, "%ld: ", (long)time(NULL));
    for (; *strp != NULLC; strp++);
  }
#endif
  
#if LOG_ITERATION_COUNT
  /* add the iteration number */
  (void)sprintf(strp, "%lu: ", _dmalloc_iterc);
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
  if (dmalloc_logpath != LOGPATH_INIT) {
    /*
     * do we need to open the outfile?
     * it will be closed by _exit().  yeach.
     */
    if (outfile < 0) {
      outfile = open(dmalloc_logpath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
      if (outfile < 0) {
	(void)sprintf(str, "debug-malloc library: could not open '%s'\n",
		      dmalloc_logpath);
	(void)write(STDERR, str, strlen(str));
	/* disable log_path */
	dmalloc_logpath = LOGPATH_INIT;
	return;
      }
      
      /*
       * NOTE: this makes it go recursive once but it will never get
       * back here
       */
      _dmalloc_message("dmalloc_logfile '%s': flags = %#lx, addr = %#lx",
		       dmalloc_logpath, _dmalloc_flags, dmalloc_address);
      
#if STORE_TIMEVAL
      _dmalloc_message("starting time = %ld.%ld",
		       _dmalloc_start.tv_sec, _dmalloc_start.tv_usec);
#else
#if HAVE_TIME /* NOT STORE_TIME */
      _dmalloc_message("starting time = %ld", _dmalloc_start);
#endif
#endif
    }
    
    /* write str to the outfile */
    (void)write(outfile, str, len);
  }
  
  /* do we need to print the message? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_ERROR))
    (void)write(STDERR, str, len);
}

/*
 * kill the program because of an internal malloc error
 */
EXPORT	void	_dmalloc_die(const char silent)
{
  char	str[1024], *stop_str;
  
  if (! silent) {
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_ABORT))
      stop_str = "dumping";
    else
      stop_str = "halting";
    
    /* print a message that we are going down */
    (void)sprintf(str, "debug-malloc library: %s program, fatal error\n",
		  stop_str);
    (void)write(STDERR, str, strlen(str));
    if (dmalloc_errno != ERROR_NONE) {
      (void)sprintf(str, "   Error: %s (err %d)\n",
		    _dmalloc_strerror(dmalloc_errno), dmalloc_errno);
      (void)write(STDERR, str, strlen(str));
    }
  }
  
  /* do I need to drop core? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_ABORT)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_DUMP)) {
#if USE_ABORT
    abort();
#else
#ifdef KILL_PROCESS
    KILL_PROCESS;
#endif
#endif
  }
  
  /*
   * NOTE: this should not be exit() because fclose will free, etc
   */
  _exit(1);
}

/*
 * handler of error codes from procedure FUNC.  the procedure should
 * have set the errno already.
 */
EXPORT	void	dmalloc_error(const char * func)
{
  /* do we need to log or print the error? */
  if (dmalloc_logpath != NULL
      || BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_ERROR)) {
    
    /* default str value */
    if (func == NULL)
      func = "_malloc_error";
    
    /* print the malloc error message */
    _dmalloc_message("ERROR: %s: %s (err %d)",
		     func, _dmalloc_strerror(dmalloc_errno), dmalloc_errno);
  }
  
  /* do I need to abort? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_ABORT))
    _dmalloc_die(FALSE);
  
#if HAVE_FORK
  /* how about just drop core? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_DUMP)) {
    int		pid = fork();
    if (pid == 0)
      _dmalloc_die(TRUE);
  }
#endif
}
