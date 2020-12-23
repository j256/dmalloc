/*
 * String appending functions that take into account buffer limit.
 *
 * Copyright 2020 by Gray Watson
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
 */

/*
 * This file holds the compatibility routines necessary for the library to
 * function just in case your system does not have them.
 */

#if HAVE_STDARG_H
# include <stdarg.h>				/* for ... */
#endif
#if HAVE_STDIO_H
# include <stdio.h>				/* for FILE */
#endif
#if HAVE_STRING_H
# include <string.h>                            /* for strlen */
#endif

#define DMALLOC_DISABLE

#include "conf.h"
#include "dmalloc.h"

#include "append.h"
#include "compat.h"
#include "dmalloc_loc.h"

/*
 * If we did not specify a precision then we need to stop somewhere
 * otherwise we get something like 99.0000100000000031741365091872284.
 * This is somewhat arbitrary.  We could try to autoconf a value based
 * on float.h and FLT_MAX or something but for internal purposes, this
 * seems fine.
 */
#define DEFAULT_DECIMAL_PRECISION	10

/*
 * Internal method to process a long or int number and write into a buffer.
 */
static	char	*handle_decimal(char *buf, char *limit, const long num, const int base)
{
  char *buf_p = buf;
  buf_p = append_long(buf_p, limit, num, base);
  append_null(buf_p, limit);
  return buf_p;
}

/*
 * Internal method to process a long or int number and write into a buffer.
 */
static	char	*handle_pointer(char *buf, char *limit, const PNT_ARITH_TYPE num, const int base)
{
  char *buf_p = buf;
  buf_p = append_pointer(buf_p, limit, num, base);
  append_null(buf_p, limit);
  return buf_p;
}

/*
 * Internal method to handle floating point numbers.
 */
static	char	*handle_float(char *buf, char *limit, double num, int decimal_precision,
			      const int no_precision)
{
  long long_num;
  char *buf_p = buf;

  /*
   * Doing a float and a double if long_arg hrows a super bad
   * exception and causes the program to abort.  Not sure if this is
   * something that we need to autoconf around.
   */
  long_num = (long)num;
  buf_p = append_long(buf_p, limit, long_num, 10);
  /* remove the int part */
  num -= (double)long_num;
  if (num == 0.0 || decimal_precision == 0) {
    append_null(buf_p, limit);
    return buf_p;
  }
  /* TODO: need to handle different number formats */
  if (buf_p < limit) {
    *buf_p++ = '.';
  }
  
  char *non_zero_limit_p = buf_p;
  while (num > 0.0 && --decimal_precision >= 0) {
    num *= 10.0;
    long_num = (long)num;
    if (long_num >= 10) {
      long_num = 9;
    }
    if (buf_p < limit) {
      *buf_p++ = '0' + long_num;
      /* track the last non-0 digit in case we need to truncate ending 000 chars */
      if (long_num != 0) {
	non_zero_limit_p = buf_p;
      }
    }
    num -= (double)long_num;
  }
  
  /* if we did not specify precision then don't end with a bunch of 0s */
  if (no_precision) {
    buf_p = non_zero_limit_p;
  }
  
  append_null(buf_p, limit);
  return buf_p;
}

/*
 * Append string argument to destination up to limit pointer.  Pointer
 * to the end of the characters added will be returned.  No \0
 * character will be added.
 */
char	*append_string(char *dest, const char *limit, const char *value)
{
  while (dest < limit && *value != '\0') {
    *dest++ = *value++;
  }
  return dest;
}

/*
 * Append long value argument to destination up to limit pointer.
 * Pointer to the end of the added characters will be returned.  No \0
 * character will be added.  Variant of itoa() written by Lukas
 * Chmela which is released under GPLv3.
 */
char	*append_long(char *dest, char *limit, long value, int base)
{
  char buf[30];
  char *ptr = buf;
  char *ptr1 = buf;
  char tmp_char;
  long tmp_value;

  /* letters that handle both negative and positive values */
  char *letters =
    "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz";
  int mid = 35;
  /* build the string with low order digits first: 100 => "001" */
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = letters[mid + tmp_value - (value * base)];
  } while (value != 0);

  /* apply negative sign */
  if (tmp_value < 0) {
    *ptr++ = '-';
  }
  *ptr-- = '\0';
  /* now we swap the characters to move high order digits first */
  while (ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr--= *ptr1;
    *ptr1++ = tmp_char;
  }
  return append_string(dest, limit, buf);
}

/*
 * Append unsigned long value argument to destination up to limit
 * pointer.  Pointer to the end of the added characters will be
 * returned.  No \0 character will be added.  Variant of itoa()
 * written by Lukas Chmela. Released under GPLv3.
 */
char	*append_ulong(char *dest, char *limit, unsigned long value, int base)
{
  char buf[30];
  char *ptr = buf;
  char *ptr1 = buf;
  char tmp_char;
  unsigned long tmp_value;

  /* letters that handle both negative and positive values */
  char *letters =
    "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz";
  int mid = 35;
  /* build the string with low order digits first: 100 => "001" */
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = letters[mid + tmp_value - (value * base)];
  } while (value != 0);

  *ptr-- = '\0';
  /* now we swap the characters to move high order digits first */
  while (ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr--= *ptr1;
    *ptr1++ = tmp_char;
  }
  return append_string(dest, limit, buf);
}

/*
 * Append pointer value argument to destination up to limit pointer.
 * Pointer to the end of the added characters will be returned.  No \0
 * character will be added.  Variant of itoa() written by Lukas
 * Chmela which is released under GPLv3.
 */
char	*append_pointer(char *dest, char *limit, PNT_ARITH_TYPE value, int base)
{
  char buf[30];
  char *ptr = buf;
  char *ptr1 = buf;
  char tmp_char;
  PNT_ARITH_TYPE tmp_value;

  /* letters that handle both negative and positive values */
  char *letters =
    "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz";
  int mid = 35;
  /* build the string with low order digits first: 100 => "001" */
  do {
    tmp_value = value;
    value /= base;
    *ptr++ = letters[mid + tmp_value - (value * base)];
  } while (value != 0);

  /* apply negative sign */
  if (tmp_value < 0) {
    *ptr++ = '-';
  }
  *ptr-- = '\0';
  /* now we swap the characters to move high order digits first */
  while (ptr1 < ptr) {
    tmp_char = *ptr;
    *ptr--= *ptr1;
    *ptr1++ = tmp_char;
  }
  return append_string(dest, limit, buf);
}

/*
 * Append a varargs format to destination.  Pointer to the end of the
 * characters added will be returned.  No \0 character will be added.
 */
char	*append_vformat(char *dest, char *limit, const char *format,
			va_list args)
{
  const char *format_p = format;
  char value_buf[64];
  char *value_limit = value_buf + sizeof(value_buf);
  char *dest_p = dest;
  int format_prefix, neg_pad, trunc, long_arg;
  char pad_char;
  char *prefix, *prefix_p;
  int width_len, trunc_len, prefix_len;
  
  while (*format_p != '\0' && dest_p < limit) {
    if (*format_p != '%') {
      *dest_p++ = *format_p++;
      continue;
    }
    format_p++;

    format_prefix = 0; 
    neg_pad = 0;
    trunc = 0;
    long_arg = 0;
    pad_char = ' ';
    prefix = "";
    width_len = -1;
    trunc_len = -1;
    prefix_len = 0;
    while (1) {
      char ch = *format_p++;
      if (ch == '\0') {
	break;
      }
      if (ch == '%') {
	if (dest_p < limit) {
	  *dest_p++ = '%';
	}
	break;
      }
      if (ch == '#') {
	format_prefix = 1;
      } else if (ch == '0' && width_len < 0 && trunc_len < 0) {
	pad_char = '0';
      } else if (ch == '-') {
	neg_pad = 1;
      } else if (ch == '.') {
	trunc = 1;
      } else if (ch == '*') {
	if (trunc) {
	  trunc_len = va_arg(args, int);
	} else {
	  width_len = va_arg(args, int);
	}
      } else if (ch >= '0' && ch <= '9') {
	if (trunc) {
	  if (trunc_len < 0) {
	    trunc_len = (ch - '0');
	  } else {
	    trunc_len = trunc_len * 10 + (ch - '0');
	  }
	} else {
	  if (width_len < 0) {
	    width_len = (ch - '0');
	  } else {
	    width_len = width_len * 10 + (ch - '0');
	  }
	}
      } else if (ch == 'l') {
	long_arg = 1;
      } else if (ch != 'c' && ch != 'd' && ch != 'f' && ch != 'o' && ch != 'p'
		 && ch != 's' && ch != 'u' && ch != 'x') {
	continue;
      }
      
      /* process the % format character */
      
      const char *value;
      if (ch == 'c') {
	value_buf[0] = (char)va_arg(args, int);
	value_buf[1] = '\0';
	value = value_buf;
      } else if (ch == 'd') {
	long num;
	if (long_arg) {
	  num = va_arg(args, long);
	} else {
	  num = va_arg(args, int);
	}
	handle_decimal(value_buf, value_limit, num, 10);
	value = value_buf;
      } else if (ch == 'f') {
	double num = va_arg(args, double);
	if (trunc_len < 0) {
	  handle_float(value_buf, value_limit, num, DEFAULT_DECIMAL_PRECISION, 1);
	} else {
	  handle_float(value_buf, value_limit, num, trunc_len, 0);
	}
	value = value_buf;
	/* special case here, the trunc length is really decimal precision */
	trunc_len = -1;
      } else if (ch == 'o') {
	long num;
	if (long_arg) {
	  num = va_arg(args, long);
	} else {
	  num = va_arg(args, int);
	}
	handle_decimal(value_buf, value_limit, num, 8);
	value = value_buf;
	if (format_prefix) {
	  prefix = "0";
	  prefix_len = 1;
	}
      } else if (ch == 'p') {
	DMALLOC_PNT pnt = va_arg(args, DMALLOC_PNT);
	PNT_ARITH_TYPE num = (PNT_ARITH_TYPE)pnt;
	handle_pointer(value_buf, value_limit, num, 16);
	value = value_buf;
	// because %#p throws a gcc warning, I've decreed that %p has a 0x hex prefix
	prefix = "0x";
	prefix_len = 2;
      } else if (ch == 's') {
	value = va_arg(args, char *);
      } else if (ch == 'u') {
	unsigned long num;
	if (long_arg) {
	  num = va_arg(args, unsigned long);
	} else {
	  num = va_arg(args, unsigned int);
	}
	char *value_buf_p = value_buf;
	value_buf_p = append_ulong(value_buf_p, value_limit, num, 10);
	append_null(value_buf_p, value_limit);
	value = value_buf;
      } else if (ch == 'x') {
	long num;
	if (long_arg) {
	  num = va_arg(args, long);
	} else {
	  num = va_arg(args, int);
	}
	handle_decimal(value_buf, value_limit, num, 16);
	if (format_prefix) {
	  prefix = "0x";
	  prefix_len = 2;
	}
	value = value_buf;
      } else {
	continue;
      }

      /* prepend optional numeric prefix which has to come before padding */
      if (prefix_len > 0) {
	prefix_p = prefix;
	while (*prefix_p != '\0' && dest_p < limit) {
	  *dest_p++ = *prefix_p++;
	}
      }

      /* handle our pre-padding with spaces or 0s */
      int pad = 0;
      if (width_len >= 0) {
	if (trunc_len >= 0) {
	  pad = width_len - strnlen(value, trunc_len) - prefix_len;
	} else {
	  pad = width_len - strlen(value) - prefix_len;
	}
	if (!neg_pad) {
	  while (pad-- > 0 && dest_p < limit) {
	    *dest_p++ = pad_char;
	  }
	}
      }

      /* change the limit if we are truncating the string */
      char *str_limit = limit;
      if (trunc_len >= 0) {
	str_limit = dest_p + trunc_len;
	if (str_limit > limit) {
	  str_limit = limit;
	}
      }

      /* copy the value watching our limit */
      while (*value != '\0' && dest_p < str_limit) {
	*dest_p++ = *value++;
      }
      
      /* maybe handle left over padding which would be negative padding */
      while (pad-- > 0 && dest_p < limit) {
	/* always pad at the end with a space */
	*dest_p++ = ' ';
      }
      break;
    }
  }
  return dest_p;
}

/*
 * Append a varargs format to destination.  Pointer to the end of the
 * added characters will be returned.  No \0 character will be added.
 */
char	*append_format(char *dest, char *limit, const char *format, ...)
{
  va_list args;  
  char *dest_p;
  
  va_start(args, format);
  dest_p = append_vformat(dest, limit, format, args);
  va_end(args);
  
  return dest_p;
}

/*
 * Append \0 character to destination.  If dest is => limit then \0
 * will be written one character before the limit.  Pointer past the
 * end of the \0 character will be returned.
 */
char    *append_null(char *dest, char *limit)
{
  if (dest < limit) {
    *dest++ = '\0';
  } else {
    *(limit - 1) = '\0';
  }
  return dest;
}

/*
 * Local implementation of snprintf.  We are doing this trying to not
 * use the system version which often can allocate memory itself
 * causing the library to go recursive.
 */
int	loc_vsnprintf(char *buf, const int size, const char *format,
		      va_list args)
{
  char *limit, *buf_p;
  limit = buf + size;
  buf_p = append_vformat(buf, limit, format, args);
  append_null(buf_p, limit);
  return (int)(buf_p - buf);
}

/*
 * Local implementation of snprintf.  We are doing this trying to not
 * use the system version which often can allocate memory itself
 * causing the library to go recursive.
 */
int	loc_snprintf(char *buf, const int size, const char *format, ...)
{
  va_list args;  
  int len;
  
  va_start(args, format);
  len = loc_vsnprintf(buf, size, format, args);
  va_end(args);
  
  return len;
}

/*
 * Local implementation of printf so we can use %p and other non-standard formats.
 */
void	loc_printf(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  loc_vfprintf(stdout, format, args);
  va_end(args);
}

/*
 * Local implementation of fprintf so we can use %p and other non-standard formats.
 */
void	loc_fprintf(FILE *file, const char *format, ...)
{
  va_list args;
  va_start(args, format);
  loc_vfprintf(file, format, args);
  va_end(args);
}

/*
 * Local implementation of vfprintf so we can use %p and other non-standard formats.
 */
void	loc_vfprintf(FILE *file, const char *format, va_list args)
{
  // these are simple messages so this limit is ok
  char buf[256];
  char *buf_p, *limit;

  limit = buf + sizeof(buf);
  buf_p = append_vformat(buf, limit, format, args);
  fwrite(buf, 1, (buf_p - buf), file);
}
