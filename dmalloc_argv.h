/*
 * Defines for a generic argv and argc processor...
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
 * The author may be contacted via http://www.letters.com/~gray/
 *
 * $Id: dmalloc_argv.h,v 1.5 1998/10/26 14:24:33 gray Exp $
 */

#ifndef __DMALLOC_ARGV_H__
#define __DMALLOC_ARGV_H__

/*
 * Version string for the library
 *
 * NOTE to gray: whenever this is changed, corresponding Changlog and
 * NEWS entries *must* be entered and 2 entries in argv.texi must be
 * updated.
 *
 * ARGV LIBRARY VERSION -- 2.3.1
 */

/* produced by configure, inserted into argv.h */
/* used to handle the const operator */
/* const is available */

/* NOTE: start of $Id: dmalloc_argv.h,v 1.5 1998/10/26 14:24:33 gray Exp $ */

/*
 * Generic and standardized argument processor.  You describe the arguments
 * that you are looking for along with their types and these routines do the
 * work to convert them into values.
 *
 * These routines also provide standardized error and usage messages as well
 * as good usage documentation and long and short options.
 */

#include <stdio.h>			/* have to for FILE * below */

/* this defines what type the standard void memory-pointer is */
#if defined(__STDC__) && __STDC__ == 1
#define ARGV_PNT	void *
#else
#define ARGV_PNT	char *
#endif

/*
 * argument information structure.  this specifies the allowable options
 * and some information about each one.
 *
 * { 'O',  "optimize",  ARGV_BOOL,  &optimize,  NULL,  "turn on optimization" }
 * { 'c',  "config",  ARGV_CHAR_P,  &config,  "file",  "configuration file" }
 */
typedef struct {
  char		ar_short_arg;		/* the char of the arg, 'd' if '-d' */
  char		*ar_long_arg;		/* long version of arg, 'delete' */
  unsigned int	ar_type;		/* type of option, see values below */
  ARGV_PNT	ar_variable;		/* address of variable that is arg */
  char		*ar_var_label;		/* label for variable descriptions */
  char		*ar_comment;		/* comment for usage message */
} argv_t;

/*
 * argument array type.  when ARGV_ARRAY is |'d with the ar_type in the above
 * structure then multiple instances of the option are allowed and each
 * instance is stored into the following structure that MUST be in ar_variable
 * in the above arg_t structure.
 * NOTE: after the arguments have been processed, if aa_entryn is > 0 then
 * aa_entries needs to be free'd by user. argv_cleanup() can be used for this
 */
typedef struct {
  int		aa_entry_n;		/* number of elements in aa_entrees */
  ARGV_PNT	aa_entries;		/* entry list specified */
} argv_array_t;

/*  extract the count of the elements from an argv ARRAY */
#define ARGV_ARRAY_COUNT(array)		((array).aa_entry_n)

/* extract WHICH entry of TYPE from an argv ARRAY */
#define ARGV_ARRAY_ENTRY(array, type, which)	\
	(((type *)(array).aa_entries)[which])

/* extract a pointer to WHICH entry of TYPE from an argv ARRAY */
#define ARGV_ARRAY_ENTRY_P(array, type, which)	\
	(((type *)(array).aa_entries) + which)

/* special ar_short_arg value to mark the last entry in the argument array */
#define ARGV_LAST	((char)255)

/*
 * special ar_short_arg value to mark mandatory arguments (i.e. arguments that
 * *must* be specified.  for arguments that are not optional like [-b].
 * to have a variable number of mandatory args then make the last MAND
 * entry be a ARG_ARRAY type.
 */
#define ARGV_MAND	((char)254)

/*
 * special ar_short_arg value to mark that there is the possibility of
 * a mandatory argument here if one is specified.
 */
#define ARGV_MAYBE	((char)253)

/*
 * special ar_short_arg value to say that the previous and next arguments in
 * the list should not be used together.
 * {'a'...}, {ARG_OR}, {'b'...}, {ARG_OR}, {'c'...} means
 * the user should only specific -a or -b or -c but not 2 or more.
 */
#define ARGV_OR		((char)252)

/*
 * special ar_short_arg value that is the same as ARGV_OR but one of the args
 * must be used.
 * {'a'...}, {ARG_ONE_OF}, {'b'...}, {ARG_ONE_OF}, {'c'...} means
 * the user must specify one of -a or -b or -c but not 2 or more.
 * ARGV_XOR is there for compatibility with older versions.
 */
#define ARGV_ONE_OF	((char)251)
#define ARGV_XOR	((char)251)

/*
 * ar_type values of arg_t
 * NOTE: if this list is changed, some defines in argv_loc need to be changed
 */
#define ARGV_BOOL	1		/* boolean type, sets to ARGV_TRUE */
#define ARGV_BOOL_NEG	2		/* like bool but sets to ARGV_FALSE */
#define ARGV_BOOL_ARG	3		/* like bool but takes a yes/no arg */
#define ARGV_CHAR	4		/* single character */
#define ARGV_CHAR_P	5		/* same as STRING */
#define ARGV_SHORT	6		/* short integer number */
#define ARGV_U_SHORT	7		/* unsigned short integer number */
#define ARGV_INT	8		/* integer number */
#define ARGV_U_INT	9		/* unsigned integer number */
#define ARGV_LONG	10		/* long integer number */
#define ARGV_U_LONG	11		/* unsinged long integer number */
#define ARGV_FLOAT	12		/* floating pointer number */
#define ARGV_DOUBLE	13		/* double floating pointer number */
#define ARGV_BIN	14		/* binary number (0s and 1s) */
#define ARGV_OCT	15		/* octal number, (base 8) */
#define ARGV_HEX	16		/* hexadecimal number, (base 16) */
#define ARGV_INCR	17		/* int arg which gets ++ each time */
#define ARGV_SIZE	18		/* long arg which knows mMbBkKgG */
#define ARGV_U_SIZE	19		/* u_long arg which knows mMbBkKgG */

#define ARGV_TYPE(t)	((t) & 0x3F)	/* strip off all but the var type */
#define ARGV_FLAG_ARRAY	(1 << 14)	/* OR with type to indicate array */
#define ARGV_FLAG_MAND	(1 << 13)	/* OR with type to mark mandatory */
/* NOTE: other internal flags defined in argv_loc.h */

/* argv_usage which argument values */
#define ARGV_USAGE_SHORT	1	/* print short usage messages */
#define ARGV_USAGE_LONG		2	/* print long-format usage messages */
#define ARGV_USAGE_DEFAULT	3	/* default usage messages */

/* boolean type settings */
#define ARGV_FALSE		0
#define ARGV_TRUE		1

/*<<<<<<<<<<  The below prototypes are auto-generated by fillproto */

/* This is a processed version of argv[0], pre-path removed: /bin/ls -> ls */
extern
char	argv_program[/* PROGRAM_NAME + 1 */];

/* A global value of argv from main after argv_process has been called */
extern
char	**argv_argv;

/* A global value of argc from main after argv_process has been called */
extern
int	argv_argc;

/* This should be set externally to provide general program help to user */
extern
char	*argv_help_string;

/* This should be set externally to provide version information to the user */
extern
char	*argv_version_string;

/*
 * Are we running interactively?  This will exit on errors.  Set to
 * false to return error codes instead.
 */
extern
char 	argv_interactive;

/*
 * The FILE stream that argv out_puts all its errors.  Set to NULL to
 * not dump any error messages.  Default is stderr.
 */
extern
FILE 	*argv_error_stream;

/*
 * Processes ARGC number of arguments from ARGV depending on argument
 * info array ARGS (if null then an empty array is used).  This
 * routine will not modify the argv array in any way.
 *
 * NOTE: it will modify the args array by setting various flags in the
 * type field.  returns 0 if no error else -1.
 */
extern
int	argv_process(argv_t *args, const int argc, char **argv);

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
extern
int	argv_web_process_string(argv_t *args, const char *arg0,
				const char *string, const char *delim);

/*
 * Processes arguments sent in via the QUERY_STRING environmental
 * variable that a web-server might send to program in ARG0.  Returns
 * 0 on noerror else -1.
 */
extern
int	argv_web_process(argv_t *args, const char *arg0);

/*
 * Print the standard usage messages for argument array ARGS (if null
 * then an empty array is used).  WHICH chooses between long or short
 * messages (see argv.h).
 *
 * NOTE: if this is called before argv_process then the program name may
 * be messed up.
 */
extern
int	argv_usage(const argv_t *args, const int which);

/*
 * See if ARG argument was used in a previous call to argv_process on
 * ARGS.  Returns 1 if yes else 0.
 */
extern
int	argv_was_used(const argv_t *args, const char arg);

/*
 * Frees up any allocations in ARGS that may have been done by
 * argv_process.  This should be done at the end of the program or
 * after all the arguments have been referenced.
 */
extern
void	argv_cleanup(const argv_t *args);

/*
 * Copy all the args (after the 0th), one after the other, into BUF of
 * MAX_SIZE.  Returns 0 on no error else -1.
 *
 * NOTE: you can get the 0th argument from argv_argv[0].
 */
extern
int	argv_copy_args(char *buf, const int max_size);

/*<<<<<<<<<<   This is end of the auto-generated output from fillproto. */

#endif /* ! __DMALLOC_ARGV_H__ */
