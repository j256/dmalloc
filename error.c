/*
 * Error and message routines
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the dmalloc package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * NON-COMMERCIAL purpose and without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies, and that the name of Gray Watson not be used in
 * advertising or publicity pertaining to distribution of the document
 * or software without specific, written prior permission.
 *
 * Please see the PERMISSIONS file or contact the author for information
 * about commercial licenses.
 *
 * Gray Watson makes no representations about the suitability of the
 * software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted via http://www.letters.com/~gray/
 *
 * $Id: error.c,v 1.79 1999/03/03 16:04:57 gray Exp $
 */

/*
 * This file contains the routines needed for processing error codes
 * produced by the library.
 */

#include <fcntl.h>				/* for O_WRONLY, etc. */
#include <stdarg.h>				/* for message vsprintf */
#include <stdio.h>				/* for sprintf */

#if HAVE_UNISTD_H
# include <unistd.h>				/* for write */
#endif

#include "conf.h"				/* up here for _INCLUDE */

/* for KILL_PROCESS define */
#if USE_ABORT == 0
#ifdef KILL_INCLUDE
#include KILL_INCLUDE				/* for kill signals */
#endif
#endif

/* for time type -- see settings.h */
#if STORE_TIME
#ifdef TIME_INCLUDE
#include TIME_INCLUDE
#endif
#endif

/* for timeval type -- see settings.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
#endif

#define DMALLOC_DISABLE

#include "dmalloc.h"

#include "compat.h"
#include "debug_val.h"
#include "env.h"				/* for LOGPATH_INIT */
#include "error.h"
#include "error_val.h"
#include "dmalloc_loc.h"
#include "version.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: error.c,v 1.79 1999/03/03 16:04:57 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: error.c,v 1.79 1999/03/03 16:04:57 gray Exp $";
#endif
#endif

#define SECS_IN_HOUR	(MINS_IN_HOUR * SECS_IN_MIN)
#define MINS_IN_HOUR	60
#define SECS_IN_MIN	60

/* external routines */
extern	char		*_dmalloc_strerror(const int errnum);

/*
 * exported variables
 */
/* logfile for dumping dmalloc info, DMALLOC_LOGFILE env var overrides this */
char		*dmalloc_logpath = LOGPATH_INIT;
/* address to look for.  when discovered call dmalloc_error() */
DMALLOC_PNT	dmalloc_address = ADDRESS_INIT;

/* global debug flags that are set my DMALLOC_DEBUG environ variable */
unsigned int	_dmalloc_flags = 0;

/* global iteration counter for activities */
unsigned long	_dmalloc_iter_c = 0;

/* how often to check the heap */
unsigned long	_dmalloc_check_interval = INTERVAL_INIT;

#if STORE_TIMEVAL
/* overhead information storing when the library started up for elapsed time */
TIMEVAL_TYPE	_dmalloc_start;
#endif
#if STORE_TIMEVAL == 0
TIME_TYPE	_dmalloc_start = 0;
#endif

/* global flag which indicates when we are aborting */
int		_dmalloc_aborting_b = FALSE;

#if STORE_TIMEVAL
/*
 * print the time into local buffer which is returned
 */
char	*_dmalloc_ptimeval(const TIMEVAL_TYPE *timeval_p, const int elapsed_b)
{
  static char	buf[64];
  unsigned long	hrs, mins, secs;
  unsigned long	usecs;
  
  buf[0] = '\0';
  
  secs = timeval_p->tv_sec;
  usecs = timeval_p->tv_usec;
  
  if (elapsed_b || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)) {
    usecs -= _dmalloc_start.tv_usec;
    if (usecs < 0) {
      secs--;
      usecs = - usecs;
    }
    secs -= _dmalloc_start.tv_sec;
  }
  
  hrs = secs / SECS_IN_HOUR;
  mins = (secs / SECS_IN_MIN) % MINS_IN_HOUR;
  secs %= SECS_IN_MIN;
  
  (void)sprintf(buf, "%lu:%lu:%lu.%lu", hrs, mins, secs, usecs);
  
  return buf;
}
#endif

#if STORE_TIMEVAL == 0
/*
 * print the time into local buffer which is returned
 */
char	*_dmalloc_ptime(const TIME_TYPE *time_p, const int elapsed_b)
{
  static char	buf[64];
  unsigned long	hrs, mins, secs;
  
  buf[0] = '\0';
  secs = *time_p;
  
  if (elapsed_b || BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_ELAPSED_TIME)) {
    secs -= _dmalloc_start;
  }
  
  hrs = secs / SECS_IN_HOUR;
  mins = (secs / SECS_IN_MIN) % MINS_IN_HOUR;
  secs %= SECS_IN_MIN;
  
  (void)sprintf(buf, "%lu:%lu:%lu", hrs, mins, secs);
  
  return buf;
}
#endif

/*
 * message writer with vprintf like arguments
 */
void	_dmalloc_vmessage(const char *format, va_list args)
{
  static int	outfile = -1;
  char		str[1024], *str_p = str;
  int		len;
  
  /* no logpath and no print then no workie */
  if (dmalloc_logpath == LOGPATH_INIT
      && ! BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_MESSAGES)) {
    return;
  }
  
#if HAVE_TIME
#if LOG_TIME_NUMBER
  (void)sprintf(str_p, "%lu: ", (unsigned long)time(NULL));
  for (; *str_p != '\0'; str_p++) {
  }
#endif /* LOG_TIME_NUMBER */
#if HAVE_CTIME
#if LOG_CTIME_STRING
  {
    TIME_TYPE	now;
    now = time(NULL);
    (void)sprintf(str_p, "%.24s: ", ctime(&now));
    for (; *str_p != '\0'; str_p++) {
    }
  }
#endif /* LOG_CTIME_STRING */
#endif /* HAVE_CTIME */
#endif /* HAVE_TIME */
  
#if LOG_ITERATION_COUNT
  /* add the iteration number */
  (void)sprintf(str_p, "%lu: ", _dmalloc_iter_c);
  for (; *str_p != '\0'; str_p++) {
  }
#endif
  
  /*
   * NOTE: the following code, as well as the function definition
   * above, would need to be altered to conform to non-ANSI-C
   * specifications if necessary.
   */
  
  /* write the format + info into str */
  va_start(args, format);
  (void)vsprintf(str_p, format, args);
  va_end(args);
  
  /* was it an empty format? */
  if (*str_p == '\0') {
    return;
  }
  
  /* find the length of str */
  for (; *str_p != '\0'; str_p++) {
  }
  
  /* tack on a '\n' if necessary */
  if (*(str_p - 1) != '\n') {
    *str_p++ = '\n';
    *str_p = '\0';
  }
  len = str_p - str;
  
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
       * NOTE: this makes it go recursive here but it will never enter
       * this section of code.
       */
      _dmalloc_message("Dmalloc version '%s'.  Public domain copy.",
		       dmalloc_version);
      _dmalloc_message("dmalloc_logfile '%s': flags = %#x, addr = %#lx",
		       dmalloc_logpath, _dmalloc_flags,
		       (unsigned long)dmalloc_address);
      _dmalloc_message("addr = %#lx, interval = %lu",
		       (unsigned long)dmalloc_address,
		       _dmalloc_check_interval);
#if LOCK_THREADS
      _dmalloc_message("threads enabled, lock-init = %d", THREAD_INIT_LOCK);
#endif
      
#if STORE_TIMEVAL
      _dmalloc_message("starting time = %lu.%lu",
		       (unsigned long)_dmalloc_start.tv_sec,
		       (unsigned long)_dmalloc_start.tv_usec);
#else
#if HAVE_TIME /* NOT STORE_TIME */
      _dmalloc_message("starting time = %lu", (unsigned long)_dmalloc_start);
#endif
#endif
    }
    
    /* write str to the outfile */
    (void)write(outfile, str, len);
  }
  
  /* do we need to print the message? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_MESSAGES)) {
    (void)write(STDERR, str, len);
  }
}

/*
 * message writer with printf like arguments
 */
void	_dmalloc_message(const char *format, ...)
  /* __attribute__ ((format (printf, 1, 2))) */
{
  va_list	args;
  
  va_start(args, format);
  _dmalloc_vmessage(format, args);
  va_end(args);
}

/*
 * kill the program because of an internal malloc error
 */
void	_dmalloc_die(const int silent_b)
{
  char	str[1024], *stop_str;
  
  if (! silent_b) {
    if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_ABORT)) {
      stop_str = "dumping";
    }
    else {
      stop_str = "halting";
    }
    
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
  
  /*
   * set this in case the following generates a recursive call for
   * some dumb reason
   */
  _dmalloc_aborting_b = TRUE;
  
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
void	dmalloc_error(const char *func)
{
  /* do we need to log or print the error? */
  if (dmalloc_logpath != NULL
      || BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_MESSAGES)) {
    
    /* default str value */
    if (func == NULL) {
      func = "_malloc_error";
    }
    
    /* print the malloc error message */
    _dmalloc_message("ERROR: %s: %s (err %d)",
		     func, _dmalloc_strerror(dmalloc_errno), dmalloc_errno);
  }
  
  /* do I need to abort? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_ABORT)) {
    _dmalloc_die(FALSE);
  }
  
#if HAVE_FORK
  /* how about just drop core? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_DUMP)) {
    if (fork() == 0) {
      _dmalloc_die(TRUE);
    }
  }
#endif
}
