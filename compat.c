/*
 * compatibility functions for those systems who are missing them.
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the dmalloc package.
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
 * This file holds the compatibility routines necessary for the library to
 * function just in case your system does not have them.
 */

#define DMALLOC_DEBUG_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "compat.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: compat.c,v 1.28 1994/09/10 23:27:14 gray Exp $";
#endif

#if HAVE_BCOPY == 0
/*
 * copy LEN characters from DEST to SRC
 */
EXPORT	void	bcopy(const char * src, char * dest, DMALLOC_SIZE len)
{
  char		*destp;
  const	char	*srcp;
  int		bytec;
  
  if (len <= 0)
    return;
  
  srcp = src;
  destp = dest;
  
  if (srcp <= destp && srcp + (len - 1) >= destp) {
    /* overlap, must copy right-to-left. */
    srcp += len - 1;
    destp += len - 1;
    for (bytec = 0; bytec < len; bytec++)
      *destp-- = *srcp--;
  } else
    for (bytec = 0; bytec < len; bytec++)
      *destp++ = *srcp++;
}
#endif /* HAVE_BCOPY == 0 */

#if HAVE_BCMP == 0
/*
 * compare LEN characters, return -1,0,1 if STR1 is <,==,> STR2
 */
EXPORT	int	bcmp(const char * str1, const char * str2, DMALLOC_SIZE len)
{
  for (; len > 0; len--, str1++, str2++)
    if (*str1 != *str2)
      return *str1 - *str2;
  
  return 0;
}
#endif /* HAVE_BCMP == 0 */

#if HAVE_GETPID == 0
/*
 * return a fake pid for brain-dead systems without getpid()
 */
EXPORT	int	getpid(void)
{
  return 0;
}
#endif /* HAVE_GETPID == 0 */

#if HAVE_TIME == 0
/*
 * return a fake time for brain-dead systems without time()
 */
EXPORT	long	time(long * timep)
{
  if (timep != NULL)
    *timep = 0L;
  return 0L;
}
#endif /* HAVE_TIME == 0 */

#if HAVE_INDEX == 0
/*
 * find CH in STR by searching backwards through the string
 */
EXPORT	char	*index(const char * str, int ch)
{
  for (; *str != NULLC; str++)
    if (*str == (char)ch)
      return (char *)str;
  
  if (ch == NULLC)
    return (char *)str;
  else
    return NULL;
}
#endif /* HAVE_INDEX == 0 */

#if HAVE_MEMSET == 0
/*
 * set LEN characters in STR to character CH
 * NOTE: remember Gray, there is no bset().
 */
EXPORT	char	*memset(char * str, int ch, DMALLOC_SIZE len)
{
  char	*hold = str;
  
  for (; len > 0; len--, str++)
    *str = (char)ch;
  
  return hold;
}
#endif /* HAVE_MEMSET == 0 */

#if HAVE_RINDEX == 0
/*
 * find CH in STR by searching backwards through the string
 */
EXPORT	char	*rindex(const char * str, int ch)
{
  const char	*pnt = NULL;
  
  for (; *str != NULLC; str++)
    if (*str == (char)ch)
      pnt = (char *)str;
  
  if (ch == NULLC)
    return (char *)str;
  else
    return (char *)pnt;
}
#endif /* HAVE_RINDEX == 0 */

#if HAVE_STRCAT == 0
/*
 * concatenate STR2 onto the end of STR1
 */
EXPORT	char	*strcat(char * str1, const char * str2)
{
  char	*hold = str1;
  
  for (; *str1 != NULLC; str1++);
  
  while (*str2 != NULLC)
    *str1++ = *str2++;
  *str1 = NULLC;
  
  return hold;
}
#endif /* HAVE_STRCAT == 0 */

#if HAVE_STRCMP == 0
/*
 * returns -1,0,1 on whether STR1 is <,==,> STR2
 */
EXPORT	int	strcmp(const char * str1, const char * str2)
{
  for (; *str1 != NULLC && *str1 == *str2; str1++, str2++);
  return *str1 - *str2;
}
#endif /* HAVE_STRCMP == 0 */

#if HAVE_STRCPY == 0
/*
 * copies STR2 to STR1.  returns STR1
 */
EXPORT	char	*strcpy(char * str1, const char * str2)
{
  char	*strp;
  
  for (strp = str1; *str2 != NULLC; strp++, str2++)
    *strp = *str2;
  *strp = NULLC;
  
  return str1;
}
#endif /* HAVE_STRCPY == 0 */

#if HAVE_STRDUP == 0
/*
 * alloc space for PTR (with NULL) and copy it to new space, user must free
 */
EXPORT	char	*strdup(const char * ptr)
{
  char	*ret;
  int	len;
  
  len = strlen(ptr);
  ret = (char *)malloc(len + 1);
  if (ret != NULL)
    (void)strcpy(ret, ptr);
  
  return ret;
}
#endif

#if HAVE_STRLEN == 0
/*
 * return the length in characters of STR
 */
EXPORT	DMALLOC_SIZE	strlen(const char * str)
{
  int	len;
  
  for (len = 0; *str != NULLC; str++, len++);
  
  return len;
}
#endif /* HAVE_STRLEN == 0 */

#if HAVE_STRNCMP == 0
/*
 * compare at most LEN chars in STR1 and STR2 and return -1,0,1 or STR1 - STR2
 */
EXPORT	int	strncmp(const char * str1, const char * str2, const int len)
{
  int	lenc;
  
  for (lenc = 0; lenc < len; lenc++, str1++, str2++)
    if (*str1 != *str2 || *str1 == NULLC)
      return *str1 - *str2;
  
  return 0;
}
#endif /* HAVE_STRNCMP == 0 */

#if HAVE_STRNCPY == 0
/*
 * copy STR2 to STR1 until LEN or null
 */
EXPORT	char	*strncpy(char * str1, const char * str2, const int len)
{
  char		*str1p, null_reached = FALSE;
  int		lenc;
  
  for (lenc = 0, str1p = str1; lenc < len; lenc++, str1p++, str2++)
    if (null_reached || *str2 == NULLC) {
      null_reached = TRUE;
      *str1p = NULLC;
    }
    else
      *str1p = *str2;
  
  return str1;
}
#endif /* HAVE_STRNCPY == 0 */

#if HAVE_STRSTR == 0
/*
 * find STR2 inside STR1, returns NULL if not found
 */
EXPORT	char	*strstr(const char * str1, const char * str2)
{
  int		len1, len2;
  
  /* find the 2 lengths */
  len1 = strlen(str1);
  len2 = strlen(str2);
  len1 -= len2;
  
  /* look for the string */
  for (; len1 >= 0; len1--, str1++)
    if (strncmp(str1, str2, len2) == 0)
      return (char *)str1;
  
  return NULL;
}
#endif

#if HAVE_STRTOK == 0
/*
 * get the next token from STR (pass in NULL on the 2nd, 3rd, etc. calls),
 * tokens are a list of characters deliminated by a character from DELIM.
 * writes null into STR to end token.
 */
EXPORT	char	*strtok(char * str, char * delim)
{
  static char	*last_str = "";
  char		*start, *delimp;
  
  /* no new strings to search? */
  if (str != NULL)
    last_str = str;
  else
    /* have we reached end of old one? */
    if (*last_str == NULLC)
      return NULL;
  
  /* parse through starting token deliminators */
  for (; *last_str != NULLC; last_str++) {
    for (delimp = delim; *delimp != NULLC; delimp++)
      if (*last_str == *delimp)
	break;
    
    /* is the character NOT in the delim list? */
    if (*delimp == NULLC)
      break;
  }
  
  /* did we reach the end? */
  if (*last_str == NULLC)
    return NULL;
  
  /* now start parsing through the string, could be NULLC already */
  for (start = last_str; *last_str != NULLC; last_str++)
    for (delimp = delim; *delimp != NULLC; delimp++)
      if (*last_str == *delimp) {
	/* punch NULL and point last_str past it */
	*last_str++ = NULLC;
	return start;
      }
  
  /* reached the end of the string */
  return start;
}
#endif /* HAVE_STRTOK == 0 */
