/*
 * program that handles the dmalloc variables.
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
 * This is the dmalloc program which is designed to enable the user
 * to easily set the environmental variables that control the dmalloc
 * library capabilities.
 *
 * NOTE: all stdout output from this program is designed to be run through
 *   eval by default.  Any messages for the user should be fprintf to stderr.
 */

#include <stdio.h>				/* for stderr */

#define DMALLOC_DISABLE

#include "argv.h"				/* for argument processing */

#include "dmalloc.h"
#include "conf.h"

#include "compat.h"
#include "debug_tok.h"
#include "env.h"
#include "error_str.h"
#include "error_val.h"
#include "dmalloc_loc.h"
#include "version.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: dmalloc.c,v 1.44 1994/09/26 16:00:15 gray Exp $";
#endif

#define HOME_ENVIRON	"HOME"			/* home directory */
#define SHELL_ENVIRON	"SHELL"			/* for the type of shell */
#define DEFAULT_CONFIG	"%s/.dmallocrc"		/* default config file */
#define TOKENIZE_CHARS	" \t,="			/* for tag lines */

#define NO_VALUE		(-1)		/* no value ... value */
#define TOKENS_PER_LINE		5		/* num debug toks per line */

/* argument variables */
LOCAL	char	*address	= NULL;		/* for ADDRESS */
LOCAL	char	bourne		= FALSE;	/* set bourne shell output */
LOCAL	char	cshell		= FALSE;	/* set c-shell output */
LOCAL	char	clear		= FALSE;	/* clear variables */
LOCAL	int	debug		= NO_VALUE;	/* for DEBUG */
LOCAL	int	errno_to_print	= NO_VALUE;	/* to print the error string */
LOCAL	char	*inpath		= NULL;		/* for config-file path */
LOCAL	int	interval	= NO_VALUE;	/* for setting INTERVAL */
LOCAL	char	keep		= FALSE;	/* keep settings override -r */
LOCAL	char	list_tags	= FALSE;	/* list rc tags */
LOCAL	char	list_tokens	= FALSE;	/* list debug tokens */
LOCAL	char	*logpath	= NULL;		/* for LOGFILE setting */
LOCAL	argv_array_t	minus;			/* tokens to remove */
LOCAL	char	no_changes	= FALSE;	/* make no changes to env */
LOCAL	argv_array_t	plus;			/* tokens to add */
LOCAL	char	remove_auto	= FALSE;	/* auto-remove settings */
LOCAL	char	*start		= NULL;		/* for START settings */
LOCAL	char	verbose		= FALSE;	/* verbose flag */
LOCAL	char	very_verbose	= FALSE;	/* very-verbose flag */
LOCAL	char	*tag		= NULL;		/* maybe a tag argument */

LOCAL	argv_t	args[] = {
  { 'a',	"address",	ARGV_CHARP,	&address,
      "address:#",		"stop when malloc sees address" },
  { 'b',	"bourne",	ARGV_BOOL,	&bourne,
      NULL,			"set output for bourne shells" },
  { ARGV_OR },
  { 'C',	"c-shell",	ARGV_BOOL,	&cshell,
      NULL,			"set output for C-type shells" },
  { 'c',	"clear",	ARGV_BOOL,	&clear,
      NULL,			"clear all variables not set" },
  { 'd',	"debug-mask",	ARGV_HEX,	&debug,
      "value",			"hex flag to set debug mask" },
  { 'e',	"errno",	ARGV_INT,	&errno_to_print,
      "errno",			"print error string for errno" },
  { 'f',	"file",		ARGV_CHARP,	&inpath,
      "path",			"config if not ~/.mallocrc" },
  { 'i',	"interval",	ARGV_INT,	&interval,
      "value",			"check heap every number times" },
  { 'k',	"keep",		ARGV_BOOL,	&keep,
      NULL,			"keep settings (override -r)" },
  { 'l',	"logfile",	ARGV_CHARP,	&logpath,
      "path",			"file to log messages to" },
  { 'L',	"list-tags",	ARGV_BOOL,	&list_tags,
      NULL,			"list tags in rc file" },
  { 'm',	"minus",	ARGV_CHARP | ARGV_ARRAY,	&minus,
      "token(s)",		"del tokens from current debug" },
  { 'n',	"no-changes",	ARGV_BOOL,	&no_changes,
      NULL,			"make no changes to the env" },
  { 'p',	"plus",		ARGV_CHARP | ARGV_ARRAY,	&plus,
      "token(s)",		"add tokens to current debug" },
  { 'r',	"remove",	ARGV_BOOL,	&remove_auto,
      NULL,			"remove other settings if tag" },
  { 's',	"start",	ARGV_CHARP,	&start,
      "file:line",		"start check heap after this" },
  { 'T',	"list-tokens",	ARGV_BOOL,	&list_tokens,
      NULL,			"list debug tokens" },
  { 'v',	"verbose",	ARGV_BOOL,	&verbose,
      NULL,			"turn on verbose output" },
  { 'V',	"very-verbose",	ARGV_BOOL,	&very_verbose,
      NULL,			"turn on very-verbose output" },
  { ARGV_MAYBE,	NULL,		ARGV_CHARP,	&tag,
      "tag",			"debug token to find in rc" },
  { ARGV_LAST }
};

/*
 * list of bourne shells
 */
LOCAL	char	*sh_shells[] = { "sh", "ash", "bash", "ksh", "zsh", NULL };

/*
 * try a check out the shell env variable to see what form of shell
 * commands we should output
 */
LOCAL	void	choose_shell(void)
{
  const char	*shell, *shellp;
  int		shellc;
  
  shell = (const char *)getenv(SHELL_ENVIRON);
  if (shell == NULL) {
    cshell = TRUE;
    return;
  }
  
  shellp = (char *)rindex(shell, '/');
  if (shellp == NULL)
    shellp = shell;
  else
    shellp++;
  
  for (shellc = 0; sh_shells[shellc] != NULL; shellc++)
    if (strcmp(sh_shells[shellc], shellp) == 0) {
      bourne = TRUE;
      return;
    }
  
  cshell = TRUE;
}

/*
 * dump the current flags set in the debug variable VAL
 */
LOCAL	void	dump_debug(const int val)
{
  attr_t	*attrp;
  int		tokc = 0, work = val;
  
  for (attrp = attributes; attrp->at_string != NULL; attrp++) {
    /* the below is necessary to handle the 'none' HACK */
    if ((work == 0 && attrp->at_value == 0)
	|| (attrp->at_value != 0
	    && (work & attrp->at_value) == attrp->at_value)) {
      work &= ~attrp->at_value;
      
      if (tokc == 0)
	(void)fprintf(stderr, "   ");
      
      if (very_verbose)
	(void)fprintf(stderr, "%s -- %s",
		      attrp->at_string, attrp->at_desc);
      else {
	(void)fprintf(stderr, "%s", attrp->at_string);
	if (work != 0)
	  (void)fprintf(stderr, ", ");
	tokc = (tokc + 1) % TOKENS_PER_LINE;
      }
      
      if (tokc == 0)
	(void)fprintf(stderr, "\n");
      
      if (work == 0)
	break;
    }
  }
  
  if (tokc != 0)
    (void)fprintf(stderr, "\n");
  
  if (work != 0)
    (void)fprintf(stderr, "%s: warning, unknown debug flags: %#x\n",
		  argv_program, work);
}

/*
 * translate TOK into its proper value which is returned
 */
LOCAL	long	token_to_value(const char * tok)
{
  int	attrc;
  long	ret = 0;
  
  for (attrc = 0; attributes[attrc].at_string != NULL; attrc++) {
    if (strcmp(tok, attributes[attrc].at_string) == 0)
      break;
  }
  
  if (attributes[attrc].at_string == NULL) {
    (void)fprintf(stderr, "%s: unknown token '%s'\n",
		  argv_program, tok);
  }
  else
    ret = attributes[attrc].at_value;
  
  return ret;
}

/*
 * process the user configuration looking for the TAG_FIND.  if it is
 * null then look for DEBUG_VALUE in the file and return the token for
 * it in STRP.  routine returns the new debug value matching tag.
 */
LOCAL	long	process(const long debug_value, const char * tag_find,
			char ** strp)
{
  static char	token[128];
  FILE		*infile = NULL;
  char		path[1024], buf[1024];
  const char	*homep;
  char		found, cont;
  int		new_debug = 0;
  
  /* do we need to have a home variable? */
  if (inpath == NULL) {
    homep = (const char *)getenv(HOME_ENVIRON);
    if (homep == NULL) {
      (void)fprintf(stderr, "%s: could not find variable '%s'\n",
		    argv_program, HOME_ENVIRON);
      exit(1);
    }
    
    (void)sprintf(path, DEFAULT_CONFIG, homep);
    inpath = path;
  }
  
  infile = fopen(inpath, "r");
  if (infile == NULL) {
    (void)fprintf(stderr, "%s: could not read '%s': ", argv_program, inpath);
    perror("");
    exit(1);
  }
  
  /* read each of the lines looking for the tag */
  found = FALSE;
  cont = FALSE;
  
  while (fgets(buf, sizeof(buf), infile) != NULL) {
    char	*tokp, *endp;
    
    /* ignore comments and empty lines */
    if (buf[0] == '#' || buf[0] == '\n')
      continue;
    
    /* chop off the ending \n */
    endp = (char *)rindex(buf, '\n');
    if (endp != NULL)
      *endp = NULLC;
    
    /* get the first token on the line */
    tokp = (char *)strtok(buf, TOKENIZE_CHARS);
    if (tokp == NULL)
      continue;
    
    /* if we're not continuing then we need to process a tag */
    if (! cont) {
      (void)strcpy(token, tokp);
      new_debug = 0;
      
      if (tag_find != NULL && strcmp(tag_find, tokp) == 0)
	found = TRUE;
      
      tokp = (char *)strtok(NULL, TOKENIZE_CHARS);
      if (tokp == NULL)
	continue;
    }
    
    cont = FALSE;
    
    do {
      /* do we have a continuation character */
      if (strcmp(tokp, "\\") == 0) {
	cont = TRUE;
	break;
      }
      
      /* are we processing the tag of choice? */
      if (found || tag_find == NULL)
	new_debug |= token_to_value(tokp);
      
      tokp = (char *)strtok(NULL, TOKENIZE_CHARS);
    } while (tokp != NULL);
    
    if (list_tags && ! cont) {
      if (verbose) {
	(void)fprintf(stderr, "%s:\n", token);
	dump_debug(new_debug);
      }
      else
	(void)fprintf(stderr, "%s\n", token);
    }
    else if (tag_find == NULL && ! cont && new_debug == debug_value) {
      found = TRUE;
      if (strp != NULL)
	*strp = token;
    }
    
    /* are we done? */
    if (found && ! cont)
      break;
  }
  
  (void)fclose(infile);
  
  /* did we find the correct value in the file? */
  if (tag_find == NULL && ! found) {
    if (strp != NULL)
      *strp = "unknown";
  }
  else if (! found && tag_find != NULL) {
    (void)fprintf(stderr, "%s: could not find tag '%s' in '%s'\n",
		  argv_program, tag_find, inpath);
    exit(1);
  }
  
  return new_debug;
}

/*
 * dump the current settings of the malloc variables
 */
LOCAL	void	dump_current(void)
{
  char		*tokp;
  char		*lpath, *start_file;
  unsigned long	addr;
  int		addr_count, inter, start_line, start_count;
  long		flags;
  
  _dmalloc_environ_get(OPTIONS_ENVIRON, &addr, &addr_count, &flags,
		       &inter, &lpath,
		       &start_file, &start_line, &start_count, NULL);
  
  if (flags == DEBUG_INIT)
    (void)fprintf(stderr, "Debug-Flags  not-set\n");
  else {
    (void)process(flags, NULL, &tokp);
    (void)fprintf(stderr, "Debug-Flags  '%#lx' (%s)\n", flags, tokp);
    if (verbose)
      dump_debug(flags);
  }
  
  if (addr == ADDRESS_INIT)
    (void)fprintf(stderr, "Address      not-set\n");
  else {
    if (addr_count == ADDRESS_COUNT_INIT)
      (void)fprintf(stderr, "Address      %#lx\n", (long)addr);
    else
      (void)fprintf(stderr, "Address      %#lx, count = %d\n",
		    (long)addr, addr_count);
  }
  
  if (inter == INTERVAL_INIT)
    (void)fprintf(stderr, "Interval     not-set\n");
  else
    (void)fprintf(stderr, "Interval     %d\n", inter);
  
  if (lpath == LOGPATH_INIT)
    (void)fprintf(stderr, "Logpath      not-set\n");
  else
    (void)fprintf(stderr, "Logpath      '%s'\n", lpath);
  
  if (start_file == START_FILE_INIT && start_count == START_COUNT_INIT)
    (void)fprintf(stderr, "Start-File   not-set\n");
  else if (start_count != START_COUNT_INIT)
    (void)fprintf(stderr, "Start-Count  %d\n", start_count);
  else
    (void)fprintf(stderr, "Start-File   '%s', line = %d\n",
		  start_file, start_line);
}

/*
 * output the code to set env VAR to VALUE
 */
LOCAL void    set_variable(const char * var, const char * value)
{
  if (bourne) {
    if (! no_changes) {
      (void)printf("%s=%s;\n", var, value);
      (void)printf("export %s;\n", var);
    }
    if (no_changes || verbose) {
      (void)fprintf(stderr, "Outputed: %s=%s;\n", var, value);
      (void)fprintf(stderr, "export %s;\n", var);
    }
  }
  else {
    if (! no_changes)
      (void)printf("setenv %s %s;\n", var, value);
    if (no_changes || verbose)
      (void)fprintf(stderr, "Outputed: setenv %s %s;\n", var, value);
  }
}

/*
 * returns the string for ERRNUM
 */
LOCAL	char	*local_strerror(const int errnum)
{
  if (! IS_MALLOC_ERRNO(errnum))
    return errlist[ERROR_BAD_ERRNO];
  else
    return errlist[errnum];
}

EXPORT	int	main(int argc, char ** argv)
{
  char		buf[1024];
  char		debug_set = FALSE, set = FALSE;
  char		*lpath = LOGPATH_INIT, *sfile = START_FILE_INIT;
  unsigned long	addr = ADDRESS_INIT;
  int		addr_count = ADDRESS_COUNT_INIT, inter = INTERVAL_INIT;
  int		sline = START_LINE_INIT, scount = START_COUNT_INIT;
  long		flags = DEBUG_INIT;
  
  argv_help_string = "Sets dmalloc library env variables.  Also try --usage.";
  argv_version_string = malloc_version;
  
  argv_process(args, argc, argv);
  
  if (very_verbose)
    verbose = TRUE;
  
  /* try to figure out the shell we are using */
  if (! bourne && ! cshell)
    choose_shell();
  
  /* get the current debug information from the env variable */
  _dmalloc_environ_get(OPTIONS_ENVIRON, NULL, NULL, &flags, NULL, NULL,
		       NULL, NULL, NULL, NULL);
  
  /* get a new debug value from tag */
  if (tag != NULL) {
    if (debug != NO_VALUE)
      (void)fprintf(stderr, "%s: warning -d ignored, processing tag '%s'\n",
		    argv_program, tag);
    debug = process(0L, tag, NULL);
    debug_set = TRUE;
  }
  
  if (plus.aa_entryn > 0) {
    int		plusc;
    
    /* get current debug value and add tokens if possible */
    if (debug == NO_VALUE)
      if (flags == DEBUG_INIT)
	debug = 0;
      else
	debug = flags;
    
    for (plusc = 0; plusc < plus.aa_entryn; plusc++)
      debug |= token_to_value(ARGV_ARRAY_ENTRY(plus, char *, plusc));
  }
  
  if (minus.aa_entryn > 0) {
    int		minusc;
    
    /* get current debug value and add tokens if possible */
    if (debug == NO_VALUE)
      if (flags == DEBUG_INIT)
	debug = 0;
      else
	debug = flags;
    
    for (minusc = 0; minusc < minus.aa_entryn; minusc++)
      debug &= ~token_to_value(ARGV_ARRAY_ENTRY(minus, char *, minusc));
  }
  
  if (debug != NO_VALUE) {
    /* special case, undefine if 0 */
    if (debug == 0)
      flags = DEBUG_INIT;
    else
      flags = debug;
    set = TRUE;
    /* should we clear the rest? */
    if (debug_set && remove_auto && ! keep)
      clear = TRUE;
  }
  
  if (clear)
    set = TRUE;
  
  if (address != NULL) {
    _dmalloc_address_break(address, &addr, &addr_count);
    set = TRUE;
  }
  else if (clear)
    addr = ADDRESS_INIT;
  
  /* NOTE: special case, interval == 0 causes it to be undefed */
  if (interval != NO_VALUE && interval > 0) {
    inter = interval;
    set = TRUE;
  }
  else if (clear)
    inter = INTERVAL_INIT;
  
  if (logpath != NULL) {
    lpath = logpath;
    set = TRUE;
  }
  else if (clear)
    lpath = LOGPATH_INIT;
  
  if (start != NULL) {
    _dmalloc_start_break(start, &sfile, &sline, &scount);
    set = TRUE;
  }
  else if (clear)
    sfile = START_FILE_INIT;
  
  if (errno_to_print != NO_VALUE) {
    (void)fprintf(stderr, "%s: dmalloc_errno value '%d' = \n",
		  argv_program, errno_to_print);
    (void)fprintf(stderr, "   '%s'\n", local_strerror(errno_to_print));
  }
  
  if (list_tags) {
    (void)fprintf(stderr, "Available Tags:\n");
    process(0L, NULL, NULL);
  }
  
  if (list_tokens) {
    attr_t	*attrp;
    (void)fprintf(stderr, "Debug Tokens:\n");
    for (attrp = attributes; attrp->at_string != NULL; attrp++)
      if (very_verbose)
	(void)fprintf(stderr, "%s (%s) -- %s (%#lx)\n",
		      attrp->at_string, attrp->at_short, attrp->at_desc,
		      attrp->at_value);
      else if (verbose)
	(void)fprintf(stderr, "%s -- %s\n",
		      attrp->at_string, attrp->at_desc);
      else
	(void)fprintf(stderr, "%s\n", attrp->at_string);
  }
  
  if (set) {
    _dmalloc_environ_set(buf, addr, addr_count, flags, inter, lpath, sfile,
			 sline, scount);
    set_variable(OPTIONS_ENVIRON, buf);
  }
  else if (errno_to_print == NO_VALUE && ! list_tags && ! list_tokens)
    dump_current();
  
  argv_cleanup(args);
  
  exit(0);
}
