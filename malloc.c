/*
 * user-level memory-allocation routines
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
 * $Id: malloc.c,v 1.155 2001/07/12 23:09:42 gray Exp $
 */

/*
 * This file contains the user-level calls to the memory allocation
 * routines.  It handles a lot of the miscellaneous support garbage for
 * chunk.c which is the real heap manager.
 */

#if HAVE_STDLIB_H
# include <stdlib.h>				/* for atexit */
#endif
#if HAVE_STRING_H
# include <string.h>				/* for strlen */
#endif

#include "conf.h"				/* up here for _INCLUDE */

#if STORE_TIMEVAL
# ifdef TIMEVAL_INCLUDE
#  include TIMEVAL_INCLUDE
# endif
#else
# if HAVE_TIME /* NOT STORE_TIME */
#  ifdef TIME_INCLUDE
#   include TIME_INCLUDE
#  endif
# endif
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
#include "malloc_funcs.h"
#include "return.h"

#if INCLUDE_RCS_IDS
#if IDENT_WORKS
#ident "$Id: malloc.c,v 1.155 2001/07/12 23:09:42 gray Exp $"
#else
static	char	*rcs_id =
  "$Id: malloc.c,v 1.155 2001/07/12 23:09:42 gray Exp $";
#endif
#endif

#if LOCK_THREADS
#if IDENT_WORKS
#ident "@(#) $Information: lock-threads is enabled $"
#else
static char *information = "@(#) $Information: lock-threads is enabled $"
#endif
#endif

/* local variables */
static	int		enabled_b = 0;		/* have we started yet? */
static	int		in_alloc_b = 0;		/* can't be here twice */
static	int		do_shutdown_b = 0;	/* execute shutdown soon */
static	int		memalign_warn_b = 0;	/* memalign warning printed?*/
static	dmalloc_track_t	tracking_func = NULL;	/* memory trxn tracking func */

/* debug variables */
static	char		*start_file = NULL;	/* file to start at */
static	int		start_line = 0;		/* line to start */
static	int		start_count = 0;	/* start after X */
static	int		thread_lock_c = 0;	/* lock counter */

/****************************** thread locking *******************************/

#if LOCK_THREADS
#ifdef THREAD_MUTEX_T
/*
 * Define a global variable we use as a lock counter.
 *
 * NOTE: we do not use the PTHREAD_MUTEX_INITIALIZER since this
 * basically delays the pthread_mutex_init call to when
 * pthread_mutex_lock is called for the first time (at least on
 * freebsd).  Since we don't want to go recursive into the pthread
 * library when we go to lock our mutex variable, we want to force the
 * initialization to happen beforehand with a call to
 * pthread_mute_init.
 */
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
      /*
       * NOTE: we do not use the PTHREAD_MUTEX_INITIALIZER since this
       * basically delays the pthread_mutex_init call to when
       * pthread_mutex_lock is called for the first time (at least on
       * freebsd).  Since we don't want to go recursive into the
       * pthread library when we go to lock our mutex variable, we
       * want to force the initialization to happen beforehand with a
       * call to pthread_mute_init.
       */
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
 * check out a pointer to see if we were looking for it.  this should
 * be re-entrant and it may not return.
 */
static	void	check_pnt(const char *file, const int line, const void *pnt,
			  const char *label)
{
  static unsigned long	addr_c = 0;
  char			where_buf[64];
  
  if (_dmalloc_address == NULL || pnt != _dmalloc_address) {
    return;
  }
  
  addr_c++;
  _dmalloc_message("address '%#lx' found in '%s' at pass %ld from '%s'",
		   (unsigned long)pnt, label, addr_c,
		   _chunk_desc_pnt(where_buf, sizeof(where_buf), file, line));
  
  /* NOTE: if address_seen_n == 0 then never quit */
  if (_dmalloc_address_seen_n > 0
      && addr_c >= _dmalloc_address_seen_n) {
    dmalloc_errno = ERROR_IS_FOUND;
    dmalloc_error("check_pnt");
  }
}

/*
 * process the values of dmalloc environ variables
 */
static	void	process_environ(const char *option_str)
{
  
  /* process the options flag */
  if (option_str != NULL) {
    _dmalloc_environ_process(option_str, &_dmalloc_address,
			     (long *)&_dmalloc_address_seen_n, &_dmalloc_flags,
			     &_dmalloc_check_interval, &_dmalloc_lock_on,
			     &dmalloc_logpath, &start_file, &start_line,
			     &start_count);
  }
  thread_lock_c = _dmalloc_lock_on;
  
  /* if we set the start stuff, then check-heap comes on later */
  if (start_count != -1) {
    BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
  }
  
  /* override dmalloc_debug value if we we call dmalloc_debug() already */
  if (_dmalloc_debug_preset != DEBUG_PRE_NONE) {
    _dmalloc_flags = _dmalloc_debug_preset;
  }
  
#if LOCK_THREADS == 0
  /* was thread-lock-on specified but not configured? */
  if (_dmalloc_lock_on > 0) {
    dmalloc_errno = ERROR_LOCK_NOT_CONFIG;
    _dmalloc_die(0);
  }
#endif
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
    do_shutdown_b = 1;
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
  static int	some_up_b = 0;
  const char	*env_str;
  
  /* have we started already? */
  if (enabled_b) {
    return 0;
  }
  
  if (! some_up_b) {
    /* set this up here so if an error occurs below, it will not try again */
    some_up_b = 1;
    
#if STORE_TIMEVAL
    GET_TIMEVAL(_dmalloc_start);
#else
#if HAVE_TIME /* NOT STORE_TIME */
    _dmalloc_start = time(NULL);
#endif
#endif
    
    /* get the options flag */
    env_str = getenv(OPTIONS_ENVIRON);
    
    /* process the environmental variable(s) */
    process_environ(env_str);
    
    /*
     * Tune the environment here.  If we have a start-file,
     * start-count, or interval enabled then make sure the check-heap
     * flag is cleared.
     */ 
    if (start_file != NULL
	|| start_count > 0
	|| _dmalloc_check_interval > 0) {
      BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
    }
    
    /* startup heap code */
    if (! _heap_startup()) {
      return 0;
    }
    
    /* startup the chunk lower-level code */
    if (! _chunk_startup()) {
      return 0;
    }
    
    /* set leap variables */
#if USE_DMALLOC_LEAP
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
    _dmalloc_debug_setup_func = _dmalloc_debug_setup;
    _dmalloc_examine_func = _dmalloc_examine;
    _dmalloc_vmessage_func = _dmalloc_vmessage;
    _dmalloc_track_func = _dmalloc_track;
    _dmalloc_mark_func = _dmalloc_mark;
    _dmalloc_log_changed_func = _dmalloc_log_changed;
    _dmalloc_strerror_func = _dmalloc_strerror;
#endif
  }
  
#if LOCK_THREADS
  if (thread_lock_c > 0) {
    return 1;
  }
#endif
  
  /*
   * We have initialized all of our code.
   *
   * NOTE: set this up here so if an error occurs below, it will not
   * try again
   */
  enabled_b = 1;
  
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
  
  return 1;
}

/*
 * Shutdown memory-allocation module, provide statistics if necessary
 */
void	_dmalloc_shutdown(void)
{
  /* NOTE: do not generate errors for IN_TWICE here */
  
  /* if we're already in die mode leave fast and quietly */
  if (_dmalloc_aborting_b) {
    return;
  }
  
  /*
   * Make sure that the log file is open.  We do this here because we
   * might cause an allocation in the open() and don't want to go
   * recursive.
   */
  _dmalloc_open_log();
  
#if LOCK_THREADS
  lock_thread();
#endif
  
  /* if we've died in dmalloc somewhere then leave fast and quietly */
  if (in_alloc_b) {
    return;
  }
  
  in_alloc_b = 1;
  
  /*
   * Check the heap since we are dumping info from it.  We check it
   * when check-blank is enabled do make sure all of the areas have
   * not been overwritten.  Thanks Randell.
   */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP)
      || BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_BLANK)) {
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
    _chunk_log_changed(0, 1, 0,
#if DUMP_UNFREED_SUMMARY_ONLY
		       0
#else
		       1
#endif
		       );
  }
  
#if STORE_TIMEVAL
  {
    TIMEVAL_TYPE	now;
    char		time_buf1[64], time_buf2[64];
    GET_TIMEVAL(now);
    _dmalloc_message("ending time = %s, elapsed since start = %s",
		     _dmalloc_ptimeval(&now, time_buf1, sizeof(time_buf1), 0),
		     _dmalloc_ptimeval(&now, time_buf2, sizeof(time_buf2), 1));
  }
#else
#if HAVE_TIME /* NOT STORE_TIME */
  {
    TIME_TYPE	now;
    char	time_buf1[64], time_buf2[64];
    now = time(NULL);
    _dmalloc_message("ending time = %s, elapsed since start = %s",
		     _dmalloc_ptime(&now, time_buf1, sizeof(time_buf1), 0),
		     _dmalloc_ptime(&now, time_buf2, sizeof(time_buf2), 1));
  }
#endif
#endif
  
  in_alloc_b = 0;
  
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

/*
 * Call to the alloc routines has been made.  Do some initialization,
 * locking, and check some debug variables.
 *
 * Returns ERROR or NOERROR.
 */
static	int	dmalloc_in(const char *file, const int line,
			   const int check_heap_b)
{
  if (_dmalloc_aborting_b) {
    return 0;
  }
  
  /*
   * NOTE: we need to do this outside of lock to get env vars
   * otherwise our _dmalloc_lock_on variable won't be initialized and
   * the THREAD_LOCK will flip.
   */
  if (! enabled_b) {
    if (! dmalloc_startup()) {
      return 0;
    }
  }
  
#if LOCK_THREADS
  lock_thread();
#endif
  
  if (in_alloc_b) {
    dmalloc_errno = ERROR_IN_TWICE;
    dmalloc_error("dmalloc_in");
    /* NOTE: dmalloc_error may die already */
    _dmalloc_die(0);
    /*NOTREACHED*/
  }
  
  in_alloc_b = 1;
  
  /* increment our interval */
  _dmalloc_iter_c++;
  
  /* check start file/line specifications */
  if ((! BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP))
      && start_file != NULL
      && file != DMALLOC_DEFAULT_FILE
      && line != DMALLOC_DEFAULT_LINE
      && strcmp(start_file, file) == 0
      && (start_line == 0 || start_line == line)) {
    BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
    /*
     * we disable the start file so we won't check this again and the
     * interval can go on/off
     */
    start_file = NULL;
  }
  
  /* start checking heap after X times */
  else if (start_count > 0) {
    if (--start_count == 0) {
      BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
    }
  }
  
  /* checking heap every X times */
  else if (_dmalloc_check_interval > 0) {
    if (_dmalloc_iter_c % _dmalloc_check_interval == 0) {
      BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
    }
    else { 
      BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
    }
  }
  
  /* after all that, do we need to check the heap? */
  if (check_heap_b && BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP)) {
    (void)_chunk_check();
  }
  
  return 1;
}

/*
 * Going out of the alloc routines back to user space.
 */
static	void	dmalloc_out(void)
{
  in_alloc_b = 0;
  
#if LOCK_THREADS
  unlock_thread();
#endif
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
}

/******************************* memory calls ********************************/

/*
 * Allocate and return a SIZE block of bytes.  FUNC_ID contains the
 * type of function.  If we are aligning our malloc then ALIGNMENT is
 * greater than 0.
 *
 * Returns 0L on error.
 */
DMALLOC_PNT	_loc_malloc(const char *file, const int line,
			    const DMALLOC_SIZE size, const int func_id,
			    const DMALLOC_SIZE alignment)
{
  void		*new_p;
  DMALLOC_SIZE	align;
  
#if DMALLOC_SIZE_UNSIGNED == 0
  if (size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("malloc");
    if (tracking_func != NULL) {
      tracking_func(file, line, func_id, size, alignment, NULL, NULL);
    }
    return MALLOC_ERROR;
  }
#endif
  
  if (! dmalloc_in(file, line, 1)) {
    if (tracking_func != NULL) {
      tracking_func(file, line, func_id, size, alignment, NULL, NULL);
    }
    return MALLOC_ERROR;
  }
  
  if (alignment == 0) {
    align = 0;
  }
  else if (alignment >= BLOCK_SIZE) {
    align = BLOCK_SIZE;
  }
  else {
    /*
     * NOTE: Currently, there is no support in the library for
     * memalign on less than block boundaries.  It will be non-trivial
     * to support valloc with fence-post checking and the lack of the
     * flag width for dblock allocations.
     */
    if (! memalign_warn_b) {
      _dmalloc_message("WARNING: memalign called without library support");
      memalign_warn_b = 1;
    }
    align = 0;
    /* align = alignment */
  }
  
  new_p = _chunk_malloc(file, line, size, func_id, align);
  
  check_pnt(file, line, new_p, "malloc");
  
  dmalloc_out();
  
  if (tracking_func != NULL) {
    tracking_func(file, line, func_id, size, alignment, NULL, new_p);
  }
  
  return new_p;
}

/*
 * Resizes OLD_PNT to NEW_SIZE bytes and return the new space after
 * either copying all of OLD_PNT to the new area or truncating.  If
 * OLD_PNT is 0L then it will do the equivalent of malloc(NEW_SIZE).
 * If NEW_SIZE is 0 and OLD_PNT is not 0L then it will do the
 * equivalent of free(OLD_PNT) and will return 0L.  If the RECALLOC_B
 * flag is enabled, it will zero any new memory.
 *
 * Returns 0L on error.
 */
DMALLOC_PNT	_loc_realloc(const char *file, const int line,
			     DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size,
			     const int func_id)
{
  void		*new_p;
  
#if DMALLOC_SIZE_UNSIGNED == 0
  if (new_size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("realloc");
    if (tracking_func != NULL) {
      tracking_func(file, line, func_id, new_size, 0, old_pnt, NULL);
    }
    return MALLOC_ERROR;
  }
#endif
  
  if (! dmalloc_in(file, line, 1)) {
    if (tracking_func != NULL) {
      tracking_func(file, line, func_id, new_size, 0, old_pnt, NULL);
    }
    return REALLOC_ERROR;
  }
  
  check_pnt(file, line, old_pnt, "realloc-in");
  
#if ALLOW_REALLOC_NULL
  if (old_pnt == NULL) {
    int		new_func_id;
    /* shift the function id to be calloc or malloc */
    if (func_id == DMALLOC_FUNC_RECALLOC) {
      new_func_id = DMALLOC_FUNC_CALLOC;
    }
    else {
      new_func_id = DMALLOC_FUNC_MALLOC;
    }
    new_p = _chunk_malloc(file, line, new_size, new_func_id, 0);
  }
  else
#endif
#if ALLOW_REALLOC_SIZE_ZERO
    if (new_size == 0) {
      /*
       * If realloc(old_pnt, 0) then free(old_pnt).  Thanks to Stefan
       * Froehlich for patiently pointing that the realloc in just
       * about every Unix has this functionality.
       */
      (void)_chunk_free(file, line, old_pnt, 0);
      new_p = NULL;
    }
    else
#endif
      new_p = _chunk_realloc(file, line, old_pnt, new_size, func_id);
  
  if (new_p != NULL) {
    check_pnt(file, line, new_p, "realloc-out");
  }
  
  dmalloc_out();
  
  if (tracking_func != NULL) {
    tracking_func(file, line, func_id, new_size, 0, old_pnt, new_p);
  }
  
  return new_p;
}

/*
 * Release PNT in the heap.
 *
 * Returns FREE_ERROR, FREE_NOERROR.
 */
int	_loc_free(const char *file, const int line, DMALLOC_PNT pnt)
{
  int		ret;
  
  if (! dmalloc_in(file, line, 1)) {
    if (tracking_func != NULL) {
      tracking_func(file, line, DMALLOC_FUNC_FREE, 0, 0, pnt, NULL);
    }
    return FREE_ERROR;
  }
  
  check_pnt(file, line, pnt, "free");
  
  ret = _chunk_free(file, line, pnt, 0);
  
  dmalloc_out();
  
  if (tracking_func != NULL) {
    tracking_func(file, line, DMALLOC_FUNC_FREE, 0, 0, pnt, NULL);
  }
  
  return ret;
}

/*************************** external memory calls ***************************/

/*
 * Allocate and return a SIZE block of bytes.
 *
 * Returns 0L on error.
 */
#undef malloc
DMALLOC_PNT	malloc(DMALLOC_SIZE size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, size, DMALLOC_FUNC_MALLOC, 0);
}

/*
 * Allocate and return a block of _zeroed_ bytes able to hold
 * NUM_ELEMENTS, each element contains SIZE bytes.
 *
 * Returns 0L on error.
 */
#undef calloc
DMALLOC_PNT	calloc(DMALLOC_SIZE num_elements, DMALLOC_SIZE size)
{
  DMALLOC_SIZE	len = num_elements * size;
  char		*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, len, DMALLOC_FUNC_CALLOC, 0);
}

/*
 * Resizes OLD_PNT to NEW_SIZE bytes and return the new space after
 * either copying all of OLD_PNT to the new area or truncating.  If
 * OLD_PNT is 0L then it will do the equivalent of malloc(NEW_SIZE).
 * If NEW_SIZE is 0 and OLD_PNT is not 0L then it will do the
 * equivalent of free(OLD_PNT) and will return 0L.
 *
 * Returns 0L on error.
 */
#undef realloc
DMALLOC_PNT	realloc(DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_realloc(file, DMALLOC_DEFAULT_LINE, old_pnt, new_size,
		      DMALLOC_FUNC_REALLOC);
}

/*
 * Resizes OLD_PNT to NEW_SIZE bytes and return the new space after
 * either copying all of OLD_PNT to the new area or truncating.  If
 * OLD_PNT is 0L then it will do the equivalent of malloc(NEW_SIZE).
 * If NEW_SIZE is 0 and OLD_PNT is not 0L then it will do the
 * equivalent of free(OLD_PNT) and will return 0L.  Any extended
 * memory space will be zeroed like calloc.
 *
 * Returns 0L on error.
 */
#undef recalloc
DMALLOC_PNT	recalloc(DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_realloc(file, DMALLOC_DEFAULT_LINE, old_pnt, new_size,
		      DMALLOC_FUNC_RECALLOC);
}

/*
 * Allocate and return a SIZE block of bytes that has been aligned to
 * ALIGNMENT bytes.  ALIGNMENT must be a power of two and must be less
 * than or equal to the block-size.
 *
 * Returns 0L on error.
 */
#undef memalign
DMALLOC_PNT	memalign(DMALLOC_SIZE alignment, DMALLOC_SIZE size)
{
  char		*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, size, DMALLOC_FUNC_MEMALIGN,
		     alignment);
}

/*
 * Allocate and return a SIZE block of bytes that has been aligned to
 * a page-size.
 *
 * Returns 0L on error.
 */
#undef valloc
DMALLOC_PNT	valloc(DMALLOC_SIZE size)
{
  char	*file;
  
  GET_RET_ADDR(file);
  return _loc_malloc(file, DMALLOC_DEFAULT_LINE, size, DMALLOC_FUNC_VALLOC,
		     BLOCK_SIZE);
}

#ifndef DMALLOC_STRDUP_MACRO
/*
 * Allocate and return a block of bytes that contains the string STR
 * including the \0.
 *
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
  
  buf = (char *)_loc_malloc(file, DMALLOC_DEFAULT_LINE, len,
			    DMALLOC_FUNC_STRDUP, 0);
  if (buf != NULL) {
    (void)memcpy(buf, str, len);
  }
  
  return buf;
}
#endif

/*
 * Release PNT in the heap.
 *
 * Returns FREE_ERROR, FREE_NOERROR or void depending on whether STDC
 * is defined by your compiler.
 */
#undef free
DMALLOC_FREE_RET	free(DMALLOC_PNT pnt)
{
  char	*file;
  int	ret;
  
  GET_RET_ADDR(file);
  ret = _loc_free(file, DMALLOC_DEFAULT_LINE, pnt);
  
#if (defined(__STDC__) && __STDC__ == 1) || defined(__cplusplus)
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
  
#if (defined(__STDC__) && __STDC__ == 1) || defined(__cplusplus)
#else
  return ret;
#endif
}

/******************************* utility calls *******************************/

/*
 * Log the heap structure plus information on the blocks if necessary.
 */
void	_dmalloc_log_heap_map(const char *file, const int line)
{
  /* check the heap since we are dumping info from it */
  if (! dmalloc_in(file, line, 1)) {
    return;
  }
  
  _chunk_log_heap_map();
  
  dmalloc_out();
}

/*
 * Dump dmalloc statistics to logfile.
 */
void	_dmalloc_log_stats(const char *file, const int line)
{
  if (! dmalloc_in(file, line, 1)) {
    return;
  }
  
  _chunk_list_count();
  _chunk_stats();
  
  dmalloc_out();
}

/*
 * Dump unfreed-memory info to logfile.
 */
void	_dmalloc_log_unfreed(const char *file, const int line)
{
  if (! dmalloc_in(file, line, 1)) {
    return;
  }
  
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("dumping the unfreed pointers");
  }
  
  /*
   * to log the non-free we are interested in the pointers currently
   * being used
   */
  _chunk_log_changed(0, 1, 0,
#if DUMP_UNFREED_SUMMARY_ONLY
		     0
#else
		     1
#endif
		     );
  
  dmalloc_out();
}

/*
 * Verify pointer PNT, if PNT is 0 then check the entire heap.
 *
 * Returns MALLOC_VERIFY_ERROR or MALLOC_VERIFY_NOERROR
 */
int	_dmalloc_verify(const char *file, const int line,
			const DMALLOC_PNT pnt)
{
  int	ret;
  
  if (! dmalloc_in(file, line, 0)) {
    return MALLOC_VERIFY_NOERROR;
  }
  
  /* should not check heap here because we will be doing it below */
  
  if (pnt == NULL) {
    ret = _chunk_check();
  }
  else {
    ret = _chunk_pnt_check("dmalloc_verify", pnt, CHUNK_PNT_EXACT, 0);
  }
  
  dmalloc_out();
  
  if (ret) {
    return MALLOC_VERIFY_NOERROR;
  }
  else {
    return MALLOC_VERIFY_ERROR;
  }
}

/*
 * Verify pointer PNT, if PNT is 0 then check the entire heap.
 *
 * Returns MALLOC_VERIFY_ERROR or MALLOC_VERIFY_NOERROR
 */
int	malloc_verify(const DMALLOC_PNT pnt)
{
  char	*file;
  
  GET_RET_ADDR(file);
  
  return _dmalloc_verify(file, DMALLOC_DEFAULT_LINE, pnt);
}

/*
 * Set the global debug functionality FLAGS (0 to disable all
 * debugging).
 *
 * NOTE: you cannot remove certain flags such as signal handlers since
 * they are setup at initialization time only.  Also you cannot add
 * certain flags such as free-space checking since they must be on
 * from the start.
 *
 * Returns the old debug flag value.
 */
unsigned int	_dmalloc_debug(const unsigned int flags)
{
  unsigned int	old_flags;
  
  if (! enabled_b) {
    (void)dmalloc_startup();
  }
  
  old_flags = _dmalloc_flags;
  
  /* make sure that the not-removable flags are preserved */
  _dmalloc_flags &= DEBUG_NOT_REMOVABLE;
  
  /* add the new flags - the not-addable ones */
  _dmalloc_flags |= flags & ~DEBUG_NOT_ADDABLE;
  
  return old_flags;
}

/*
 * unsigned int _dmalloc_debug_current
 *
 * DESCRIPTION:
 *
 * Returns the current debug functionality flags.  This allows you to
 * save a dmalloc library state to be restored later.
 *
 * RETURNS:
 *
 * Current debug flags.
 *
 * ARGUMENTS:
 *
 * None.
 */
unsigned int	_dmalloc_debug_current(void)
{
  if (! enabled_b) {
    (void)dmalloc_startup();
  }
  
  /* should not check the heap here since we are dumping the debug variable */
  return _dmalloc_flags;
}

/*
 * void _dmalloc_debug_setup
 *
 * DESCRIPTION:
 *
 * Set the global debugging functionality as an option string.
 * Normally this would be pased in in the DMALLOC_OPTIONS
 * environmental variable.  This is here to override the env or for
 * circumstances where it does not apply.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * options_str -> Options string to set the library flags.
 */
void	_dmalloc_debug_setup(const char *options_str)
{
  if (! enabled_b) {
    (void)dmalloc_startup();
  }
  
  process_environ(options_str);
}

/*
 * int _dmalloc_examine
 *
 * DESCRIPTION:
 *
 * Examine a pointer and return information on its allocation size as
 * well as the file and line-number where it was allocated.  If the
 * file and line number is not available, then it will return the
 * allocation location's return-address if available.
 *
 * RETURNS:
 *
 * Success - DMALLOC_NOERROR
 *
 * Failure - DMALLOC_ERROR
 *
 * ARGUMENTS:
 *
 * file -> File were we are examining the pointer.
 *
 * line -> Line-number from where we are examining the pointer.
 *
 * pnt -> Pointer we are checking.
 *
 * size_p <- Pointer to an unsigned int which, if not NULL, will be
 * set to the size of bytes from the pointer.
 *
 * file_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the file where the pointer was allocated.
 *
 * line_p <- Pointer to a character pointer which, if not NULL, will
 * be set to the line-number where the pointer was allocated.
 *
 * ret_attr_p <- Pointer to a void pointer, if not NULL, will be set
 * to the return-address where the pointer was allocated.
 */
int	_dmalloc_examine(const char *file, const int line,
			 const DMALLOC_PNT pnt, DMALLOC_SIZE *size_p,
			 char **file_p, unsigned int *line_p,
			 DMALLOC_PNT *ret_attr_p)
{
  int		ret;
  unsigned int	size_map;
  
  /* need to check the heap here since we are geting info from it below */
  if (! dmalloc_in(file, line, 1)) {
    return DMALLOC_ERROR;
  }
  
  /* NOTE: we do not need the alloc-size info */
  ret = _chunk_read_info(pnt, "dmalloc_examine", &size_map, NULL, file_p,
			 line_p, ret_attr_p, NULL, NULL, NULL);
  
  dmalloc_out();
  
  if (ret) {
    if (size_p != NULL) {
      *size_p = size_map;
    }
    return DMALLOC_NOERROR;
  }
  else {
    return DMALLOC_ERROR;
  }
}

/*
 * Register an allocation tracking function which will be called each
 * time an allocation occurs.  Pass in NULL to disable.
 */
void	_dmalloc_track(const dmalloc_track_t track_func)
{
  tracking_func = track_func;
}

/*
 * Return to the caller the current ``mark'' which can be used later
 * to dmalloc_log_changed pointers since this point.  Multiple marks
 * can be saved and used.
 */
unsigned long	_dmalloc_mark(void)
{
  if (! enabled_b) {
    (void)dmalloc_startup();
  }
  
  return _dmalloc_iter_c;
}

/*
 * Dump the pointers that have changed since the mark which was
 * returned by dmalloc_mark.  If not_freed_b is set to non-0 then log
 * the new pointers that are non-freed.  If free_b is set to non-0
 * then log the new pointers that are freed.  If details_b set to
 * non-0 then dump the individual pointers that have changed otherwise
 * just dump the summaries.
 */
void	_dmalloc_log_changed(const char *file, const int line,
			     const unsigned long mark, const int not_freed_b,
			     const int free_b, const int details_b)
{
  if (! dmalloc_in(file, line, 1)) {
    return;
  }
  
  _chunk_log_changed(mark, not_freed_b, free_b, details_b);
  
  dmalloc_out();
}

/*
 * Dmalloc version of strerror to return the string version of
 * ERROR_NUM.
 *
 * Returns an invaid errno string if ERROR_NUM is out-of-range.
 */
const char	*_dmalloc_strerror(const int error_num)
{
  error_str_t	*err_p;
  
  /* should not dmalloc_in here because _dmalloc_error calls this */
  
  for (err_p = error_list; err_p->es_error != 0; err_p++) {
    if (err_p->es_error == error_num) {
      return err_p->es_string;
    }
  }
  
  return INVALID_ERROR;
}
