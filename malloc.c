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
 * The author may be contacted at gray.watson@letters.com
 */

/*
 * This file contains the user-level calls to the memory allocation
 * routines.  It handles a lot of the miscellaneous support garbage for
 * chunk.c which is the real heap manager.
 */

#include "conf.h"				/* up here for _INCLUDE */
#include "return.h"				/* up here for asm includes */

/* for timeval type -- see conf.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
#endif

#if LOCK_THREADS
#ifdef THREAD_INCLUDE
#include THREAD_INCLUDE
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

#if INCLUDE_RCS_IDS
static	char	*rcs_id =
  "$Id: malloc.c,v 1.101 1997/12/22 00:25:47 gray Exp $";
#endif

/*
 * exported variables
 */
/*
 * argument to dmalloc_address, if 0 then never call dmalloc_error()
 * else call it after seeing dmalloc_address for this many times.
 */
int		dmalloc_address_count = ADDRESS_COUNT_INIT;

/* local routines */
static	int		dmalloc_startup(void);
void		_dmalloc_shutdown(void);
void		_dmalloc_log_heap_map(void);
void		_dmalloc_log_stats(void);
void		_dmalloc_log_unfreed(void);
int		_dmalloc_verify(const DMALLOC_PNT pnt);
int		_malloc_verify(const DMALLOC_PNT pnt);
void		_dmalloc_debug(const int flags);
int		_dmalloc_debug_current(void);
int		_dmalloc_examine(const DMALLOC_PNT pnt, DMALLOC_SIZE * size,
				 char ** file, unsigned int * line,
				 DMALLOC_PNT * ret_attr);
const char	*_dmalloc_strerror(const int error_num);


/* local variables */
static	int		enabled_b	= FALSE; /* have we started yet? */
static	int		in_alloc_b	= FALSE; /* can't be here twice */
static	int		do_shutdown_b	= FALSE; /* execute shutdown soon */

#ifdef THREAD_LOCK_GLOBAL
/* define the global thread-lock variable(s) if needed */
THREAD_LOCK_GLOBAL
#endif

/* debug variables */
static	char		*start_file = START_FILE_INIT; /* file to start at */
static	int		start_line = START_LINE_INIT; /* line to start */
static	int		start_count = START_COUNT_INIT; /* start after X */
static	int		check_interval = INTERVAL_INIT; /* check every X */
static	int		thread_lock_on = LOCK_ON_INIT;	/* turn locking on */

/****************************** local utilities ******************************/

/*
 * a call to the alloc routines has been made, check the debug variables
 * returns ERROR or NOERROR.
 */
static	int	check_debug_vars(const char * file, const int line)
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
  
  THREAD_LOCK;
  
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
static	void	check_pnt(const char * file, const int line, const void * pnt,
			  const char * label)
{
  static int	addc = 0;
  
  if (dmalloc_address == ADDRESS_INIT || pnt != dmalloc_address) {
    return;
  }
  
  addc++;
  _dmalloc_message("address '%#lx' found in '%s' at pass %d from '%s'",
		   pnt, label, addc, _chunk_display_where(file, line));
  
  /* NOTE: if dmalloc_address_count == 0 then never quit */
  if (dmalloc_address_count == ADDRESS_COUNT_INIT
      || (dmalloc_address_count > 0 && addc >= dmalloc_address_count)) {
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
		       &dmalloc_address_count, &_dmalloc_flags,
		       &check_interval, &thread_lock_on, &dmalloc_logpath,
		       &start_file, &start_line, &start_count);
  
  /* if it was not set then no flags set */
  if (_dmalloc_flags == DEBUG_INIT) {
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
  _dmalloc_shutdown_func = _dmalloc_shutdown;
  _dmalloc_log_heap_map_func = _dmalloc_log_heap_map;
  _dmalloc_log_stats_func = _dmalloc_log_stats;
  _dmalloc_log_unfreed_func = _dmalloc_log_unfreed;
  _dmalloc_verify_func = _dmalloc_verify;
  _malloc_verify_func = _malloc_verify;
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
  {
    unsigned int	line_hold = _dmalloc_line;
    char		*file_hold = _dmalloc_file;
    
    /* NOTE: I use the else here in case some dumb systems has both */
#if HAVE_ATEXIT
    (void)atexit(_dmalloc_shutdown);
#else
#if HAVE_ON_EXIT
    (void)on_exit(_dmalloc_shutdown, NULL);
#endif
#endif
    
    _dmalloc_line = line_hold;
    _dmalloc_file = file_hold;
  }
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
  
  THREAD_LOCK;
  
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
  
  THREAD_UNLOCK;
  
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
 * allocate and return a SIZE block of bytes.  returns 0L on error.
 */
#undef malloc
DMALLOC_PNT	malloc(DMALLOC_SIZE size)
{
  void		*new_p;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
#if DMALLOC_SIZE_UNSIGNED == 0
  if (size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("malloc");
    return MALLOC_ERROR;
  }
#endif
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
    return MALLOC_ERROR;
  }
  
  new_p = _chunk_malloc(_dmalloc_file, _dmalloc_line, size);
  in_alloc_b = FALSE;
  
  check_pnt(_dmalloc_file, _dmalloc_line, new_p, "malloc");
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  return new_p;
}

/*
 * allocate and return a block of _zeroed_ bytes able to hold
 * NUM_ELEMENTS, each element contains SIZE bytes.  returns 0L on
 * error.
 */
#undef calloc
DMALLOC_PNT	calloc(DMALLOC_SIZE num_elements, DMALLOC_SIZE size)
{
  void		*new_p;
  DMALLOC_SIZE	len = num_elements * size;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  /* needs to be done here */
  _calloc_count++;
  
  new_p = malloc(len);
  if (new_p != MALLOC_ERROR && len > 0) {
    (void)memset(new_p, 0, len);
  }
  
  return new_p;
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
  void		*new_p;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
#if DMALLOC_SIZE_UNSIGNED == 0
  if (new_size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("realloc");
    return MALLOC_ERROR;
  }
#endif
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
    return REALLOC_ERROR;
  }
  
  check_pnt(_dmalloc_file, _dmalloc_line, old_pnt, "realloc-in");
  
#if ALLOW_REALLOC_NULL
  if (old_pnt == NULL) {
    new_p = _chunk_malloc(_dmalloc_file, _dmalloc_line, new_size);
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
      (void)_chunk_free(_dmalloc_file, _dmalloc_line, old_pnt);
      new_p = NULL;
    }
    else
#endif
      new_p = _chunk_realloc(_dmalloc_file, _dmalloc_line, old_pnt, new_size);
  
  in_alloc_b = FALSE;
  
  if (new_p != NULL) {
    check_pnt(_dmalloc_file, _dmalloc_line, new_p, "realloc-out");
  }
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  return new_p;
}

/*
 * release PNT in the heap, returning FREE_ERROR, FREE_NOERROR or void
 * depending on whether STDC is defined by your compiler.
 */
#undef free
DMALLOC_FREE_RET	free(DMALLOC_PNT pnt)
{
  int		ret;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
#if defined(__STDC__) && __STDC__ == 1
    return;
#else
    return FREE_ERROR;
#endif
  }
  
  check_pnt(_dmalloc_file, _dmalloc_line, pnt, "free");
  
#if ALLOW_FREE_NULL
  if (pnt == NULL) {
#if ALLOW_FREE_NULL_MESSAGE
    _dmalloc_message("WARNING: tried to free(0) from '%s'",
		     _chunk_display_where(_dmalloc_file, _dmalloc_line));
#endif
    ret = FREE_NOERROR;
  }
  else
    ret = _chunk_free(_dmalloc_file, _dmalloc_line, pnt);
#else /* ! ALLOW_FREE_NULL */
  ret = _chunk_free(_dmalloc_file, _dmalloc_line, pnt);
#endif
  
  in_alloc_b = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
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
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
#if defined(__STDC__) && __STDC__ == 1
  free(pnt);
#else
  return free(pnt);
#endif
}

/******************************* utility calls *******************************/

/*
 * log the heap structure plus information on the blocks if necessary.
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_log_heap_map(void)
{
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  /* check the heap since we are dumping info from it */
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
    return;
  }
  
  _chunk_log_heap_map();
  
  in_alloc_b = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
}

/*
 * dump dmalloc statistics to logfile
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_log_stats(void)
{
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
    return;
  }
  
  _chunk_list_count();
  _chunk_stats();
  
  in_alloc_b = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
}

/*
 * dump unfreed-memory info to logfile
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
void	_dmalloc_log_unfreed(void)
{
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
    return;
  }
  
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS)) {
    _dmalloc_message("dumping the unfreed pointers");
  }
  
  _chunk_dump_unfreed();
  
  in_alloc_b = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
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
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  /* should not check heap here because we will be doing it below */
  
  if (_dmalloc_aborting_b) {
    return DMALLOC_VERIFY_ERROR;
  }
  
  THREAD_LOCK;
  
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
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
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
 * same as dmalloc_verify
 */
int	_malloc_verify(const DMALLOC_PNT pnt)
{
  return _dmalloc_verify(pnt);
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
int	_dmalloc_examine(const DMALLOC_PNT pnt, DMALLOC_SIZE *size,
			 char **file, unsigned int *line,
			 DMALLOC_PNT *ret_attr)
{
  int		ret;
  unsigned int	size_map;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  /* need to check the heap here since we are geting info from it below */
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR) {
    return ERROR;
  }
  
  /* NOTE: we do not need the alloc-size info */
  ret = _chunk_read_info(pnt, &size_map, NULL, file, line, ret_attr,
			 "dmalloc_examine", NULL);
  in_alloc_b = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  THREAD_UNLOCK;
  
  if (do_shutdown_b) {
    _dmalloc_shutdown();
  }
  
  if (ret == NOERROR) {
    if (size != NULL) {
      *size = size_map;
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
