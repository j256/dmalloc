/*
 * program that handles the malloc debug variables.
 *
 * Copyright 1993 by the Antaire Corporation
 *
 * This file is part of the malloc-debug package.
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
 * The author of the program may be contacted at gray.watson@antaire.com
 */

/*
 * This is the malloc_dbg program which is designed to enable the user
 * to easily set the environmental variables that control the malloc-debug
 * library capabilities.
 *
 * NOTE: all stdout output from this program is designed to be run through
 *   eval by default.  Any messages for the user should be fprintf to stderr.
 */

#include <stdio.h>				/* for stderr */

#define MALLOC_DEBUG_DISABLE

#include "argv.h"				/* for argument processing */

#include "malloc_dbg.h"
#include "conf.h"

#include "compat.h"
#include "dbg_tokens.h"
#include "malloc_loc.h"
#include "version.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: dmalloc.c,v 1.25 1993/08/24 22:17:20 gray Exp $";
#endif

#define HOME_ENVIRON	"HOME"			/* home directory */
#define SHELL_ENVIRON	"SHELL"			/* for the type of shell */
#define DEFAULT_CONFIG	"%s/.mallocrc"		/* default config file */
#define TOKENIZE_CHARS	" \t,="			/* for tag lines */

#define NO_VALUE		(-1)		/* no value ... value */
#define TOKENS_PER_LINE		4		/* num debug toks per line */

/* local variables */
LOCAL	char	printed		= FALSE;	/* did we outputed anything? */

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
LOCAL	char	list		= FALSE;	/* list rc tokens */
LOCAL	char	*logpath	= NULL;		/* for LOGFILE setting */
LOCAL	char	remove_auto	= FALSE;	/* auto-remove settings */
LOCAL	char	*start		= NULL;		/* for START settings */
LOCAL	char	*trace		= NULL;		/* for TRACE */
LOCAL	char	verbose		= FALSE;	/* verbose flag */
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
  { 'L',	"list",		ARGV_BOOL,	&list,
      NULL,			"list tokens in rc file" },
  { 'r',	"remove",	ARGV_BOOL,	&remove_auto,
      NULL,			"remove other settings if tag" },
  { 's',	"start",	ARGV_CHARP,	&start,
      "file:line",		"start check heap after this" },
  { 't',	"trace",	ARGV_CHARP,	&trace,
      "address",		"trace activity on this address" },
  { 'v',	"verbose",	ARGV_BOOL,	&verbose,
      NULL,			"turn on verbose output" },
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
  char	*shell, *shellp;
  int	shellc;
  
  shell = (char *)getenv(SHELL_ENVIRON);
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
 * hexadecimal STR to long translation
 */
LOCAL	long	hex_to_long(char * str)
{
  long		ret;
  
  /* strip off spaces */
  for (; *str == ' ' || *str == '\t'; str++);
  
  /* skip a leading 0[xX] */
  if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X'))
    str += 2;
  
  for (ret = 0;; str++) {
    if (*str >= '0' && *str <= '9')
      ret = ret * 16 + (*str - '0');
    else if (*str >= 'a' && *str <= 'f')
      ret = ret * 16 + (*str - 'a' + 10);
    else if (*str >= 'A' && *str <= 'F')
      ret = ret * 16 + (*str - 'A' + 10);
    else
      break;
  }
  
  return ret;
}

/*
 * dump the current flags set in the debug variable VAL
 */
LOCAL	void	dump_debug(const int val)
{
  attr_t	*attrp;
  int		tokc = 0, work = val;
  
  if (val == 0) {
    (void)fprintf(stderr, "   none\n");
    return;
  }
  
  for (attrp = attributes; attrp->at_string != NULL; attrp++) {
    if (attrp->at_value != 0 && (work & attrp->at_value) == attrp->at_value) {
      if (tokc == 0)
	(void)fprintf(stderr, "   %s", attrp->at_string);
      else if (tokc == TOKENS_PER_LINE - 1)
	(void)fprintf(stderr, ", %s\n", attrp->at_string);
      else
	(void)fprintf(stderr, ", %s", attrp->at_string);
      tokc = (tokc + 1) % TOKENS_PER_LINE;
      work &= ~attrp->at_value;
    }
  }
  
  if (tokc != 0)
    (void)fprintf(stderr, "\n");
  
  if (work != 0)
    (void)fprintf(stderr, "%s: warning, unknown debug flags: %#x\n",
		  argv_program, work);
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
  char		path[1024], buf[1024], *homep;
  char		found, cont;
  long		new_debug = 0;
  
  /* do we need to have a home variable? */
  if (inpath == NULL) {
    homep = (char *)getenv(HOME_ENVIRON);
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
    int		attrc;
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
    }
    
    cont = FALSE;
    
    do {
      /* do we have a continuation character */
      if (strcmp(tokp, "\\") == 0) {
	cont = TRUE;
	break;
      }
      
      /* are we processing the tag of choice? */
      if (found || tag_find == NULL) {
	for (attrc = 0; attributes[attrc].at_string != NULL; attrc++) {
	  if (strcmp(tokp, attributes[attrc].at_string) == 0)
	    break;
	}
	
	if (attributes[attrc].at_string == NULL) {
	  (void)fprintf(stderr, "%s: unknown token '%s'\n",
			argv_program, tokp);
	}
	else
	  new_debug |= attributes[attrc].at_value;
      }
      
      tokp = (char *)strtok(NULL, TOKENIZE_CHARS);
    } while (tokp != NULL);
    
    if (list && ! cont) {
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
  char		*str;
  long		num;
  
  str = (char *)getenv(DEBUG_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", DEBUG_ENVIRON);
  else {
    num = hex_to_long(str);
    (void)process(num, NULL, &str);
    (void)fprintf(stderr, "%s == '%#lx' (%s)\n", DEBUG_ENVIRON, num, str);
    
    if (verbose)
      dump_debug(num);
  }
  
  str = (char *)getenv(ADDRESS_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", ADDRESS_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", ADDRESS_ENVIRON, str);
  
  str = (char *)getenv(INTERVAL_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", INTERVAL_ENVIRON);
  else {
    num = atoi(str);
    (void)fprintf(stderr, "%s == '%d'\n", INTERVAL_ENVIRON, num);
  }
  
  str = (char *)getenv(LOGFILE_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", LOGFILE_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", LOGFILE_ENVIRON, str);
  
  str = (char *)getenv(START_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", START_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", START_ENVIRON, str);
  
  str = (char *)getenv(TRACE_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", TRACE_ENVIRON);
  else
    (void)fprintf(stderr, "%s == '%s'\n", TRACE_ENVIRON, str);
}

/*
 * output the code to set env VAR to VALUE
 */
LOCAL	void	set_variable(char * var, char * value)
{
  if (bourne) {
    (void)printf("%s=%s; export %s;\n", var, value, var);
    if (verbose)
      (void)fprintf(stderr, "Outputed: %s=%s; export %s;\n", var, value, var);
  }
  else {
    (void)printf("setenv %s %s;\n", var, value);
    if (verbose)
      (void)fprintf(stderr, "Outputed: setenv %s %s;\n", var, value);
  }
  
  printed = TRUE;
}

/*
 * output the code to un-set env VAR
 */
LOCAL	void	unset_variable(char * var)
{
  if (bourne) {
    (void)printf("unset %s;\n", var);
    if (verbose)
      (void)fprintf(stderr, "Outputed: unset %s;\n", var);
  }
  else {
    (void)printf("unsetenv %s;\n", var);
    if (verbose)
      (void)fprintf(stderr, "Outputed: unsetenv %s;\n", var);
  }
  
  printed = TRUE;
}

EXPORT	int	main(int argc, char ** argv)
{
  char	buf[20];
  
  /*
   * turn off debugging for this program
   * NOTE: gcc has already called malloc unfortunately
   */
  (void)malloc_debug(0);
  
  argv_help_string = "Sets malloc_dbg library env variables.";
  argv_version_string = malloc_version;
  
  argv_process(args, argc, argv);
  
  /* try to figure out the shell we are using */
  if (! bourne && ! cshell)
    choose_shell();
  
  /* get a new debug value from tag */
  if (tag != NULL) {
    if (debug != NO_VALUE)
      (void)fprintf(stderr, "%s: warning -d ignored, processing tag '%s'\n",
		    argv_program, tag);
    debug = process(0L, tag, NULL);
  }
  
  if (debug != NO_VALUE) {
    (void)sprintf(buf, "%#lx", debug);
    set_variable(DEBUG_ENVIRON, buf);
    
    /* should we clear the rest? */
    if (remove_auto && ! keep)
      clear = TRUE;
  }
  
  if (address != NULL)
    set_variable(ADDRESS_ENVIRON, address);
  else if (clear)
    unset_variable(ADDRESS_ENVIRON);
  
  if (interval != NO_VALUE) {
    (void)sprintf(buf, "%d", interval);
    set_variable(INTERVAL_ENVIRON, buf);
  }
  else if (clear)
    unset_variable(INTERVAL_ENVIRON);
  
  if (logpath != NULL)
    set_variable(LOGFILE_ENVIRON, logpath);
  else if (clear)
    unset_variable(LOGFILE_ENVIRON);
  
  if (start != NULL)
    set_variable(START_ENVIRON, start);
  else if (clear)
    unset_variable(START_ENVIRON);
  
  if (trace != NULL)
    set_variable(TRACE_ENVIRON, trace);
  else if (clear)
    unset_variable(TRACE_ENVIRON);
  
  if (errno_to_print != NO_VALUE) {
    (void)fprintf(stderr, "%s: malloc_errno value '%d' = \n",
		  argv_program, errno_to_print);
    (void)fprintf(stderr, "   '%s'\n", malloc_strerror(errno_to_print));
    printed = TRUE;
  }
  
  if (list)
    process(0L, NULL, NULL);
  else if (! printed)
    dump_current();
  
  exit(0);
}
