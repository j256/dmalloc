/*
 * user-level memory-allocation routines
 *
 * program that handles the malloc debug variables.
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
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * The author of the program may be contacted at gray.watson@antaire.com
 */

#define MALLOC_MAIN

#include "malloc.h"
#include "malloc_loc.h"
#include "chunk.h"
#include "error.h"
#include "error_str.h"
#include "heap.h"
#include "malloc_errno.h"
#include "malloc_leap.h"
#include "malloc_loc.h"
#include "proto.h"

LOCAL	char	*rcs_id =
  "$Id: malloc.c,v 1.5 1992/11/06 05:41:04 gray Exp $";

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
LOCAL	char		log_path[128];		/* storage for env path */

/* debug variables */
LOCAL	char		*malloc_address	= NULL;	/* address to catch */
LOCAL	int		address_count	= 0;	/* address argument */
LOCAL	char		start_file[128] = { NULLC }; /* file to start at */
LOCAL	int		start_line	= 0;	/* line in module to start */
LOCAL	int		start_count	= -1;	/* start after X */
LOCAL	int		check_interval	= -1;	/* check every X */

/****************************** local utilities ******************************/

/*
 * hexadecimal STR to long translation
 */
LOCAL	long	hex_to_int(char * str)
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
 * returns [NO]ERROR.
 */
LOCAL	int	check_debug_vars(char * file, int line)
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
      && start_file[0] != NULLC && file != NULL
      && STRING_COMPARE(start_file, file) == 0
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
  
  return NOERROR;
}

/*************************** startup/shutdown calls **************************/

/*
 * get the values of malloc environ variables
 */
LOCAL	void	get_environ(void)
{
  char		*env;
  
  /* get the malloc_debug level */
  if ((env = GET_ENV(DEBUG_ENVIRON)) != NULL)
    _malloc_debug = hex_to_int(env);
  
  /* get the malloc debug logfile name into a holding variable */
  if ((env = GET_ENV(LOGFILE_ENVIRON)) != NULL) {
    (void)strcpy(log_path, env);
    malloc_logpath = log_path;
  }
  
  /* watch for a specific address and die when we get it */
  if ((env = GET_ENV(ADDRESS_ENVIRON)) != NULL) {
    char	*addp;
    
    if ((addp = STRING_SEARCH(env, ':')) != NULL) {
      *addp = NULLC;
      address_count = atoi(addp + 1);
    }
    else
      address_count = 1;
    
    malloc_address = (char *)hex_to_int(env);
  }
  
  /* check the heap every X times */
  if ((env = GET_ENV(INTERVAL_ENVIRON)) != NULL)
    check_interval = atoi(env);
  
  /*
   * start checking the heap after X iterations OR
   * start at a file:line combination
   */
  if ((env = GET_ENV(START_ENVIRON)) != NULL) {
    char	*startp;
    
    BIT_CLEAR(_malloc_debug, DEBUG_CHECK_HEAP);
    
    if ((startp = STRING_SEARCH(env, ':')) != NULL) {
      *startp = NULLC;
      (void)strcpy(start_file, env);
      start_line = atoi(startp + 1);
      start_count = 0;
    }
    else
      start_count = atoi(env);
  }
}

/*
 * startup the alloc module
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
 * shutdown alloc module, provide statistics
 */
EXPORT	void	malloc_shutdown(void)
{
  /* NOTE: do not test for IN_TWICE here */
  
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

/******************************** memory calls *******************************/

/*
 * allocate NUM_ELEMENTS of elements of SIZE, then zero's the block
 */
EXPORT	char	*calloc(unsigned int num_elements, unsigned int size)
{
  char		*newp;
  unsigned int	len = num_elements * size;
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return CALLOC_ERROR;
  
  /* needs to be done here */
  _calloc_count++;
  
  /* alloc and watch for the die address */
  newp = (char *)_chunk_malloc(_malloc_file, _malloc_line, len);
  
  /* is this the address we are looking for? */
  if (malloc_address != NULL && newp == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  MEMORY_SET(newp, NULLC, len);
  
  in_alloc = FALSE;
  
  return newp;
}

/*
 * release PNT in the heap
 */
EXPORT	int	free(char * pnt)
{
  int		ret;
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return FREE_ERROR;
  
  /* is this the address we are looking for? */
  if (malloc_address != NULL && pnt == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  ret = _chunk_free(_malloc_file, _malloc_line, pnt);
  
  in_alloc = FALSE;
  
  return ret;
}

/*
 * same as free
 */
EXPORT	int	cfree(char * pnt)
{
  return free(pnt);
}

/*
 * allocate a SIZE block of bytes
 */
EXPORT	char	*malloc(unsigned int size)
{
  char		*newp;
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return MALLOC_ERROR;
  
  newp = (char *)_chunk_malloc(_malloc_file, _malloc_line, size);
  
  /* is this the address we are looking for? */
  if (malloc_address != NULL && newp == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  in_alloc = FALSE;
  
  return newp;
}

/*
 * resizes OLD_PNT to SIZE bytes either copying or truncating
 */
EXPORT	char	*realloc(char * old_pnt, unsigned int new_size)
{
  char		*newp;
  
#ifndef NO_REALLOC_NULL
  if (old_pnt == NULL)
    return malloc(new_size);
#endif /* ! NO_REALLOC_NULL */
  
  if (check_debug_vars(_malloc_file, _malloc_line) != NOERROR)
    return REALLOC_ERROR;
  
  /* is the old address the one we are looking for? */
  if (malloc_address != NULL && old_pnt == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  newp = (char *)_chunk_realloc(_malloc_file, _malloc_line, old_pnt, new_size);
  
  /* is the new address the one we are looking for? */
  if (malloc_address != NULL && newp == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  in_alloc = FALSE;
  
  return newp;
}

/******************************** utility calls ******************************/

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
 * verify pointer PNT or if it equals 0, the entire heap
 * returns MALLOC_VERIFY_[NO]ERROR
 */
EXPORT	int	malloc_verify(char * pnt)
{
  int	ret;
  
  if (check_debug_vars(NULL, 0) != NOERROR)
    return MALLOC_VERIFY_ERROR;
  
  if (pnt != 0)
    ret = _chunk_pnt_check(pnt);
  else
    ret = _chunk_heap_check();
  
  in_alloc = FALSE;
  
  if (ret == NOERROR)
    return MALLOC_VERIFY_NOERROR;
  else
    return MALLOC_VERIFY_ERROR;
}

/*
 * set the global debug functionality flags to DEBUG.
 * returns [NO]ERROR
 */
EXPORT	int	malloc_debug(long debug)
{
  int	hold;
  
  /* make sure that the not-changeable flags' values are preserved */
  hold = _malloc_debug & DEBUG_NOT_CHANGEABLE;
  debug &= ~DEBUG_NOT_CHANGEABLE;
  _malloc_debug = debug | hold;
  
  return NOERROR;
}

/*
 * examine pointer PNT and returns SIZE, and FILE / LINE info on it
 * if any of the pointers are not NULL.
 * returns NOERROR or ERROR depending on whether PNT is good or not
 */
EXPORT	int	malloc_examine(char * pnt, int * size, char ** file,
			       int * line)
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
  if (! IS_MALLOC_ERRNO(errnum))
    return malloc_errlist[MALLOC_BAD_ERRNO];
  else
    return malloc_errlist[errnum];
}
