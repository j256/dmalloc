/*
 * test program for malloc code
 *
 * Copyright 1991 by the Antaire Corporation
 * Please see the LICENSE file in this directory for license information
 *
 * malloc_t.c 1.12 11/27/91
 */

#define MALLOC_TEST_MAIN

#include <signal.h>

#include "useful.h"
#include "assert.h"
#include "ourstring.h"
#include "terminate.h"

#include "alloc.h"

SCCS_ID("@(#)malloc_t.c	1.12 GRAY@ANTAIRE.COM 11/27/91");

main(argc, argv)
  int	argc;
  char	**argv;
{
  int	size, count;
  long	pnt, magic;
  char	line[80];
  
  argc--, argv++;

  assert_on_signal(SIGSEGV);
  assert_on_signal(SIGBUS);
  assert_on_signal(SIGSYS);

  for (;;) {
    (void)printf("------------------------------------------------------\n");
    (void)printf("prompt> ");
    (void)gets(line);

    if (STREQL(line, "?") || STREQL(line, "help")) {
      (void)printf("------------------------------------------------------\n");
      (void)printf("HELP:\n\n");

      (void)printf("malloc    - allocate memory\n");
      (void)printf("free      - deallocate memory\n");
      (void)printf("realloc   - reallocate memory\n\n");

      (void)printf("overwrite - overwrite some memory to test errors\n");
      (void)printf("random    - randomly to a number of malloc/frees\n");
      (void)printf("verify    - check out a memory address\n\n");

      (void)printf("quit      - quit program\n");
      continue;
    }

    if (STREQL(line, "quit"))
      break;

    if (STREQL(line, "malloc")) {
      (void)printf("How much: ");
      (void)gets(line);
      size = atoi(line);
      (void)printf("malloc(%d) returned: %#lx\n", size, MALLOC(size));
      continue;
    }

    if (STREQL(line, "free")) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);
      (void)printf("free(%#lx) returned: %s\n",
		   pnt, (FREE(pnt) == FREE_NOERROR ? "success" : "failure"));
      continue;
    }
    
    /* do random heap hits */
    if (STREQL(line, "random")) {
      for (count = 1; count < 1000; count += 10)
	FREE(MALLOC(random() % (count * 10) + 1));
      continue;
    }

    if (STREQL(line, "realloc")) {
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

    if (STREQL(line, "map")) {
      malloc_heap_map();
      continue;
    }

    if (STREQL(line, "overwrite")) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);

      magic = 0x12345678;
      bcopy(&magic, pnt, sizeof(magic));
      continue;
    }

    if (STREQL(line, "verify")) {
      (void)printf("Hex address: ");
      (void)gets(line);
      pnt = htoi(line);

      (void)printf("malloc_verify(%#lx) returned: %s\n",
		   pnt, (malloc_verify(pnt) == TRUE ? "success" : "failure"));
      continue;
    }

    (void)printf("Unknown command.\n");
  }

  terminate(0);
}
