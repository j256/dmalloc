/*
 * compatibility functions for those systems who are missing them.
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
 * This file holds the compatibility routines necessary for the library to
 * function just in case your system does not have them.
 */

#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "compat.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
static	char	*rcs_id =
  "$Id: compat.c,v 1.37 1997/12/05 21:09:38 gray Exp $";
#endif

#if HAVE_MEMCPY == 0
/*
 * Copy LEN characters from SRC to DEST
 */
void	memcpy(char *dest, const char *src, DMALLOC_SIZE len)
{
  char		*dest_p;
  const	char	*src_p;
  int		byte_c;
  
  if (len <= 0) {
    return;
  }
  
  src_p = src;
  dest_p = dest;
  
  if (src_p <= dest_p && src_p + (len - 1) >= dest_p) {
    /* overlap, must copy right-to-left. */
    src_p += len - 1;
    dest_p += len - 1;
    for (byte_c = 0; byte_c < len; byte_c++) {
      *dest_p-- = *src_p--;
    }
  } else {
    for (byte_c = 0; byte_c < len; byte_c++) {
      *dest_p++ = *src_p++;
    }
  }
}
#endif /* HAVE_MEMCPY == 0 */

#if HAVE_MEMCMP == 0
/*
 * Compare LEN characters, return -1,0,1 if STR1 is <,==,> STR2
 */
int	memcmp(const char *str1, const char *str2, DMALLOC_SIZE len)
{
  for (; len > 0; len--, str1++, str2++) {
    if (*str1 != *str2) {
      return *str1 - *str2;
    }
  }
  
  return 0;
}
#endif /* HAVE_MEMCMP == 0 */

#if HAVE_MEMSET == 0
/*
 * Set LEN characters in STR to character CH
 */
char	*memset(char *str, int ch, DMALLOC_SIZE len)
{
  char	*hold = str;
  
  for (; len > 0; len--, str++) {
    *str = (char)ch;
  }
  
  return hold;
}
#endif /* HAVE_MEMSET == 0 */

#if HAVE_STRCHR == 0
/*
 * Find CH in STR by searching backwards through the string
 */
char	*strchr(const char *str, int ch)
{
  for (; *str != '\0'; str++) {
    if (*str == (char)ch) {
      return (char *)str;
    }
  }
  
  if (ch == '\0') {
    return (char *)str;
  }
  else {
    return NULL;
  }
}
#endif /* HAVE_STRCHR == 0 */

#if HAVE_STRRCHR == 0
/*
 * Find CH in STR by searching backwards through the string
 */
char	*strrchr(const char *str, int ch)
{
  const char	*pnt = NULL;
  
  for (; *str != '\0'; str++) {
    if (*str == (char)ch) {
      pnt = (char *)str;
    }
  }
  
  if (ch == '\0') {
    return (char *)str;
  }
  else {
    return (char *)pnt;
  }
}
#endif /* HAVE_STRRCHR == 0 */

#if HAVE_STRCAT == 0
/*
 * Concatenate STR2 onto the end of STR1
 */
char	*strcat(char *str1, const char *str2)
{
  char	*hold = str1;
  
  for (; *str1 != '\0'; str1++);
  
  while (*str2 != '\0') {
    *str1++ = *str2++;
  }
  *str1 = '\0';
  
  return hold;
}
#endif /* HAVE_STRCAT == 0 */

#if HAVE_STRLEN == 0
/*
 * Return the length in characters of STR
 */
int	strlen(const char *str)
{
  int	len;
  
  for (len = 0; *str != '\0'; str++, len++);
  
  return len;
}
#endif /* HAVE_STRLEN == 0 */

#if HAVE_STRCMP == 0
/*
 * Returns -1,0,1 on whether STR1 is <,==,> STR2
 */
int	strcmp(const char *str1, const char *str2)
{
  for (; *str1 != '\0' && *str1 == *str2; str1++, str2++);
  return *str1 - *str2;
}
#endif /* HAVE_STRCMP == 0 */

#if HAVE_STRNCMP == 0
/*
 * Compare at most LEN chars in STR1 and STR2 and return -1,0,1 or
 * STR1 - STR2
 */
int	strncmp(const char *str1, const char *str2, const int len)
{
  int	len_c;
  
  for (len_c = 0; len_c < len; len_c++, str1++, str2++) {
    if (*str1 != *str2 || *str1 == '\0') {
      return *str1 - *str2;
    }
  }
  
  return 0;
}
#endif /* HAVE_STRNCMP == 0 */

#if HAVE_STRCPY == 0
/*
 * Copies STR2 to STR1.  Returns STR1.
 */
char	*strcpy(char *str1, const char *str2)
{
  char	*str_p;
  
  for (str_p = str1; *str2 != '\0'; str_p++, str2++) {
    *str_p = *str2;
  }
  *str_p = '\0';
  
  return str1;
}
#endif /* HAVE_STRCPY == 0 */

#if HAVE_STRNCPY == 0
/*
 * Copy STR2 to STR1 until LEN or null
 */
char	*strncpy(char *str1, const char *str2, const int len)
{
  char		*str1_p, null_reached_b = FALSE;
  int		len_c;
  
  for (len_c = 0, str1_p = str1; len_c < len; len_c++, str1_p++, str2++) {
    if (null_reached || *str2 == '\0') {
      null_reached = TRUE;
      *str1_p = '\0';
    }
    else {
      *str1_p = *str2;
    }
  }
  
  return str1;
}
#endif /* HAVE_STRNCPY == 0 */

#if HAVE_STRTOK == 0
/*
 * Get the next token from STR (pass in NULL on the 2nd, 3rd,
 * etc. calls), tokens are a list of characters deliminated by a
 * character from DELIM.  writes null into STR to end token.
 */
char	*strtok(char *str, char *delim)
{
  static char	*last_str = "";
  char		*start, *delim_p;
  
  /* no new strings to search? */
  if (str != NULL) {
    last_str = str;
  }
  else {
    /* have we reached end of old one? */
    if (*last_str == '\0') {
      return NULL;
    }
  }
  
  /* parse through starting token deliminators */
  for (; *last_str != '\0'; last_str++) {
    for (delim_p = delim; *delim_p != '\0'; delim_p++) {
      if (*last_str == *delim_p) {
	break;
      }
    }
    
    /* is the character NOT in the delim list? */
    if (*delim_p == '\0') {
      break;
    }
  }
  
  /* did we reach the end? */
  if (*last_str == '\0') {
    return NULL;
  }
  
  /* now start parsing through the string, could be '\0' already */
  for (start = last_str; *last_str != '\0'; last_str++) {
    for (delim_p = delim; *delim_p != '\0'; delim_p++) {
      if (*last_str == *delim_p) {
	/* punch NULL and point last_str past it */
	*last_str++ = '\0';
	return start;
      }
    }
  }
  
  /* reached the end of the string */
  return start;
}
#endif /* HAVE_STRTOK == 0 */
