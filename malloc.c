/*
 * user-level memory-allocation routines
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
 * $Id: malloc.c,v 1.114 1998/11/09 15:57:18 gray Exp $
 */

/*
 * This file contains the user-level calls to the memory allocation
 * routines.  It handles a lot of the miscellaneous support garbage for
 * chunk.c which is the real heap manager.
 */

#if HAVE_STDLIB_H
# include <stdlib.h>				/* for atexit */
#endif

#include "conf.h"				/* up here for _INCLUDE */

/* for timeval type -- see conf.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
#endif

#if LOCK_THREADS
#if HAVE_PTHREAD_H
#include <pthread.h>
#endif
#if HAVE_PTHREADS_H
#include <pthreads.h>
#endif
#endif

#if SIGNAL_OKAY
#include <signal.h>
#endif

#define DMALLOC_DISABLE

#include "dmalloc.h"

#include "chunk.h"
#include "compat.h"
#include "debug_val.h"
#include "env.h"
#include "error.h"
#include "error_str.h"
#include "error_val.h"
#include "heap.h"
#include "dmalloc_loc.h"
#include "dmalloc_lp.h"
#include "return.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: malloc.c,v 1.114 1998/11/09 15:57:18 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: malloc.c,v 1.114 1998/11/09 15:57:18 gray Exp $";
#endif
#endif

/* local routines */
static	int	dmalloc_startup(void);
void		_dmalloc_shutdown(void);
DMALLOC_PNT	_loc_malloc(const char *file, const int line,
			    DMALLOC_SIZE size, const int calloc_b,
			    const int valloc_b);
DMALLOC_PNT	_loc_realloc(const char *file, const int line,
			     DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size,
			     const int recalloc_b);
int	_loc_free(const char *file, const int line, DMALLOC_PNT pnt);
void	_dmalloc_log_heap_map(const char *file, const int line);
void	_dmalloc_log_stats(const char *file, const int line);
void	_dmalloc_log_unfreed(const char *file, const int line);
int	_dmalloc_verify(const DMALLOC_PNT pnt);
void	_dmalloc_debug(const int flags);
int	_dmalloc_debug_current(void);
int	_dmalloc_examine(const char *file, const int line,
			 const DMALLOC_PNT pnt, DMALLOC_SIZE *size_p,
			 char **file_p, unsigned int *line_p,
			 DMALLOC_PNT *ret_attr_p);
const char	*_dmalloc_strerror(const int error_num);

/* local variables */
static	int		enabled_b	= FALSE; /* have we started yet? */
static	int		in_alloc_b	= FALSE; /* can't be here twice */
static	int		do_shutdown_b	= FALSE; /* execute shutdown soon */

/* debug variables */
static	int		address_seen_n = ADDRESS_COUNT_INIT; /* # stop addr */
static	char		*start_file = START_FILE_INIT;	/* file to start at */
static	int		start_line = START_LINE_INIT;	/* line to start */
static	int		start_count = START_COUNT_INIT; /* start after X */
static	int		check_interval = INTERVAL_INIT; /* check every X */
static	int		thread_lock_on = LOCK_ON_INIT;	/* start locking when*/
static	int		thread_lock_c = 0;		/* lock counter */

/****************************** thread locking *******************************/

#if LOCK_THREADS
#ifdef THREAD_MUTEX_T
/* define a global variable we use as a lock counter */
static THREAD_MUTEX_T dmalloc_mutex;
#else
#error We need to have THREAD_MUTEX_T defined by the configure script
#endif
#endif

/*
 * THREADS LOCKING:
 *
 * Because we need to protect for multiple threads making calls into
 * the dmalloc library at the same time, we need to initialize and use
 * a thread mutex variable.  The problem is that most thread libraries
 * uses malloc itself and do not like to go recursive.
 *
 * There are two places where we may have this problem.  One is when
 * we try to use our mutex-lock variable when pthreads is starting up
 * in a shaky state.  The thread library allocates some space, the
 * dmalloc library needs to lock its mutex variable so calls back into
 * the pthread library before it is ready for a call, and a core dump
 * is probably the result.
 *
 * We hopefully solve this by having the dmalloc library not lock
 * during the first couple of memory transactions.  The number is
 * controlled by lock-on dmalloc program environmental setting (set
 * with ``dmalloc -o X'').  You will have to play with the value.  Too
 * many will cause two threads to march into the dmalloc code at the
 * same time generating a ERROR_IN_TWICE error.  Too few and you will
 * get a core dump in the pthreads initialization code.
 *
 * The second place where we might go recursive is when we go to
 * actually initialize our mutex-lock before we can use it.  The
 * THREAD_INIT_LOCK variable (in settings.h) defines that the
 * initialization happens 2 memory transactions before the library
 * begins to use the mutex (lock-on - 2).  It we waited to initialize
 * the variable right before we used it, the pthread library might
 * want to allocate some memory for the variable causing a recursive
 * call and probably a seg-fault -- at least in OSF.  If people need
 * to have this variable also be runtime configurable or would like to
 * present an alternative default, please let me know.
 */

#if LOCK_THREADS
/*
 * mutex lock the malloc library
 */
static	void	lock_thread(void)
{
  /* we only lock if the lock-on counter has reached 0 */
  if (thread_lock_c == 0) {
#if HAVE_PTHREAD_MUTEX_LOCK
    pthread_mutex_lock(&dmalloc_mutex);
#endif
  }
#else /* ! LOCK_THREADS */
  /* was thread-lock-on specified but not configured? */
  if (thread_lock_on != LOCK_ON_INIT) {
    dmalloc_errno = ERROR_LOCK_NOT_CONFIG;
    _dmalloc_die(0);
  }
}

/*
 * mutex unlock the malloc library
 */
static	void	unlock_thread(void)
{
  /* if the lock-on counter has not reached 0 then count it down */
  if (thread_lock_c > 0) {
    thread_lock_c--;
    /*
     * As we approach the time when we start mutex locking the
     * library, we need to init the mutex variable.  This sets how
     * many times before we start locking should we init the variable
     * taking in account that the init itself might generate a call
     * into the library.  Ugh.
     */
    if (thread_lock_c == THREAD_INIT_LOCK) {
#if HAVE_PTHREAD_MUTEX_INIT
      /* we init instead of the lock point to avoid recursion */
      pthread_mutex_init(&dmalloc_mutex, THREAD_LOCK_INIT_VAL);
#endif
    }
  }
  else if (thread_lock_c == 0) {
#if HAVE_PTHREAD_MUTEX_UNLOCK
    pthread_mutex_unlock(&dmalloc_mutex);
#endif
  }
}
#endif

/****************************** local utilities ******************************/

/*
 * a call to the alloc routines has been made, check the debug variables
 * returns ERROR or NOERROR.
 */
static	int	check_debug_vars(const char *file, const int line)
{
  if (_dmalloc_aborting_b) {
    return ERROR;
  }
  
  /*
   * NOTE: we need to do this outside of lock to get env vars
   * otherwise our thread_lock_on variable won't be initialized and
   * the THREAD_LOCK will flip.
   */
  if (! enabled_b) {
    if (dmalloc_startup() != NOERROR) {
      return ERROR;
    }
  }
  
#if LOCK_THREADS
  lock_thread();
#endif
  
  if (in_alloc_b) {
    dmalloc_errno = ERROR_IN_TWICE;
    dmalloc_error("check_debug_vars");
    /* NOTE: dmalloc_error may die already */
    _dmalloc_die(FALSE);
    /*NOTREACHED*/
  }
  
  in_alloc_b = TRUE;
  
  /* check start file/line specifications */
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP)
      && start_file != START_FILE_INIT
      && file != DMALLOC_DEFAULT_FILE
      && line != DMALLOC_DEFAULT_LINE
      && strcmp(start_file, file) == 0
      && (start_line == 0 || start_line == line)) {
    BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
  }
  
  /* start checking heap after X times */
  if (start_count != START_COUNT_INIT && --start_count == 0) {
    BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
  }
  
  /* checking heap every X times */
  _dmalloc_iter_c++;
  if (check_interval != INTERVAL_INIT && check_interval > 0) {
    if (_dmalloc_iter_c % check_interval == 0) {
      BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
    }
    else { 
      BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
    }
  }
  
  /* after all that, do we need to check the heap? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP)) {
    (void)_chunk_check();
  }
  
  return NOERROR;
}

/*
 * check out a pointer to see if we were looking for it.  this should
 * be re-entrant and it may not return.
 */
static	void	check_pnt(const char *file, const int line, const void *pnt,
			  const char *label)
{
  static int	addr_c = 0;
  
  if (dmalloc_address == ADDRESS_INIT || pnt != dmalloc_address) {
    return;
  }
  
  addr_c++;
  _dmalloc_message("address '%#lx' found in '%s' at pass %d from '%s'",
		   (unsigned long)pnt, label, addr_c,
		   _chunk_display_where(file, line));
  
  /* NOTE: if address_seen_n == 0 then never quit */
  if (address_seen_n == ADDRESS_COUNT_INIT
      || (address_seen_n > 0 && addr_c >= address_seen_n)) {
    dmalloc_errno = ERROR_IS_FOUND;
    dmalloc_error("check_pnt");
  }
}

/*
 * process the values of dmalloc environ variables
 */
static	void	process_environ(void)
{
  _dmalloc_environ_get(OPTIONS_ENVIRON, (unsigned long *)&dmalloc_address,
		       &address_seen_n, &_dmalloc_flags,
		       &check_interval, &thread_lock_on, &dmalloc_logpath,
		       &start_file, &start_line, &start_count);
  thread_lock_c = thread_lock_on;
  
  /* if it was not set then no flags set */
  if (_dmalloc_flags == (unsigned int)DEBUG_INIT) {
    _dmalloc_flags = 0;
  }
  
  /* if we set the start stuff, then check-heap comes on later */
  if (start_count != -1) {
    BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
  }
  
  /* override dmalloc_debug value if we we call dmalloc_debug() already */
  if (_dmalloc_debug_preset != DEBUG_PRE_NONE) {
    _dmalloc_flags = _dmalloc_debug_preset;
  }
}

/************************** startup/shutdown calls ***************************/

#if SIGNAL_OKAY
/*
 * signal catcher
 */
static	RETSIGTYPE	signal_handler(const int sig)
{
  _dmalloc_message("caught signal %d", sig);
  /* if we are already inside malloc then do the shutdown later */
  if (in_alloc_b) {
    do_shutdown_b = TRUE;
  }
  else {
    _dmalloc_shutdown();
  }
}
#endif

/*
 * startup the memory-allocation module
 */
static	int	dmalloc_startup(void)
{
  /* have we started already? */
  if (enabled_b) {
    return ERROR;
  }
  
  /* set this here so if an error occurs below, it will not try again */
  enabled_b = TRUE;
  
#if STORE_TIMEVAL
  GET_TIMEVAL(_dmalloc_start);
#else
#if HAVE_TIME /* NOT STORE_TIME */
  _dmalloc_start = time(NULL);
#endif
#endif
  
  /* process the environmental variable(s) */
  process_environ();
  
  /* startup heap code */
  if (_heap_startup() == ERROR) {
    return ERROR;
  }
  
  /* startup the chunk lower-level code */
  if (_chunk_startup() == ERROR) {
    return ERROR;
  }
  
  /* set leap variables */
  _dmalloc_malloc_func = _loc_malloc;
  _dmalloc_realloc_func = _loc_realloc;
  _dmalloc_free_func = _loc_free;
  _dmalloc_shutdown_func = _dmalloc_shutdown;
  _dmalloc_log_heap_map_func = _dmalloc_log_heap_map;
  _dmalloc_log_stats_func = _dmalloc_log_stats;
  _dmalloc_log_unfreed_func = _dmalloc_log_unfreed;
  _dmalloc_verify_func = _dmalloc_verify;
  _dmalloc_debug_func = _dmalloc_debug;
  _dmalloc_debug_current_func = _dmalloc_debug_current;
  _dmalloc_examine_func = _dmalloc_examine;
  _dmalloc_strerror_func = _dmalloc_strerror;
  
  /*
   * NOTE: we may go recursive below here becasue atexit or on_exit
   * may ask for memory to be allocated.  We won't worry about it and
   * will just give it to them.  We hope that atexit didn't start the
   * allocating.  Ugh.
   */
#if AUTO_SHUTDOWN
  /* NOTE: I use the else here in case some dumb systems has both */
#if HAVE_ATEXIT
  (void)atexit(_dmalloc_shutdown);
#else
#if HAVE_ON_EXIT
  (void)on_exit(_dmalloc_shutdown, NULL);
#endif /* HAVE_ON_EXIT */
#endif /* ! HAVE_ATEXIT */
#endif /* AUTO_SHUTDOWN */
  
#if SIGNAL_OKAY
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CATCH_SIGNALS)) {
#ifdef SIGNAL1
    (void)signal(SIGNAL1, signal_handler);
#endif
#ifdef SIGNAL2
    (void)signal(SIGNAL2, signal_handler);
#endif
#ifdef SIGNAL3
    (void)signal(SIGNAL3, signal_handler);
#endif
#ifdef SIGNAL4
    (void)signal(SIGNAL4, signal_handler);
#endif
#ifdef SIGNAL5
    (void)signal(SIGNAL5, signal_handler);
#endif
#ifdef SIGNAL6
    (void)signal(SIGNAL6, signal_handler);
#endif
  }
#endif /* SIGNAL_OKAY */
  
  return NOERROR;
}

/*
 * shutdown memory-allocation module, provide statistics if necessary
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_shutdown(void)
{
  /* NOTE: do not generate errors for IN_TWICE here */
  
  /* if we're already in die mode leave fast and quietly */
  if (_dmalloc_aborting_b) {
    return;
  }
  
  thread_lock();
  
  /* if we've died in dmalloc somewhere then leave fast and quietly */
  if (in_alloc_b) {
    return;
  }
  
  in_alloc_b = TRUE;
  
  /* check the heap since we are dumping info from it */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP)) {
    (void)_chunk_check();
  }
  
  /* dump some statistics to the logfile */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_STATS)) {
    _chunk_list_count();
  }
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_STATS)) {
    _chunk_stats();
  }
  
  /* report on non-freed pointers */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE)) {
    _chunk_dump_unfreed();
  }
  
#if STORE_TIMEVAL
  {
    struct timeval	now;
    GET_TIMEVAL(now);
    _dmalloc_message("ending time = %ld.%ld, elapsed since start = %s",
		     now.tv_sec, now.tv_usec, _dmalloc_ptimeval(&now, TRUE));
  }
#else
#if HAVE_TIME
  {
    long	now = time(NULL);
    _dmalloc_message("ending time = %ld, elapsed since start = %s",
		     now, _dmalloc_ptime(&now, TRUE));
  }
#endif
#endif
  
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  /* NOTE: do not set enabled_b to false here */
}

#if FINI_DMALLOC
/*
 * Automatic OSF function to close dmalloc.  Pretty cool OS/compiler
 * hack.  By default it is not necessary because we use atexit() and
 * on_exit() to register the close functions.  These are more
 * portable.
 */
void	__fini_dmalloc()
{
  dmalloc_shutdown();
}
#endif

/******************************* memory calls ********************************/

/*
 * Allocate and return a SIZE block of bytes.  If it is a calloc then
 * the CALLOC_B is set to 1 else 0.  If valloc then CALLOC_B is set to
 * 1 else 0.  Returns 0L on error.
 */
DMALLOC_PNT	_loc_malloc(const char *file, const int line,
			    DMALLOC_SIZE size, const int calloc_b,
			    const int valloc_b)
{
  void		*new_p;
  
#if DMALLOC_SIZE_UNSIGNED == 0
  if (size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("malloc");
    return MALLOC_ERROR;
  }
#endif
  
  if (check_debug_vars(file, line) != NOERROR) {
    return MALLOC_ERROR;
  }
  
  new_p = _chunk_malloc(file, line, size, calloc_b, 0, valloc_b);
  in_alloc_b = FALSE;
  
  check_pnt(file, line, new_p, "malloc");
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  return new_p;
}

/*
 * Resizes OLD_PNT to NEW_SIZE bytes and return the new space after
 * either copying all of OLD_PNT to the new area or truncating.  If
 * OLD_PNT is 0L then it will do the equivalent of malloc(NEW_SIZE).
 * If NEW_SIZE is 0 and OLD_PNT is not 0L then it will do the
 * equivalent of free(OLD_PNT) and will return 0L.  If the RECALLOC_B
 * flag is enabled, it will zero any new memory.  Returns 0L on error.
 */
DMALLOC_PNT	_loc_realloc(const char *file, const int line,
			     DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size,
			     const int recalloc_b)
{
  void		*new_p;
  
#if DMALLOC_SIZE_UNSIGNED == 0
  if (new_size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("realloc");
    return MALLOC_ERROR;
  }
#endif
  
  if (check_debug_vars(file, line) != NOERROR) {
    return REALLOC_ERROR;
  }
  
  check_pnt(file, line, old_pnt, "realloc-in");
  
#if ALLOW_REALLOC_NULL
  if (old_pnt == NULL) {
    /* recalloc_b acts the same way as a calloc */
    new_p = _chunk_malloc(file, line, new_size, recalloc_b, 0, 0);
  }
  else
#endif
#if ALLOW_REALLOC_SIZE_ZERO
    if (new_size == 0) {
      /*
       * If realloc(old_pnt, 0) then free(old_pnt).  Thanks to
       * Stefan.Froehlich@tuwien.ac.at for patiently pointing that the
       * realloc in just about every Unix has this functionality.
       */
      (void)_chunk_free(file, line, old_pnt, 0);
      new_p = NULL;
    }
    else
#endif
      new_p = _chunk_realloc(file, line, old_pnt, new_size, recalloc_b);
  
  in_alloc_b = FALSE;
  
  if (new_p != NULL) {
    check_pnt(file, line, new_p, "realloc-out");
  }
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  return new_p;
}

/*
 * release PNT in the heap, returning FREE_ERROR, FREE_NOERROR.
 */
int	_loc_free(const char *file, const int line, DMALLOC_PNT pnt)
{
  int		ret;
  
  if (check_debug_vars(file, line) != NOERROR) {
    return FREE_ERROR;
  }
  
  check_pnt(file, line, pnt, "free");
  
  ret = _chunk_free(file, line, pnt, 0);
  
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  return ret;
}

/*************************** external memory calls ***************************/

/*
 * allocate and return a SIZE block of bytes.  returns 0L on error.
 */
#undef malloc
DMALLOC_PNT	malloc(DMALLOC_SIZE size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, size, 0, 0);
}

/*
 * allocate and return a block of _zeroed_ bytes able to hold
 * NUM_ELEMENTS, each element contains SIZE bytes.  returns 0L on
 * error.
 */
#undef calloc
DMALLOC_PNT	calloc(DMALLOC_SIZE num_elements, DMALLOC_SIZE size)
{
  DMALLOC_SIZE	len = num_elements * size;
  char		*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, len, 1, 0);
}

/*
 * Resizes OLD_PNT to NEW_SIZE bytes and return the new space after
 * either copying all of OLD_PNT to the new area or truncating.  If
 * OLD_PNT is 0L then it will do the equivalent of malloc(NEW_SIZE).
 * If NEW_SIZE is 0 and OLD_PNT is not 0L then it will do the
 * equivalent of free(OLD_PNT) and will return 0L.  Returns 0L on
 * error.
 */
#undef realloc
DMALLOC_PNT	realloc(DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_realloc(file, DMALLOC_DEFAULT_LINE, old_pnt, new_size, 0);
}

/*
 * Resizes OLD_PNT to NEW_SIZE bytes and return the new space after
 * either copying all of OLD_PNT to the new area or truncating.  If
 * OLD_PNT is 0L then it will do the equivalent of malloc(NEW_SIZE).
 * If NEW_SIZE is 0 and OLD_PNT is not 0L then it will do the
 * equivalent of free(OLD_PNT) and will return 0L.  Any extended
 * memory space will be zeroed like calloc.  Returns 0L on error.
 */
#undef recalloc
DMALLOC_PNT	recalloc(DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_realloc(file, DMALLOC_DEFAULT_LINE, old_pnt, new_size, 1);
}

/*
 * Allocate and return a SIZE block of bytes that has been aligned to
 * a page-size.  Returns 0L on error.
 */
#undef valloc
DMALLOC_PNT	valloc(DMALLOC_SIZE size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, size, 0, 1);
}

/*
 * Allocate and return a block of bytes that contains the STR.
 * Returns 0L on error.
 */
#undef strdup
char	*strdup(const char *str)
{
  int	len;
  char	*buf, *file;
  
  GET_RET_ADDR(file);
  
  /* len + \0 */
  len = strlen(str) + 1;
  
  buf = (char *)_loc_malloc(file, DMALLOC_DEFAULT_LINE, len, 0, 0);
  if (buf != NULL) {
    (void)strcpy(buf, str);
  }
  
  return buf;
}

/*
 * release PNT in the heap, returning FREE_ERROR, FREE_NOERROR or void
 * depending on whether STDC is defined by your compiler.
 */
#undef free
DMALLOC_FREE_RET	free(DMALLOC_PNT pnt)
{
  char	*file;
  int	ret;
  
  GET_RET_ADDR(file);
  ret = _loc_free(file, DMALLOC_DEFAULT_LINE, pnt);
  
#if defined(__STDC__) && __STDC__ == 1
#else
  return ret;
#endif
}

/*
 * same as free PNT
 */
#undef cfree
DMALLOC_FREE_RET	cfree(DMALLOC_PNT pnt)
{
  char	*file;
  int	ret;
  
  GET_RET_ADDR(file);
  ret = _loc_free(file, DMALLOC_DEFAULT_LINE, pnt);
  
#if defined(__STDC__) && __STDC__ == 1
#else
  return ret;
#endif
}

/******************************* utility calls *******************************/

/*
 * log the heap structure plus information on the blocks if necessary.
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_log_heap_map(const char *file, const int line)
{
  /* check the heap since we are dumping info from it */
  if (check_debug_vars(file, line) != NOERROR) {
    return;
  }
  
  _chunk_log_heap_map();
  
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
}

/*
 * dump dmalloc statistics to logfile
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_log_stats(const char *file, const int line)
{
  if (check_debug_vars(file, line) != NOERROR) {
    return;
  }
  
  _chunk_list_count();
  _chunk_stats();
  
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
}

/*
 * dump unfreed-memory info to logfile
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_log_unfreed(const char *file, const int line)
{
  if (check_debug_vars(file, line) != NOERROR) {
    return;
  }
  
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("dumping the unfreed pointers");
  }
  
  _chunk_dump_unfreed();
  
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
}

/*
 * verify pointer PNT, if PNT is 0 then check the entire heap.
 * NOTE: called by way of leap routine in dmalloc_lp.c
 * returns MALLOC_VERIFY_ERROR or MALLOC_VERIFY_NOERROR
 */
int	_dmalloc_verify(const DMALLOC_PNT pnt)
{
  int	ret;
  
  /* should not check heap here because we will be doing it below */
  
  if (_dmalloc_aborting_b) {
    return DMALLOC_VERIFY_ERROR;
  }
  
  thread_lock();
  
  if (in_alloc_b) {
    dmalloc_errno = ERROR_IN_TWICE;
    dmalloc_error("dmalloc_verify");
    /* NOTE: dmalloc_error may die already */
    _dmalloc_die(FALSE);
    /*NOTREACHED*/
  }
  
  in_alloc_b = TRUE;
  
  if (pnt == NULL) {
    ret = _chunk_check();
  }
  else {
    ret = _chunk_pnt_check("dmalloc_verify", pnt, CHUNK_PNT_EXACT, 0);
  }
  
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  if (ret == NOERROR) {
    return DMALLOC_VERIFY_NOERROR;
  }
  else {
    return DMALLOC_VERIFY_ERROR;
  }
}

/*
 * set the global debug functionality FLAGS (0 to disable all
 * debugging).  NOTE: after this module has started up, you cannot set
 * certain flags such as fence-post or free-space checking.
 */
void	_dmalloc_debug(const int flags)
{
  /* should not check the heap here since we are setting the debug variable */
  
  /* if we've not started up then set the variable */
  if (! enabled_b) {
    _dmalloc_flags = flags;
  }
  else {
    /* make sure that the not-changeable flag values are preserved */
    _dmalloc_flags &= DEBUG_NOT_CHANGEABLE;
    
    /* add the new flags - the not-addable ones */
    _dmalloc_flags |= flags & ~DEBUG_NOT_ADDABLE;
  }
}

/*
 * returns the current debug functionality flags.  this allows you to
 * save a dmalloc library state to be restored later.
 */
int	_dmalloc_debug_current(void)
{
  /* should not check the heap here since we are dumping the debug variable */
  return _dmalloc_flags;
}

/*
 * examine pointer PNT and returns SIZE, and FILE / LINE info on it,
 * or return-address RET_ADDR if any of the pointers are not 0L.
 * if FILE returns 0L then RET_ATTR may have a value and vice versa.
 * returns NOERROR or ERROR depending on whether PNT is good or not
 */
int	_dmalloc_examine(const char *file, const int line,
			 const DMALLOC_PNT pnt, DMALLOC_SIZE *size_p,
			 char **file_p, unsigned int *line_p,
			 DMALLOC_PNT *ret_attr_p)
{
  int		ret;
  unsigned int	size_map;
  
  /* need to check the heap here since we are geting info from it below */
  if (check_debug_vars(file, line) != NOERROR) {
    return ERROR;
  }
  
  /* NOTE: we do not need the alloc-size info */
  ret = _chunk_read_info(pnt, &size_map, NULL, file_p, line_p, ret_attr_p,
			 "dmalloc_examine", NULL);
  in_alloc_b = FALSE;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  if (ret == NOERROR) {
    if (size_p != NULL) {
      *size_p = size_map;
    }
    return NOERROR;
  }
  else {
    return ERROR;
  }
}

/*
 * Dmalloc version of strerror to return the string version of
 * ERROR_NUM.  Returns an invaid errno string if ERROR_NUM is
 * out-of-range.
 */
const char	*_dmalloc_strerror(const int error_num)
{
  error_str_t	*err_p;
  
  /* should not check_debug_vars here because _dmalloc_error calls this */
  
  for (err_p = error_list; err_p->es_error != 0; err_p++) {
    if (err_p->es_error == error_num) {
      return err_p->es_string;
    }
  }
  
  return INVALID_ERROR;
}
