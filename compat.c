/*
 * compatibility functions for those systems who are missing them.
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

#define COMPAT_MAIN

#include "malloc.h"
#include "malloc_loc.h"

#include "conf.h"

LOCAL	char	*rcs_id =
  "$Id: compat.c,v 1.1 1992/11/10 00:24:47 gray Exp $";

#ifndef HAVE_STRCHR
/*
 * find CH in STR by searching backwards through the string
 */
EXPORT	char	*strchr(char * str, char ch)
{
  for (; *str != NULLC; str++)
    if (*str == ch)
      return str;
  
  return NULL;
}
#endif /* ! HAVE_STRCHR */

#ifndef HAVE_STRRCHR
/*
 * find CH in STR by searching backwards through the string
 */
EXPORT	char	*strrchr(char * str, char ch)
{
  char	*pnt = NULL;
  
  for (; *str != NULLC; str++)
    if (*str == ch)
      pnt = str;
  
  return pnt;
}
#endif /* ! HAVE_STRRCHR */

#ifndef HAVE_STRCAT
/*
 * concatenate STR2 onto the end of STR1
 */
EXPORT	char	*strcat(char * str1, char * str2)
{
  char	*hold = str1;
  
  for (; *str1 != NULLC; str1++);
  
  while (*str2 != NULLC)
    *str1++ = *str2++;
  *str1 = NULLC;
  
  return hold;
}
#endif /* ! HAVE_STRCAT */

#ifndef HAVE_STRCMP
/*
 * returns -1,0,1 on whether STR1 is <,==,> STR2
 */
EXPORT	char	*strcmp(char * str1, char * str2)
{
  for (; *str1 != NULLC && *str1 == *str2; str1++, str2++);
  return *str1 - *str2;
}
#endif /* ! HAVE_STRCMP */

#ifndef HAVE_STRLEN
/*
 * return the length in characters of STR
 */
EXPORT	int	strlen(char * str)
{
  int	len;
  
  for (len = 0; *str != NULLC; str++, len++);
  
  return len;
}
#endif /* ! HAVE_STRLEN */

#ifndef HAVE_STRTOK
/*
 * get the next token from STR (pass in NULL on the 2nd, 3rd, etc. calls),
 * tokens are a list of characters deliminated by a character from DELIM.
 * writes null into STR to end token.
 */
EXPORT	char	*strtok(char * str, char * delim)
{
  static char	*last_str = "";
  char		*start, *delimp;
  
  /* str could be NULL, idiot */
  IS_ARG(delim, !=, NULL, strtok);
  
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
#endif /* ! HAVE_STRTOK */
