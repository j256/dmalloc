/*
 * program that handles the malloc debug variables.
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
 * License along with this library (see COPYING-LIB); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * The author of the program may be contacted at gray.watson@antaire.com
 */

/*
 * This is the malloc_dbg program which is designed to enable the user
 * to easily enable the multitude of malloc-debug capabilities.
 *
 * NOTE: all stdout output from this program is designed to be run through
 *   eval by default.  Any messages for the user should be fprintf to stderr.
 */

#include <stdio.h>				/* for stderr */

#define MALLOC_DEBUG_DISABLE

#include "malloc_dbg.h"
#include "malloc_loc.h"

#include "compat.h"
#include "conf.h"
#include "dbg_tokens.h"
#include "version.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: dmalloc.c,v 1.10 1993/04/15 21:58:22 gray Exp $";
#endif

#define HOME_ENVIRON	"HOME"			/* home directory */
#define DEFAULT_CONFIG	"%s/.mallocrc"		/* default config file */
#define TOKENIZE_CHARS	" \t,="			/* for tag lines */

#define USAGE_STRING	"--usage"		/* show the usage message */
#define VERSION_STRING	"--version"		/* show the version message */
#define NO_VALUE	(-1)			/* no value ... value */

/* local variables */
LOCAL	char	printed		= FALSE;	/* did we outputed anything? */
LOCAL	char	*program	= NULL;		/* our program name */

/* argument variables */
LOCAL	char	*address	= NULL;		/* for ADDRESS */
LOCAL	char	bourne		= FALSE;	/* set bourne shell output */
LOCAL	char	clear		= FALSE;	/* clear variables */
LOCAL	int	debug		= NO_VALUE;	/* for DEBUG */
LOCAL	int	errno_to_print	= NO_VALUE;	/* to print the error string */
LOCAL	char	*inpath		= NULL;		/* for config-file path */
LOCAL	int	interval	= NO_VALUE;	/* for setting INTERVAL */
LOCAL	char	*logpath	= NULL;		/* for LOGFILE setting */
LOCAL	char	*start		= NULL;		/* for START settings */
LOCAL	char	*tag		= NULL;		/* the debug tag */

/*
 * hexadecimal STR to long translation
 */
LOCAL	int	hex_to_int(char * str)
{
  int		ret;
  
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
 * print a usage message for the program.
 */
LOCAL	void	usage(void)
{
  (void)fprintf(stderr,	"Usage: %s\n", program);
  (void)fprintf(stderr,	"  [-a address:#]    = stop when malloc sees ");
  (void)fprintf(stderr,   "address [for #th time] %%s\n");
  (void)fprintf(stderr,	"  [-b]              = set the output for bourne ");
  (void)fprintf(stderr,   "shells (sh, ksh) %%t\n");
  (void)fprintf(stderr,	"  [-c]              = clear all variables not ");
  (void)fprintf(stderr,   "specified %%t\n");
  (void)fprintf(stderr,	"  [-d bitmask]      = hex flag to directly set ");
  (void)fprintf(stderr,   "debug mask %%x\n");
  (void)fprintf(stderr,	"  [-e errno]        = print the error string for ");
  (void)fprintf(stderr,    "errno %%d\n");
  (void)fprintf(stderr,	"  [-f file]         = config file to read from ");
  (void)fprintf(stderr,    "when not ~/.mallocrc %%s\n");
  (void)fprintf(stderr, "  [-i number]       = check heap every number ");
  (void)fprintf(stderr,    "times %%d\n");
  (void)fprintf(stderr,	"  [-l file]         = file to log messages to %%s\n");
  (void)fprintf(stderr, "  [-s file:line]    = start checking heap after ");
  (void)fprintf(stderr,    "seeing file [and line] %%s\n");
  (void)fprintf(stderr, "  [tag]             = debug token to find in ");
  (void)fprintf(stderr,    "mallocrc\n");
  (void)fprintf(stderr, "if no arguments are specified then it dumps the ");
  (void)fprintf(stderr,    "current env setings.\n");
}

/*
 * process the command-line arguments
 * I've got a great library to do this automatically.  sigh.
 */
LOCAL	void	process_arguments(int argc, char ** argv)
{
  program = (char *)rindex(*argv, '/');
  if (program == NULL)
    program = *argv;
  else
    program++;
  
  argc--, argv++;
  
  for (; argc > 0; argc--, argv++) {
    
    /* special usage message */
    if (strcmp(*argv, USAGE_STRING) == 0) {
      usage();
      exit(0);
    }
    
    /* special version message */
    if (strcmp(*argv, VERSION_STRING) == 0) {
      (void)fprintf(stderr, "Version '%s'\n", malloc_version);
      exit(0);
    }
    
    /* if no - then assume it is the tag */
    if (**argv != '-') {
      if (tag != NULL) {
	(void)fprintf(stderr,
		      "Usage problem: debug-tag was already specified\n");
	usage();
	exit(1);
      }
      tag = *argv;
      continue;
    }
    
    /* can only handle -a not -ab */
    if ((*argv)[1] == NULLC || (*argv)[2] != NULLC) {
      (void)fprintf(stderr, "Usage problem: incorrect argument format '%s'\n",
		    *argv);
      usage();
      exit(1);
    }
    
    switch ((*argv)[1]) {
      
    case 'a':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -a\n");
	usage();
	exit(1);
      }
      address = *argv;
      break;
      
    case 'b':
      bourne = TRUE;
      break;
      
    case 'c':
      clear = TRUE;
      break;
      
    case 'd':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -d\n");
	usage();
	exit(1);
      }
      debug = hex_to_int(*argv);
      break;
      
    case 'e':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -e\n");
	usage();
	exit(1);
      }
      errno_to_print = atoi(*argv);
      break;
      
    case 'f':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -f\n");
	usage();
	exit(1);
      }
      inpath = *argv;
      break;
      
    case 'i':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -i\n");
	usage();
	exit(1);
      }
      interval = atoi(*argv);
      break;
      
    case 'l':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -l\n");
	usage();
	exit(1);
      }
      logpath = *argv;
      break;
      
    case 's':
      argc--, argv++;
      if (argc == 0) {
	(void)fprintf(stderr, "Usage problem: missing argument to -s\n");
	usage();
	exit(1);
      }
      start = *argv;
      break;
      
    default:
      (void)fprintf(stderr, "Usage problem: unknown argument '%s'\n", *argv);
      usage();
      exit(1);
      break;
    }
  }
}

/*
 * process the user configuration looking for the tag.  if tag is null then
 * look for DEBUG_VALUE in the file and return the token for it in STR.
 * routine returns the new debug value matching tag.
 */
LOCAL	int	process(int debug_value, char ** strp)
{
  static char	token[128];
  FILE		*infile = NULL;
  char		path[1024], buf[1024], *homep;
  char		found, cont;
  int		new_debug = 0;
  
  /* do we need to have a home variable? */
  if (inpath == NULL) {
    homep = (char *)getenv(HOME_ENVIRON);
    if (homep == NULL) {
      (void)fprintf(stderr, "%s: could not find variable '%s'\n",
		    program, HOME_ENVIRON);
      exit(1);
    }
    
    (void)sprintf(path, DEFAULT_CONFIG, homep);
    inpath = path;
  }
  
  infile = fopen(inpath, "r");
  if (infile == NULL) {
    (void)fprintf(stderr, "%s: could not read '%s': ", program, inpath);
    (void)perror("");
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
    
    tokp = (char *)strtok(buf, TOKENIZE_CHARS);
    
    /* if we're not continuing then we need to process a tag */
    if (! cont) {
      (void)strcpy(token, tokp);
      new_debug = 0;
      
      if (tag != NULL && strcmp(tag, tokp) == 0)
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
      if (found || tag == NULL) {
	for (attrc = 0; attributes[attrc].at_string != NULL; attrc++) {
	  if (strcmp(tokp, attributes[attrc].at_string) == 0)
	    break;
	}
	
	if (attributes[attrc].at_string == NULL) {
	  (void)fprintf(stderr, "%s: unknown token '%s'\n",
			program, tokp);
	}
	else
	  new_debug |= attributes[attrc].at_value;
      }
      
      tokp = (char *)strtok(NULL, TOKENIZE_CHARS);
    } while (tokp != NULL);
    
    if (tag == NULL && ! cont && new_debug == debug_value) {
      found = TRUE;
      if (strp != NULL)
	*strp = token;
      break;
    }
    
    /* are we done? */
    if (found && ! cont)
      break;
  }
  
  (void)fclose(infile);
  
  /* did we find the correct value in the file? */
  if (tag == NULL && ! found) {
    if (strp != NULL)
      *strp = "unknown";
  }
  else if (! found && tag != NULL) {
    (void)fprintf(stderr, "%s: could not find tag '%s' in '%s'\n",
		  program, tag, inpath);
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
  int		num;
  
  str = (char *)getenv(DEBUG_ENVIRON);
  if (str == NULL)
    (void)fprintf(stderr, "%s not set\n", DEBUG_ENVIRON);
  else {
    num = hex_to_int(str);
    (void)process(num, &str);
    (void)fprintf(stderr, "%s == '%#lx' (%s)\n", DEBUG_ENVIRON, num, str);
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
}

/*
 * output the code to set env VAR to VALUE
 */
LOCAL	void	set_variable(char * var, char * value)
{
  if (bourne)
    (void)printf("%s=%s; export %s;\n", var, value, var);
  else
    (void)printf("setenv %s %s;\n", var, value);
  
  printed = TRUE;
}

/*
 * output the code to un-set env VAR
 */
LOCAL	void	unset_variable(char * var)
{
  if (bourne)
    (void)printf("unset %s;\n", var);
  else
    (void)printf("unsetenv %s;\n", var);
  
  printed = TRUE;
}

EXPORT	int	main(int argc, char ** argv)
{
  char	buf[20];
  
  /* turn off debugging for this program */
  (void)malloc_debug(0);
  
  process_arguments(argc, argv);
  
  /* get a new debug value from tag */
  if (tag != NULL) {
    if (debug != NO_VALUE)
      (void)fprintf(stderr, "%s: warning -d ignored, processing tag '%s'\n",
		    program, tag);
    debug = process(0, NULL);
  }
  
  if (tag != NULL || debug != NO_VALUE) {
    (void)sprintf(buf, "%#lx", debug);
    set_variable(DEBUG_ENVIRON, buf);
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
  
  if (errno_to_print != NO_VALUE) {
    (void)fprintf(stderr, "%s: malloc_errno value '%d' = \n",
		  program, errno_to_print);
    (void)fprintf(stderr, "   '%s'\n", malloc_strerror(errno_to_print));
  }
  else if (! printed)
    dump_current();
  
  exit(0);
}
