/*
 * Functions for testing of string routines arguments.
 *
 * Copyright 2000 by Gray Watson
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
 * The author may be contacted via http://dmalloc.com/
 *
 * $Id: arg_check.c,v 1.28 2000/10/10 23:06:23 gray Exp $
 */

/*
 * This file contains functions to be used to verify the arguments of
 * string functions.   If enabled these can discover problems with
 * heap-based strings (such as fence errors) much closer to the error.
 */

#define DMALLOC_DISABLE

#if HAVE_STRING_H
# include <string.h>
#endif

#include "dmalloc.h"
#include "conf.h"

#include "chunk.h"
#include "debug_val.h"
#include "error.h"
#include "dmalloc_loc.h"
#include "arg_check.h"

#if INCLUDE_RCS_IDS
#if IDENT_WORKS
#ident "$Id: arg_check.c,v 1.28 2000/10/10 23:06:23 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: arg_check.c,v 1.28 2000/10/10 23:06:23 gray Exp $";
#endif
#endif

#if HAVE_BCMP
/*
 * Dummy function for checking bcmp's arguments.
 */
int	_dmalloc_bcmp(const void *b1, const void *b2, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("bcmp", b1, CHUNK_PNT_LOOSE, len))
	|| (! _chunk_pnt_check("bcmp", b2, CHUNK_PNT_LOOSE, len))) {
      _dmalloc_message("bad pointer argument found in bcmp");
    }
  }
  return bcmp(b1, b2, len);
}
#endif

#if HAVE_BCOPY
/*
 * Dummy function for checking bcopy's arguments.
 */
void	_dmalloc_bcopy(const void *from, void *to, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("bcopy", from, CHUNK_PNT_LOOSE, len))
	|| (! _chunk_pnt_check("bcopy", to, CHUNK_PNT_LOOSE, len))) {
      _dmalloc_message("bad pointer argument found in bcopy");
    }
  }
  bcopy(from, to, len);
}
#endif

#if HAVE_MEMCMP
/*
 * Dummy function for checking memcmp's arguments.
 */
int	_dmalloc_memcmp(const void *b1, const void *b2, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("memcmp", b1, CHUNK_PNT_LOOSE, len))
	|| (! _chunk_pnt_check("memcmp", b2, CHUNK_PNT_LOOSE, len))) {
      _dmalloc_message("bad pointer argument found in memcmp");
    }
  }
  return memcmp(b1, b2, len);
}
#endif

#if HAVE_MEMCPY
/*
 * Dummy function for checking memcpy's arguments.
 */
void	*_dmalloc_memcpy(void *to, const void *from, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("memcpy", to, CHUNK_PNT_LOOSE, len))
	|| (! _chunk_pnt_check("memcpy", from, CHUNK_PNT_LOOSE, len))) {
      _dmalloc_message("bad pointer argument found in memcpy");
    }
  }
  return (void *)memcpy(to, from, len);
}
#endif

#if HAVE_MEMSET
/*
 * Dummy function for checking memset's arguments.
 */
void	*_dmalloc_memset(void *buf, const int ch, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("memset", buf, CHUNK_PNT_LOOSE, len)) {
      _dmalloc_message("bad pointer argument found in memset");
    }
  }
  return (void *)memset(buf, ch, len);
}
#endif

#if HAVE_INDEX
/*
 * Dummy function for checking index's arguments.
 */
char	*_dmalloc_index(const char *str, const char ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("index", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			   0)) {
      _dmalloc_message("bad pointer argument found in index");
    }
  }
  return (char *)index(str, ch);
}
#endif

#if HAVE_RINDEX
/*
 * Dummy function for checking rindex's arguments.
 */
char	*_dmalloc_rindex(const char *str, const char ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("rindex", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			   0)) {
      _dmalloc_message("bad pointer argument found in rindex");
    }
  }
  return (char *)rindex(str, ch);
}
#endif

#if HAVE_STRCAT
/*
 * Dummy function for checking strcat's arguments.
 */
char	*_dmalloc_strcat(char *to, const char *from)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strcat", to, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    strlen(to) + strlen(from) + 1))
	|| (! _chunk_pnt_check("strcat", from,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strcat");
    }
  }
  return (char *)strcat(to, from);
}
#endif

#if HAVE_STRCMP
/*
 * Dummy function for checking strcmp's arguments.
 */
int	_dmalloc_strcmp(const char *s1, const char *s2)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strcmp", s1, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strcmp", s2, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			       0))) {
      _dmalloc_message("bad pointer argument found in strcmp");
    }
  }
  return strcmp(s1, s2);
}
#endif

#if HAVE_STRLEN
/*
 * Dummy function for checking strlen's arguments.
 */
DMALLOC_SIZE	_dmalloc_strlen(const char *str)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("strlen", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			   0)) {
      _dmalloc_message("bad pointer argument found in strlen");
    }
  }
  return strlen(str);
}
#endif

#if HAVE_STRTOK
/*
 * Dummy function for checking strtok's arguments.
 */
char	*_dmalloc_strtok(char *str, const char *sep)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((str != NULL
	 && (! _chunk_pnt_check("strtok", str,
				CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0)))
	|| (! _chunk_pnt_check("strtok", sep,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strtok");
    }
  }
  return (char *)strtok(str, sep);
}
#endif

#if HAVE_BZERO
/*
 * Dummy function for checking bzero's arguments.
 */
void	_dmalloc_bzero(void *buf, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("bzero", buf, CHUNK_PNT_LOOSE, len)) {
      _dmalloc_message("bad pointer argument found in bzero");
    }
  }
  bzero(buf, len);
}
#endif

#if HAVE_MEMCCPY
/*
 * Dummy function for checking memccpy's arguments.
 */
void	*_dmalloc_memccpy(void *s1, const void *s2, const int ch,
			  const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* maybe len maybe first ch */
    if ((! _chunk_pnt_check("memccpy", s1, CHUNK_PNT_LOOSE, 0))
	|| (! _chunk_pnt_check("memccpy", s2, CHUNK_PNT_LOOSE, 0))) {
      _dmalloc_message("bad pointer argument found in memccpy");
    }
  }
  return (void *)memccpy(s1, s2, ch, len);
}
#endif

#if HAVE_MEMCHR
/*
 * Dummy function for checking memchr's arguments.
 */
void	*_dmalloc_memchr(const void *s1, const int ch, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("memchr", s1, CHUNK_PNT_LOOSE, len)) {
      _dmalloc_message("bad pointer argument found in memchr");
    }
  }
  return (void *)memchr(s1, ch, len);
}
#endif

#if HAVE_STRCHR
/*
 * Dummy function for checking strchr's arguments.
 */
char	*_dmalloc_strchr(const char *str, const int ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("strchr", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			   0)) {
      _dmalloc_message("bad pointer argument found in strchr");
    }
  }
  return (char *)strchr(str, ch);
}
#endif

#if HAVE_STRRCHR
/*
 * Dummy function for checking strrchr's arguments.
 */
char	*_dmalloc_strrchr(const char *str, const int ch)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if (! _chunk_pnt_check("strrchr", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			   0)) {
      _dmalloc_message("bad pointer argument found in strrchr");
    }
  }
  return (char *)strrchr(str, ch);
}
#endif

#if HAVE_STRCPY
/*
 * Dummy function for checking strcpy's arguments.
 */
char	*_dmalloc_strcpy(char *to, const char *from)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strcpy", to, CHUNK_PNT_LOOSE,
			    strlen(from) + 1))
	|| (! _chunk_pnt_check("strcpy", from,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strcpy");
    }
  }
  return (char *)strcpy(to, from);
}
#endif

#if HAVE_STRNCPY
/*
 * Dummy function for checking strncpy's arguments.
 */
char	*_dmalloc_strncpy(char *to, const char *from, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* len or until nullc */
    if ((! _chunk_pnt_check("strncpy", to, CHUNK_PNT_LOOSE, 0))
	|| (! _chunk_pnt_check("strncpy", from,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strncpy");
    }
  }
  return (char *)strncpy(to, from, len);
}
#endif

#if HAVE_STRCASECMP
/*
 * Dummy function for checking strcasecmp's arguments.
 */
int	_dmalloc_strcasecmp(const char *s1, const char *s2)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strcasecmp", s1,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))
	|| (! _chunk_pnt_check("strcasecmp", s2,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strcasecmp");
    }
  }
  return strcasecmp(s1, s2);
}
#endif

#if HAVE_STRNCASECMP
/*
 * Dummy function for checking strncasecmp's arguments.
 */
int	_dmalloc_strncasecmp(const char *s1, const char *s2,
			     const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* len or until nullc */
    if ((! _chunk_pnt_check("strncasecmp", s1,
			    CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))
	|| (! _chunk_pnt_check("strncasecmp", s2,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strncasecmp");
    }
  }
  return strncasecmp(s1, s2, len);
}
#endif

#if HAVE_STRSPN
/*
 * Dummy function for checking strspn's arguments.
 */
int	_dmalloc_strspn(const char *str, const char *list)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strspn", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strspn", list,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strspn");
    }
  }
  return strspn(str, list);
}
#endif

#if HAVE_STRCSPN
/*
 * Dummy function for checking strcspn's arguments.
 */
int	_dmalloc_strcspn(const char *str, const char *list)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strcspn", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strcspn", list,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strcspn");
    }
  }
  return strcspn(str, list);
}
#endif

#if HAVE_STRNCAT
/*
 * Dummy function for checking strncat's arguments.
 */
char	*_dmalloc_strncat(char *to, const char *from, const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* either len or nullc */
    if ((! _chunk_pnt_check("strncat", to, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strncat", from,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strncat");
    }
  }
  return (char *)strncat(to, from, len);
}
#endif

#if HAVE_STRNCMP
/*
 * Dummy function for checking strncmp's arguments.
 */
int	_dmalloc_strncmp(const char *s1, const char *s2,
			 const DMALLOC_SIZE len)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    /* either len or nullc */
    if ((! _chunk_pnt_check("strncmp", s1, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strncmp", s2,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strncmp");
    }
  }
  return strncmp(s1, s2, len);
}
#endif

#if HAVE_STRPBRK
/*
 * Dummy function for checking strpbrk's arguments.
 */
char	*_dmalloc_strpbrk(const char *str, const char *list)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strpbrk", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strpbrk", list,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strpbrk");
    }
  }
  return (char *)strpbrk(str, list);
}
#endif

#if HAVE_STRSTR
/*
 * Dummy function for checking strstr's arguments.
 */
char	*_dmalloc_strstr(const char *str, const char *pat)
{
  if (BIT_IS_SET(_dmalloc_flags, DEBUG_CHECK_FUNCS)) {
    if ((! _chunk_pnt_check("strstr", str, CHUNK_PNT_LOOSE | CHUNK_PNT_NULL,
			    0))
	|| (! _chunk_pnt_check("strstr", pat,
			       CHUNK_PNT_LOOSE | CHUNK_PNT_NULL, 0))) {
      _dmalloc_message("bad pointer argument found in strstr");
    }
  }
  return (char *)strstr(str, pat);
}
#endif
