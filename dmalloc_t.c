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

#include <stdio.h>				/* for stdin */

#define MALLOC_T_MAIN

#include "malloc.h"
#include "malloc_loc.h"

#include "conf.h"
#include "compat.h"

#if INCLUDE_RCS_IDS
LOCAL	char	*rcs_id =
  "$Id: dmalloc_t.c,v 1.9 1992/12/17 23:30:53 gray Exp $";
#endif

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
  int	size, count;
  long	pnt, magic;
  char	line[80], *linep;
  
  argc--, argv++;
  
  (void)srand(time(0) ^ 0xdeadbeef);
  
  (void)printf("------------------------------------------------------\n");
  (void)printf("Malloc test program.  Type 'help' for assistance.\n");
  
  for (;;) {
    (void)printf("------------------------------------------------------\n");
    (void)printf("prompt> ");
    (void)fgets(line, sizeof(line), stdin);
    linep = strrchr(line, '\n');
    if (linep != NULL)
      *linep = '\0';
    
    if (strcmp(line, "?") == 0
	|| strcmp(line, "help") == 0) {
      (void)printf("------------------------------------------------------\n");
      (void)printf("HELP:\n\n");
      
      (void)printf("help      - print this message\n\n");
      
      (void)printf("malloc    - allocate memory\n");
      (void)printf("free      - deallocate memory\n");
      (void)printf("realloc   - reallocate memory\n\n");
      
      (void)printf("map       - map the heap to the logfile\n");
      (void)printf("overwrite - overwrite some memory to test errors\n");
      (void)printf("random    - randomly to a number of malloc/frees\n");
      (void)printf("verify    - check out a memory address\n\n");
      
      (void)printf("quit      - quit this test program\n");
      continue;
    }
    
    if (strcmp(line, "quit") == 0)
      break;
    
    if (strcmp(line, "malloc") == 0) {
      (void)printf("How much: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      (void)printf("malloc(%d) returned: %#lx\n", size, (long)MALLOC(size));
      continue;
    }
    
    if (strcmp(line, "free") == 0) {
      pnt = get_address();
      (void)printf("free(%#lx) returned: %s\n",
		   pnt, (FREE(pnt) == FREE_NOERROR ? "success" : "failure"));
      continue;
    }
    
    if (strcmp(line, "realloc") == 0) {
      pnt = get_address();
      
      (void)printf("How much: ");
      (void)fgets(line, sizeof(line), stdin);
      size = atoi(line);
      
      (void)printf("realloc(%#lx, %d) returned: %#lx\n",
		   pnt, size, (long)REMALLOC(pnt, size));
      
      continue;
    }
    
    if (strcmp(line, "map") == 0) {
      (void)malloc_heap_map();
      continue;
    }
    
    if (strcmp(line, "overwrite") == 0) {
      pnt = get_address();
      
      magic = 0x12345678;
      bcopy(&magic, (char *)pnt, sizeof(magic));
      continue;
    }
    
    /* do random heap hits */
    if (strcmp(line, "random") == 0) {
      for (count = 1; count < 1000; count += 10) {
	int	amount;
	char	*data;
	
	amount = rand() % (count * 10) + 1;
	data = MALLOC(amount);
	(void)FREE(data);
      }
      continue;
    }
    
    if (strcmp(line, "verify") == 0) {
      pnt = get_address();
      
      (void)printf("malloc_verify(%#lx) returned: %s\n",
		   pnt,
		   (malloc_verify((char *)pnt) == MALLOC_VERIFY_NOERROR
		    ? "success" : "failure"));
      continue;
    }
    
    (void)printf("Unknown command.\n");
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
