/*
 * test program for malloc code
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
 * Test program for the malloc library.  Current it is interactive although
 * probably should be script based.
 */

#include <stdio.h>				/* for stdin */

#include "malloc.h"
#include "malloc_loc.h"

#include "conf.h"
#include "compat.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: dmalloc_t.c,v 1.12 1993/03/31 00:36:04 gray Exp $";
#endif

#define DEFAULT_ITERATIONS	1000

/*
 * hexadecimal STR to integer translation
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
 * read an address from the user
 */
LOCAL	long	get_address(void)
{
  char	line[80];
  long	pnt;
  
  do {
    (void)printf("Enter a hex address: ");
    (void)fgets(line, sizeof(line), stdin);
  } while (line[0] == '\0');
  
  pnt = hex_to_int(line);
  
  return pnt;
}

EXPORT	int	main(int argc, char ** argv)
{
  argc--, argv++;
  
  (void)srand(time(0) ^ 0xDEADBEEF);
  
  (void)printf("------------------------------------------------------\n");
  (void)printf("Malloc test program.  Type 'help' for assistance.\n");
  
  for (;;) {
    int		len;
    char	line[128], *linep;
    
    (void)printf("------------------------------------------------------\n");
    (void)printf("prompt> ");
    (void)fgets(line, sizeof(line), stdin);
    linep = strrchr(line, '\n');
    if (linep != NULL)
      *linep = '\0';
    
    len = strlen(line);
    if (len == 0)
      continue;
    
    if (strncmp(line, "?", len) == 0
	|| strncmp(line, "help", len) == 0) {
      (void)printf("------------------------------------------------------\n");
      (void)printf("HELP:\n\n");
      
      (void)printf("help      - print this message\n\n");
      
      (void)printf("malloc    - allocate memory\n");
      (void)printf("free      - deallocate memory\n");
      (void)printf("realloc   - reallocate memory\n\n");
      
      (void)printf("map       - map the heap to the logfile\n");
      (void)printf("overwrite - overwrite some memory to test errors\n");
      (void)printf("random    - randomly execute a number of malloc/frees\n");
      (void)printf("verify    - check out a memory address\n\n");
      
      (void)printf("quit      - quit this test program\n");
      continue;
    }
    
    if (strncmp(line, "quit", len) == 0)
      break;
    
    if (strncmp(line, "malloc", len) == 0) {
      int	size;
      
      (void)printf("How much: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      (void)printf("malloc(%d) returned: %#lx\n", size, (long)MALLOC(size));
      continue;
    }
    
    if (strncmp(line, "free", len) == 0) {
      long	pnt;
      
      pnt = get_address();
      (void)printf("free(%#lx) returned: %s\n",
		   pnt, (FREE(pnt) == FREE_NOERROR ? "success" : "failure"));
      continue;
    }
    
    if (strncmp(line, "realloc", len) == 0) {
      int	size;
      long	pnt;
      
      pnt = get_address();
      
      (void)printf("How much: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      
      (void)printf("realloc(%#lx, %d) returned: %#lx\n",
		   pnt, size, (long)REMALLOC(pnt, size));
      
      continue;
    }
    
    if (strncmp(line, "map", len) == 0) {
      (void)malloc_heap_map();
      (void)printf("Done.\n");
      continue;
    }
    
    if (strncmp(line, "overwrite", len) == 0) {
      long	pnt, magic;
      
      pnt = get_address();
      
      magic = 0x12345678;
      bcopy(&magic, (char *)pnt, sizeof(magic));
      
      (void)printf("Done.\n");
      continue;
    }
    
    /* do random heap hits */
    if (strncmp(line, "random", len) == 0) {
      int	count, max;
      
      (void)printf("How many iterations[%d]: ", DEFAULT_ITERATIONS);
      (void)fgets(line, sizeof(line), stdin);
      if (line[0] == '\0' || line[0] == '\n')
	max = DEFAULT_ITERATIONS;
      else
	max = atoi(line);
      
      for (count = 1; count < max; count += 10) {
	int	amount;
	char	*data;
	
	amount = rand() % (count * 10) + 1;
	data = MALLOC(amount);
	(void)FREE(data);
      }
      
      (void)printf("Done.\n");
      continue;
    }
    
    if (strncmp(line, "verify", len) == 0) {
      long	pnt;
      int	ret;
      
      pnt = get_address();
      
      ret = malloc_verify((char *)pnt);
      (void)printf("malloc_verify(%#lx) returned: %s\n",
		   pnt,
		   (ret == MALLOC_VERIFY_NOERROR ? "success" : "failure"));
      continue;
    }
    
    (void)printf("Unknown command '%s'.\n", line);
    (void)printf("Type 'help' for assistance.\n");
  }
  
  /* shutdown the alloc routines */
  malloc_shutdown();
  
  (void)printf("------------------------------------------------------\n");
  (void)printf("final malloc_verify returned: %s\n",
	       (malloc_verify(NULL) == MALLOC_VERIFY_NOERROR ? "success" :
		"failure"));
  (void)printf("------------------------------------------------------\n");
  
  exit(0);
}
