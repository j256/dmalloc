/*
 * program that handles the dmalloc variables.
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
 * The author may be contacted via http://www.dmalloc.com/
 *
 * $Id: dmalloc.c,v 1.84 1999/03/04 16:32:40 gray Exp $
 */

/*
 * This is the dmalloc program which is designed to enable the user to
 * easily set the environmental variables that control the dmalloc
 * library capabilities.
 */
/*
 * NOTE: all standard-output from this program is designed to be run
 * through a shell evaluation command by default.  Any messages for
 * the user should therefore be send to stderr.
 */

#include <stdio.h>				/* for stderr */

#define DMALLOC_DISABLE

#include "dmalloc_argv.h"			/* for argument processing */

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include "conf.h"
#include "dmalloc.h"

#include "compat.h"
#include "debug_tok.h"
#include "debug_val.h"
#include "env.h"
#include "error_str.h"
#include "error_val.h"
#include "dmalloc_loc.h"
#include "version.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: dmalloc.c,v 1.84 1999/03/04 16:32:40 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: dmalloc.c,v 1.84 1999/03/04 16:32:40 gray Exp $";
#endif
#endif

#define HOME_ENVIRON	"HOME"			/* home directory */
#define SHELL_ENVIRON	"SHELL"			/* for the type of shell */
#define DEFAULT_CONFIG	"%s/.dmallocrc"		/* default config file */
#define TOKENIZE_CHARS	" \t,="			/* for tag lines */

#define NO_VALUE		(-1)		/* no value ... value */
#define INTERVAL_NO_VALUE	0		/* no value ... value */
#define TOKENS_PER_LINE		5		/* num debug toks per line */

/*
 * default flag information
 */
typedef struct {
  char		*de_string;			/* default name */
  long		de_flags;			/* default settings */
} default_t;

#define RUNTIME_FLAGS	(DEBUG_LOG_STATS | DEBUG_LOG_NONFREE | \
			 DEBUG_LOG_BAD_SPACE | DEBUG_LOG_UNKNOWN | \
			 DEBUG_CHECK_FENCE | \
			 DEBUG_CATCH_NULL | DEBUG_ALLOW_NONLINEAR)
#define LOW_FLAGS	(RUNTIME_FLAGS | \
			 DEBUG_LOG_ELAPSED_TIME | \
			 DEBUG_FREE_BLANK | DEBUG_ERROR_ABORT | \
			 DEBUG_ALLOC_BLANK)
#define MEDIUM_FLAGS	(LOW_FLAGS | \
			 DEBUG_CHECK_HEAP | DEBUG_CHECK_LISTS | \
			 DEBUG_REALLOC_COPY)
#define HIGH_FLAGS	(MEDIUM_FLAGS | \
			 DEBUG_CHECK_BLANK | DEBUG_CHECK_FUNCS)
#define ALL_FLAGS	(HIGH_FLAGS | \
			 DEBUG_LOG_TRANS | DEBUG_LOG_ADMIN | \
			 DEBUG_LOG_BLOCKS | \
			 DEBUG_HEAP_CHECK_MAP | DEBUG_NEVER_REUSE)
/* NOTE: print-error is not in this list because it is special */

static	default_t	defaults[] = {
  { "runtime",		RUNTIME_FLAGS },
  { "run",		RUNTIME_FLAGS },
  { "low",		LOW_FLAGS },
  { "med",		MEDIUM_FLAGS },
  { "medium",		MEDIUM_FLAGS },
  { "high",		HIGH_FLAGS },
  { "all",		ALL_FLAGS },
  { NULL }
};

/* argument variables */
static	char	*address	= NULL;		/* for ADDRESS */
static	char	bourne_b	= FALSE;	/* set bourne shell output */
static	char	cshell_b	= FALSE;	/* set c-shell output */
static	char	clear_b		= FALSE;	/* clear variables */
static	int	debug		= NO_VALUE;	/* for DEBUG */
static	int	errno_to_print	= NO_VALUE;	/* to print the error string */
static	char	gdb_b		= FALSE;	/* set gdb output */
static	char	*inpath		= NULL;		/* for config-file path */
static	unsigned long interval	= INTERVAL_NO_VALUE; /* for setting INTERVAL */
static	int	thread_lock_on	= NO_VALUE;	/* for setting LOCK_ON */
static	char	keep_b		= FALSE;	/* keep settings override -r */
static	char	list_tags_b	= FALSE;	/* list rc tags */
static	char	debug_tokens_b	= FALSE;	/* list debug tokens */
static	char	*logpath	= NULL;		/* for LOGFILE setting */
static	char	long_tokens_b	= FALSE;	/* long-tok output */
static	argv_array_t	minus;			/* tokens to remove */
static	char	make_changes_b	= TRUE;		/* make no changes to env */
static	argv_array_t	plus;			/* tokens to add */
static	char	remove_auto_b	= FALSE;	/* auto-remove settings */
static	char	short_tokens_b	= FALSE;	/* short-tok output */
static	char	*start		= NULL;		/* for START settings */
static	char	verbose_b	= FALSE;	/* verbose flag */
static	char	very_verbose_b	= FALSE;	/* very-verbose flag */
static	char	*tag		= NULL;		/* maybe a tag argument */

static	argv_t	args[] = {
  { 'b',	"bourne",	ARGV_BOOL,	&bourne_b,
      NULL,			"set output for bourne shells" },
  { ARGV_OR },
  { 'C',	"c-shell",	ARGV_BOOL,	&cshell_b,
      NULL,			"set output for C-type shells" },
  { ARGV_OR },
  { 'g',	"gdb",		ARGV_BOOL,	&gdb_b,
      NULL,			"set output for gdb parsing" },
  
  { 'L',	"long-tokens",	ARGV_BOOL,	&long_tokens_b,
      NULL,			"output long-tokens not 0x..." },
  { ARGV_OR },
  { 'S',	"short-tokens",	ARGV_BOOL,	&short_tokens_b,
      NULL,			"output short-tokens not 0x..." },
  
  { 'a',	"address",	ARGV_CHAR_P,	&address,
      "address:#",		"stop when malloc sees address" },
  { 'c',	"clear",	ARGV_BOOL,	&clear_b,
      NULL,			"clear all variables not set" },
  { 'd',	"debug-mask",	ARGV_HEX,	&debug,
      "value",			"hex flag to set debug mask" },
  { 'D',	"debug-tokens",	ARGV_BOOL,	&debug_tokens_b,
      NULL,			"list debug tokens" },
  { 'e',	"errno",	ARGV_INT,	&errno_to_print,
      "errno",			"print error string for errno" },
  { 'f',	"file",		ARGV_CHAR_P,	&inpath,
      "path",			"config if not ~/.mallocrc" },
  { 'i',	"interval",	ARGV_U_LONG,	&interval,
      "value",			"check heap every number times" },
  { 'k',	"keep",		ARGV_BOOL,	&keep_b,
      NULL,			"keep settings (override -r)" },
  { 'l',	"logfile",	ARGV_CHAR_P,	&logpath,
      "path",			"file to log messages to" },
  { 'm',	"minus",	ARGV_CHAR_P | ARGV_FLAG_ARRAY,	&minus,
      "token(s)",		"del tokens from current debug" },
  { 'n',	"no-changes",	ARGV_BOOL_NEG,	&make_changes_b,
      NULL,			"make no changes to the env" },
  { 'o',	"lock-on",	ARGV_INT,	&thread_lock_on,
      "number",			"number of times to not lock" },
  { 'p',	"plus",		ARGV_CHAR_P | ARGV_FLAG_ARRAY,	&plus,
      "token(s)",		"add tokens to current debug" },
  { 'r',	"remove",	ARGV_BOOL,	&remove_auto_b,
      NULL,			"remove other settings if tag" },
  { 's',	"start",	ARGV_CHAR_P,	&start,
      "file:line",		"start check heap after this" },
  { 't',	"list-tags",	ARGV_BOOL,	&list_tags_b,
      NULL,			"list tags in rc file" },
  { 'v',	"verbose",	ARGV_BOOL,	&verbose_b,
      NULL,			"turn on verbose output" },
  { 'V',	"very-verbose",	ARGV_BOOL,	&very_verbose_b,
      NULL,			"turn on very-verbose output" },
  { ARGV_MAYBE,	NULL,		ARGV_CHAR_P,	&tag,
      "tag",			"debug token to find in rc" },
  { ARGV_LAST }
};

/*
 * list of bourne shells
 */
static	char	*sh_shells[] = { "sh", "ash", "bash", "ksh", "zsh", NULL };

/*
 * try a check out the shell env variable to see what form of shell
 * commands we should output
 */
static	void	choose_shell(void)
{
  const char	*shell, *shell_p;
  int		shell_c;
  
  shell = getenv(SHELL_ENVIRON);
  if (shell == NULL) {
    /* oh well, we just guess on c-shell */
    cshell_b = TRUE;
    return;
  }
  
  shell_p = strrchr(shell, '/');
  if (shell_p == NULL) {
    shell_p = shell;
  }
  else {
    shell_p++;
  }
  
  for (shell_c = 0; sh_shells[shell_c] != NULL; shell_c++) {
    if (strcmp(sh_shells[shell_c], shell_p) == 0) {
      bourne_b = TRUE;
      return;
    }
  }
  
  cshell_b = TRUE;
}

/*
 * dump the current flags set in the debug variable VAL
 */
static	void	dump_debug(const int val)
{
  attr_t	*attr_p;
  int		tok_c = 0, work = val;
  
  for (attr_p = attributes; attr_p->at_string != NULL; attr_p++) {
    /* the below is necessary to handle the 'none' HACK */
    if ((work == 0 && attr_p->at_value == 0)
	|| (attr_p->at_value != 0
	    && BIT_IS_SET(work, attr_p->at_value))) {
      BIT_CLEAR(work, attr_p->at_value);
      
      if (tok_c == 0) {
	(void)fprintf(stderr, "   ");
      }
      
      if (very_verbose_b) {
	(void)fprintf(stderr, "%s -- %s", attr_p->at_string, attr_p->at_desc);
      }
      else {
	(void)fprintf(stderr, "%s", attr_p->at_string);
	if (work != 0) {
	  (void)fprintf(stderr, ", ");
	}
	tok_c = (tok_c + 1) % TOKENS_PER_LINE;
      }
      
      if (tok_c == 0) {
	(void)fprintf(stderr, "\n");
      }
      
      if (work == 0) {
	break;
      }
    }
  }
  
  if (tok_c != 0) {
    (void)fprintf(stderr, "\n");
  }
  
  if (work != 0) {
    (void)fprintf(stderr, "%s: warning, unknown debug flags: %#x\n",
		  argv_program, work);
  }
}

/*
 * translate TOK into its proper value which is returned
 */
static	long	token_to_value(const char *tok)
{
  attr_t	*attr_p;
  
  /* find the matching attribute string */
  for (attr_p = attributes; attr_p->at_string != NULL; attr_p++) {
    if (strcmp(tok, attr_p->at_string) == 0
	|| strcmp(tok, attr_p->at_short) == 0) {
      break;
    }
  }
  
  if (attr_p->at_string == NULL) {
    (void)fprintf(stderr, "%s: unknown token '%s'\n", argv_program, tok);
    return 0;
  }
  
  /* if we have a 0 value and not none then this is a disabled token */
  if (attr_p->at_value == 0 && strcmp(tok, "none") != 0) {
    (void)fprintf(stderr, "%s: token '%s' has been disabled: %s\n",
		  argv_program, tok, attr_p->at_desc);
    return 0;
  }
  
  return attr_p->at_value;
}

/*
 * process the user configuration looking for the TAG_FIND.  if it is
 * null then look for DEBUG_VALUE in the file and return the token for
 * it in STR_P.  routine returns the new debug value matching tag.
 */
static	long	process(const long debug_value, const char *tag_find,
			char **str_p)
{
  static char	token[128];
  FILE		*infile = NULL;
  char		path[1024], buf[1024];
  default_t	*def_p;
  const char	*home_p;
  int		found_b, cont_b;
  long		new_debug = 0;
  
  /* do we need to have a home variable? */
  if (inpath == NULL) {
    home_p = getenv(HOME_ENVIRON);
    if (home_p == NULL) {
      (void)fprintf(stderr, "%s: could not find variable '%s'\n",
		    argv_program, HOME_ENVIRON);
      exit(1);
    }
    
    (void)sprintf(path, DEFAULT_CONFIG, home_p);
    inpath = path;
  }
  
  infile = fopen(inpath, "r");
  /* do not test for error here */
  
  if (list_tags_b && infile != NULL) {
    (void)fprintf(stderr, "Tags available from '%s':\n", inpath);
  }
  
  /* read each of the lines looking for the tag */
  found_b = FALSE;
  cont_b = FALSE;
  
  while (infile != NULL && fgets(buf, sizeof(buf), infile) != NULL) {
    char	*tok_p, *end_p;
    
    /* ignore comments and empty lines */
    if (buf[0] == '#' || buf[0] == '\n') {
      continue;
    }
    
    /* chop off the ending \n */
    end_p = strrchr(buf, '\n');
    if (end_p != NULL) {
      *end_p = '\0';
    }
    
    /* get the first token on the line */
    tok_p = strtok(buf, TOKENIZE_CHARS);
    if (tok_p == NULL) {
      continue;
    }
    
    /* if we're not continuing then we need to process a tag */
    if (! cont_b) {
      (void)strcpy(token, tok_p);
      new_debug = 0;
      
      if (tag_find != NULL && strcmp(tag_find, tok_p) == 0) {
	found_b = TRUE;
      }
      
      tok_p = strtok(NULL, TOKENIZE_CHARS);
      if (tok_p == NULL) {
	continue;
      }
    }
    
    cont_b = FALSE;
    
    do {
      /* do we have a continuation character */
      if (strcmp(tok_p, "\\") == 0) {
	cont_b = TRUE;
	break;
      }
      
      /* are we processing the tag of choice? */
      if (found_b || tag_find == NULL) {
	new_debug |= token_to_value(tok_p);
      }
      
      tok_p = strtok(NULL, TOKENIZE_CHARS);
    } while (tok_p != NULL);
    
    if (list_tags_b && (! cont_b)) {
      if (verbose_b) {
	(void)fprintf(stderr, "%s (%#lx):\n", token, new_debug);
	dump_debug(new_debug);
      }
      else
	(void)fprintf(stderr, "%s\n", token);
    }
    else if (tag_find == NULL && (! cont_b) && new_debug == debug_value) {
      found_b = TRUE;
      if (str_p != NULL) {
	*str_p = token;
      }
    }
    
    /* are we done? */
    if (found_b && (! cont_b)) {
      break;
    }
  }
  
  if (infile != NULL) {
    (void)fclose(infile);
  }
  
  /* did we find the correct value in the file? */
  if (found_b) {
    return new_debug;
  }
  
  if ((! list_tags_b) && tag_find == NULL) {
    
    /* check the default list to see if we have a match there */
    if (str_p != NULL) {
      for (def_p = defaults; def_p->de_string != NULL; def_p++) {
	if (def_p->de_flags == debug_value) {
	  *str_p = def_p->de_string;
	  break;
	}
      }
      if (def_p->de_string == NULL) {
	*str_p = "unknown";
      }
    }
  }
  else {
    
    if (list_tags_b) {
      (void)fprintf(stderr, "\n");
      (void)fprintf(stderr, "Tags available by default:\n");
    }
    
    for (def_p = defaults; def_p->de_string != NULL; def_p++) {
      if (list_tags_b) {
	if (verbose_b) {
	  (void)fprintf(stderr, "%s (%#lx):\n",
			def_p->de_string, def_p->de_flags);
	  dump_debug(def_p->de_flags);
	}
	else
	  (void)fprintf(stderr, "%s\n", def_p->de_string);
      }
      if (tag_find != NULL && strcmp(tag_find, def_p->de_string) == 0) {
	break;
      }
    }
    if (! list_tags_b) {
      if (def_p->de_string == NULL) {
	if (infile == NULL) {
	  (void)fprintf(stderr, "%s: could not read '%s': ",
			argv_program, inpath);
	  perror("");
	}
	else {
	  (void)fprintf(stderr, "%s: could not find tag '%s' in '%s'\n",
			argv_program, tag_find, inpath);
	}
	exit(1);
      }
      new_debug = def_p->de_flags;
    }
  }
  
  return new_debug;
}

/*
 * dump the current settings of the malloc variables
 */
static	void	dump_current(void)
{
  char		*tok_p;
  char		*lpath, *start_file;
  unsigned long	addr, inter;
  int		addr_count, lock_on, start_line, start_count;
  unsigned int	flags;
  
  _dmalloc_environ_get(OPTIONS_ENVIRON, &addr, &addr_count, &flags,
		       &inter, &lock_on, &lpath,
		       &start_file, &start_line, &start_count);
  
  if (flags == (unsigned int)DEBUG_INIT) {
    (void)fprintf(stderr, "Debug-Flags  not-set\n");
  }
  else {
    (void)process(flags, NULL, &tok_p);
    (void)fprintf(stderr, "Debug-Flags %#x (%u) (%s)\n", flags, flags, tok_p);
    if (verbose_b) {
      dump_debug(flags);
    }
  }
  
  if (addr == ADDRESS_INIT) {
    (void)fprintf(stderr, "Address      not-set\n");
  }
  else {
    if (addr_count == ADDRESS_COUNT_INIT) {
      (void)fprintf(stderr, "Address      %#lx\n", (long)addr);
    }
    else {
      (void)fprintf(stderr, "Address      %#lx, count = %d\n",
		    (long)addr, addr_count);
    }
  }
  
  if (inter == INTERVAL_INIT) {
    (void)fprintf(stderr, "Interval     not-set\n");
  }
  else {
    (void)fprintf(stderr, "Interval     %lu\n", inter);
  }
  
  if (lock_on == LOCK_ON_INIT) {
    (void)fprintf(stderr, "Lock-On      not-set\n");
  }
  else {
    (void)fprintf(stderr, "Lock-On      %d\n", lock_on);
  }
  
  if (lpath == LOGPATH_INIT) {
    (void)fprintf(stderr, "Logpath      not-set\n");
  }
  else {
    (void)fprintf(stderr, "Logpath      '%s'\n", lpath);
  }
  
  if (start_file == START_FILE_INIT && start_count == START_COUNT_INIT) {
    (void)fprintf(stderr, "Start-File   not-set\n");
  }
  else if (start_count != START_COUNT_INIT) {
    (void)fprintf(stderr, "Start-Count  %d\n", start_count);
  }
  else {
    (void)fprintf(stderr, "Start-File   '%s', line = %d\n",
		  start_file, start_line);
  }
}

/*
 * output the code to set env VAR to VALUE
 */
static	void    set_variable(const char *var, const char *value)
{
  char	comm[1024];
  
  if (bourne_b) {
    (void)sprintf(comm, "%s=%s;\nexport %s;\n", var, value, var);
  }
  else if (gdb_b) {
    (void)sprintf(comm, "set env %s %s;\n", var, value);
  }
  else {
    (void)sprintf(comm, "setenv %s %s;\n", var, value);
  }
  
  if (make_changes_b) {
    (void)printf("%s", comm);
  }
  if ((! make_changes_b) || verbose_b) {
    (void)fprintf(stderr, "Outputed:\n");
    (void)fprintf(stderr, "%s", comm);
  }
}

/*
 * Returns the string for ERROR_NUM.
 */
static	char	*local_strerror(const int error_num)
{
  error_str_t	*err_p;
  
  for (err_p = error_list; err_p->es_error != 0; err_p++) {
    if (err_p->es_error == error_num) {
      return err_p->es_string;
    }
  }
  
  return INVALID_ERROR;
}

int	main(int argc, char **argv)
{
  char		buf[1024];
  int		debug_set_b = FALSE, set_b = FALSE;
  char		*lpath = LOGPATH_INIT, *sfile = START_FILE_INIT;
  unsigned long	addr = ADDRESS_INIT, inter = INTERVAL_INIT;
  int		addr_count = ADDRESS_COUNT_INIT;
  int		lock_on = LOCK_ON_INIT;
  int		sline = START_LINE_INIT, scount = START_COUNT_INIT;
  unsigned int	flags = DEBUG_INIT;
  
  argv_help_string = "Sets dmalloc library env variables.  Also try --usage.";
  argv_version_string = dmalloc_version;
  
  argv_process(args, argc, argv);
  
  if (very_verbose_b) {
    verbose_b = TRUE;
  }
  
  /* try to figure out the shell we are using */
  if ((! bourne_b) && (! cshell_b) && (! gdb_b)) {
    choose_shell();
  }
  
  /* get the current debug information from the env variable */
  _dmalloc_environ_get(OPTIONS_ENVIRON, &addr, &addr_count, &flags, &inter,
		       &lock_on, &lpath, &sfile, &sline, &scount);
  
  /* get a new debug value from tag */
  if (tag != NULL) {
    if (debug != NO_VALUE) {
      (void)fprintf(stderr, "%s: warning -d ignored, processing tag '%s'\n",
		    argv_program, tag);
    }
    debug = process(0L, tag, NULL);
    debug_set_b = TRUE;
  }
  
  if (plus.aa_entry_n > 0) {
    int		plus_c;
    
    /* get current debug value and add tokens if possible */
    if (debug == NO_VALUE) {
      if (flags == (unsigned int)DEBUG_INIT) {
	debug = 0;
      }
      else {
	debug = flags;
      }
    }
    
    for (plus_c = 0; plus_c < plus.aa_entry_n; plus_c++) {
      debug |= token_to_value(ARGV_ARRAY_ENTRY(plus, char *, plus_c));
    }
  }
  
  if (minus.aa_entry_n > 0) {
    int		minus_c;
    
    /* get current debug value and add tokens if possible */
    if (debug == NO_VALUE) {
      if (flags == (unsigned int)DEBUG_INIT) {
	debug = 0;
      }
      else {
	debug = flags;
      }
    }
    
    for (minus_c = 0; minus_c < minus.aa_entry_n; minus_c++) {
      debug &= ~token_to_value(ARGV_ARRAY_ENTRY(minus, char *, minus_c));
    }
  }
  
  /*
   * NOTE: this should be ahead of lock_on setting which tests this.
   */
  if (debug != NO_VALUE) {
    /* special case, undefine if 0 */
    if (debug == 0) {
      flags = DEBUG_INIT;
    }
    else {
      flags = debug;
    }
    set_b = TRUE;
    /* should we clear the rest? */
    if (debug_set_b && remove_auto_b && ! keep_b) {
      clear_b = TRUE;
    }
  }
  
  if (clear_b) {
    set_b = TRUE;
  }
  
  if (address != NULL) {
    _dmalloc_address_break(address, &addr, &addr_count);
    set_b = TRUE;
  }
  else if (clear_b) {
    addr = ADDRESS_INIT;
  }
  
  if (interval != INTERVAL_NO_VALUE) {
    /* NOTE: special case, == 0 causes it to be undef'ed */
    if (interval == 0) {
      inter = INTERVAL_INIT;
    }
    else {
      inter = interval;
    }
    set_b = TRUE;
  }
  else if (clear_b) {
    inter = INTERVAL_INIT;
  }
  
  /*
   * NOTE: this should be after the debug setting which this tests.
   */
  if (thread_lock_on != NO_VALUE) {
    /* NOTE: special case, == 0 causes it to be undef'ed */
    if (thread_lock_on == 0) {
      lock_on = LOCK_ON_INIT;
    }
    else {
      lock_on = thread_lock_on;
    }
    set_b = TRUE;
    if (! BIT_IS_SET(flags, DEBUG_ALLOW_NONLINEAR)) {
      (void)fprintf(stderr,
		    "WARNING: the allow-nonlinear flag is not enabled\n");
    }
  }
  else if (clear_b) {
    lock_on = LOCK_ON_INIT;
  }
  
  if (logpath != NULL) {
    lpath = logpath;
    set_b = TRUE;
  }
  else if (clear_b) {
    lpath = LOGPATH_INIT;
  }
  
  if (start != NULL) {
    _dmalloc_start_break(start, &sfile, &sline, &scount);
    set_b = TRUE;
  }
  else if (clear_b) {
    sfile = START_FILE_INIT;
    scount = START_COUNT_INIT;
  }
  
  if (errno_to_print != NO_VALUE) {
    (void)fprintf(stderr, "%s: dmalloc_errno value '%d' = \n",
		  argv_program, errno_to_print);
    (void)fprintf(stderr, "   '%s'\n", local_strerror(errno_to_print));
  }
  
  if (list_tags_b) {
    process(0L, NULL, NULL);
  }
  
  if (debug_tokens_b) {
    attr_t		*attr_p;
    unsigned int	left = 0x7fffffff;
    
    (void)fprintf(stderr, "Debug Tokens:\n");
    for (attr_p = attributes; attr_p->at_string != NULL; attr_p++) {
      /* skip any disabled tokens */
      if (attr_p->at_value == 0 && strcmp(attr_p->at_string, "none") != 0) {
	continue;
      }
      if (attr_p->at_value != 0 && (! BIT_IS_SET(left, attr_p->at_value))) {
	/* skip any tokens we've seen before */
	continue;
      }
      if (very_verbose_b) {
	(void)fprintf(stderr, "%s (%s) -- %s (%#lx)\n",
		      attr_p->at_string, attr_p->at_short, attr_p->at_desc,
		      attr_p->at_value);
      }
      else if (verbose_b) {
	(void)fprintf(stderr, "%s -- %s\n",
		      attr_p->at_string, attr_p->at_desc);
      }
      else {
	(void)fprintf(stderr, "%s\n", attr_p->at_string);
      }
      BIT_CLEAR(left, attr_p->at_value);
    }
  }
  
  if (set_b) {
    _dmalloc_environ_set(buf, long_tokens_b, short_tokens_b, addr, addr_count,
			 flags, inter, lock_on, lpath, sfile, sline, scount);
    set_variable(OPTIONS_ENVIRON, buf);
  }
  else if (errno_to_print == NO_VALUE
	   && (! list_tags_b)
	   && (! debug_tokens_b)) {
    dump_current();
  }
  
  argv_cleanup(args);
  
  exit(0);
}
