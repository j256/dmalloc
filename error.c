/*
 * Error and message routines
 *
 * Copyright 2000 by Gray Watson
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
 *
 * $Id: error.c,v 1.98 2001/05/23 13:34:16 gray Exp $
 */

/*
 * This file contains the routines needed for processing error codes
 * produced by the library.
 */

#include <fcntl.h>				/* for O_WRONLY, etc. */
#include <stdio.h>				/* for sprintf */

#if HAVE_STDARG_H
# include <stdarg.h>				/* for message vsprintf */
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>				/* for abort */
#endif
#if HAVE_STRING_H
# include <string.h>				/* for strlen */
#endif
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

#if STORE_TIMEVAL
# ifdef TIMEVAL_INCLUDE
#  include TIMEVAL_INCLUDE
# endif
#else
# if HAVE_TIME
#  ifdef TIME_INCLUDE
#   include TIME_INCLUDE
#  endif
# endif
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
#if IDENT_WORKS
#ident "$Id: error.c,v 1.98 2001/05/23 13:34:16 gray Exp $"
#else
static	char	*rcs_id =
  "$Id: error.c,v 1.98 2001/05/23 13:34:16 gray Exp $";
#endif
#endif

#if LOCK_THREADS
#if IDENT_WORKS
#ident "@(#) $Information: lock-threads is enabled $"
#else
static char *information = "@(#) $Information: lock-threads is enabled $"
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
char		*dmalloc_logpath = NULL;
/* address to look for.  when discovered call dmalloc_error() */
DMALLOC_PNT	_dmalloc_address = NULL;
/* when to stop at an address */
unsigned long	_dmalloc_address_seen_n = 0;

/* global debug flags that are set my DMALLOC_DEBUG environ variable */
unsigned int	_dmalloc_flags = 0;

/* global iteration counter for activities */
unsigned long	_dmalloc_iter_c = 0;

/* how often to check the heap */
unsigned long	_dmalloc_check_interval = 0;

#if STORE_TIMEVAL
/* overhead information storing when the library started up for elapsed time */
TIMEVAL_TYPE	_dmalloc_start;
#endif

/* NOTE: we do the ifdef this way for fillproto */
#if STORE_TIMEVAL == 0
#if HAVE_TIME
TIME_TYPE	_dmalloc_start = 0;
#endif
#endif

/* when we are going to startup our locking subsystem */
int		_dmalloc_lock_on = 0;

/* global flag which indicates when we are aborting */
int		_dmalloc_aborting_b = 0;

/* local variables */
static int	outfile_fd = -1;		/* output file descriptor */

/*
 * Open up our log file and write some version of settings
 * information.
 */
void	_dmalloc_open_log(void)
{
  char	str[1024];
  
  /* if it's already open or if we don't have a log file configured */
  if (outfile_fd >= 0
      || dmalloc_logpath == NULL) {
    return;
  }
  
  /* open our logfile */
  outfile_fd = open(dmalloc_logpath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (outfile_fd < 0) {
    (void)loc_snprintf(str, sizeof(str),
		       "debug-malloc library: could not open '%s'\r\n",
		       dmalloc_logpath);
    (void)write(STDERR, str, strlen(str));
    /* disable log_path */
    dmalloc_logpath = NULL;
    return;
  }
  
  /*
   * NOTE: this makes it go recursive here, but it will never enter
   * this section of code.
   */
  
  _dmalloc_message("Dmalloc version '%s' from '%s'",
		   dmalloc_version, DMALLOC_HOME);
  _dmalloc_message("flags = %#x, logfile '%s'",
		   _dmalloc_flags, dmalloc_logpath);
  _dmalloc_message("interval = %lu, addr = %#lx, seen # = %ld",
		   _dmalloc_check_interval,
		   (unsigned long)_dmalloc_address,
		   _dmalloc_address_seen_n);
#if LOCK_THREADS
  _dmalloc_message("threads enabled, lock-on = %d, lock-init = %d",
		   _dmalloc_lock_on, THREAD_INIT_LOCK);
#endif
    
#if STORE_TIMEVAL
  {
    char	time_buf[64];
    _dmalloc_message("starting time = %s",
		     _dmalloc_ptimeval(&_dmalloc_start, time_buf,
				       sizeof(time_buf), 0));
  }
#else
#if HAVE_TIME /* NOT STORE_TIME */
  {
    char	time_buf[64];
    _dmalloc_message("starting time = %s",
		     _dmalloc_ptime(&_dmalloc_start, time_buf,
				    sizeof(time_buf), 0));
  }
#endif
#endif
}

#if STORE_TIMEVAL
/*
 * print the time into local buffer which is returned
 */
char	*_dmalloc_ptimeval(const TIMEVAL_TYPE *timeval_p, char *buf,
			   const int buf_size, const int elapsed_b)
{
  unsigned long	hrs, mins, secs;
  unsigned long	usecs;
  
  secs = timeval_p->tv_sec;
  usecs = timeval_p->tv_usec;
  
  if (elapsed_b) {
    if (usecs >= _dmalloc_start.tv_usec) {
      usecs -= _dmalloc_start.tv_usec;
    }
    else {
      usecs = _dmalloc_start.tv_usec - usecs;
      secs--;
    }
    secs -= _dmalloc_start.tv_sec;
    
    hrs = secs / SECS_IN_HOUR;
    mins = (secs / SECS_IN_MIN) % MINS_IN_HOUR;
    secs %= SECS_IN_MIN;
    
    (void)loc_snprintf(buf, buf_size, "%lu:%02lu:%02lu.%06lu",
		       hrs, mins, secs, usecs);
  }
  else {
    (void)loc_snprintf(buf, buf_size, "%lu.%06lu",
		       secs, usecs);
  }
  
  return buf;
}
#endif

/* NOTE: we do the ifdef this way for fillproto */
#if STORE_TIMEVAL == 0 && HAVE_TIME
/*
 * print the time into local buffer which is returned
 */
char	*_dmalloc_ptime(const TIME_TYPE *time_p, char *buf, const int buf_size,
			const int elapsed_b)
{
  unsigned long	hrs, mins, secs;
  
  secs = *time_p;
  
  if (elapsed_b) {
    secs -= _dmalloc_start;
    
    hrs = secs / SECS_IN_HOUR;
    mins = (secs / SECS_IN_MIN) % MINS_IN_HOUR;
    secs %= SECS_IN_MIN;
    
    (void)loc_snprintf(buf, buf_size, "%lu:%02lu:%02lu", hrs, mins, secs);
  }
  else {
    (void)loc_snprintf(buf, buf_size, "%lu", secs);
  }
  
  return buf;
}
#endif

/*
 * message writer with vprintf like arguments
 */
void	_dmalloc_vmessage(const char *format, va_list args)
{
  char		str[1024], *str_p, *bounds_p;
  int		len;
  
  str_p = str;
  bounds_p = str_p + sizeof(str);
  
  /* no logpath and no print then no workie */
  if (dmalloc_logpath == NULL
      && ! BIT_IS_SET(_dmalloc_flags, DEBUG_PRINT_MESSAGES)) {
    return;
  }
  
  /* do we need to open the logfile? */
  if (dmalloc_logpath != NULL && outfile_fd < 0) {
    _dmalloc_open_log();
  }
  
#if HAVE_TIME
#if LOG_TIME_NUMBER
  str_p += loc_snprintf(str_p, bounds_p - str_p, "%lu: ",
			(unsigned long)time(NULL));
#endif /* LOG_TIME_NUMBER */
#if HAVE_CTIME
#if LOG_CTIME_STRING
  {
    TIME_TYPE	now;
    now = time(NULL);
    str_p += loc_snprintf(str_p, bounds_p - str_p, "%.24s: ", ctime(&now));
  }
#endif /* LOG_CTIME_STRING */
#endif /* HAVE_CTIME */
#endif /* HAVE_TIME */
  
#if LOG_ITERATION_COUNT
  /* add the iteration number */
  str_p += loc_snprintf(str_p, bounds_p - str_p, "%lu: ", _dmalloc_iter_c);
#endif
  
  /*
   * NOTE: the following code, as well as the function definition
   * above, would need to be altered to conform to non-ANSI-C
   * specifications if necessary.
   */
  
  /* write the format + info into str */
  len = loc_vsnprintf(str_p, bounds_p - str_p, format, args);
  
  /* was it an empty format? */
  if (len == 0) {
    return;
  }
  str_p += len;
  
  /* tack on a '\n' if necessary */
  if (*(str_p - 1) != '\n') {
    *str_p++ = '\n';
    *str_p = '\0';
  }
  len = str_p - str;
  
  /* do we need to write the message to the logfile */
  if (dmalloc_logpath != NULL) {
    (void)write(outfile_fd, str, len);
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
    (void)loc_snprintf(str, sizeof(str),
		       "debug-malloc library: %s program, fatal error\r\n",
		       stop_str);
    (void)write(STDERR, str, strlen(str));
    if (dmalloc_errno != ERROR_NONE) {
      (void)loc_snprintf(str, sizeof(str), "   Error: %s (err %d)\r\n",
			 _dmalloc_strerror(dmalloc_errno), dmalloc_errno);
      (void)write(STDERR, str, strlen(str));
    }
  }
  
  /*
   * set this in case the following generates a recursive call for
   * some dumb reason
   */
  _dmalloc_aborting_b = 1;
  
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
    _dmalloc_die(0);
  }
  
#if HAVE_FORK
  /* how about just drop core? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_ERROR_DUMP)) {
    if (fork() == 0) {
      _dmalloc_die(1);
    }
  }
#endif
}
