/*
 * user-level memory-allocation routines
 *
 * Copyright 1991 by the Antaire Corporation
 */

#define MALLOC_MAIN

#if defined(ANTAIRE)
#include "useful.h"
#include "assert.h"
#include "terminate.h"
#endif

#include "chunk.h"
#include "error.h"
#include "heap.h"
#include "malloc.h"
#include "malloc_dbg.h"
#include "malloc_errno.h"
#include "malloc_errors.h"
#include "malloc_loc.h"

RCS_ID("$Id: malloc.c,v 1.1 1992/10/21 07:34:14 gray Exp $");

/*
 * library calls
 */
IMPORT	char	*getenv();

/*
 * exported variables
 */
/* production flag which indicates that no malloc features should be enabled */
EXPORT	char		malloc_production = FALSE;

/* logfile for dumping malloc info, MALLOC_LOGFILE env. var overrides this */
EXPORT	char		*malloc_logpath	= NULL;

/* local routines */
LOCAL	int		malloc_startup(void);	/* used before defined */
EXPORT	void		malloc_shutdown(void);

/* local variables */
LOCAL	int		malloc_enabled	= 0;	/* have we started yet? */
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
    _malloc_errno = MALLOC_IN_TWICE;
    _malloc_perror("check_debug_vars");
    /* malloc_perror may die already */
    _malloc_die();
    /*NOTREACHED*/
  }
  
  in_alloc = TRUE;
  
  if (malloc_enabled == 0)
    if (malloc_startup() != NOERROR)
      return ERROR;
  
  /* check start file/line specifications */
  if (! _malloc_check
      && start_file[0] != NULLC && file != NULL
      && strcmp(start_file, file) == 0
      && (line == 0 || line == start_line))
    _malloc_check = TRUE;
  
  /* start checking heap after X times */
  if (start_count != -1 && --start_count == 0)
    _malloc_check = TRUE;
  
  /* checking heap every X times */
  if (check_interval != -1) {
    if (++iterc >= check_interval) {
      _malloc_check = TRUE;
      iterc = 0;
    }
    else
      _malloc_check = FALSE;
  }
  
  return NOERROR;
}

/*************************** startup/shudown calls ***************************/

/*
 * get the values of malloc environ variables
 */
LOCAL	void	get_environ()
{
  char		*env;
  
  /* get the malloc_debug level */
  if ((env = getenv(DEBUG_ENVIRON)) != NULL)
    _malloc_debug = hex_to_int(env);
  
  /* get the malloc debug logfile name into a holding variable */
  if ((env = getenv(LOGFILE_ENVIRON)) != NULL) {
    (void)strcpy(log_path, env);
    malloc_logpath = log_path;
  }
  
  /* watch for a specific address and die when we get it */
  if ((env = getenv(ADDRESS_ENVIRON)) != NULL) {
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
  if ((env = getenv(INTERVAL_ENVIRON)) != NULL)
    check_interval = atoi(env);
  
  /*
   * start checking the heap after X interations OR
   * start at a file:line combination
   */
  if ((env = getenv(START_ENVIRON)) != NULL) {
    char	*startp;
    
    _malloc_check = FALSE;
    
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
  if (malloc_enabled == 1)
    return ERROR;
  
  /* set this here so if an error occurs below, it will not try again */
  malloc_enabled = 1;
  
  if (! malloc_production)
    get_environ();
  
  /* startup heap code */
  _heap_startup();
  
  /* startup the chunk lower-level code */
  if (_chunk_startup() == ERROR)
    return ERROR;
  
#if defined(ANTAIRE)
  /* check out _malloc_errno when we assert */
  assert_check(&_malloc_errno, "malloc error code (see malloc_err.h)", 0,
	       malloc_errlist);
  
  terminate_catch(malloc_shutdown);
#endif
  
  return NOERROR;
}

/*
 * shutdown alloc module, provide statistics
 */
EXPORT	void	malloc_shutdown(void)
{
  /* NOTE: do not test for IN_TWICE here */
  
#if defined(ANTAIRE)
  if (! terminating)
    terminate_uncatch(malloc_shutdown);
#endif
  
  /* dump some statistics to the logfile */
  _chunk_list_count();
  _chunk_stats();
  
  /* report on non-freed pointers */
  if (BIT_IS_SET(_malloc_debug, DEBUG_LOG_NONFREE))
    _chunk_dump_not_freed();
}

/******************************** calloc calls *******************************/

/*
 * allocate ELEN of elements of SIZE, then zero's the block
 */
EXPORT	char	*_calloc_info(char * file, int line, unsigned int elen,
			      unsigned int size)
{
  char		*newp;
  unsigned int	len = elen * size;
  
  if (check_debug_vars(file, line) != NOERROR)
    return CALLOC_ERROR;
  
  /* needs to be done here */
  _calloc_count++;
  
  /* alloc and watch for the die address */
  newp = (char *)_chunk_malloc(file, line, len);
  
  /* is this the address we are looking for? */
  if (malloc_address != NULL && newp == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  MEMORY_ZERO(newp, len);
  
  in_alloc = FALSE;
  
  return newp;
}

/*
 * non-debug version of calloc_info
 */
EXPORT	char	*calloc(unsigned int elen, unsigned int size)
{
  char		*newp;
  
  newp = _calloc_info(DEFAULT_FILE, DEFAULT_LINE, elen, size);
  
  return newp;
}

/********************************* free calls ********************************/

/*
 * release PNT in the heap from FILE at LINE
 */
EXPORT	int	_free_info(char * file, int line, char * pnt)
{
  int		ret;
  
  if (check_debug_vars(file, line) != NOERROR)
    return FREE_ERROR;
  
  /* is this the address we are looking for? */
  if (malloc_address != NULL && pnt == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  ret = _chunk_free(file, line, pnt);
  
  in_alloc = FALSE;
  
  return ret;
}

/*
 * non-debug version of free_info
 */
EXPORT	int	free(char * pnt)
{
  int		ret;
  
  ret = _free_info(DEFAULT_FILE, DEFAULT_LINE, pnt);
  
  return ret;
}

/*
 * another non-debug version of free_info
 */
EXPORT	int	cfree(char * pnt)
{
  int		ret;
  
  ret = _free_info(DEFAULT_FILE, DEFAULT_LINE, pnt);
  
  return ret;
}

/******************************* malloc calls ********************************/

/*
 * allocate a SIZE block of bytes from FILE at LINE
 */
EXPORT	char	*_malloc_info(char * file, int line, unsigned int size)
{
  char		*newp;
  
  if (check_debug_vars(file, line) != NOERROR)
    return MALLOC_ERROR;
  
  newp = (char *)_chunk_malloc(file, line, size);
  
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
 * non-debug version of malloc_info
 */
EXPORT	char	*malloc(unsigned int size)
{
  char		*newp;
  
  newp = _malloc_info(DEFAULT_FILE, DEFAULT_LINE, size);
  
  return newp;
}

/******************************* realloc calls *******************************/

/*
 * resizes PNT to SIZE bytes either copying or truncating
 */
EXPORT	char	*_realloc_info(char * file, int line, char * oldp,
			       unsigned int new_size)
{
  char		*newp;
  
#ifndef NO_REALLOC_NULL
  if (oldp == NULL)
    return _malloc_info(file, line, new_size);
#endif /* ! NO_REALLOC_NULL */
  
  if (check_debug_vars(file, line) != NOERROR)
    return REALLOC_ERROR;
  
  /* is the old address the one we are looking for? */
  if (malloc_address != NULL && oldp == malloc_address) {
    if (address_count - 1 <= 0)
      _malloc_die();
    else
      address_count--;
  }
  
  newp = (char *)_chunk_realloc(file, line, oldp, new_size);
  
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

/*
 * non-debug version of realloc_info
 */
EXPORT	char	*realloc(char * old_pnt, unsigned int new_size)
{
  char		*newp;
  
  newp = _realloc_info(DEFAULT_FILE, DEFAULT_LINE, old_pnt, new_size);
  
  return newp;
}

/******************************** utility calls ******************************/

/*
 * call through to _heap_map function, returns [NO]ERROR
 */
EXPORT	int	malloc_heap_map()
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
    ret = _chunk_heap_check(CHUNK_CHECK_ALL);
  
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
 * returns NOERROR or ERROR depending on whether PNT is good or not
 */
EXPORT	int	malloc_examine(char * pnt, int * size, char ** file,
			       int * line)
{
  int		ret;
  
  if (check_debug_vars(NULL, 0) != NOERROR)
    return ERROR;
  
  ret = chunk_read_info(pnt, size, file, line);
  
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
