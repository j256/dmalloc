/*
 * test program for malloc code
 *
 * Copyright 1991 by the Antaire Corporation
 *
 * malloc_t.c 2.1 12/23/91
 */

#define MALLOC_TEST_MAIN

#include "useful.h"
#include "alloc.h"

LOCAL	char	sccs_id[] = "@(#)malloc_t.c	2.1 GRAY@ANTAIRE.COM 12/23/91";

/* hexadecimal STR to integer translation */
LOCAL	int	htoi(str)
  char		*str;
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

main(argc, argv)
  int	argc;
  char	**argv;
{
  int	size, count;
  long	pnt, magic;
  char	line[80];
  
  argc--, argv++;
  
  (void)printf("------------------------------------------------------\n");
  (void)printf("Malloc test program.  Type 'help' for assistance.\n");
  
  for (;;) {
    (void)printf("------------------------------------------------------\n");
    (void)printf("prompt> ");
    (void)gets(line);
    
    if (strcmp(line, "?") == 0 || strcmp(line, "help") == 0) {
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
      (void)gets(line);
      size = atoi(line);
      (void)printf("malloc(%d) returned: %#lx\n", size, MALLOC(size));
      continue;
    }
    
    if (strcmp(line, "free") == 0) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);
      (void)printf("free(%#lx) returned: %s\n",
		   pnt, (FREE(pnt) == FREE_NOERROR ? "success" : "failure"));
      continue;
    }
    
    if (strcmp(line, "realloc") == 0) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);
      
      (void)printf("How much: ");
      (void)gets(line);
      size = atoi(line);
      
      (void)printf("realloc(%#lx, %d) returned: %#lx\n",
		   pnt, size, REMALLOC(pnt, size));
      
      continue;
    }
    
    if (strcmp(line, "map") == 0) {
      malloc_heap_map();
      continue;
    }
    
    if (strcmp(line, "overwrite") == 0) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);
      
      magic = 0x12345678;
      bcopy(&magic, pnt, sizeof(magic));
      continue;
    }
    
    /* do random heap hits */
    if (strcmp(line, "random") == 0) {
      for (count = 1; count < 1000; count += 10)
	FREE(MALLOC(random() % (count * 10) + 1));
      continue;
    }
    
    if (strcmp(line, "verify") == 0) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);
      
      (void)printf("malloc_verify(%#lx) returned: %s\n",
		   pnt,
		   (malloc_verify(pnt) == MALLOC_VERIFY_NOERROR ? "success" :
		    "failure"));
      continue;
    }
    
    (void)printf("Unknown command.\n");
  }
  
  /* shutdown the alloc routines */
  alloc_shutdown();
  
  (void)printf("------------------------------------------------------\n");
  (void)printf("final malloc_verify returned: %s\n",
	       (malloc_verify(NULL) == MALLOC_VERIFY_NOERROR ? "success" :
		"failure"));
  (void)printf("------------------------------------------------------\n");
  
  (void)exit(0);
}
