/*
 * user-level memory-allocation routines
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
 * This file contains the user-level calls to the memory allocation
 * routines.  It handles a lot of the miscellaneous support garbage for
 * chunk.c which is the real heap manager.
 */

/* for timeval type -- see conf.h */
#if STORE_TIMEVAL
#ifdef TIMEVAL_INCLUDE
#include TIMEVAL_INCLUDE
#endif
#endif

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

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
LOCAL	char	*rcs_id =
  "$Id: malloc.c,v 1.75 1995/05/12 20:34:35 gray Exp $";
#endif

/*
 * exported variables
 */
/* logfile for dumping dmalloc info, DMALLOC_LOGFILE env var overrides this */
EXPORT	char		*dmalloc_logpath = LOGPATH_INIT;
/* internal dmalloc error number for reference purposes only */
EXPORT	int		dmalloc_errno = ERROR_NONE;
/* address to look for.  when discovered call dmalloc_error() */
EXPORT	DMALLOC_PNT	dmalloc_address = ADDRESS_INIT;
/*
 * argument to dmalloc_address, if 0 then never call dmalloc_error()
 * else call it after seeing dmalloc_address for this many times.
 */
EXPORT	int		dmalloc_address_count = ADDRESS_COUNT_INIT;

/* local routines */
LOCAL	int		dmalloc_startup(void);
EXPORT	void		_dmalloc_shutdown(void);
EXPORT	void		_dmalloc_log_heap_map(void);
EXPORT	void		_dmalloc_log_stats(void);
EXPORT	void		_dmalloc_log_unfreed(void);
EXPORT	int		_dmalloc_verify(const DMALLOC_PNT pnt);
EXPORT	int		_malloc_verify(const DMALLOC_PNT pnt);
EXPORT	void		_dmalloc_debug(const int debug);
EXPORT	int		_dmalloc_debug_current(void);
EXPORT	int		_dmalloc_examine(const DMALLOC_PNT pnt, DMALLOC_SIZE * size,
					 char ** file, unsigned int * line,
					 DMALLOC_PNT * ret_attr);
EXPORT	char		*_dmalloc_strerror(const int errnum);


/* local variables */
LOCAL	char		enabled		= FALSE; /* have we started yet? */
LOCAL	char		in_alloc	= FALSE; /* can't be here twice */

/* debug variables */
LOCAL	char		*start_file = START_FILE_INIT; /* file to start at */
LOCAL	int		start_line = START_LINE_INIT; /* line to start */
LOCAL	int		start_count = START_COUNT_INIT; /* start after X */
LOCAL	int		check_interval = INTERVAL_INIT; /* check every X */

/****************************** local utilities ******************************/

/*
 * a call to the alloc routines has been made, check the debug variables
 * returns ERROR or NOERROR.
 */
LOCAL	int	check_debug_vars(const char * file, const int line)
{
  if (in_alloc) {
    dmalloc_errno = ERROR_IN_TWICE;
    dmalloc_error("check_debug_vars");
    /* NOTE: dmalloc_error may die already */
    _dmalloc_die();
    /*NOTREACHED*/
  }
  
  in_alloc = TRUE;
  
  if (! enabled)
    if (dmalloc_startup() != NOERROR)
      return ERROR;
  
  /* check start file/line specifications */
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP)
      && start_file != START_FILE_INIT
      && file != DMALLOC_DEFAULT_FILE
      && line != DMALLOC_DEFAULT_LINE
      && strcmp(start_file, file) == 0
      && (start_line == 0 || start_line == line))
    BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
  
  /* start checking heap after X times */
  if (start_count != START_COUNT_INIT && --start_count == 0)
    BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
  
  /* checking heap every X times */
  _dmalloc_iterc++;
  if (check_interval != INTERVAL_INIT && check_interval > 0) {
    if (_dmalloc_iterc % check_interval == 0)
      BIT_SET(_dmalloc_flags, DEBUG_CHECK_HEAP);
    else
      BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
  }
  
  /* after all that, do we need to check the heap? */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP))
    (void)_chunk_heap_check();
  
  return NOERROR;
}

/*
 * check out a pointer to see if we were looking for it.  may not
 * return.
 */
LOCAL	void	check_pnt(const char * file, const int line, char * pnt,
			  const char * label)
{
  static int	addc = 0;
  
  if (dmalloc_address == ADDRESS_INIT || pnt != dmalloc_address)
    return;
  
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
 * process the values of dmalloc environ variable(s)
 */
LOCAL	void	process_environ(void)
{
  _dmalloc_environ_get(OPTIONS_ENVIRON, (unsigned long *)&dmalloc_address,
		       &dmalloc_address_count, &_dmalloc_flags,
		       &check_interval, &dmalloc_logpath,
		       &start_file, &start_line, &start_count);
  
  /* if it was not set then no flags set */
  if (_dmalloc_flags == DEBUG_INIT)
    _dmalloc_flags = 0;
  
  /* if we set the start stuff, then check-heap comes on later */
  if (start_count != -1)
    BIT_CLEAR(_dmalloc_flags, DEBUG_CHECK_HEAP);
  
  /* override dmalloc_debug value if we we call dmalloc_debug() already */
  if (_dmalloc_debug_preset != DEBUG_PRE_NONE)
    _dmalloc_flags = _dmalloc_debug_preset;
}

/************************** startup/shutdown calls ***************************/

/*
 * startup the memory-allocation module
 */
LOCAL	int	dmalloc_startup(void)
{
  /* have we started already? */
  if (enabled)
    return ERROR;
  
  /* set this here so if an error occurs below, it will not try again */
  enabled = TRUE;
  
#if HAVE_TIME /* NOT STORE_TIME */ && STORE_TIMEVAL == 0
  _dmalloc_start = time(NULL);
#endif
#if STORE_TIMEVAL
  GET_TIMEVAL(_dmalloc_start);
#endif
  
  /* process the environmental variable(s) */
  process_environ();
  
  /* startup heap code */
  if (_heap_startup() == ERROR)
    return ERROR;
  
  /* startup the chunk lower-level code */
  if (_chunk_startup() == ERROR)
    return ERROR;
  
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
  
#if AUTO_SHUTDOWN
  {
    unsigned int	line_hold = _dmalloc_line;
    char		*file_hold = _dmalloc_file;
    
    /*
     * HACK: we have to disable the in_alloc because we might be about
     * to go recursize.  this should not get back here since dmalloc
     * has been enabled.
     */
    in_alloc = FALSE;
    
    /* NOTE: I use the else here in case some dumb systems has both */
#if HAVE_ATEXIT
    (void)atexit(_dmalloc_shutdown);
#else
#if HAVE_ON_EXIT
    (void)on_exit(_dmalloc_shutdown, NULL);
#endif
#endif
    
    in_alloc = TRUE;
    
    _dmalloc_line = line_hold;
    _dmalloc_file = file_hold;
  }
#endif /* AUTO_SHUTDOWN */
  
  return NOERROR;
}

/*
 * shutdown memory-allocation module, provide statistics if necessary
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
EXPORT	void	_dmalloc_shutdown(void)
{
  /* NOTE: do not generate errors for IN_TWICE here */
  
  /* if we've died in dmalloc somewhere then leave fast and quietly */
  if (in_alloc)
    return;
  
  /* check the heap since we are dumping info from it */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_HEAP))
    (void)_chunk_heap_check();
  
  /* dump some statistics to the logfile */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_STATS))
    _chunk_list_count();
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_STATS))
    _chunk_stats();
  
  /* report on non-freed pointers */
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_NONFREE))
    _chunk_dump_unfreed();
  
#if HAVE_TIME /* NOT STORE_TIME */ && STORE_TIMEVAL == 0
  {
    long	now = time(NULL);
    _dmalloc_message("ending time = %ld, elapsed since start = %s",
		     now, _dmalloc_ptime(&now, TRUE));
  }
#endif
  
#if STORE_TIMEVAL
  {
    struct timeval	now;
    GET_TIMEVAL(now);
    _dmalloc_message("ending time = %ld.%ld, elapsed since start = %s",
		     now.tv_sec, now.tv_usec, _dmalloc_ptime(&now, TRUE));
  }
#endif
  /* NOTE: do not set enabled to false here */
}

/******************************* memory calls ********************************/

/*
 * allocate and return a SIZE block of bytes.  returns 0L on error.
 */
EXPORT	DMALLOC_PNT	malloc(DMALLOC_SIZE size)
{
  void		*newp;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR)
    return MALLOC_ERROR;
  
  /* yes, this could be always false if size is unsigned */
  if (size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("malloc");
    return MALLOC_ERROR;
  }
  
  newp = _chunk_malloc(_dmalloc_file, _dmalloc_line, size);
  check_pnt(_dmalloc_file, _dmalloc_line, newp, "malloc");
  
  in_alloc = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  return newp;
}

/*
 * allocate and return a block of bytes able to hold NUM_ELEMENTS of elements
 * of SIZE bytes and zero the block.  returns 0L on error.
 */
EXPORT	DMALLOC_PNT	calloc(DMALLOC_SIZE num_elements, DMALLOC_SIZE size)
{
  void		*newp;
  DMALLOC_SIZE	len = num_elements * size;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  /* needs to be done here */
  _calloc_count++;
  
  newp = malloc(len);
  if (newp != MALLOC_ERROR && len > 0)
    (void)memset(newp, NULLC, len);
  
  return newp;
}

/*
 * resizes OLD_PNT to SIZE bytes and return the new space after either copying
 * all of OLD_PNT to the new area or truncating.  returns 0L on error.
 */
EXPORT	DMALLOC_PNT	realloc(DMALLOC_PNT old_pnt, DMALLOC_SIZE new_size)
{
  void		*newp;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR)
    return REALLOC_ERROR;
  
  check_pnt(_dmalloc_file, _dmalloc_line, old_pnt, "realloc-in");
  
  /* yes, this could be always false if new_size is unsigned */
  if (new_size < 0) {
    dmalloc_errno = ERROR_BAD_SIZE;
    dmalloc_error("realloc");
    return MALLOC_ERROR;
  }
  
#if ALLOW_REALLOC_NULL
  if (old_pnt == NULL)
    newp = _chunk_malloc(_dmalloc_file, _dmalloc_line, new_size);
  else
#endif
    newp = _chunk_realloc(_dmalloc_file, _dmalloc_line, old_pnt, new_size);
  
  check_pnt(_dmalloc_file, _dmalloc_line, newp, "realloc-out");
  
  in_alloc = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
  return newp;
}

/*
 * release PNT in the heap, returning FREE_ERROR, FREE_NOERROR or void
 * depending on whether STDC is defined by your compiler.
 */
EXPORT	DMALLOC_FREE_RET	free(DMALLOC_PNT pnt)
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
		     _chunk_display_pnt(_dmalloc_file, _dmalloc_line));
#endif
    ret = FREE_NOERROR;
  }
  else
    ret = _chunk_free(_dmalloc_file, _dmalloc_line, pnt);
#else /* ! ALLOW_FREE_NULL */
  ret = _chunk_free(_dmalloc_file, _dmalloc_line, pnt);
#endif
  
  in_alloc = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
  
#if defined(__STDC__) && __STDC__ == 1
#else
  return ret;
#endif
}

/*
 * same as free(PNT)
 */
EXPORT	DMALLOC_FREE_RET	cfree(DMALLOC_PNT pnt)
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
EXPORT	void	_dmalloc_log_heap_map(void)
{
  /* check the heap since we are dumping info from it */
  if (check_debug_vars(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE) != NOERROR)
    return;
  
  _chunk_log_heap_map();
  
  in_alloc = FALSE;
}

/*
 * dump dmalloc statistics to logfile
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
EXPORT	void	_dmalloc_log_stats(void)
{
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR)
    return;
  
  _chunk_list_count();
  _chunk_stats();
  
  in_alloc = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
}

/*
 * dump unfreed-memory info to logfile
 * NOTE: called by way of leap routine in dmalloc_lp.c
 */
EXPORT	void	_dmalloc_log_unfreed(void)
{
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (check_debug_vars(_dmalloc_file, _dmalloc_line) != NOERROR)
    return;
  
  if (! BIT_IS_SET(_dmalloc_flags, DEBUG_LOG_TRANS))
    _dmalloc_message("dumping the unfreed pointers");
  
  _chunk_dump_unfreed();
  
  in_alloc = FALSE;
  
  _dmalloc_file = DMALLOC_DEFAULT_FILE;
  _dmalloc_line = DMALLOC_DEFAULT_LINE;
}

/*
 * verify pointer PNT, if PNT is 0 then check the entire heap.
 * NOTE: called by way of leap routine in dmalloc_lp.c
 * returns MALLOC_VERIFY_ERROR or MALLOC_VERIFY_NOERROR
 */
EXPORT	int	_dmalloc_verify(const DMALLOC_PNT pnt)
{
  int	ret;
  
  /* should not check heap here because we will be doing it below */
  
  if (in_alloc) {
    dmalloc_errno = ERROR_IN_TWICE;
    dmalloc_error("check_debug_vars");
    /* NOTE: dmalloc_error may die already */
    _dmalloc_die();
    /*NOTREACHED*/
  }
  
  in_alloc = TRUE;
  
  if (pnt == NULL)
    ret = _chunk_heap_check();
  else
    ret = _chunk_pnt_check("dmalloc_verify", pnt, CHUNK_PNT_EXACT, 0);
  
  in_alloc = FALSE;
  
  if (ret == NOERROR)
    return DMALLOC_VERIFY_NOERROR;
  else
    return DMALLOC_VERIFY_ERROR;
}

/*
 * same as dmalloc_verify
 */
EXPORT	int	_malloc_verify(const DMALLOC_PNT pnt)
{
  return _dmalloc_verify(pnt);
}

/*
 * set the global debug functionality flags to DEBUG (0 to disable).
 * NOTE: after this module has started up, you cannot set certain flags
 * such as fence-post or free-space checking.
 */
EXPORT	void	_dmalloc_debug(const int debug)
{
  /* should not check the heap here since we are setting the debug variable */
  
  /* if we've not started up then set the variable */
  if (! enabled)
    _dmalloc_flags = debug;
  else {
    /* make sure that the not-changeable flag values are preserved */
    _dmalloc_flags &= DEBUG_NOT_CHANGEABLE;
    
    /* add the new flags - the not-addable ones */
    _dmalloc_flags |= debug & ~DEBUG_NOT_ADDABLE;
  }
}

/*
 * returns the current debug functionality flags.  this allows you to
 * save a dmalloc library state to be restored later.
 */
EXPORT	int	_dmalloc_debug_current(void)
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
EXPORT	int	_dmalloc_examine(const DMALLOC_PNT pnt, DMALLOC_SIZE * size,
				 char ** file, unsigned int * line,
				 DMALLOC_PNT * ret_attr)
{
  int		ret;
  
  /* need to check the heap here since we are geting info from it below */
  if (check_debug_vars(DMALLOC_DEFAULT_FILE, DMALLOC_DEFAULT_LINE) != NOERROR)
    return ERROR;
  
  /* NOTE: we do not need the alloc-size info */
  ret = _chunk_read_info(pnt, size, NULL, file, line, ret_attr,
			 "dmalloc_examine", NULL);
  
  in_alloc = FALSE;
  
  if (ret == NOERROR)
    return NOERROR;
  else
    return ERROR;
}

/*
 * dmalloc version of strerror to return the string version of ERRNUM
 * returns the string for MALLOC_BAD_ERRNO if ERRNUM is out-of-range.
 */
EXPORT	char	*_dmalloc_strerror(const int errnum)
{
  /* should not check_debug_vars here because _dmalloc_error calls this */
  
  if (! IS_MALLOC_ERRNO(errnum))
    return errlist[ERROR_BAD_ERRNO];
  else
    return errlist[errnum];
}
