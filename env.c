/*
 * Environment handling routines
 *
 * Copyright 1999 by Gray Watson
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
 * The author may be contacted via http://www.dmalloc.com/
 *
 * $Id: env.c,v 1.20 1999/03/04 19:11:18 gray Exp $
 */

/*
 * This file contains short routine(s) to process the environment
 * variable(s) used by the library to get the runtime option(s).
 */

#include <stdio.h>				/* for sprintf */

#define DMALLOC_DISABLE

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>				/* for getpid */
#endif

#include "conf.h"
#include "dmalloc.h"

#include "dmalloc_loc.h"
#include "debug_tok.h"
#include "env.h"
#include "error.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: env.c,v 1.20 1999/03/04 19:11:18 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: env.c,v 1.20 1999/03/04 19:11:18 gray Exp $";
#endif
#endif

/* local variables */
static	char		log_path[512]	= { '\0' }; /* storage for env path */
static	char		start_file[512] = { '\0' }; /* file to start at */

/****************************** local utilities ******************************/

/*
 * Hexadecimal STR to int translation
 */
static	long	hex_to_long(const char *str)
{
  long		ret;
  
  /* strip off spaces */
  for (; *str == ' ' || *str == '\t'; str++) {
  }
  
  /* skip a leading 0[xX] */
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) {
    str += 2;
  }
  
  for (ret = 0;; str++) {
    if (*str >= '0' && *str <= '9') {
      ret = ret * 16 + (*str - '0');
    }
    else if (*str >= 'a' && *str <= 'f') {
      ret = ret * 16 + (*str - 'a' + 10);
    }
    else if (*str >= 'A' && *str <= 'F') {
      ret = ret * 16 + (*str - 'A' + 10);
    }
    else {
      break;
    }
  }
  
  return ret;
}

/***************************** exported routines *****************************/

/*
 * Break up ADDR_ALL into ADDR_P and ADDR_COUNT_P
 */
void	_dmalloc_address_break(const char *addr_all, unsigned long *addr_p,
			       int *addr_count_p)
{
  char	*colon_p;
  
  if (addr_p != NULL) {
    *addr_p = hex_to_long(addr_all);
  }
  if (addr_count_p != NULL) {
    colon_p = strchr(addr_all, ':');
    if (colon_p != NULL) {
      *addr_count_p = atoi(colon_p + 1);
    }
  }
}

/*
 * Break up START_ALL into SFILE_P, SLINE_P, and SCOUNT_P
 */
void	_dmalloc_start_break(const char *start_all, char **sfile_p,
			     int *sline_p, int *scount_p)
{
  char	*start_p;
  
  start_p = strchr(start_all, ':');
  if (start_p != NULL) {
    (void)strcpy(start_file, start_all);
    if (sfile_p != NULL) {
      *sfile_p = start_file;
    }
    start_p = start_file + (start_p - start_all);
    *start_p = '\0';
    if (sline_p != NULL) {
      *sline_p = atoi(start_p + 1);
    }
  }
  else if (scount_p != NULL) {
    *scount_p = atoi(start_all);
  }
}

/*
 * Process the values of dmalloc environ variable(s) from ENVIRON
 * string.
 */
void	_dmalloc_environ_get(const char *environ, unsigned long *addr_p,
			     int *addr_count_p, unsigned int *debug_p,
			     unsigned long *interval_p, int *lock_on_p,
			     char **logpath_p, char **sfile_p,
			     int *sline_p, int *scount_p)
{
  const char	*env;
  char		*env_p, *this_p;
  char		buf[1024];
  int		len, done_b = 0;
  unsigned int	flags = 0;
  attr_t	*attr_p;
  
  if (addr_p != NULL) {
    *addr_p = ADDRESS_INIT;
  }
  if (addr_count_p != NULL) {
    *addr_count_p = ADDRESS_COUNT_INIT;
  }
  if (debug_p != NULL) {
    *debug_p = DEBUG_INIT;
  }
  if (interval_p != NULL) {
    *interval_p = INTERVAL_INIT;
  }
  if (lock_on_p != NULL) {
    *lock_on_p = LOCK_ON_INIT;
  }
  if (logpath_p != NULL) {
    *logpath_p = LOGPATH_INIT;
  }
  if (sfile_p != NULL) {
    *sfile_p = START_FILE_INIT;
  }
  if (sline_p != NULL) {
    *sline_p = START_LINE_INIT;
  }
  if (scount_p != NULL) {
    *scount_p = START_COUNT_INIT;
  }
  
  /* get the options flag */
  env = getenv(environ);
  if (env == NULL) {
    return;
  }
  
  /* make a copy */
  (void)strcpy(buf, env);
  
  /* handle each of tokens, in turn */
  for (env_p = buf, this_p = buf; ! done_b; env_p++, this_p = env_p) {
    
    /* find the comma of end */
    for (;; env_p++) {
      if (*env_p == '\0') {
	done_b = 1;
	break;
      }
      if (*env_p == ',' && (env_p == buf || *(env_p - 1) != '\\')) {
	break;
      }
    }
    
    /* should we strip ' ' or '\t' here? */
    
    if (this_p == env_p) {
      continue;
    }
    
    *env_p = '\0';
    
    len = strlen(ADDRESS_LABEL);
    if (strncmp(this_p, ADDRESS_LABEL, len) == 0
	&& *(this_p + len) == ASSIGNMENT_CHAR) {
      this_p += len + 1;
      _dmalloc_address_break(this_p, addr_p, addr_count_p);
      continue;
    }
    
    len = strlen(DEBUG_LABEL);
    if (strncmp(this_p, DEBUG_LABEL, len) == 0
	&& *(this_p + len) == ASSIGNMENT_CHAR) {
      this_p += len + 1;
      if (debug_p != NULL) {
	*debug_p = hex_to_long(this_p);
      }
      continue;
    }
    
    len = strlen(INTERVAL_LABEL);
    if (strncmp(this_p, INTERVAL_LABEL, len) == 0
	&& *(this_p + len) == ASSIGNMENT_CHAR) {
      this_p += len + 1;
      if (interval_p != NULL) {
	*interval_p = (unsigned long)atol(this_p);
      }
      continue;
    }
    
    len = strlen(LOCK_ON_LABEL);
    if (strncmp(this_p, LOCK_ON_LABEL, len) == 0
	&& *(this_p + len) == ASSIGNMENT_CHAR) {
      this_p += len + 1;
      if (lock_on_p != NULL) {
	*lock_on_p = atoi(this_p);
      }
      continue;
    }
    
    /* get the dmalloc logfile name into a holding variable */
    len = strlen(LOGFILE_LABEL);
    if (strncmp(this_p, LOGFILE_LABEL, len) == 0
	&& *(this_p + len) == ASSIGNMENT_CHAR) {
      this_p += len + 1;
#if HAVE_GETPID
      /*
       * NOTE: this may cause core dumps if this_p contains a bad
       * format string
       */
      (void)sprintf(log_path, this_p, getpid());
#else
      (void)strcpy(log_path, this_p);
#endif
      if (logpath_p != NULL) {
	*logpath_p = log_path;
      }
      continue;
    }
    
    /*
     * start checking the heap after X iterations OR
     * start at a file:line combination
     */
    len = strlen(START_LABEL);
    if (strncmp(this_p, START_LABEL, len) == 0
	&& *(this_p + len) == ASSIGNMENT_CHAR) {
      this_p += len + 1;
      _dmalloc_start_break(this_p, sfile_p, sline_p, scount_p);
      continue;
    }
    
    /* need to check the short/long debug options */
    for (attr_p = attributes; attr_p->at_string != NULL; attr_p++) {
      if (strcmp(this_p, attr_p->at_string) == 0
	  || strcmp(this_p, attr_p->at_short) == 0) {
	flags |= attr_p->at_value;
	break;
      }
    }
    if (attr_p->at_string != NULL) {
      continue;
    }
  }
  
  /* append the token settings to the debug setting */
  if (debug_p != NULL) {
    if (*debug_p == (unsigned int)DEBUG_INIT) {
      *debug_p = flags;
    }
    else {
      *debug_p |= flags;
    }
  }
}

/*
 * Set dmalloc environ variable(s) with the values (maybe SHORT debug
 * info) into BUF.
 */
void	_dmalloc_environ_set(char *buf, const int long_tokens_b,
			     const int short_tokens_b,
			     const unsigned long address,
			     const int addr_count, const unsigned int debug,
			     const int interval, const int lock_on,
			     const char *logpath, const char *sfile,
			     const int sline, const int scount)
{
  char	*buf_p = buf;
  
  if (debug != (unsigned int)DEBUG_INIT) {
    if (short_tokens_b || long_tokens_b) {
      attr_t	*attr_p;
      
      for (attr_p = attributes; attr_p->at_string != NULL; attr_p++) {
	if (debug & attr_p->at_value) {
	  if (short_tokens_b) {
	    (void)sprintf(buf_p, "%s,", attr_p->at_short);
	  }
	  else {
	    (void)sprintf(buf_p, "%s,", attr_p->at_string);
	  }
	  for (; *buf_p != '\0'; buf_p++) {
	  }
	}
      }
    }
    else {
      (void)sprintf(buf_p, "%s%c%#x,", DEBUG_LABEL, ASSIGNMENT_CHAR, debug);
      for (; *buf_p != '\0'; buf_p++) {
      }
    }
  }
  if (address != ADDRESS_INIT) {
    if (addr_count != ADDRESS_COUNT_INIT) {
      (void)sprintf(buf_p, "%s%c%#lx:%d,",
		    ADDRESS_LABEL, ASSIGNMENT_CHAR, address, addr_count);
    }
    else {
      (void)sprintf(buf_p, "%s%c%#lx,",
		    ADDRESS_LABEL, ASSIGNMENT_CHAR, address);
    }
    for (; *buf_p != '\0'; buf_p++) {
    }
  }
  if (interval != INTERVAL_INIT) {
    (void)sprintf(buf_p, "%s%c%d,", INTERVAL_LABEL, ASSIGNMENT_CHAR, interval);
    for (; *buf_p != '\0'; buf_p++) {
    }
  }
  if (lock_on != LOCK_ON_INIT) {
    (void)sprintf(buf_p, "%s%c%d,", LOCK_ON_LABEL, ASSIGNMENT_CHAR, lock_on);
    for (; *buf_p != '\0'; buf_p++) {
    }
  }
  if (logpath != LOGPATH_INIT) {
    (void)sprintf(buf_p, "%s%c%s,", LOGFILE_LABEL, ASSIGNMENT_CHAR, logpath);
    for (; *buf_p != '\0'; buf_p++) {
    }
  }
  if (sfile != START_FILE_INIT) {
    if (sline != START_LINE_INIT) {
      (void)sprintf(buf_p, "%s%c%s:%d,",
		    START_LABEL, ASSIGNMENT_CHAR, sfile, sline);
    }
    else {
      (void)sprintf(buf_p, "%s%c%s,", START_LABEL, ASSIGNMENT_CHAR, sfile);
    }
    for (; *buf_p != '\0'; buf_p++) {
    }
  }
  else if (scount != START_COUNT_INIT) {
    (void)sprintf(buf_p, "%s%c%d,", START_LABEL, ASSIGNMENT_CHAR, scount);
    for (; *buf_p != '\0'; buf_p++) {
    }
  }
  
  /* cut off the last comma */
  if (buf_p > buf) {
    buf_p--;
  }
  
  *buf_p = '\0';
}
