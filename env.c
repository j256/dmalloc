/*
 * environment handling routines
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
 * This file contains short routine(s) to process the environment
 * variable(s) used by the library to get the runtime option(s).
 */

#define DMALLOC_DISABLE

#include "dmalloc.h"

#include "dmalloc_loc.h"
#include "debug_tok.h"
#include "env.h"
#include "error.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: env.c,v 1.7 1995/06/21 18:20:06 gray Exp $";
#endif

/* local variables */
LOCAL	char		log_path[512]	= { NULLC }; /* storage for env path */
LOCAL	char		start_file[512] = { NULLC }; /* file to start at */

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

/***************************** exported routines *****************************/

/*
 * break up ADDR_ALL into ADDRP and ADDR_COUNTP
 */
EXPORT	void	_dmalloc_address_break(const char * addr_all,
				       unsigned long * addrp,
				       int * addr_countp)
{
  char	*colonp;
  
  if (addrp != NULL)
    *addrp = hex_to_long(addr_all);
  if (addr_countp != NULL) {
    colonp = (char *)index(addr_all, ':');
    if (colonp != NULL)
      *addr_countp = atoi(colonp + 1);
  }
}

/*
 * break up START_ALL into SFILEP, SLINEP, and SCOUNTP
 */
EXPORT	void	_dmalloc_start_break(const char * start_all,
				     char ** sfilep, int * slinep,
				     int * scountp)
{
  char	*startp;
  
  startp = (char *)index(start_all, ':');
  if (startp != NULL) {
    (void)strcpy(start_file, start_all);
    if (sfilep != NULL)
      *sfilep = start_file;
    startp = start_file + (startp - start_all);
    *startp = NULLC;
    if (slinep != NULL)
      *slinep = atoi(startp + 1);
  }
  else if (scountp != NULL)
    *scountp = atoi(start_all);
}

/*
 * process the values of dmalloc environ variable(s) from ENVIRON
 * string.
 */
EXPORT	void	_dmalloc_environ_get(const char * environ,
				     unsigned long * addrp,
				     int * addr_countp,
				     long * debugp, int * intervalp,
				     char ** logpathp,
				     char ** sfilep, int * slinep,
				     int * scountp)
{
  const char	*env;
  char		*envp, *thisp;
  char		buf[1024], done = FALSE;
  int		len;
  long		flags = 0;
  attr_t	*attrp;
  
  if (addrp != NULL)
    *addrp = ADDRESS_INIT;
  if (addr_countp != NULL)
    *addr_countp = ADDRESS_COUNT_INIT;
  if (debugp != NULL)
    *debugp = DEBUG_INIT;
  if (intervalp != NULL)
    *intervalp = INTERVAL_INIT;
  if (logpathp != NULL)
    *logpathp = LOGPATH_INIT;
  if (sfilep != NULL)
    *sfilep = START_FILE_INIT;
  if (slinep != NULL)
    *slinep = START_LINE_INIT;
  if (scountp != NULL)
    *scountp = START_COUNT_INIT;
  
  /* get the options flag */
  env = (const char *)getenv(environ);
  if (env == NULL)
    return;
  
  /* make a copy */
  (void)strcpy(buf, env);
  
  /* handle each of tokens, in turn */
  for (envp = buf, thisp = buf; ! done; envp++, thisp = envp) {
    
    /* find the comma of end */
    for (;; envp++) {
      if (*envp == NULLC) {
	done = TRUE;
	break;
      }
      if (*envp == ',' && (envp == buf || *(envp - 1) != '\\'))
	break;
    }
    
    /* should we strip ' ' or '\t' here? */
    
    if (thisp == envp)
      continue;
    
    *envp = NULLC;
    
    len = strlen(ADDRESS_LABEL);
    if (strncmp(thisp, ADDRESS_LABEL, len) == 0
	&& *(thisp + len) == ASSIGNMENT_CHAR) {
      thisp += len + 1;
      _dmalloc_address_break(thisp, addrp, addr_countp);
      continue;
    }
    
    len = strlen(DEBUG_LABEL);
    if (strncmp(thisp, DEBUG_LABEL, len) == 0
	&& *(thisp + len) == ASSIGNMENT_CHAR) {
      thisp += len + 1;
      if (debugp != NULL)
	*debugp = hex_to_long(thisp);
      continue;
    }
    
    len = strlen(INTERVAL_LABEL);
    if (strncmp(thisp, INTERVAL_LABEL, len) == 0
	&& *(thisp + len) == ASSIGNMENT_CHAR) {
      thisp += len + 1;
      if (intervalp != NULL)
	*intervalp = atoi(thisp);
      continue;
    }
    
    /* get the dmalloc logfile name into a holding variable */
    len = strlen(LOGFILE_LABEL);
    if (strncmp(thisp, LOGFILE_LABEL, len) == 0
	&& *(thisp + len) == ASSIGNMENT_CHAR) {
      thisp += len + 1;
#if HAVE_GETPID
      /*
       * NOTE: this may cause core dumps if thisp contains a bad
       * format string
       */
      (void)sprintf(log_path, thisp, getpid());
#else
      (void)strcpy(log_path, thisp);
#endif
      if (logpathp != NULL)
	*logpathp = log_path;
      continue;
    }
    
    /*
     * start checking the heap after X iterations OR
     * start at a file:line combination
     */
    len = strlen(START_LABEL);
    if (strncmp(thisp, START_LABEL, len) == 0
	&& *(thisp + len) == ASSIGNMENT_CHAR) {
      thisp += len + 1;
      _dmalloc_start_break(thisp, sfilep, slinep, scountp);
      continue;
    }
    
    /* need to check the short/long debug options */
    for (attrp = attributes; attrp->at_string != NULL; attrp++) {
      if (strcmp(thisp, attrp->at_string) == 0
	  || strcmp(thisp, attrp->at_short) == 0) {
	flags |= attrp->at_value;
	break;
      }
    }
    if (attrp->at_string != NULL)
      continue;
  }
  
  /* append the token settings to the debug setting */
  if (debugp != NULL) {
    if (*debugp == DEBUG_INIT)
      *debugp = flags;
    else
      *debugp |= flags;
  }
}

/*
 * set dmalloc environ variable(s) with the values (maybe SHORT debug
 * info) into BUF
 */
EXPORT	void	_dmalloc_environ_set(char * buf, const char long_tokens,
				     const char short_tokens,
				     const unsigned long address,
				     const int addr_count, const long debug,
				     const int interval,
				     const char * logpath,
				     const char * sfile,
				     const int sline,
				     const int scount)
{
  char	*bufp = buf;
  
  if (debug != DEBUG_INIT) {
    if (short_tokens || long_tokens) {
      attr_t	*attrp;
      
      for (attrp = attributes; attrp->at_string != NULL; attrp++)
	if (debug & attrp->at_value) {
	  if (short_tokens)
	    (void)sprintf(bufp, "%s,", attrp->at_short);
	  else
	    (void)sprintf(bufp, "%s,", attrp->at_string);
	  for (; *bufp != NULLC; bufp++);
	}
    }
    else {
      (void)sprintf(bufp, "%s%c%#lx,", DEBUG_LABEL, ASSIGNMENT_CHAR, debug);
      for (; *bufp != NULLC; bufp++);
    }
  }
  if (address != ADDRESS_INIT) {
    if (addr_count != ADDRESS_COUNT_INIT)
      (void)sprintf(bufp, "%s%c%#lx:%d,",
		    ADDRESS_LABEL, ASSIGNMENT_CHAR, address, addr_count);
    else
      (void)sprintf(bufp, "%s%c%#lx,",
		    ADDRESS_LABEL, ASSIGNMENT_CHAR, address);
    for (; *bufp != NULLC; bufp++);
  }
  if (interval != INTERVAL_INIT) {
    (void)sprintf(bufp, "%s%c%d,", INTERVAL_LABEL, ASSIGNMENT_CHAR, interval);
    for (; *bufp != NULLC; bufp++);
  }
  if (logpath != LOGPATH_INIT) {
    (void)sprintf(bufp, "%s%c%s,", LOGFILE_LABEL, ASSIGNMENT_CHAR, logpath);
    for (; *bufp != NULLC; bufp++);
  }
  if (sfile != START_FILE_INIT) {
    if (sline != START_LINE_INIT)
      (void)sprintf(bufp, "%s%c%s:%d,",
		    START_LABEL, ASSIGNMENT_CHAR, sfile, sline);
    else
      (void)sprintf(bufp, "%s%c%s,", START_LABEL, ASSIGNMENT_CHAR, sfile);
    for (; *bufp != NULLC; bufp++);
  }
  else if (scount != START_COUNT_INIT) {
    (void)sprintf(bufp, "%s%c%d,", START_LABEL, ASSIGNMENT_CHAR, scount);
    for (; *bufp != NULLC; bufp++);
  }
  
  /* cut off the last comma */
  if (bufp > buf)
    bufp--;
  
  *bufp = NULLC;
}
