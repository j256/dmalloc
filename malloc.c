/*
 * user-level memory-allocation routines
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Antaire not be used in advertising or publicity pertaining to
 * distribution of the document or software without specific, written prior
 * permission.
 *
 * The Antaire Corporation makes no representations about the suitability of
 * the software described herein for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * The author may be contacted at gray.watson@antaire.com
 */

/*
 * This file contains the user-level calls to the memory allocation
 * routines.  It handles a lot of the miscellaneous support garbage for
 * chunk.c which is the real heap manager.
 */

#define MALLOC_DEBUG_DISABLE

#include "malloc_dbg.h"
#include "conf.h"

#include "chunk.h"
#include "compat.h"
#include "debug_val.h"
#include "error.h"
#include "error_str.h"
#include "error_val.h"
#include "heap.h"
#include "malloc_loc.h"
#include "malloc_lp.h"
#include "return.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: malloc.c,v 1.59 1994/03/30 06:06:47 gray Exp $";
#endif

/*
 * exported variables
 */
/* logfile for dumping malloc info, MALLOC_LOGFILE env var overrides this */
EXPORT	char		*malloc_logpath	= NULL;
/* internal malloc error number for reference purposes only */
EXPORT	int		malloc_errno = ERROR_NONE;
/* address to look for.  when discovered call _malloc_error() */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	void		*malloc_address	= NULL;
#else
EXPORT	char		*malloc_address	= NULL;
#endif
/*
 * argument to malloc_address, if 0 then never call _malloc_error()
 * else call it after seeing malloc_address for this many times.
 */
EXPORT	int		malloc_address_count	= 0;

/* local routines */
LOCAL	int		malloc_startup(void);
EXPORT	void		_malloc_shutdown(void);
#if defined(__STDC__) && __STDC__ == 1
EXPORT	int		_malloc_verify(const void * pnt);
#else
EXPORT	int		_malloc_verify(const char * pnt);
#endif
EXPORT	void		_malloc_log_heap_map(void);
EXPORT	void		_malloc_log_stats(void);
EXPORT	void		_malloc_log_unfreed(void);
EXPORT	void		_malloc_debug(const int debug);
EXPORT	int		_malloc_debug_current(void);
#if defined(__STDC__) && __STDC__ == 1
EXPORT	int		_malloc_examine(const void * pnt, MALLOC_SIZE * size,
					char ** file, unsigned int * line,
					void ** ret_attr);
#else
EXPORT	int		_malloc_examine(const char * pnt, MALLOC_SIZE * size,
					char ** file, unsigned int * line,
					char ** ret_attr);
#endif
EXPORT	char		*_malloc_strerror(const int errnum);


/* local variables */
LOCAL	int		malloc_enabled	= FALSE; /* have we started yet? */
LOCAL	char		in_alloc	= FALSE; /* can't be here twice */
LOCAL	char		log_path[512]	= { NULLC }; /* storage for env path */

/* debug variables */
LOCAL	char		start_file[512] = { NULLC }; /* file to start at */
LOCAL	int		start_line	= 0;	/* line in module to start */
LOCAL	int		start_count	= -1;	/* start after X */
LOCAL	int		check_interval	= -1;	/* check every X */

/****************************** local utilities ******************************/

/*
 * hexadecimal STR to int translation
 */
LOCAL	long	hex_to_long(const char * str)
{
  long		ret;
  
  /* strip off spaces */
  for (; *str == ' ' || *str == '\t'; str++);
  
  /* skip a leading 0[xX] */
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
    str += 2;
  
  for (ret = 0;; str++) {
    if (*str >= '0' && *str <= '9')
      ret = ret * 16 + (*str - '0');
    else if (*str >= 'a' && *str <= 'f')
      ret = ret * 16 + (*str - 'a' + 10);
    else if (*str >= 'A' && *str <= 'F')
      ret = ret * 16 + (*str - 'A' + 10);
    else
      break;
  }
  
  return ret;
}

/*
 * a call to the alloc routines has been made, check the debug variables
 * returns ERROR or NOERROR.
 */
LOCAL	int	check_debug_vars(const char * file, const int line)
{
  if (in_alloc) {
    malloc_errno = ERROR_IN_TWICE;
    _malloc_error("check_debug_vars");
    /* NOTE: malloc_error may die already */
    _malloc_die();
    /*NOTREACHED*/
  }
  
  in_alloc = TRUE;
  
  if (! malloc_enabled)
    if (malloc_startup() != NOERROR)
      return ERROR;
  
  /* check start file/line specifications */
  if (! BIT_IS_SET(_malloc_flags, DEBUG_CHECK_HEAP)
      && start_file[0] != NULLC
      && file != NULL
      && line != MALLOC_DEFAULT_LINE
      && strcmp(start_file, file) == 0
      && (start_line == 0 || start_line == line))
    BIT_SET(_malloc_flags, DEBUG_CHECK_HEAP);
  
  /* start checking heap after X times */
  if (start_count != -1 && --start_count == 0)
    BIT_SET(_malloc_flags, DEBUG_CHECK_HEAP);
  
  /* checking heap every X times */
  _malloc_iterc++;
  if (check_interval != -1) {
    if (_malloc_iterc >= check_interval) {
      BIT_SET(_malloc_flags, DEBUG_CHECK_HEAP);
      _malloc_iterc = 0;
    }
    else
      BIT_CLEAR(_malloc_flags, DEBUG_CHECK_HEAP);
  }
  
  /* after all that, do we need to check the heap? */
  if (BIT_IS_SET(_malloc_flags, DEBUG_CHECK_HEAP))
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
  
  if (malloc_address == NULL || pnt != malloc_address)
    return;
  
  addc++;
  _malloc_message("address '%#lx' found in '%s' at pass %d from '%s'",
		  pnt, label, addc, _chunk_display_pnt(file, line));
  
  /* NOTE: if malloc_address_count == 0 then never quit */
  if (malloc_address_count > 0 && addc >= malloc_address_count) {
    malloc_errno = ERROR_IS_FOUND;
    _malloc_error("check_pnt");
  }
}

/*
 * get the values of malloc environ variables
 */
LOCAL	void	get_environ(void)
{
  const char	*env;
  
  /* get the malloc_debug value -- if we haven't called malloc_debug yet */
  if (_malloc_debug_preset != DEBUG_PRE_NONE)
    _malloc_flags = _malloc_debug_preset;
  else {
    env = (const char *)getenv(DEBUG_ENVIRON);
    if (env != NULL)
      _malloc_flags = (int)hex_to_long(env);
  }
  
  /* get the malloc debug logfile name into a holding variable */
  env = (const char *)getenv(LOGFILE_ENVIRON);
  if (env != NULL) {
    /* NOTE: this may cause core dumps if env contains a bad format string */
    (void)sprintf(log_path, env, getpid());
    malloc_logpath = log_path;
  }
  
  /* watch for a specific address and die when we get it */
  env = (const char *)getenv(ADDRESS_ENVIRON);
  if (env != NULL) {
    char	*addp;
    
    addp = (char *)index(env, ':');
    if (addp != NULL)
      malloc_address_count = atoi(addp + 1);
    else
      malloc_address_count = 1;
    
    malloc_address = (char *)hex_to_long(env);
  }
  
  /* check the heap every X times */
  env = (const char *)getenv(INTERVAL_ENVIRON);
  if (env != NULL)
    check_interval = atoi(env);
  
  /*
   * start checking the heap after X iterations OR
   * start at a file:line combination
   */
  env = (const char *)getenv(START_ENVIRON);
  if (env != NULL) {
    char	*startp;
    
    BIT_CLEAR(_malloc_flags, DEBUG_CHECK_HEAP);
    
    startp = (char *)index(env, ':');
    if (startp != NULL) {
      (void)strcpy(start_file, env);
      startp = start_file + (startp - env);
      *startp = NULLC;
      start_line = atoi(startp + 1);
      start_count = 0;
    }
    else
      start_count = atoi(env);
  }
}

/************************** startup/shutdown calls ***************************/

/*
 * startup the memory-allocation module
 */
LOCAL	int	malloc_startup(void)
{
  /* have we started already? */
  if (malloc_enabled)
    return ERROR;
  
  /* set this here so if an error occurs below, it will not try again */
  malloc_enabled = TRUE;
  
  /* get the environmental variables */
  get_environ();
  
  /* startup heap code */
  if (_heap_startup() == ERROR)
    return ERROR;
  
  /* startup the chunk lower-level code */
  if (_chunk_startup() == ERROR)
    return ERROR;
  
  _malloc_shutdown_func = _malloc_shutdown;
  _malloc_log_heap_map_func = _malloc_log_heap_map;
  _malloc_log_stats_func = _malloc_log_stats;
  _malloc_log_unfreed_func = _malloc_log_unfreed;
  _malloc_verify_func = _malloc_verify;
  _malloc_debug_func = _malloc_debug;
  _malloc_debug_current_func = _malloc_debug_current;
  _malloc_examine_func = _malloc_examine;
  _malloc_strerror_func = _malloc_strerror;
  
#if AUTO_SHUTDOWN
  {
    unsigned int	line_hold = _malloc_line;
    char		*file_hold = _malloc_file;
    
    /*
     * HACK: we have to disable the in_alloc because we might be about
     * to go recursize.  this should not get back here since malloc
     * has been enabled.
     */
    in_alloc = FALSE;
    
    /* NOTE: I use the else here in case some dumb systems has both */
#if HAVE_ATEXIT
    (void)atexit(_malloc_shutdown);
#else
#if HAVE_ON_EXIT
    (void)on_exit(_malloc_shutdown, NULL);
#endif
#endif
    
    in_alloc = TRUE;
    
    _malloc_line = line_hold;
    _malloc_file = file_hold;
  }
#endif /* AUTO_SHUTDOWN */
  
  return NOERROR;
}

/*
 * shutdown memory-allocation module, provide statistics if necessary
 * NOTE: called by way of leap routine in malloc_lp.c
 */
EXPORT	void	_malloc_shutdown(void)
{
  /* NOTE: do not test for IN_TWICE here */
  
  /* check the heap since we are dumping info from it */
  if (BIT_IS_SET(_malloc_flags, DEBUG_CHECK_HEAP))
    (void)_chunk_heap_check();
  
  /* dump some statistics to the logfile */
  if (BIT_IS_SET(_malloc_flags, DEBUG_LOG_STATS))
    _chunk_list_count();
  if (BIT_IS_SET(_malloc_flags, DEBUG_LOG_STATS))
    _chunk_stats();
  
  /* report on non-freed pointers */
  if (BIT_IS_SET(_malloc_flags, DEBUG_LOG_NONFREE))
    _chunk_dump_unfreed();
  
  /* NOTE: do not set malloc_enabled to false here */
}

/******************************* memory calls ********************************/

/*
 * allocate and return a SIZE block of bytes.  returns 0L on error.
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	void	*malloc(MALLOC_SIZE size)
#else
EXPORT	char	*malloc(MALLOC_SIZE size)
#endif
{
  void		*newp;
  
  SET_RET_ADDR(_malloc_file, _malloc_line);
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return MALLOC_ERROR;
  
  newp = _chunk_malloc(_malloc_file, _malloc_line, size);
  check_pnt(_malloc_file, _malloc_line, newp, "malloc");
  
  in_alloc = FALSE;
  
  _malloc_file = MALLOC_DEFAULT_FILE;
  _malloc_line = MALLOC_DEFAULT_LINE;
  
  return newp;
}

/*
 * allocate and return a block of bytes able to hold NUM_ELEMENTS of elements
 * of SIZE bytes and zero the block.  returns 0L on error.
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	void	*calloc(MALLOC_SIZE num_elements, MALLOC_SIZE size)
#else
EXPORT	char	*calloc(MALLOC_SIZE num_elements, MALLOC_SIZE size)
#endif
{
  void		*newp;
  unsigned int	len = num_elements * size;
  
  SET_RET_ADDR(_malloc_file, _malloc_line);
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return CALLOC_ERROR;
  
  /* needs to be done here */
  _calloc_count++;
  
  /* alloc and watch for the die address */
  newp = _chunk_malloc(_malloc_file, _malloc_line, len);
  check_pnt(_malloc_file, _malloc_line, newp, "calloc");
  
  (void)memset(newp, NULLC, len);
  
  in_alloc = FALSE;
  
  _malloc_file = MALLOC_DEFAULT_FILE;
  _malloc_line = MALLOC_DEFAULT_LINE;
  
  return newp;
}

/*
 * resizes OLD_PNT to SIZE bytes and return the new space after either copying
 * all of OLD_PNT to the new area or truncating.  returns 0L on error.
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	void	*realloc(void * old_pnt, MALLOC_SIZE new_size)
#else
EXPORT	char	*realloc(char * old_pnt, MALLOC_SIZE new_size)
#endif
{
  void		*newp;
  
  SET_RET_ADDR(_malloc_file, _malloc_line);
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return REALLOC_ERROR;
  
  check_pnt(_malloc_file, _malloc_line, old_pnt, "realloc-in");
  
#if ALLOW_REALLOC_NULL
  if (old_pnt == NULL)
    newp = _chunk_malloc(_malloc_file, _malloc_line, new_size);
  else
#endif
    newp = _chunk_realloc(_malloc_file, _malloc_line, old_pnt, new_size);
  
  check_pnt(_malloc_file, _malloc_line, newp, "realloc-out");
  
  in_alloc = FALSE;
  
  _malloc_file = MALLOC_DEFAULT_FILE;
  _malloc_line = MALLOC_DEFAULT_LINE;
  
  return newp;
}

/*
 * release PNT in the heap, returning FREE_ERROR, FREE_NOERROR or void
 * depending on whether STDC is defined by your compiler.
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	void	free(void * pnt)
#else
EXPORT	int	free(void * pnt)
#endif
{
  int		ret;
  
  SET_RET_ADDR(_malloc_file, _malloc_line);
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR) {
#if defined(__STDC__) && __STDC__ == 1
    return;
#else
    return FREE_ERROR;
#endif
  }
  
  check_pnt(_malloc_file, _malloc_line, pnt, "free");
  
#if ALLOW_FREE_NULL
  if (pnt == NULL) {
#if ALLOW_FREE_NULL_MESSAGE
    _malloc_message("WARNING: tried to free(0) from '%s'",
		    _chunk_display_pnt(_malloc_file, _malloc_line));
#endif
    ret = FREE_NOERROR;
  }
  else
    ret = _chunk_free(_malloc_file, _malloc_line, pnt);
#else /* ! ALLOW_FREE_NULL */
  ret = _chunk_free(_malloc_file, _malloc_line, pnt);
#endif
  
  in_alloc = FALSE;
  
  _malloc_file = MALLOC_DEFAULT_FILE;
  _malloc_line = MALLOC_DEFAULT_LINE;
  
#if defined(__STDC__) && __STDC__ == 1
#else
  return ret;
#endif
}

/*
 * same as free(PNT)
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	void	cfree(void * pnt)
#else
EXPORT	int	cfree(void * pnt)
#endif
{
#if defined(__STDC__) && __STDC__ == 1
  free(pnt);
#else
  return free(pnt);
#endif
}

/******************************* utility calls *******************************/

/*
 * log the heap structure plus information on the blocks if necessary.
 * NOTE: called by way of leap routine in malloc_lp.c
 */
EXPORT	void	_malloc_log_heap_map(void)
{
  /* check the heap since we are dumping info from it */
  if (check_debug_vars(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE) != NOERROR)
    return;
  
  _chunk_log_heap_map();
  
  in_alloc = FALSE;
}

/*
 * dump malloc statistics to logfile
 * NOTE: called by way of leap routine in malloc_lp.c
 */
EXPORT	void	_malloc_log_stats(void)
{
  SET_RET_ADDR(_malloc_file, _malloc_line);
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return;
  
  _chunk_list_count();
  _chunk_stats();
  
  in_alloc = FALSE;
  
  _malloc_file = MALLOC_DEFAULT_FILE;
  _malloc_line = MALLOC_DEFAULT_LINE;
}

/*
 * dump unfreed-memory info to logfile
 * NOTE: called by way of leap routine in malloc_lp.c
 */
EXPORT	void	_malloc_log_unfreed(void)
{
  SET_RET_ADDR(_malloc_file, _malloc_line);
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return;
  
  if (! BIT_IS_SET(_malloc_flags, DEBUG_LOG_TRANS))
    _malloc_message("dumping the unfreed pointers");
  
  _chunk_dump_unfreed();
  
  in_alloc = FALSE;
  
  _malloc_file = MALLOC_DEFAULT_FILE;
  _malloc_line = MALLOC_DEFAULT_LINE;
}

/*
 * verify pointer PNT, if PNT is 0 then check the entire heap.
 * NOTE: called by way of leap routine in malloc_lp.c
 * returns MALLOC_VERIFY_ERROR or MALLOC_VERIFY_NOERROR
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	int	_malloc_verify(const void * pnt)
#else
EXPORT	int	_malloc_verify(const char * pnt)
#endif
{
  int	ret;
  
  /* should not check heap here because we will be doing it below */
  
  if (in_alloc) {
    malloc_errno = ERROR_IN_TWICE;
    _malloc_error("check_debug_vars");
    /* NOTE: malloc_error may die already */
    _malloc_die();
    /*NOTREACHED*/
  }
  
  in_alloc = TRUE;
  
  if (pnt == NULL)
    ret = _chunk_heap_check();
  else
    ret = _chunk_pnt_check("malloc_verify", pnt, CHUNK_PNT_EXACT, 0);
  
  in_alloc = FALSE;
  
  if (ret == NOERROR)
    return MALLOC_VERIFY_NOERROR;
  else
    return MALLOC_VERIFY_ERROR;
}

/*
 * set the global debug functionality flags to DEBUG (0 to disable).
 * NOTE: after this module has started up, you cannot set certain flags
 * such as fence-post or free-space checking.
 */
EXPORT	void	_malloc_debug(const int debug)
{
  /* should not check the heap here since we are setting the debug variable */
  
  /* if we've not started up then set the variable */
  if (! malloc_enabled)
    _malloc_flags = debug;
  else {
    /* make sure that the not-changeable flag values are preserved */
    _malloc_flags &= DEBUG_NOT_CHANGEABLE;
    
    /* add the new flags - the not-addable ones */
    _malloc_flags |= debug & ~DEBUG_NOT_ADDABLE;
  }
}

/*
 * returns the current debug functionality flags.  this allows you to
 * save a malloc library state to be restored later.
 */
EXPORT	int	_malloc_debug_current(void)
{
  /* should not check the heap here since we are dumping the debug variable */
  return _malloc_flags;
}

/*
 * examine pointer PNT and returns SIZE, and FILE / LINE info on it,
 * or return-address RET_ADDR if any of the pointers are not 0L.
 * if FILE returns 0L then RET_ATTR may have a value and vice versa.
 * returns NOERROR or ERROR depending on whether PNT is good or not
 */
#if defined(__STDC__) && __STDC__ == 1
EXPORT	int	_malloc_examine(const void * pnt, MALLOC_SIZE * size,
				char ** file, unsigned int * line,
				void ** ret_attr)
#else
EXPORT	int	_malloc_examine(const char * pnt, MALLOC_SIZE * size,
				char ** file, unsigned int * line,
				char ** ret_attr)
#endif
{
  int		ret;
  
  /* need to check the heap here since we are geting info from it below */
  if (check_debug_vars(MALLOC_DEFAULT_FILE, MALLOC_DEFAULT_LINE) != NOERROR)
    return ERROR;
  
  ret = _chunk_read_info(pnt, size, file, line, ret_attr);
  
  in_alloc = FALSE;
  
  if (ret == NOERROR)
    return NOERROR;
  else
    return ERROR;
}

/*
 * malloc version of strerror to return the string version of ERRNUM
 * returns the string for MALLOC_BAD_ERRNO if ERRNUM is out-of-range.
 */
EXPORT	char	*_malloc_strerror(const int errnum)
{
  /* should not check_debug_vars here because _malloc_error calls this */
  
  if (! IS_MALLOC_ERRNO(errnum))
    return malloc_errlist[ERROR_BAD_ERRNO];
  else
    return malloc_errlist[errnum];
}
