/*
 * Generic argv processor...
 *
 * Copyright 1995 by Gray Watson
 *
 * This file is part of the argv library.
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
 * The author may be contacted at gray.watson@letters.com
 */

#include <ctype.h>
#include <stdio.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>				/* for read */
#endif

#ifdef LOCAL
#include "dmalloc.h"
#endif

#include "dmalloc_argv.h"
#include "dmalloc_argv_loc.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: dmalloc_argv.c,v 1.6 1998/10/15 20:26:08 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: dmalloc_argv.c,v 1.6 1998/10/15 20:26:08 gray Exp $";
#endif
#endif

/* internal routines */
static	void	do_list(argv_t *grid, const int argc, char **argv,
			char *okay_p);

/*
 * exported variables
 */
/* This is a processed version of argv[0], pre-path removed: /bin/ls -> ls */
char	argv_program[PROGRAM_NAME + 1] = "Unknown";

/* A global value of argv from main after argv_process has been called */
char	**argv_argv = NULL;
/* A global value of argc from main after argv_process has been called */
int	argv_argc = 0;

/* This should be set externally to provide general program help to user */
char	*argv_help_string = NULL;
/* This should be set externally to provide version information to the user */
char	*argv_version_string = NULL;

/*
 * Are we running interactively?  This will exit on errors.  Set to
 * false to return error codes instead.
 */
char 	argv_interactive = ARGV_TRUE;

/*
 * The FILE stream that argv out_puts all its errors.  Set to NULL to
 * not dump any error messages.  Default is stderr.
 */
FILE 	*argv_error_stream = ERROR_STREAM_INIT;

/* local variables */
static	argv_t	**queue_list = NULL;		/* our little queue struct */
static	int	queue_head = 0;			/* head of queue */
static	int	queue_tail = 0;			/* tail of queue */
static	argv_t	empty[] = {{ ARGV_LAST }};	/* empty argument array */
static	char	enabled = ARGV_FALSE;		/* are the lights on? */

/* global settings */
static	int	global_close	= GLOBAL_CLOSE_ENABLE;	/* close processing */
static	int	global_env	= GLOBAL_ENV_BEFORE;	/* env processing */
static	int	global_error	= GLOBAL_ERROR_SEE;	/* error processing */
static	int	global_multi	= GLOBAL_MULTI_ACCEPT;	/* multi processing */
static	int	global_usage	= GLOBAL_USAGE_LONG;	/* usage processing */
static	int	global_lasttog	= GLOBAL_LASTTOG_DISABLE; /*last-arg toggling*/

/****************************** startup routine ******************************/

/*
 * Turn on the lights.
 */
static	void	argv_startup(void)
{
  if (enabled) {
    return;
  }
  enabled = ARGV_TRUE;
  
  /* ANSI says we cannot predefine this above */
  if (argv_error_stream == ERROR_STREAM_INIT) {
    argv_error_stream = stderr;
  }
}

/***************************** general utilities *****************************/

/*
 * Binary STR to integer translation
 */
static	int	btoi(const char *str)
{
  int		ret = 0;
  
  /* strip off spaces */
  for (; isspace(*str); str++);
  
  for (; *str == '0' || *str == '1'; str++) {
    ret *= 2;
    ret += *str - '0';
  }
  
  return ret;
}

/*
 * Octal STR to integer translation
 */
static	int	otoi(const char *str)
{
  int		ret = 0;
  
  /* strip off spaces */
  for (; isspace(*str); str++);
  
  for (; *str >= '0' && *str <= '7'; str++) {
    ret *= 8;
    ret += *str - '0';
  }
  
  return ret;
}

/*
 * Hexadecimal STR to integer translation
 */
static	int	htoi(const char *str)
{
  int		ret = 0;
  
  /* strip off spaces */
  for (; isspace(*str); str++);
  
  /* skip a leading 0[xX] */
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) {
    str += 2;
  }
  
  for (; isdigit(*str) ||
       (*str >= 'a' && *str <= 'f') || (*str >= 'A' && *str <= 'F');
       str++) {
    ret *= 16;
    if (*str >= 'a' && *str <= 'f') {
      ret += *str - 'a' + 10;
    }
    else if (*str >= 'A' && *str <= 'F') {
      ret += *str - 'A' + 10;
    }
    else {
      ret += *str - '0';
    }
  }
  
  return ret;
}

/*
 * Basically a strdup for compatibility sake
 */
static	char	*string_copy(const char *ptr)
{
  const char	*ptr_p;
  char		*ret, *ret_p;
  int		len;
  
  len = strlen(ptr);
  ret = (char *)malloc(len + 1);
  if (ret == NULL) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return NULL;
  }
  
  for (ptr_p = ptr, ret_p = ret; *ptr_p != '\0';) {
    *ret_p++ = *ptr_p++;
  }
  *ret_p = '\0';
  
  return ret;
}

/*
 * Break STR and return an array of char * whose values are tokenized
 * by TOK.  it passes back the number of tokens in TOKN.
 *
 * NOTE: the return value should be freed later and the STR should stay
 * around until that time.
 */
static	char	**vectorize(char *str, const char *tok, int *tokn)
{
  char	**vect_p;
  char	*tmp, *tok_p;
  int	tok_c;
  
  /* count the tokens */
  tmp = string_copy(str);
  if (tmp == NULL) {
    return NULL;
  }
  
  tok_p = strtok(tmp, tok);
  for (tok_c = 0; tok_p != NULL; tok_c++) {
    tok_p = strtok(NULL, tok);
  }
  free(tmp);
  
  *tokn = tok_c;
  
  if (tok_c == 0) {
    return NULL;
  }
  
  /* allocate the pointer grid */
  vect_p = (char **)malloc(sizeof(char *) * tok_c);
  if (vect_p == NULL) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return NULL;
  }
  
  /* load the tokens into the list */
  vect_p[0] = strtok(str, tok);
  
  for (tok_c = 1; tok_c < *tokn; tok_c++) {
    vect_p[tok_c] = strtok(NULL, tok);
  }
  
  return vect_p;
}

/*
 * Display printable chars from BUF of SIZE, non-printables as \%03o
 */
static	char	*expand_buf(const void *buf, const int size)
{
  static char	out[DUMP_SPACE_BUF];
  int		size_c;
  void		*buf_p;
  char	 	*out_p;
  
  for (size_c = 0, out_p = out, buf_p = (void *)buf; size_c < size;
       size_c++, buf_p = (char *)buf_p + 1) {
    char	*spec_p;
    
    /* handle special chars */
    if (out_p + 2 >= out + sizeof(out)) {
      break;
    }
    
    /* search for special characters */
    for (spec_p = SPECIAL_CHARS + 1; *(spec_p - 1) != '\0'; spec_p += 2) {
      if (*spec_p == *(char *)buf_p) {
	break;
      }
    }
    
    /* did we find one? */
    if (*(spec_p - 1) != '\0') {
      if (out_p + 2 >= out + sizeof(out)) {
	break;
      }
      (void)sprintf(out_p, "\\%c", *(spec_p - 1));
      out_p += 2;
      continue;
    }
    
    if (*(unsigned char *)buf_p < 128 && isprint(*(char *)buf_p)) {
      if (out_p + 1 >= out + sizeof(out)) {
	break;
      }
      *out_p = *(char *)buf_p;
      out_p += 1;
    }
    else {
      if (out_p + 4 >= out + sizeof(out)) {
	break;
      }
      (void)sprintf(out_p, "\\%03o", *(unsigned char *)buf_p);
      out_p += 4;
    }
  }
  
  *out_p = '\0';
  return out;
}

/****************************** usage routines *******************************/

/*
 * Print a short-format usage message.
 */
static	void	usage_short(const argv_t *args, const int flag)
{
  const argv_t	*arg_p;
  int		len, col_c = 0;
  char		mark = ARGV_FALSE, *prefix;
  
  if (argv_error_stream == NULL) {
    return;
  }
  
  /* print the usage message header */
  (void)fprintf(argv_error_stream, "%s%s", USAGE_LABEL, argv_program);
  col_c += USAGE_LABEL_LENGTH + strlen(argv_program);
  
  /*
   * print all of the boolean arguments first.
   * NOTE: we assume they all fit on the line
   */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    
    /* skip or-specifiers */
    if (arg_p->ar_short_arg == ARGV_OR
	|| arg_p->ar_short_arg == ARGV_XOR) {
      continue;
    }
    
    /* skip non booleans */
    if (HAS_ARG(arg_p->ar_type)) {
      continue;
    }
    
    /* skip args with no short component */
    if (arg_p->ar_short_arg == '\0') {
      continue;
    }
    
    if (! mark) {
      len = 2 + SHORT_PREFIX_LENGTH;
      prefix = " [";
      
      /* we check for -2 here because we should have 1 arg and ] on line */
      if (col_c + len > SCREEN_WIDTH - 2) {
	(void)fprintf(argv_error_stream, "\n%*.*s",
		      (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "");
	col_c = USAGE_LABEL_LENGTH;
	
	/* if we are the start of a line, skip any starting spaces */
	if (*prefix == ' ') {
	  prefix++;
	  len--;
	}
      }
      
      (void)fprintf(argv_error_stream, "%s%s", prefix, SHORT_PREFIX);
      col_c += len;
      mark = ARGV_TRUE;
    }
    
    len = 1;
    /* we check for -1 here because we should need ] */
    if (col_c + len > SCREEN_WIDTH - 1) {
      (void)fprintf(argv_error_stream, "]\n%*.*s",
		    (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "");
      col_c = USAGE_LABEL_LENGTH;
      
      /* restart the short option list */
      (void)fprintf(argv_error_stream, "[%s", SHORT_PREFIX);
      col_c += 1 + SHORT_PREFIX_LENGTH;
    }
    
    (void)fprintf(argv_error_stream, "%c", arg_p->ar_short_arg);
    col_c++;
  }
  
  if (mark) {
    (void)fprintf(argv_error_stream, "]");
    col_c++;
  }
  
  /* print remaining (non-boolean) arguments */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    int		var_len;
    char	*var_str, *postfix;
    
    /* skip or-specifiers */
    if (arg_p->ar_short_arg == ARGV_OR
	|| arg_p->ar_short_arg == ARGV_XOR) {
      continue;
    }
    
    /* skip booleans types */
    if (! HAS_ARG(arg_p->ar_type)) {
      continue;
    }
    
    if (arg_p->ar_var_label == NULL) {
      if (ARGV_TYPE(arg_p->ar_type) == ARGV_BOOL_ARG) {
	var_str = BOOL_ARG_LABEL;
	var_len = BOOL_ARG_LENGTH;
      }
      else {
	var_len = UNKNOWN_ARG_LENGTH;
	var_str = UNKNOWN_ARG;
      }
    }
    else {
      var_len = strlen(arg_p->ar_var_label);
      var_str = arg_p->ar_var_label;
    }
    
    if (arg_p->ar_short_arg == ARGV_MAND) {
      /* print the mandatory argument desc */
      len = 1 + var_len;
      prefix = " ";
      postfix = "";
    }
    else if (arg_p->ar_short_arg == ARGV_MAYBE) {
      /* print the maybe argument desc */      
      len = 2 + var_len + 1;
      prefix = " [";
      postfix = "]";
    }
    else {
      /* handle options with arguments */
      
      /* " [" + short_prefix + char */
      len = 2 + SHORT_PREFIX_LENGTH + 1;
      prefix = " [";
      
      /* do we need to wrap */
      if (col_c + len > SCREEN_WIDTH) {
	(void)fprintf(argv_error_stream, "\n%*.*s",
		      (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "");
	col_c = USAGE_LABEL_LENGTH;
	
	/* if we are the start of a line, skip any starting spaces */
	if (*prefix == ' ') {
	  prefix++;
	  len--;
	}
      }
      (void)fprintf(argv_error_stream, "%s%s%c",
		    prefix, SHORT_PREFIX, arg_p->ar_short_arg);
      col_c += len;
      
      len = 1 + var_len + 1;
      prefix = " ";
      postfix = "]";
    }
    
    if (col_c + len > SCREEN_WIDTH) {
      (void)fprintf(argv_error_stream, "\n%*.*s",
		    (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "");
      col_c = USAGE_LABEL_LENGTH;
      
      /* if we are the start of a line, skip any starting spaces */
      if (*prefix == ' ') {
	prefix++;
	len--;
      }
    }
    
    (void)fprintf(argv_error_stream, "%s%s%s", prefix, var_str, postfix);
    col_c += len;
  }
  
  (void)fprintf(argv_error_stream, "\n");
  
  if (flag == GLOBAL_USAGE_SHORTREM) {
    (void)fprintf(argv_error_stream,
		  "%*.*sUse the '%s%s' argument for more assistance.\n",
		  (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "",
		  LONG_PREFIX, USAGE_ARG);
  }
}

/*
 * Display an argument type while keeping track of COL_C.
 */
static	void	display_arg(FILE *stream, const argv_t *arg_p, const int max,
			    int *col_c)
{
  int	var_len, len;
  
  if (arg_p->ar_var_label == NULL) {
    var_len = 0;
  }
  else {
    var_len = strlen(arg_p->ar_var_label);
  }
  
  switch (ARGV_TYPE(arg_p->ar_type)) {
    
  case ARGV_BOOL:
  case ARGV_BOOL_NEG:
  case ARGV_INCR:
    break;
    
  case ARGV_BOOL_ARG:
    (void)fprintf(stream, "%s", BOOL_ARG_LABEL);
    (*col_c) += BOOL_ARG_LENGTH;
    break;
    
  case ARGV_CHAR:
  case ARGV_CHAR_P:
  case ARGV_SHORT:
  case ARGV_U_SHORT:
  case ARGV_INT:
  case ARGV_U_INT:
  case ARGV_LONG:
  case ARGV_U_LONG:
  case ARGV_FLOAT:
  case ARGV_DOUBLE:
  case ARGV_BIN:
  case ARGV_OCT:
  case ARGV_HEX:
  case ARGV_SIZE:
  case ARGV_U_SIZE:
    if (arg_p->ar_var_label == NULL) {
      len = max - *col_c;
      (void)fprintf(stream, "%-.*s", len, UNKNOWN_ARG);
      *col_c += MIN(len, (int)UNKNOWN_ARG_LENGTH);
    }
    else {
      len = max - *col_c;
      (void)fprintf(stream, "%-.*s", len, arg_p->ar_var_label);
      *col_c += MIN(len, var_len);
    }
    break;
  }
}

/*
 * Display an option entry ARG_P to STREAM while counting COL_C.
 */
static	void	display_option(FILE *stream, const argv_t *arg_p, int *col_c)
{
  if (stream == NULL) {
    return;
  }
  
  (void)fputc('[', stream);
  (*col_c)++;
  
  /* arg maybe does not have a -? preface */
  if (arg_p->ar_short_arg != ARGV_MAYBE) {
    (void)fprintf(stream, "%s%c",
		  SHORT_PREFIX, arg_p->ar_short_arg);
    *col_c += SHORT_PREFIX_LENGTH + 1;
    
    if (HAS_ARG(arg_p->ar_type)) {
      /* display optional argument */
      (void)fputc(' ', stream);
      (*col_c)++;
    }
  }
  
  display_arg(stream, arg_p, LONG_COLUMN - 1, col_c);
  (void)fputc(']', stream);
  (*col_c)++;
}

/*
 * Print a long-format usage message.
 */
static	void	usage_long(const argv_t *args)
{
  const argv_t	*arg_p;
  int		col_c, len;
  
  if (argv_error_stream == NULL) {
    return;
  }
  
  /* print the usage message header */
  (void)fprintf(argv_error_stream, "%s%s\n", USAGE_LABEL, argv_program);
  
  /* run through the argument structure */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    
    /* skip or specifiers */
    if (arg_p->ar_short_arg == ARGV_OR || arg_p->ar_short_arg == ARGV_XOR) {
      continue;
    }
    
    /* indent to the short-option col_c */
    (void)fprintf(argv_error_stream, "%*.*s", SHORT_COLUMN, SHORT_COLUMN, "");
    
    /* start column counter */
    col_c = SHORT_COLUMN;
    
    /* print the short-arg stuff if there */
    if (arg_p->ar_short_arg == '\0') {
      (void)fputc('[', argv_error_stream);
      col_c++;
    }
    else {
      if (arg_p->ar_short_arg == '\0') {
	;
      }
      else if (arg_p->ar_short_arg == ARGV_MAND) {
	display_arg(argv_error_stream, arg_p, COMMENT_COLUMN, &col_c);
      }
      else {
	/* ARGV_MAYBE handled here */
	display_option(argv_error_stream, arg_p, &col_c);
      }
      
      /* put the long-option message on the correct column */
      if (col_c < LONG_COLUMN) {
	(void)fprintf(argv_error_stream, "%*.*s",
		      LONG_COLUMN - col_c, LONG_COLUMN - col_c, "");
	col_c = LONG_COLUMN;
      }
    }
    
    /* print the long-option message */
    if (arg_p->ar_long_arg != NULL) {
      len = COMMENT_COLUMN - col_c - (LONG_PREFIX_LENGTH + 1);
      if (arg_p->ar_short_arg != '\0') {
	(void)fprintf(argv_error_stream, "%s", LONG_LABEL);
	col_c += LONG_LABEL_LENGTH;
	len -= LONG_LABEL_LENGTH;
      }
      (void)fprintf(argv_error_stream, "%s%-.*s",
		    LONG_PREFIX, len, arg_p->ar_long_arg);
      col_c += LONG_PREFIX_LENGTH + MIN(len, (int)strlen(arg_p->ar_long_arg));
    }
    
    /* add the optional argument if no short-arg */
    if (arg_p->ar_short_arg == '\0') {
      if (HAS_ARG(arg_p->ar_type)) {
	(void)fputc(' ', argv_error_stream);
	col_c++;
      }
      
      /* display any optional arguments */
      display_arg(argv_error_stream, arg_p, COMMENT_COLUMN - 1, &col_c);
      (void)fputc(']', argv_error_stream);
      col_c++;
    }
    
    /* print the comment */
    if (arg_p->ar_comment != NULL) {
      /* put the comment message on the correct column */
      if (col_c < COMMENT_COLUMN) {
	(void)fprintf(argv_error_stream, "%*.*s",
		      COMMENT_COLUMN - col_c,
		      COMMENT_COLUMN - col_c, "");
	col_c = COMMENT_COLUMN;
      }
      
      len = SCREEN_WIDTH - col_c - COMMENT_LABEL_LENGTH;
      (void)fprintf(argv_error_stream, "%s%-.*s",
		    COMMENT_LABEL, len, arg_p->ar_comment);
    }
    
    (void)fprintf(argv_error_stream, "\n");
  }
}

/*
 * Do the usage depending on FLAG.
 */
static	void	do_usage(const argv_t *args, const int flag)
{
  if (argv_error_stream == NULL) {
    return;
  }
  
  if (flag == GLOBAL_USAGE_SEE) {
    (void)fprintf(argv_error_stream,
		  "%*.*sUse the '%s%s' argument for assistance.\n",
		  (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "",
		  LONG_PREFIX, USAGE_ARG);
  }
  else if (flag == GLOBAL_USAGE_SHORT || flag == GLOBAL_USAGE_SHORTREM) {
    usage_short(args, flag);
  }
  else if (flag == GLOBAL_USAGE_LONG || flag == GLOBAL_USAGE_ALL) {
    usage_long(args);
  }
  
  if (flag == GLOBAL_USAGE_ALL) {
    (void)fprintf(argv_error_stream, "\n");
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' for default usage information.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, USAGE_ARG);
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' for short usage information.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, USAGE_SHORT_ARG);
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' for long usage information.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, USAGE_LONG_ARG);
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' for all usage information.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, USAGE_ALL_ARG);
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' to display the help message.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, HELP_ARG);
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' to display the version message.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, VERSION_ARG);
    (void)fprintf(argv_error_stream,
		  "%*.*sUse '%s%s' to display the options and their values.\n",
		  SHORT_COLUMN, SHORT_COLUMN, "",
		  LONG_PREFIX, DISPLAY_ARG);
  }
}

/******************************* preprocessing *******************************/

/*
 * Preprocess argument array ARGS of ARG_N entries and set the MAND
 * and MAYBE boolean arrays.  Returns [NO]ERROR.
 */
static	int	preprocess_array(argv_t *args, const int arg_n)
{
  argv_t	*arg_p;
  char		mand_array = ARGV_FALSE, maybe_field = ARGV_FALSE;
  
  /* count the args and find the first mandatory */
  for (arg_p = args; arg_p < args + arg_n; arg_p++) {
    
    /* clear internal flags */
    arg_p->ar_type &= ~ARGV_FLAG_USED;
    
    /* do we have a mandatory-array? */
    if (arg_p->ar_short_arg == ARGV_MAND) {
      if (mand_array) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, no ARGV_MAND's can follow a MAND or MAYBE array\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      if (maybe_field) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, no ARGV_MAND's can follow a ARGV_MAYBE\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      
#if 0
      if (arg_p->ar_long_arg != NULL) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, ARGV_MAND's should not have long-options\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
#endif
      
      if (arg_p->ar_type & ARGV_FLAG_ARRAY) {
	mand_array = ARGV_TRUE;
      }
    }
    
    /* do we have a maybe field? */
    if (arg_p->ar_short_arg == ARGV_MAYBE) {
      if (mand_array) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, no ARGV_MAYBE's can follow a MAND or MAYBE array\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      
      maybe_field = ARGV_TRUE;
      if (arg_p->ar_type & ARGV_FLAG_ARRAY) {
	mand_array = ARGV_TRUE;
      }
    }
    
    /* handle initializing the argument array */
    if (arg_p->ar_type & ARGV_FLAG_ARRAY) {
      argv_array_t	*arrp = (argv_array_t *)arg_p->ar_variable;
      
      if (! HAS_ARG(arg_p->ar_type)) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, cannot have an array of boolean values\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      if (ARGV_TYPE(arg_p->ar_type) == ARGV_INCR) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, cannot have an array of incremental values\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      arrp->aa_entry_n = 0;
    }
    
#if 0
    /* must have a valid ar_short_arg */
    if (arg_p->ar_short_arg == '\0') {
      if (argv_error_stream != NULL) {
	(void)fprintf(argv_error_stream,
		      "%s: %s, short-option character is '\\0'\n",
		      argv_program, INTERNAL_ERROR_NAME);
      }
      if (argv_interactive) {
	(void)exit(EXIT_CODE);
      }
      return ERROR;
    }
#endif
    
    /* verify variable pointer */
    if (arg_p->ar_variable == NULL
	&& arg_p->ar_short_arg != ARGV_OR
	&& arg_p->ar_short_arg != ARGV_XOR) {
      if (argv_error_stream != NULL) {
	(void)fprintf(argv_error_stream,
		      "%s: %s, NULL variable specified in arg array\n",
		      argv_program, INTERNAL_ERROR_NAME);
      }
      if (argv_interactive) {
	(void)exit(EXIT_CODE);
      }
      return ERROR;
    }
    
    /* verify [X]OR's */
    if (arg_p->ar_short_arg == ARGV_OR
	|| arg_p->ar_short_arg == ARGV_XOR) {
      
      /* that they are not at the start or end of list */
      if (arg_p == args || arg_p >= (args + arg_n - 1)) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, ARGV_[X]OR entries cannot be at start or end of array\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      
      /* that two aren't next to each other */
      if ((arg_p - 1)->ar_short_arg == ARGV_OR
	  || (arg_p - 1)->ar_short_arg == ARGV_XOR) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, two ARGV_[X]OR entries cannot be next to each other\n",
			argv_program, INTERNAL_ERROR_NAME);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
    }
  }
  
  return NOERROR;
}

/*
 * Translate string argument ARG into VAR depending on its TYPE.
 * Returns [NO]ERROR.
 */
static	int	translate_value(const char *arg, ARGV_PNT var,
				const unsigned int type)
{
  argv_array_t	*arr_p;
  argv_type_t	*type_p;
  unsigned int	val_type = ARGV_TYPE(type), size = 0;
  
  /* find the type and the size for array */
  for (type_p = argv_types; type_p->at_value != 0; type_p++) {
    if (type_p->at_value == val_type) {
      size = type_p->at_size;
      break;
    }
  }
  
  if (type_p->at_value == 0) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream, "%s: illegal variable type %d\n",
		    __FILE__, val_type);
    }
    return ERROR;
  }
  
  if (type & ARGV_FLAG_ARRAY) {
    arr_p = (argv_array_t *)var;
    
    if (arr_p->aa_entry_n == 0) {
      arr_p->aa_entries = (char *)malloc(ARRAY_INCR *size);
    }
    else if (arr_p->aa_entry_n % ARRAY_INCR == 0) {
      arr_p->aa_entries =
	(char *)realloc(arr_p->aa_entries, (arr_p->aa_entry_n + ARRAY_INCR) *
			size);
    }
    
    if (arr_p->aa_entries == NULL) {
      if (argv_error_stream != NULL) {
	(void)fprintf(argv_error_stream,
		      "%s: memory error during argument processing\n",
		      argv_program);
      }
      if (argv_interactive) {
	(void)exit(EXIT_CODE);
      }
      return ERROR;
    }
    
    var = (char *)(arr_p->aa_entries) + arr_p->aa_entry_n * size;
    arr_p->aa_entry_n++;
  }
  
  /* translate depending on type */
  switch (val_type) {
    
  case ARGV_BOOL:
    /* if no close argument, set to true */
    if (arg == NULL) {
      *(char *)var = ARGV_TRUE;
    }
    else if (*(char *)arg == 't' || *(char *)arg == 'T'
	     || *(char *)arg == 'y' || *(char *)arg == 'Y'
	     || *(char *)arg == '1') {
      *(char *)var = ARGV_TRUE;
    }
    else {
      *(char *)var = ARGV_FALSE;
    }
    break;
    
  case ARGV_BOOL_NEG:
    /* if no close argument, set to false */
    if (arg == NULL) {
      *(char *)var = ARGV_FALSE;
    }
    else if (*(char *)arg == 't' || *(char *)arg == 'T'
	     || *(char *)arg == 'y' || *(char *)arg == 'Y'
	     || *(char *)arg == '1') {
      *(char *)var = ARGV_TRUE;
    }
    else {
      *(char *)var = ARGV_FALSE;
    }
    break;
    
  case ARGV_BOOL_ARG:
    if (*(char *)arg == 't' || *(char *)arg == 'T'
	|| *(char *)arg == 'y' || *(char *)arg == 'Y'
	|| *(char *)arg == '1') {
      *(char *)var = ARGV_TRUE;
    }
    else {
      *(char *)var = ARGV_FALSE;
    }
    break;
    
  case ARGV_CHAR:
    *(char *)var = *(char *)arg;
    break;
    
  case ARGV_CHAR_P:
    *(char **)var = string_copy((char *)arg);
    if (*(char **)var == NULL) {
      return ERROR;
    }
    break;
    
  case ARGV_SHORT:
    *(short *)var = (short)atoi(arg);
    break;
    
  case ARGV_U_SHORT:
    *(unsigned short *)var = (unsigned short)atoi(arg);
    break;
    
  case ARGV_INT:
    *(int *)var = atoi(arg);
    break;
    
  case ARGV_U_INT:
    *(unsigned int *)var = atoi(arg);
    break;
    
  case ARGV_LONG:
    *(long *)var = atol(arg);
    break;
    
  case ARGV_U_LONG:
    *(unsigned long *)var = atol(arg);
    break;
    
  case ARGV_FLOAT:
    (void)sscanf(arg, "%f", (float *)var);
    break;
    
  case ARGV_DOUBLE:
    (void)sscanf(arg, "%lf", (double *)var);
    break;
    
  case ARGV_BIN:
    *(int *)var = btoi(arg);
    break;
    
  case ARGV_OCT:
    *(int *)var = otoi(arg);
    break;
    
  case ARGV_HEX:
    *(int *)var = htoi(arg);
    break;
    
  case ARGV_INCR:
    /* if no close argument then increment else set the value */
    if (arg == NULL) {
      (*(int *)var)++;
    }
    else {
      *(int *)var = atoi(arg);
    }
    break;
    
  case ARGV_SIZE:
    {
      const char	*arg_p;
      long		val;
      
      /* take initial integer point */
      val = atol(arg);
      for (arg_p = arg;
	   *arg_p == ' ' || *arg_p == '-' || *arg_p == '+'
	     || (*arg_p >= '0' && *arg_p <= '9');
	   arg_p++);
      if (*arg_p == 'b' || *arg_p == 'B') {
	val *= 1;
      }
      else if (*arg_p == 'k' || *arg_p == 'B') {
	val *= 1024;
      }
      else if (*arg_p == 'm' || *arg_p == 'M') {
	val *= 1024 * 1024;
      }
      else if (*arg_p == 'g' || *arg_p == 'G') {
	val *= 1024 * 1024 * 1024;
      }
      *(long *)var = val;
    }
    break;
    
  case ARGV_U_SIZE:
    {
      const char	*arg_p;
      unsigned long	val;
      
      /* take initial integer point */
      val = (unsigned long)atol(arg);
      for (arg_p = arg;
	   *arg_p == ' ' || *arg_p == '-' || *arg_p == '+'
	     || (*arg_p >= '0' && *arg_p <= '9');
	   arg_p++);
      if (*arg_p == 'b' || *arg_p == 'B') {
	val *= 1;
      }
      else if (*arg_p == 'k' || *arg_p == 'B') {
	val *= 1024;
      }
      else if (*arg_p == 'm' || *arg_p == 'M') {
	val *= 1024 * 1024;
      }
      else if (*arg_p == 'g' || *arg_p == 'G') {
	val *= 1024 * 1024 * 1024;
      }
      *(unsigned long *)var = val;
    }
    break;
  }
  
  return NOERROR;
}

/*
 * Translate value from VAR into string STR depending on its TYPE.
 */
static	void	display_value(const ARGV_PNT var, const unsigned int type)
{
  int	val_type = ARGV_TYPE(type);
  
  /* translate depending on type */
  switch (val_type) {
    
  case ARGV_BOOL:
  case ARGV_BOOL_NEG:
  case ARGV_BOOL_ARG:
    if (*(char *)var) {
      (void)fprintf(argv_error_stream, "ARGV_TRUE");
    }
    else {
      (void)fprintf(argv_error_stream, "ARGV_FALSE");
    }
    break;
    
  case ARGV_CHAR:
    (void)fprintf(argv_error_stream, "'%s'", expand_buf((char *)var, 1));
    break;
    
  case ARGV_CHAR_P:
    {
      int	len;
      if (*(char **)var == NULL) {
	(void)fprintf(argv_error_stream, "(null)");
      }
      else {
	len = strlen(*(char **)var);
	(void)fprintf(argv_error_stream, "\"%s\"",
		      expand_buf(*(char **)var, len));
      }
    }
    break;
    
  case ARGV_SHORT:
    (void)fprintf(argv_error_stream, "%d", *(short *)var);
    break;
    
  case ARGV_U_SHORT:
    (void)fprintf(argv_error_stream, "%d", *(unsigned short *)var);
    break;
    
  case ARGV_INT:
  case ARGV_INCR:
    (void)fprintf(argv_error_stream, "%d", *(int *)var);
    break;
    
  case ARGV_U_INT:
    (void)fprintf(argv_error_stream, "%u", *(unsigned int *)var);
    break;
    
  case ARGV_LONG:
    (void)fprintf(argv_error_stream, "%ld", *(long *)var);
    break;
    
  case ARGV_U_LONG:
    (void)fprintf(argv_error_stream, "%lu", *(unsigned long *)var);
    break;
    
  case ARGV_FLOAT:
    (void)fprintf(argv_error_stream, "%f", *(float *)var);
    break;
    
  case ARGV_DOUBLE:
    (void)fprintf(argv_error_stream, "%f", *(double *)var);
    break;
    
    /* this should be a routine */
  case ARGV_BIN:
    {
      int	bit_c;
      char	first = ARGV_FALSE;
      
      (void)fputc('0', argv_error_stream);
      
      if (*(int *)var != 0) {
	(void)fputc('b', argv_error_stream);
	
	for (bit_c = sizeof(int) * BITS_IN_BYTE - 1; bit_c >= 0; bit_c--) {
	  int	bit = *(int *)var & (1 << bit_c);
	  
	  if (bit == 0) {
	    if (first) {
	      (void)fputc('0', argv_error_stream);
	    }
	  }
	  else {
	    (void)fputc('1', argv_error_stream);
	    first = ARGV_TRUE;
	  }
	}
      }
    }
    break;
    
  case ARGV_OCT:
    (void)fprintf(argv_error_stream, "%#o", *(int *)var);
    break;
    
  case ARGV_HEX:
    (void)fprintf(argv_error_stream, "%#x", *(int *)var);
    break;
    
  case ARGV_SIZE:
    {
      long	morf, val = *(long *)var;
      
      if (val == 0) {
	(void)fprintf(argv_error_stream, "0b");
      }
      else if (val % (1024 * 1024 * 1024) == 0) {
	morf = val / (1024 * 1024 * 1024);
	(void)fprintf(argv_error_stream, "%ldg (%ld)", morf, val);
      }
      else if (val % (1024 * 1024) == 0) {
	morf = val / (1024 * 1024);
	(void)fprintf(argv_error_stream, "%ldm (%ld)", morf, val);
      }
      else if (val % 1024 == 0) {
	morf = val / 1024;
	(void)fprintf(argv_error_stream, "%ldk (%ld)", morf, val);
      }
      else {
	(void)fprintf(argv_error_stream, "%ldb", val);
      }
    }
    break;
    
  case ARGV_U_SIZE:
    {
      unsigned long	morf, val = *(unsigned long *)var;
      
      if (val == 0) {
	(void)fprintf(argv_error_stream, "0b");
      }
      else if (val % (1024 * 1024 * 1024) == 0) {
	morf = val / (1024 * 1024 * 1024);
	(void)fprintf(argv_error_stream, "%ldg (%ld)", morf, val);
      }
      else if (val % (1024 * 1024) == 0) {
	morf = val / (1024 * 1024);
	(void)fprintf(argv_error_stream, "%ldm (%ld)", morf, val);
      }
      else if (val % 1024 == 0) {
	morf = val / 1024;
	(void)fprintf(argv_error_stream, "%ldk (%ld)", morf, val);
      }
      else {
	(void)fprintf(argv_error_stream, "%ldb", val);
      }
    }
    break;
  }
}

/*
 * Translate value from VAR into string STR depending on its TYPE.
 */
static	void	display_variables(const argv_t *args)
{
  const argv_t	*arg_p;
  argv_type_t	*type_p;
  int		len;
  
  /* run through the argument structure */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    int		col_c, val_type = ARGV_TYPE(arg_p->ar_type);
    
    /* skip or specifiers */
    if (arg_p->ar_short_arg == ARGV_OR || arg_p->ar_short_arg == ARGV_XOR) {
      continue;
    }
    
    col_c = 0;
    if (arg_p->ar_short_arg == '\0') {
      if (arg_p->ar_long_arg != NULL) {
	len = COMMENT_COLUMN - col_c - (LONG_PREFIX_LENGTH + 1);
	if (arg_p->ar_short_arg != '\0') {
	  (void)fprintf(argv_error_stream, "%s", LONG_LABEL);
	  col_c += LONG_LABEL_LENGTH;
	  len -= LONG_LABEL_LENGTH;
	}
	(void)fprintf(argv_error_stream, "%s%-.*s",
		      LONG_PREFIX, len, arg_p->ar_long_arg);
	col_c += LONG_PREFIX_LENGTH + MIN(len,
					  (int)strlen(arg_p->ar_long_arg));
      }
    }
    else if (arg_p->ar_short_arg == ARGV_MAND) {
      display_arg(argv_error_stream, arg_p, COMMENT_COLUMN, &col_c);
    }
    else /* ARGV_MAYBE handled here */
      display_option(argv_error_stream, arg_p, &col_c);
    
    /* put the type in the correct column */
    if (col_c < LONG_COLUMN) {
      (void)fprintf(argv_error_stream, "%*.*s",
		    LONG_COLUMN - col_c, LONG_COLUMN - col_c, "");
      col_c = LONG_COLUMN;
    }
    
    /* find the type */
    type_p = NULL;
    for (type_p = argv_types; type_p->at_value != 0; type_p++) {
      if (type_p->at_value == ARGV_TYPE(arg_p->ar_type)) {
	int	tlen;
	
	len = COMMENT_COLUMN - col_c - 1;
	tlen = strlen(type_p->at_name);
	(void)fprintf(argv_error_stream, " %-.*s", len, type_p->at_name);
	col_c += MIN(len, tlen);
	if (arg_p->ar_type & ARGV_FLAG_ARRAY) {
	  (void)fprintf(argv_error_stream, "%s", ARRAY_LABEL);
	  col_c += sizeof(ARRAY_LABEL) - 1;
	}
	break;
      }
    }
    
    if (col_c < COMMENT_COLUMN) {
      (void)fprintf(argv_error_stream, "%*.*s",
		    COMMENT_COLUMN - col_c, COMMENT_COLUMN - col_c, "");
      col_c = COMMENT_COLUMN;
    }
    
    if (arg_p->ar_type & ARGV_FLAG_ARRAY) {
      argv_array_t	*arr_p;
      int		entry_c, size = 0;
      
      /* find the type and the size for array */
      if (type_p == NULL) {
	(void)fprintf(argv_error_stream, "%s: illegal variable type %d\n",
		      __FILE__, val_type);
	continue;
      }
      size = type_p->at_size;
      arr_p = (argv_array_t *)arg_p->ar_variable;
      
      if (arr_p->aa_entry_n == 0) {
	(void)fprintf(argv_error_stream, "no entries");
      }
      else {
	for (entry_c = 0; entry_c < arr_p->aa_entry_n; entry_c++) {
	  ARGV_PNT	var;
	  if (entry_c > 0) {
	    (void)fputc(',', argv_error_stream);
	  }
	  var = (char *)(arr_p->aa_entries) + entry_c * size;
	  display_value(var, val_type);
	}
      }
    }
    else {
      display_value(arg_p->ar_variable, val_type);
    }
    (void)fputc('\n', argv_error_stream);
  }
}

/************************** checking used arguments **************************/

/*
 * Check out if WHICH argument from ARGS has an *or* specified
 * attached to it.  Returns [NO]ERROR
 */
static	int	check_or(const argv_t *args, const argv_t *which)
{
  const argv_t	*arg_p, *match_p = NULL;
  
  /* check ORs below */
  for (arg_p = which - 2; arg_p >= args; arg_p -= 2) {
    if ((arg_p + 1)->ar_short_arg != ARGV_OR
	&& (arg_p + 1)->ar_short_arg != ARGV_XOR) {
      break;
    }
    if (arg_p->ar_type & ARGV_FLAG_USED) {
      match_p = arg_p;
      break;
    }
  }
  
  /* check ORs above */
  if (match_p == NULL) {
    /* NOTE: we assume that which is not pointing now to ARGV_LAST */
    for (arg_p = which + 2;
	 arg_p->ar_short_arg != ARGV_LAST
	 && (arg_p - 1)->ar_short_arg != ARGV_LAST;
	 arg_p += 2) {
      if ((arg_p - 1)->ar_short_arg != ARGV_OR
	  && (arg_p - 1)->ar_short_arg != ARGV_XOR) {
	break;
      }
      if (arg_p->ar_type & ARGV_FLAG_USED) {
	match_p = arg_p;
	break;
      }
    }
  }
  
  /* did we not find a problem? */
  if (match_p == NULL) {
    return NOERROR;
  }
  
  if (argv_error_stream == NULL) {
    return ERROR;
  }
  
  (void)fprintf(argv_error_stream,
		"%s: %s, specify only one of the following:\n",
		argv_program, USAGE_ERROR_NAME);
  
  /* little hack to print the one that matched and the one we were checking */
  for (;;) {
    if (match_p->ar_long_arg == NULL) {
      (void)fprintf(argv_error_stream, "%*.*s%s%c\n",
		    (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "",
		    SHORT_PREFIX, match_p->ar_short_arg);
    }
    else {
      (void)fprintf(argv_error_stream, "%*.*s%s%c (%s%s)\n",
		    (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "",
		    SHORT_PREFIX, match_p->ar_short_arg,
		    LONG_PREFIX, match_p->ar_long_arg);
    }
    
    if (match_p == which) {
      break;
    }
    match_p = which;
  }
  
  return ERROR;
}

/*
 * Find all the XOR arguments and make sure each group has at least
 * one specified in it.  Returns [NO]ERROR.
 */
static	int	check_xor(const argv_t *args)
{
  const argv_t	*start_p = NULL, *arg_p;
  
  /* run through the list of arguments */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    
    /* only check the XORs */
    if (arg_p->ar_short_arg != ARGV_XOR) {
      continue;
    }
    
    start_p = arg_p;
    
    /*
     * NOTE: we are guaranteed that we are on a XOR so there is
     * something below and above...
     */
    if ((arg_p - 1)->ar_type & ARGV_FLAG_USED) {
      start_p = NULL;
    }
    
    /* run through all XORs */
    for (;;) {
      arg_p++;
      if (arg_p->ar_type & ARGV_FLAG_USED) {
	start_p = NULL;
      }
      if ((arg_p + 1)->ar_short_arg != ARGV_XOR) {
	break;
      }
      arg_p++;
    }
    
    /* were none of the xor's filled? */
    if (start_p != NULL) {
      break;
    }
  }
  
  /* did we not find a problem? */
  if (start_p == NULL) {
    return NOERROR;
  }
  
  /* arg_p points to the first XOR which failed */
  if (argv_error_stream == NULL) {
    return ERROR;
  }
  
  (void)fprintf(argv_error_stream, "%s: %s, must specify one of:\n",
		argv_program, USAGE_ERROR_NAME);
  
  for (arg_p = start_p;; arg_p += 2) {
    /*
     * NOTE: we are guaranteed that we are on a XOR so there is
     * something below and above...
     */
    (void)fprintf(argv_error_stream, "%*.*s%s%c",
		  (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "",
		  SHORT_PREFIX, (arg_p - 1)->ar_short_arg);
    if ((arg_p - 1)->ar_long_arg != NULL) {
      (void)fprintf(argv_error_stream, " (%s%s)",
		    LONG_PREFIX, (arg_p - 1)->ar_long_arg);
    }
    (void)fprintf(argv_error_stream, "\n");
    
    if (arg_p->ar_short_arg != ARGV_XOR) {
      break;
    }
  }
  
  return ERROR;
}

/*
 * Check to see if any mandatory arguments left.  Returns [NO]ERROR.
 */
static	int	check_mand(const argv_t *args)
{
  const argv_t	*arg_p;
  int		mand_c = 0, flag_c = 0;
  
  /* see if there are any mandatory args left */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    if (arg_p->ar_short_arg == ARGV_MAND
	&& (! (arg_p->ar_type & ARGV_FLAG_USED))) {
      mand_c++;
    }
    if (arg_p->ar_type & ARGV_FLAG_MAND
	&& (! (arg_p->ar_type & ARGV_FLAG_USED))) {
      flag_c++;
      if (argv_error_stream != NULL) {
	if (flag_c == 1) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, these mandatory flags must be specified:\n",
			argv_program, USAGE_ERROR_NAME);
	}
	(void)fprintf(argv_error_stream, "%*.*s%s%c",
		      (int)USAGE_LABEL_LENGTH, (int)USAGE_LABEL_LENGTH, "",
		      SHORT_PREFIX, arg_p->ar_short_arg);
	if (arg_p->ar_long_arg != NULL) {
	  (void)fprintf(argv_error_stream, " (%s%s)",
			LONG_PREFIX, arg_p->ar_long_arg);
	}
	(void)fprintf(argv_error_stream, "\n");
      }
    }
  }
  
  if (mand_c > 0 && argv_error_stream != NULL) {
    (void)fprintf(argv_error_stream,
		  "%s: %s, %d more mandatory argument%s must be specified\n",
		  argv_program, USAGE_ERROR_NAME,
		  mand_c, (mand_c == 1 ? "" : "s"));
  }
  
  if (mand_c > 0 || flag_c > 0) {
    return ERROR;
  }
  else {
    return NOERROR;
  }
}

/*
 * Check for any missing argument options.  Returns [NO]ERROR
 */
static	int	check_opt(void)
{
  int	queue_c;
  
  queue_c = queue_head - queue_tail;
  if (queue_c > 0) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: %s, %d more option-argument%s must be specified\n",
		    argv_program, USAGE_ERROR_NAME,
		    queue_c, (queue_c == 1 ? "" : "s"));
    }
    return ERROR;
  }
  
  return NOERROR;
}

/**************************** argument processing ****************************/

/*
 * Read in arguments from PATH and run them from the GRID.  OKAY_P is
 * a pointer to the boolean error marker.  Returns [NO]ERROR.
 */
static	void	file_args(const char *path, argv_t *grid, char *okay_p)
{
  char	**argv, **argv_p;
  int	argc, max;
  FILE	*infile;
  char	line[FILE_LINE_SIZE + 1], *line_p;
  
  /* open the input file */
  infile = fopen(path, "r");
  if (infile == NULL) {
    *okay_p = ARGV_FALSE;
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: could not load command-line arguments from: %s\n",
		    argv_program, path);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return;
  }
  
  /* get an array of char * */
  argc = 0;
  max = ARRAY_INCR;
  argv = malloc(sizeof(char *) * max);
  if (argv == NULL) {
    *okay_p = ARGV_FALSE;
    (void)fclose(infile);
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return;
  }
  argv_p = argv;
  
  /* read in the file lines */
  while (fgets(line, FILE_LINE_SIZE, infile) != NULL) {
    /* punch the \n at end of line */
    for (line_p = line; *line_p != '\n' && *line_p != '\0'; line_p++);
    *line_p = '\0';
    
    *argv_p = string_copy(line);
    if (*argv_p == NULL) {
      *okay_p = ARGV_FALSE;
      return;
    }
    
    argv_p++;
    argc++;
    if (argc == max) {
      max += ARRAY_INCR;
      argv = realloc(argv, sizeof(char *) * max);
      if (argv == NULL) {
	*okay_p = ARGV_FALSE;
	(void)fclose(infile);
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: memory error during argument processing\n",
			argv_program);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return;
      }
      argv_p = argv + argc;
    }
  }
  
  /* now do the list */
  do_list(grid, argc, argv, okay_p);
  
  /* now free up the list */
  for (argv_p = argv; argv_p < argv + argc; argv_p++) {
    free(*argv_p);
  }
  free(argv);
  
  (void)fclose(infile);
}

/*
 * Process an argument in MATCH_P which looking at GRID. sets okay_p to
 * FALSE if the argument was not okay.
 */
static	void	do_arg(argv_t *grid, argv_t *match_p, const char *close_p,
		       char *okay_p)
{
  if (global_multi == GLOBAL_MULTI_REJECT) {
    /*
     * have we used this one before?
     * NOTE: should this be a warning or a non-error altogether?
     */
    if (match_p->ar_type & ARGV_FLAG_USED
	&& (! (match_p->ar_type & ARGV_FLAG_ARRAY))
	&& ARGV_TYPE(match_p->ar_type) != ARGV_INCR) {
      if (argv_error_stream != NULL) {
	(void)fprintf(argv_error_stream,
		      "%s: %s, you've already specified the '%c' argument\n",
		      argv_program, USAGE_ERROR_NAME,
		      match_p->ar_short_arg);
      }
      *okay_p = ARGV_FALSE;
    }
  }
  
  /* we used this argument */
  match_p->ar_type |= ARGV_FLAG_USED;
  
  /* check arguments that must be OR'd */
  if (check_or(grid, match_p) != NOERROR) {
    /*
     * don't return here else we might generate an XOR error
     * because the argument wasn't specified
     */
    *okay_p = ARGV_FALSE;
  }
  
  /*
   * If we have a close argument, pass to translate.  If it is a
   * boolean or increment variable, then pass in a value of null
   * else queue it for needing a value argument.
   */
  if (global_close == GLOBAL_CLOSE_ENABLE && close_p != NULL) {
    if (translate_value(close_p, match_p->ar_variable,
			match_p->ar_type) != NOERROR) {
      *okay_p = ARGV_FALSE;
    }
  }
  else if (! HAS_ARG(match_p->ar_type)) {
    if (translate_value(NULL, match_p->ar_variable,
			match_p->ar_type) != NOERROR) {
      *okay_p = ARGV_FALSE;
    }
  }
  else if (global_close == GLOBAL_CLOSE_ENABLE && close_p != NULL) {
    if (translate_value(close_p, match_p->ar_variable,
			match_p->ar_type) != NOERROR) {
      *okay_p = ARGV_FALSE;
    }
  }
  else {
    queue_list[queue_head] = match_p;
    queue_head++;
  }
}

/*
 * Examine an argument string STR to see if it really is a negative
 * number being passed into a previously specified argument.  Returns
 * 1 if a number otherwise 0.  Thanks much to Nick Kisseberth
 * <nkissebe@hera.itg.uiuc.edu> for pointing out this oversight.
 */
static	int	is_number(const char *str)
{
  const char	*str_p;
  
  /* empty strings are not numbers */
  if (str[0] == '\0') {
    return 0;
  }
  
  /*
   * All chars in the string should be number chars for it to be a
   * number.  Yes this will return yes if the argument is "00-" but
   * we'll chalk this up to user error.
   */
  for (str_p = str; *str_p != '\0'; str_p++) {
    if (strchr(NUMBER_ARG_CHARS, *str_p) == NULL) {
      return 0;
    }
  }
  
  return 1;
}

/*
 * Process a list of arguments ARGV and ARGV as it applies to ARGS.
 * on ARG_N members.  OKAY_P is a pointer to the boolean error
 * marker.
 */
static	void	do_list(argv_t *grid, const int argc, char **argv,
			char *okay_p)
{
  argv_t	*grid_p, *match_p;
  int		len, char_c, unwant_c = 0;
  int		last_arg_b = ARGV_FALSE;
  char		*close_p = NULL, **arg_p;
  
  /* run throught rest of arguments */
  for (arg_p = argv; arg_p < argv + argc; arg_p++) {
    
    /* have we reached the LAST_ARG marker? */
    if (strcmp(LAST_ARG, *arg_p) == 0) {
      if (last_arg_b) {
	if (global_lasttog == GLOBAL_LASTTOG_ENABLE) {
	  last_arg_b = ARGV_FALSE;
	  continue;
	}
      }
      else {
	last_arg_b = ARGV_TRUE;
	continue;
      }
    }
    
    /* are we processing a long option? */
    if ((! last_arg_b)
	&& strncmp(LONG_PREFIX, *arg_p, LONG_PREFIX_LENGTH) == 0) {
      
      /*
       * check for close equals marker
       *
       * NOTE: duplicated in the short prefix section below.  In here otherwise
       * we process normal args with x=5 instead of just -x=5.
       */
      if (global_close == GLOBAL_CLOSE_ENABLE) {
	close_p = strchr(*arg_p, ARG_EQUALS);
	/* if we found the special char then punch the null and set pointer */
	if (close_p != NULL) {
	  *close_p = '\0';
	  close_p++;
	}
      }
      
      /* get length of rest of argument */
      len = strlen(*arg_p) - LONG_PREFIX_LENGTH;
      
      /* we need more than the prefix */
      if (len <= 0) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, empty long-option prefix '%s'\n",
			argv_program, USAGE_ERROR_NAME, *arg_p);
	}
	*okay_p = ARGV_FALSE;
	continue;
      }
      
      match_p = NULL;
      
      /* run though long options looking for a match */
      for (grid_p = grid; grid_p->ar_short_arg != ARGV_LAST; grid_p++) {
	if (grid_p->ar_long_arg == NULL) {
	  continue;
	}
	
	if (strncmp(*arg_p + LONG_PREFIX_LENGTH,
		    grid_p->ar_long_arg, len) == 0) {
	  if (match_p != NULL) {
	    if (argv_error_stream != NULL) {
	      (void)fprintf(argv_error_stream,
			    "%s: %s, '%s' might be '%s' or '%s'\n",
			    argv_program, USAGE_ERROR_NAME, *arg_p,
			    grid_p->ar_long_arg, match_p->ar_long_arg);
	    }
	    *okay_p = ARGV_FALSE;
	    break;
	  }
	  
	  /* record a possible match */
	  match_p = grid_p;
	  
	  /* don't break, need to see if another one matches */
	}
      }
      
      /* if we found a match but quit then we must have found two matches */
      if (match_p != NULL && grid_p->ar_short_arg != ARGV_LAST) {
	continue;
      }
      
      if (match_p != NULL) {
	(void)do_arg(grid, match_p, close_p, okay_p);
	continue;
      }
      
      /* we did not find long-option match */
      
      /* check for special file value */
      if (strncmp(FILE_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (global_close == GLOBAL_CLOSE_ENABLE && close_p != NULL) {
	  /* open the file and read in the args */
	  file_args(close_p, grid, okay_p);
	}
	else {
	  /* HACK: we enqueue null for the file argument */
	  queue_list[queue_head] = NULL;
	  queue_head++;
	}
	continue;
      }
      
      /* check for special usage value */
      if (strncmp(USAGE_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0
	  || strncmp(HELP_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  do_usage(grid, global_usage);
	  (void)exit(0);
	}
	continue;
      }
      
      /* check for special short-usage value */
      if (strncmp(USAGE_SHORT_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  do_usage(grid, GLOBAL_USAGE_SHORT);
	  (void)exit(0);
	}
	continue;
      }
      
      /* check for special long-usage value */
      if (strncmp(USAGE_LONG_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  do_usage(grid, GLOBAL_USAGE_LONG);
	  (void)exit(0);
	}
	continue;
      }
      
      /* check for special long-usage value */
      if (strncmp(USAGE_ALL_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  do_usage(grid, GLOBAL_USAGE_ALL);
	  (void)exit(0);
	}
	continue;
      }
      
      /* check for special help value */
      if (strncmp(HELP_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  if (argv_error_stream != NULL) {
	    if (argv_help_string == NULL) {
	      (void)fprintf(argv_error_stream,
			    "%s: I'm sorry, no help is available.\n",
			    argv_program);
	    }
	    else {
	      (void)fprintf(argv_error_stream, "%s: %s\n",
			    argv_program, argv_help_string);
	    }
	  }
	  (void)exit(0);
	}
	continue;
      }
      
      /* check for special version value */
      if (strncmp(VERSION_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  if (argv_error_stream != NULL) {
	    if (argv_version_string == NULL) {
	      (void)fprintf(argv_error_stream,
			    "%s: no version information is available.\n",
			    argv_program);
	    }
	    else {
	      (void)fprintf(argv_error_stream, "%s: %s%s\n",
			    argv_program, VERSION_LABEL,
			    argv_version_string);
	    }
	  }
	  (void)exit(0);
	}
	continue;
      }
      
      /* check for display arguments value */
      if (strncmp(DISPLAY_ARG, *arg_p + LONG_PREFIX_LENGTH, len) == 0) {
	if (argv_interactive) {
	  if (argv_error_stream != NULL) {
	    display_variables(grid);
	  }
	  (void)exit(0);
	}
	continue;
      }
      
      if (argv_error_stream != NULL) {
	(void)fprintf(argv_error_stream,
		      "%s: %s, unknown long option '%s'.\n",
		      argv_program, USAGE_ERROR_NAME, *arg_p);
      }
      *okay_p = ARGV_FALSE;
      continue;
    }
    
    /* are we processing a short option? */
    if ((! last_arg_b)
	&& strncmp(SHORT_PREFIX, *arg_p, SHORT_PREFIX_LENGTH) == 0) {
      
      /*
       * check for close equals marker
       *
       * NOTE: duplicated in the long prefix section above.  In here otherwise
       * we process normal args with x=5 instead of just -x=5.
       */
      if (global_close == GLOBAL_CLOSE_ENABLE) {
	close_p = strchr(*arg_p, ARG_EQUALS);
	/* if we found the special char then punch the null and set pointer */
	if (close_p != NULL) {
	  *close_p = '\0';
	  close_p++;
	}
      }
      
      /* get length of rest of argument */
      len = strlen(*arg_p) - SHORT_PREFIX_LENGTH;
      
      /* we need more than the prefix */
      if (len <= 0) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: %s, empty short-option prefix '%s'\n",
			argv_program, USAGE_ERROR_NAME, *arg_p);
	}
	*okay_p = ARGV_FALSE;
	continue;
      }
      
      /* run through the chars in this option */
      for (char_c = 0; char_c < len; char_c++) {
	
	/* run through the arg list looking for a match */
	for (match_p = grid; match_p->ar_short_arg != ARGV_LAST; match_p++) {
	  if (match_p->ar_short_arg ==
	      (*arg_p)[SHORT_PREFIX_LENGTH + char_c]) {
	    break;
	  }
	}
	
	/* did we not find argument? */
	if (match_p->ar_short_arg == ARGV_LAST) {
	  
	  /* check for special usage value */
	  if ((*arg_p)[SHORT_PREFIX_LENGTH + char_c] == USAGE_CHAR_ARG) {
	    if (argv_interactive) {
	      do_usage(grid, global_usage);
	      (void)exit(0);
	    }
	    continue;
	  }
	  
	  /*
	   * allow values with negative signs if we are at the start
	   * of an argument list, and if the argument is a number, and
	   * we already have a variable looking for a value.  Thanks
	   * to Nick Kisseberth <nkissebe@hera.itg.uiuc.edu> for
	   * pointing out this oversight.
	   */
	  if (char_c == 0 && is_number(*arg_p) && queue_head > queue_tail) {
	    
	    match_p = queue_list[queue_tail];
	    /*
	     * NOTE: we don't advance the queue tail here unless we
	     * find out that we can use it below
	     */
	    
	    switch (ARGV_TYPE(match_p->ar_type)) {
	      
	    case ARGV_SHORT:
	    case ARGV_INT:
	    case ARGV_LONG:
	    case ARGV_FLOAT:
	    case ARGV_DOUBLE:
              translate_value(*arg_p, match_p->ar_variable, match_p->ar_type);
	      char_c = len;
	      /* we actually used it so we advance the queue tail position */
	      queue_tail++;
	      continue;
	      break;
	    }
	  }
	  
	  /* create an error string */
	  if (argv_error_stream != NULL) {
	    (void)fprintf(argv_error_stream,
			  "%s: %s, unknown short option '%s%c'.\n",
			  argv_program, USAGE_ERROR_NAME, SHORT_PREFIX,
			  (*arg_p)[SHORT_PREFIX_LENGTH + char_c]);
	  }
	  *okay_p = ARGV_FALSE;
	  continue;
	}
	
	do_arg(grid, match_p, close_p, okay_p);
      }
      
      continue;
    }
    
    /* could this be a value? */
    if (grid->ar_short_arg != ARGV_LAST && queue_head > queue_tail) {
      
      /* pull the variable waiting for a value from the queue */
      match_p = queue_list[queue_tail];
      queue_tail++;
      
      /* HACK: is this the file argument */
      if (match_p == NULL) {
	file_args(*arg_p, grid, okay_p);
      }
      else {
	if (translate_value(*arg_p, match_p->ar_variable,
			    match_p->ar_type) != NOERROR) {
	  *okay_p = ARGV_FALSE;
	}
      }
      continue;
    }
    
    /* process mandatory args if some left to process */
    for (grid_p = grid; grid_p->ar_short_arg != ARGV_LAST; grid_p++) {
      if (grid_p->ar_short_arg == ARGV_MAND
	  && ((! (grid_p->ar_type & ARGV_FLAG_USED))
	      || grid_p->ar_type & ARGV_FLAG_ARRAY)) {
	break;
      }
    }
    if  (grid_p->ar_short_arg != ARGV_LAST) {
      /* absorb another mand. arg */
      if (translate_value(*arg_p, grid_p->ar_variable,
			  grid_p->ar_type) != NOERROR) {
	*okay_p = ARGV_FALSE;
      }
      grid_p->ar_type |= ARGV_FLAG_USED;
      continue;
    }
    
    /* process maybe args if some left to process */
    for (grid_p = grid; grid_p->ar_short_arg != ARGV_LAST; grid_p++) {
      if (grid_p->ar_short_arg == ARGV_MAYBE
	  && ((! (grid_p->ar_type & ARGV_FLAG_USED))
	      || grid_p->ar_type & ARGV_FLAG_ARRAY)) {
	break;
      }
    }
    if  (grid_p->ar_short_arg != ARGV_LAST) {
      /* absorb another maybe arg */
      if (translate_value(*arg_p, grid_p->ar_variable,
			  grid_p->ar_type) != NOERROR) {
	*okay_p = ARGV_FALSE;
      }
      grid_p->ar_type |= ARGV_FLAG_USED;
      continue;
    }
    
    /* default is an error */
    unwant_c++;
    *okay_p = ARGV_FALSE;
  }
  
  if (unwant_c > 0 && argv_error_stream != NULL) {
    (void)fprintf(argv_error_stream,
		  "%s: %s, %d unwanted additional argument%s\n",
		  argv_program, USAGE_ERROR_NAME,
		  unwant_c, (unwant_c == 1 ? "" : "s"));
  }
}

/****************************** env processing *******************************/

/*
 * Handle the args from the ENV variable.  Returns [NO]ERROR.
 */
static	int	do_env_args(argv_t *args, char *okay_p)
{
  int	env_c, env_n;
  char	**vect_p, env_name[256], *environ_p;
  
  /* create the env variable */
  (void)sprintf(env_name, ENVIRON_FORMAT, argv_program);
  
  /* NOTE: by default the env name is all uppercase */
  for (environ_p = env_name; *environ_p != '\0'; environ_p++) {
    if (islower(*environ_p)) {
      *environ_p = toupper(*environ_p);
    }
  }
  
  environ_p = getenv(env_name);
  if (environ_p == NULL) {
    return NOERROR;
  }
  
  /* break the list into tokens and do the list */
  environ_p = string_copy(environ_p);
  if (environ_p == NULL) {
    return ERROR;
  }
  
  vect_p = vectorize(environ_p, " \t", &env_n);
  if (vect_p != NULL) {
    do_list(args, env_n, vect_p, okay_p);
    
    /* free token list */
    for (env_c = 0; env_c < env_n; env_c++) {
      free(vect_p[env_c]);
    }
    free(vect_p);
  }
  free(environ_p);
  
  return NOERROR;
}

/*
 * Process the global env variable.  Returns [NO]ERROR.
 */
static	int	process_env(void)
{
  static char	done = ARGV_FALSE;
  char		*environ_p, *env_p, *arg;
  int		len;
  
  /* make sure we only do this once */
  if (done) {
    return NOERROR;
  }
  
  done = ARGV_TRUE;
  
  /* get the argv information */
  environ_p = getenv(GLOBAL_NAME);
  if (environ_p == NULL) {
    return NOERROR;
  }
  
  /* save a copy of it */
  environ_p = string_copy(environ_p);
  if (environ_p == NULL) {
    return ERROR;
  }
  
  arg = environ_p;
  
  for (;;) {
    env_p = strtok(arg, " \t,:");
    if (env_p == NULL) {
      break;
    }
    arg = NULL;
    
    len = strlen(GLOBAL_CLOSE);
    if (strncmp(GLOBAL_CLOSE, env_p, len) == 0) {
      env_p += len;
      if (strcmp(env_p, "disable") == 0
	  || strcmp(env_p, "off") == 0
	  || strcmp(env_p, "no") == 0
	  || strcmp(env_p, "0") == 0) {
	global_close = GLOBAL_CLOSE_DISABLE;
      }
      else if (strcmp(env_p, "enable") == 0
	       || strcmp(env_p, "on") == 0
	       || strcmp(env_p, "yes") == 0
	       || strcmp(env_p, "1") == 0) {
	global_close = GLOBAL_CLOSE_ENABLE;
      }
      else {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: illegal env variable '%s' '%s' argument '%s'\n",
			__FILE__, GLOBAL_NAME, GLOBAL_CLOSE, env_p);
	}
      }
      continue;
    }
    
    len = strlen(GLOBAL_LASTTOG);
    if (strncmp(GLOBAL_LASTTOG, env_p, len) == 0) {
      env_p += len;
      if (strcmp(env_p, "disable") == 0
	  || strcmp(env_p, "off") == 0
	  || strcmp(env_p, "no") == 0
	  || strcmp(env_p, "0") == 0) {
	global_lasttog = GLOBAL_LASTTOG_DISABLE;
      }
      else if (strcmp(env_p, "enable") == 0
	       || strcmp(env_p, "on") == 0
	       || strcmp(env_p, "yes") == 0
	       || strcmp(env_p, "1") == 0) {
	global_lasttog = GLOBAL_LASTTOG_ENABLE;
      }
      else {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: illegal env variable '%s' '%s' argument '%s'\n",
			__FILE__, GLOBAL_NAME, GLOBAL_LASTTOG, env_p);
	}
      }
      continue;
    }
    
    len = strlen(GLOBAL_ENV);
    if (strncmp(GLOBAL_ENV, env_p, len) == 0) {
      env_p += len;
      if (strcmp(env_p, "none") == 0) {
	global_env = GLOBAL_ENV_NONE;
      }
      else if (strcmp(env_p, "before") == 0) {
	global_env = GLOBAL_ENV_BEFORE;
      }
      else if (strcmp(env_p, "after") == 0) {
	global_env = GLOBAL_ENV_AFTER;
      }
      else {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: illegal env variable '%s' '%s' argument '%s'\n",
			__FILE__, GLOBAL_NAME, GLOBAL_ENV, env_p);
	}
      }
      continue;
    }
    
    len = strlen(GLOBAL_ERROR);
    if (strncmp(GLOBAL_ERROR, env_p, len) == 0) {
      env_p += len;
      if (strcmp(env_p, "none") == 0) {
	global_error = GLOBAL_ERROR_NONE;
      }
      else if (strcmp(env_p, "see") == 0) {
	global_error = GLOBAL_ERROR_SEE;
      }
      else if (strcmp(env_p, "short") == 0) {
	global_error = GLOBAL_ERROR_SHORT;
      }
      else if (strcmp(env_p, "shortrem") == 0) {
	global_error = GLOBAL_ERROR_SHORTREM;
      }
      else if (strcmp(env_p, "long") == 0) {
	global_error = GLOBAL_ERROR_LONG;
      }
      else if (strcmp(env_p, "all") == 0) {
	global_error = GLOBAL_ERROR_ALL;
      }
      else {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: illegal env variable '%s' '%s' argument '%s'\n",
			__FILE__, GLOBAL_NAME, GLOBAL_ERROR, env_p);
	}
      }
      continue;
    }
    
    len = strlen(GLOBAL_MULTI);
    if (strncmp(GLOBAL_MULTI, env_p, len) == 0) {
      env_p += len;
      if (strcmp(env_p, "reject") == 0) {
	global_multi = GLOBAL_MULTI_REJECT;
      }
      else if (strcmp(env_p, "accept") == 0) {
	global_multi = GLOBAL_MULTI_ACCEPT;
      }
      else {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: illegal env variable '%s' '%s' argument '%s'\n",
			__FILE__, GLOBAL_NAME, GLOBAL_MULTI, env_p);
	}
      }
      continue;
    }
    
    len = strlen(GLOBAL_USAGE);
    if (strncmp(GLOBAL_USAGE, env_p, len) == 0) {
      env_p += len;
      if (strcmp(env_p, "short") == 0) {
	global_usage = GLOBAL_USAGE_SHORT;
      }
      else if (strcmp(env_p, "shortrem") == 0) {
	global_usage = GLOBAL_USAGE_SHORTREM;
      }
      else if (strcmp(env_p, "long") == 0) {
	global_usage = GLOBAL_USAGE_LONG;
      }
      else if (strcmp(env_p, "all") == 0) {
	global_usage = GLOBAL_USAGE_ALL;
      }
      else {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: illegal env variable '%s' '%s' argument '%s'\n",
			__FILE__, GLOBAL_NAME, GLOBAL_USAGE, env_p);
	}
      }
      continue;
    }
    
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: illegal env variable '%s' setting '%s'\n",
		    __FILE__, GLOBAL_NAME, env_p);
    }
  }
  
  free(environ_p);
  return NOERROR;
}

/*
 * Processes ARGS from ARGC and ARGV.  Returns 0 if no error else -1.
 */
static	int	process_args(argv_t *args, const int argc, char **argv)
{
  int		arg_n;
  const char	*prog_p;
  char		okay = ARGV_TRUE;
  argv_t	*arg_p;
  
  if (process_env() != NOERROR) {
    return ERROR;
  }
  
  if (args == NULL) {
    args = empty;
  }
  
  if (argc < 0) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: %s, argc argument to argv_process is %d\n",
		    __FILE__, INTERNAL_ERROR_NAME, argc);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  if (argv == NULL) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: %s, argv argument to argv_process is NULL\n",
		    __FILE__, INTERNAL_ERROR_NAME);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  /* set global variables */
  argv_argv = argv;
  argv_argc = argc;
  
  /* build the program name from the argv[0] path */
  {
    const char	*tmp_p;
    
    prog_p = *argv;
    for (tmp_p = *argv; *tmp_p != '\0'; tmp_p++) {
      if (*tmp_p == '/') {
	prog_p = tmp_p + 1;
      }
    }
  }
  
  /* so we can step on the environmental space */
  (void)strncpy(argv_program, prog_p, PROGRAM_NAME);
  
  /* count the args */
  arg_n = 0;
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    arg_n++;
  }
  
  /* verify the argument array */
  if (preprocess_array(args, arg_n) != NOERROR) {
    return ERROR;
  }
  
  /* allocate our value queue */
  if (arg_n > 0) {
    /* allocate our argument queue */
    queue_list = (argv_t **)malloc(sizeof(argv_t *) * arg_n);
    if (queue_list == NULL) {
      return ERROR;
    }
    queue_head = 0;
    queue_tail = 0;
  }
  
  /* do the env args before? */
  if (global_env == GLOBAL_ENV_BEFORE) {
    if (do_env_args(args, &okay) != NOERROR) {
      return ERROR;
    }
  }
  
  /* do the external args */
  do_list(args, argc - 1, argv + 1, &okay);
  
  /* do the env args after? */
  if (global_env == GLOBAL_ENV_AFTER) {
    do_env_args(args, &okay);
    if (do_env_args(args, &okay) != NOERROR) {
      return ERROR;
    }
  }
  
  /* make sure the XOR and MAND args and argument-options are okay */
  if (check_mand(args) != NOERROR) {
    okay = ARGV_FALSE;
  }
  if (check_opt() != NOERROR) {
    okay = ARGV_FALSE;
  }
  if (check_xor(args) != NOERROR) {
    okay = ARGV_FALSE;
  }
  
  /* if we allocated the space then free it */
  if (arg_n > 0) {
    free(queue_list);
  }
  
  /* was there an error? */
  if (! okay) {
    if (argv_error_stream != NULL) {
      do_usage(args, global_error);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  return NOERROR;
}

/***************************** exported routines *****************************/

/*
 * Processes ARGC number of arguments from ARGV depending on argument
 * info array ARGS (if null then an empty array is used).  This
 * routine will not modify the argv array in any way.
 *
 * NOTE: it will modify the args array by setting various flags in the
 * type field.  returns 0 if no error else -1.
 */
int	argv_process(argv_t *args, const int argc, char **argv)
{
  if (! enabled) {
    argv_startup();
  }
  
  if (process_args(args, argc, argv) == NOERROR) {
    return NOERROR;
  }
  else {
    return ERROR;
  }
}

/*
 * Processes arguments sent in via the STRING that a web-server might
 * send to program in ARG0.  Use DELIM to set up the delimiters of the
 * arguments in the string.  query_string processing should have "&"
 * and path_info should have "/".  You may want to add "=" if you use
 * arg=value.  The '=' delimiter is treated as special so //x=// will
 * strip the extra /'s in a row but will create a null argument for x.
 *
 * WARNING: you cannot use argv_copy_args after this is called because a
 * temporary grid is created.  returns 0 on noerror else -1.
 */
int	argv_web_process_string(argv_t *args, const char *arg0,
				const char *string, const char *delim)
{
  const char	*str_p, *delim_p, *delim_str;
  char		*copy, *copy_p, **argv;
  int		argc, ret, alloced;
  
  if (! enabled) {
    argv_startup();
  }
  
  if (delim == NULL) {
    delim_str = "";
  }
  else {
    delim_str = delim;
  }
  
  /* copy incoming string so we can punch nulls */
  copy = malloc(strlen(string) + 1);
   if (copy == NULL) {
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  /* create argv array */
  alloced = ARG_MALLOC_INCR;
  argv = (char **)malloc(sizeof(char *) * alloced);
  if (argv == NULL) {
    free(copy);
    if (argv_error_stream != NULL) {
      (void)fprintf(argv_error_stream,
		    "%s: memory error during argument processing\n",
		    argv_program);
    }
    if (argv_interactive) {
      (void)exit(EXIT_CODE);
    }
    return ERROR;
  }
  
  argc = 0;
  argv[argc++] = (char *)arg0;
  str_p = string;
  /*  skip starting multiple arg delimiters */
  for (; *str_p != '\0'; str_p++) {
    for (delim_p = delim_str; *delim_p != '\0'; delim_p++) {
      if (*str_p == *delim_p) {
	break;
      }
    }
    if (*delim_p == '\0') {
      break;
    }
  }
  
  /* start of the string is argv[1] */
  if (*str_p != '\0') {
    if (argc >= alloced) {
      alloced += ARG_MALLOC_INCR;
      argv = (char **)realloc(argv, sizeof(char *) * alloced);
      if (argv == NULL) {
	free(copy);
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: memory error during argument processing\n",
			argv_program);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
    }
    argv[argc++] = copy;
  }
  
  for (copy_p = copy;; str_p++) {
    int		val;
    
    /* are we done? */
    if (*str_p == '\0') {
      *copy_p = '\0';
      break;
    }
    
    /* is this a argument seperator? */
    for (delim_p = delim_str; *delim_p != '\0'; delim_p++) {
      if (*str_p == *delim_p) {
	break;
      }
    }
    if (*delim_p != '\0') {
      *copy_p++ = '\0';
      
      /*
       * look ahead and skip multiple arg delimiters.  we have a
       * special case if the delimiter is '='.  This means that we
       * need to generate a null string argument.
       */
      if (*str_p != '=') {
	for (;; str_p++) {
	  for (delim_p = delim_str; *delim_p != '\0'; delim_p++) {
	    if (*(str_p + 1) == *delim_p) {
	      break;
	    }
	  }
	  if (*delim_p == '\0') {
	    break;
	  }
	}
      }
      
      /* if we are not at the end of the string, create a new arg */
      if (*str_p == '=' || *(str_p + 1) != '\0') {
	if (argc >= alloced) {
	  alloced += ARG_MALLOC_INCR;
	  argv = (char **)realloc(argv, sizeof(char *) * alloced);
	  if (argv == NULL) {
	    if (argv_error_stream != NULL) {
	      (void)fprintf(argv_error_stream,
			    "%s: memory error during argument processing\n",
			    argv_program);
	    }
	    if (argv_interactive) {
	      (void)exit(EXIT_CODE);
	    }
	    return ERROR;
	  }
	}
	argv[argc++] = copy_p;
      }
      continue;
    }
    
    /* a space */
    if (*str_p == '+') {
      *copy_p++ = ' ';
      continue;
    }
    
    /* no binary character, than it is normal */
    if (*str_p != '%') {
      *copy_p++ = *str_p;
      continue;
    }      
    
    str_p++;
    
    if (*str_p >= 'a' && *str_p <= 'f') {
      val = 10 + *str_p - 'a';
    }
    else if (*str_p >= 'A' && *str_p <= 'F') {
      val = 10 + *str_p - 'A';
    }
    else if (*str_p >= '0' && *str_p <= '9') {
      val = *str_p - '0';
    }
    else {
      continue;
    }
    
    str_p++;
    
    if (*str_p >= 'a' && *str_p <= 'f') {
      val = val * 16 + (10 + *str_p - 'a');
    }
    else if (*str_p >= 'A' && *str_p <= 'F') {
      val = val * 16 + (10 + *str_p - 'A');
    }
    else if (*str_p >= '0' && *str_p <= '9') {
      val = val * 16 + (*str_p - '0');
    }
    else {
      str_p--;
    }
    
    *copy_p++ = (char)val;
  }
  
  ret = process_args(args, argc, argv);
  
  free(copy);
  free(argv);
  
  if (ret == NOERROR) {
    return NOERROR;
  }
  else {
    return ERROR;
  }
}

/*
 * Processes arguments sent in via the QUERY_STRING environmental
 * variable that a web-server might send to program in ARG0.  Returns
 * 0 on noerror else -1.
 */
int	argv_web_process(argv_t *args, const char *arg0)
{
  char	*env, *work = NULL;
  int	ret, len;
  
  if (! enabled) {
    argv_startup();
  }
  
  env = getenv("REQUEST_METHOD");
  if (env != NULL && strcmp(env, "POST") == 0) {
    env = getenv("CONTENT_LENGTH");
    if (env != NULL) {
      len = atoi(env);
      if (len > 0) {
	work = (char *)malloc(len + 1);
	if (work == NULL) {
	  if (argv_error_stream != NULL) {
	    (void)fprintf(argv_error_stream,
			  "%s: memory error during argument processing\n",
			  argv_program);
	  }
	  if (argv_interactive) {
	    (void)exit(EXIT_CODE);
	  }
	  return ERROR;
	}
	(void)read(STDIN, work, len);
	work[len] = '\0';
      }
    }
  }
  
  if (work == NULL) {
    env = getenv("QUERY_STRING");
    
    /* if it is not set or empty, then nothing to do */
    if (env == NULL || *env == '\0') {
      work = (char *)malloc(1);
      if (work == NULL) {
	if (argv_error_stream != NULL) {
	  (void)fprintf(argv_error_stream,
			"%s: memory error during argument processing\n",
			argv_program);
	}
	if (argv_interactive) {
	  (void)exit(EXIT_CODE);
	}
	return ERROR;
      }
      work[0] = '\0';
    }
    else {
      work = string_copy(env);
      if (work == NULL) {
	return ERROR;
      }
    }
  }
  
  ret = argv_web_process_string(args, arg0, work, "&=");
  free(work);
  
  if (ret == NOERROR) {
    return NOERROR;
  }
  else {
    return ERROR;
  }
}

/*
 * Print the standard usage messages for argument array ARGS (if null
 * then an empty array is used).  WHICH chooses between long or short
 * messages (see argv.h).
 *
 * NOTE: if this is called before argv_process then the program name may
 * be messed up.
 */
int	argv_usage(const argv_t *args, const int which)
{
  if (! enabled) {
    argv_startup();
  }
  
  if (process_env() != NOERROR) {
    return ERROR;
  }
  
  if (args == NULL) {
    args = empty;
  }
  
  if (which == ARGV_USAGE_SHORT) {
    usage_short(args, 0);
  }
  else if (which == ARGV_USAGE_LONG) {
    usage_long(args);
  }
  else {
    /* default/env settings */
    do_usage(args, global_usage);
  }
  
  return NOERROR;
}

/*
 * See if ARG argument was used in a previous call to argv_process on
 * ARGS.  Returns 1 if yes else 0.
 */
int	argv_was_used(const argv_t *args, const char arg)
{
  const argv_t	*arg_p;
  
  if (! enabled) {
    argv_startup();
  }
  
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    if (arg_p->ar_short_arg == arg) {
      if (arg_p->ar_type & ARGV_FLAG_USED) {
	return 1;
      }
      else {
	return 0;
      }
    }
  }
  
  return 0;
}

/*
 * Frees up any allocations in ARGS that may have been done by
 * argv_process.  This should be done at the end of the program or
 * after all the arguments have been referenced.
 */
void	argv_cleanup(const argv_t *args)
{
  const argv_t	*arg_p;
  int		entry_c;
  
  if (! enabled) {
    argv_startup();
  }
  
  if (args == NULL) {
    return;
  }
  
  /* run through the argument structure */
  for (arg_p = args; arg_p->ar_short_arg != ARGV_LAST; arg_p++) {
    /* handle any arrays */
    if (arg_p->ar_type & ARGV_FLAG_ARRAY) {
      argv_array_t	*arr_p = (argv_array_t *)arg_p->ar_variable;
      
      /* free any entries */
      if (arr_p->aa_entry_n > 0) {
	if (ARGV_TYPE(arg_p->ar_type) == ARGV_CHAR_P) {
	  for (entry_c = 0; entry_c < arr_p->aa_entry_n; entry_c++) {
	    free(ARGV_ARRAY_ENTRY(*arr_p, char *, entry_c));
	  }
	}
	free(arr_p->aa_entries);
      }
      arr_p->aa_entries = NULL;
      arr_p->aa_entry_n = 0;
      continue;
    }
    
    /* handle individual charps */
    if (arg_p->ar_type & ARGV_FLAG_USED
	&& ARGV_TYPE(arg_p->ar_type) == ARGV_CHAR_P) {
      free(*(char **)arg_p->ar_variable);
      continue;
    }
  }
}

/*
 * Copy all the args (after the 0th), one after the other, into BUF of
 * MAX_SIZE.  Returns 0 on no error else -1.
 *
 * NOTE: you can get the 0th argument from argv_argv[0].
 */
int	argv_copy_args(char *buf, const int max_size)
{
  char	**argv_p, *buf_p = buf, *arg_p;
  int	argc, size_c = max_size;
  
  if (! enabled) {
    argv_startup();
  }
  
  if (process_env() != NOERROR) {
    return ERROR;
  }
  
  if (max_size == 0) {
    return NOERROR;
  }
  
  *buf_p = '\0';
  
  if (argv_argv == NULL || max_size == 1) {
    return NOERROR;
  }
  
  for (argv_p = argv_argv + 1, argc = 1; argc < argv_argc; argv_p++, argc++) {
    
    if (size_c < 2) {
      break;
    }
    
    if (argv_p > argv_argv + 1) {
      *buf_p++ = ' ';
      size_c--;
    }
    
    for (arg_p = *argv_p; *arg_p != '\0' && size_c >= 2; size_c--) {
      *buf_p++ = *arg_p++;
    }
  }
  
  *buf_p = '\0';
  return NOERROR;
}
