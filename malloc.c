/*
 * user-level memory-allocation routines
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

/*
 * This file contains the user-level calls to the memory allocation
 * routines.  It handles a lot of the miscellaneous support garbage for
 * chunk.c which is the real heap manager.
 */

#define MALLOC_DEBUG_DISABLE

#include "malloc.h"
#include "malloc_loc.h"

#include "chunk.h"
#include "compat.h"
#include "conf.h"
#include "dbg_values.h"
#include "error.h"
#include "error_str.h"
#include "error_val.h"
#include "heap.h"
#include "malloc_lp.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: malloc.c,v 1.25 1993/04/14 22:13:57 gray Exp $";
#endif

/*
 * exported variables
 */
/* logfile for dumping malloc info, MALLOC_LOGFILE env. var overrides this */
EXPORT	char		*malloc_logpath	= NULL;
/* internal malloc error number for reference purposes only */
EXPORT	int		malloc_errno = 0;

/* local routines */
LOCAL	int		malloc_startup(void);
EXPORT	void		malloc_shutdown(void);

/* local variables */
LOCAL	int		malloc_enabled	= FALSE; /* have we started yet? */
LOCAL	char		in_alloc	= FALSE; /* can't be here twice */
LOCAL	char		log_path[128]	= { NULLC }; /* storage for env path */

/* debug variables */
LOCAL	char		*malloc_address	= NULL;	/* address to catch */
LOCAL	int		address_count	= 0;	/* address argument */
LOCAL	char		start_file[128] = { NULLC }; /* file to start at */
LOCAL	int		start_line	= 0;	/* line in module to start */
LOCAL	int		start_count	= -1;	/* start after X */
LOCAL	int		check_interval	= -1;	/* check every X */

/****************************** local utilities ******************************/

/*
 * hexadecimal STR to int translation
 */
LOCAL	int	hex_to_int(char * str)
{
  int		ret;
  
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
 * returns [NO]ERROR.
 */
LOCAL	int	check_debug_vars(const char * file, const int line)
{
  static int	iterc = 0;
  
  if (in_alloc) {
    malloc_errno = MALLOC_IN_TWICE;
    _malloc_perror("check_debug_vars");
    /* malloc_perror may die already */
    _malloc_die();
    /*NOTREACHED*/
  }
  
  in_alloc = TRUE;
  
  if (! malloc_enabled)
    if (malloc_startup() != NOERROR)
      return ERROR;
  
  /* check start file/line specifications */
  if (! BIT_IS_SET(_malloc_debug, DEBUG_CHECK_HEAP)
      && start_file[0] != NULLC
      && file != NULL
      && strcmp(start_file, file) == 0
      && (line == 0 || line == start_line))
    BIT_SET(_malloc_debug, DEBUG_CHECK_HEAP);
  
  /* start checking heap after X times */
  if (start_count != -1 && --start_count == 0)
    BIT_SET(_malloc_debug, DEBUG_CHECK_HEAP);
  
  /* checking heap every X times */
  if (check_interval != -1) {
    if (++iterc >= check_interval) {
      BIT_SET(_malloc_debug, DEBUG_CHECK_HEAP);
      iterc = 0;
    }
    else
      BIT_CLEAR(_malloc_debug, DEBUG_CHECK_HEAP);
  }
  
  /* after all that, do we need to check the heap? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_HEAP))
    (void)_chunk_heap_check();
  
  return NOERROR;
}

/*
 * check out a pointer to see if we were looking for it.
 * may not return.
 */
LOCAL	void	check_pnt(const char * file, const int line, char * pnt)
{
  static int	addc = 0;
  
  if (malloc_address == NULL || pnt != malloc_address)
    return;
  
  if (++addc < address_count)
    return;
  
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_BAD_POINTER))
    _malloc_message("found address '%#lx' after %d pass%s from '%s:%u'",
		    pnt, addc, (addc == 1 ? "" : "es"), file, line);
  malloc_errno = MALLOC_POINTER_FOUND;
  _malloc_perror("check_pnt");
}

/*
 * get the values of malloc environ variables
 */
LOCAL	void	get_environ(void)
{
  char		*env;
  
  /* get the malloc_debug value */
  env = (char *)getenv(DEBUG_ENVIRON);
  if (env != NULL)
    _malloc_debug = hex_to_int(env);
  
  /* get the malloc debug logfile name into a holding variable */
  env = (char *)getenv(LOGFILE_ENVIRON);
  if (env != NULL) {
    (void)strcpy(log_path, env);
    malloc_logpath = log_path;
  }
  
  /* watch for a specific address and die when we get it */
  env = (char *)getenv(ADDRESS_ENVIRON);
  if (env != NULL) {
    char	*addp;
    
    addp = (char *)index(env, ':');
    if (addp != NULL) {
      *addp = NULLC;
      address_count = atoi(addp + 1);
    }
    else
      address_count = 1;
    
    malloc_address = (char *)hex_to_int(env);
  }
  
  /* check the heap every X times */
  env = (char *)getenv(INTERVAL_ENVIRON);
  if (env != NULL)
    check_interval = atoi(env);
  
  /*
   * start checking the heap after X iterations OR
   * start at a file:line combination
   */
  env = (char *)getenv(START_ENVIRON);
  if (env != NULL) {
    char	*startp;
    
    BIT_CLEAR(_malloc_debug, DEBUG_CHECK_HEAP);
    
    startp = (char *)index(env, ':');
    if (startp != NULL) {
      *startp = NULLC;
      (void)strcpy(start_file, env);
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
  _heap_startup();
  
  /* startup the chunk lower-level code */
  if (_chunk_startup() == ERROR)
    return ERROR;
  
  return NOERROR;
}

/*
 * shutdown memory-allocation module, provide statistics if necessary
 */
EXPORT	void	malloc_shutdown(void)
{
  /* NOTE: do not test for IN_TWICE here */
  
  /* do we need to check the heap? */
  if (BIT_IS_SET(_malloc_debug, DEBUG_CHECK_HEAP))
    (void)_chunk_heap_check();
  
  /* dump some statistics to the logfile */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_STATS))
    _chunk_list_count();
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_STATS))
    _chunk_stats();
  
  /* report on non-freed pointers */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_NONFREE))
    _chunk_dump_not_freed();
  
  /* NOTE: do not set malloc_enabled to false here */
}

/******************************* memory calls ********************************/

/*
 * allocate and return a SIZE block of bytes.  returns NULL on error.
 */
EXPORT	void	*malloc(MALLOC_SIZE size)
{
  void		*newp;
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return MALLOC_ERROR;
  
  newp = _chunk_malloc(_malloc_file, _malloc_line, size);
  check_pnt(_malloc_file, _malloc_line, newp);
  
  in_alloc = FALSE;
  
  return newp;
}

/*
 * allocate and return a block of bytes able to hold NUM_ELEMENTS of elements
 * of SIZE bytes and zero the block.  returns NULL on error.
 */
EXPORT	void	*calloc(unsigned int num_elements, MALLOC_SIZE size)
{
  void		*newp;
  unsigned int	len = num_elements * size;
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return CALLOC_ERROR;
  
  /* needs to be done here */
  _calloc_count++;
  
  /* alloc and watch for the die address */
  newp = _chunk_malloc(_malloc_file, _malloc_line, len);
  check_pnt(_malloc_file, _malloc_line, newp);
  
  (void)memset(newp, NULLC, len);
  
  in_alloc = FALSE;
  
  return newp;
}

/*
 * resizes OLD_PNT to SIZE bytes and return the new space after either copying
 * all of OLD_PNT to the new area or truncating.  returns NULL on error.
 */
EXPORT	void	*realloc(void * old_pnt, MALLOC_SIZE new_size)
{
  void		*newp;
  
#if ALLOW_REALLOC_NULL
  if (old_pnt == NULL)
    return malloc(new_size);
#endif
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return REALLOC_ERROR;
  
  check_pnt(_malloc_file, _malloc_line, old_pnt);
  newp = _chunk_realloc(_malloc_file, _malloc_line, old_pnt, new_size);
  check_pnt(_malloc_file, _malloc_line, newp);
  
  in_alloc = FALSE;
  
  return newp;
}

/*
 * release PNT in the heap, returning FREE_[NO]ERROR or void
 */
#if __STDC__
EXPORT	void	free(void * pnt)
#else
EXPORT	int	free(void * pnt)
#endif
{
  int		ret;
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR) {
#if __STDC__
    return;
#else
    return FREE_ERROR;
#endif
  }
  
  check_pnt(_malloc_file, _malloc_line, pnt);
  ret = _chunk_free(_malloc_file, _malloc_line, pnt);
  
  in_alloc = FALSE;
  
#if ! __STDC__
  return ret;
#endif
}

/******************************* utility calls *******************************/

/*
 * call through to _heap_map function, returns [NO]ERROR
 */
EXPORT	int	malloc_heap_map(void)
{
  if (check_debug_vars(NULL, 0) != NOERROR)
    return ERROR;
  
  _chunk_log_heap_map();
  
  in_alloc = FALSE;
  
  return NOERROR;
}

/*
 * verify pointer PNT, if PNT is 0 then check the entire heap.
 * returns MALLOC_VERIFY_[NO]ERROR
 */
EXPORT	int	malloc_verify(void * pnt)
{
  int	ret;
  
  if (check_debug_vars(NULL, 0) != NOERROR)
    return MALLOC_VERIFY_ERROR;
  
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
 * returns [NO]ERROR
 */
EXPORT	int	malloc_debug(int debug)
{
  int	hold;
  
  if (check_debug_vars(NULL, 0) != NOERROR)
    return MALLOC_ERROR;
  
  /* make sure that the not-changeable flags' values are preserved */
  hold = _malloc_debug & DEBUG_NOT_CHANGEABLE;
  debug &= ~DEBUG_NOT_CHANGEABLE;
  _malloc_debug = debug | hold;
  
  in_alloc = FALSE;
  
  return NOERROR;
}

/*
 * examine pointer PNT and returns SIZE, and FILE / LINE info on it
 * if any of the pointers are not NULL.
 * returns NOERROR or ERROR depending on whether PNT is good or not
 */
EXPORT	int	malloc_examine(void * pnt, MALLOC_SIZE * size,
			       char ** file, unsigned int * line)
{
  int		ret;
  
  if (check_debug_vars(NULL, 0) != NOERROR)
    return ERROR;
  
  ret = _chunk_read_info(pnt, size, file, line);
  
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
EXPORT	char	*malloc_strerror(int errnum)
{
  /*
   * NOTE: should not check_debug_vars here because _malloc_perror calls this.
   */
  
  if (! IS_MALLOC_ERRNO(errnum))
    return malloc_errlist[MALLOC_BAD_ERRNO];
  else
    return malloc_errlist[errnum];
}
