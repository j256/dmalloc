/*
 * defines to get the return-address for non-dmalloc_lp malloc calls.
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
 *
 * $Id: return.h,v 1.9 1994/10/04 17:47:03 gray Exp $
 */

/*
 * this file contains the definition of the SET_RET_ADDR macro which
 * is designed to contain the archecture/compiler specific hacks to
 * determine the return-address from inside the malloc library.  With
 * this information, the library can display caller information from
 * calls that do not use the malloc_lp functions.
 *
 * Most of the archectures here have been contributed by other
 * individuals and may need to be toggled a bit to remove local
 * configuration differences.
 *
 * PLEASE send all submissions, comments, problems to the author.
 *
 * NOTE: examining the assembly code for x = __builtin_return_address(0);
 * with gcc version 2+ should give you a good start on building a hack
 * for your box.
 */

#ifndef __RETURN_H__
#define __RETURN_H__

/* from conf.h */
#if USE_RET_ADDRESS

/* for Sun SparcStations with GCC */
#if __sparc && __GNUC__ > 1

/*
 * NOTE: %i7 seems to be more reliable than the [%fp+4] used by
 * __builtin_return_address.  [%fp+4] is on the stack however, meaning
 * it may be better -- less prone to be erased.  However, it produces
 * some bogus data -- it seems to return the last return-address or
 * something like that.
 */
#define SET_RET_ADDR(file, line)	\
  do { \
    if (file == DMALLOC_DEFAULT_FILE) \
      asm("st %%i7,%0" : \
	  "=g" (file) : \
	  /* no inputs */ ); \
  } while(0)

#if 0
      asm("ld [%%fp+4],%%o0; st %%o0,%0" : \
	  "=g" (file) : \
	  /* no inputs */ : \
	  "o0");
#endif

#endif /* __sparc */

/* for i[34]86 machines with GCC */
#if __i386 && __GNUC__ > 1

#define SET_RET_ADDR(file, line)	\
  do { \
    if (file == DMALLOC_DEFAULT_FILE) \
      asm("movl 4(%%ebp),%%eax ; movl %%eax,%0" : \
	  "=g" (file) : \
	  /* no inputs */ : \
	  "eax"); \
  } while(0)

#endif /* __i386 */

/******************************* contributions *******************************/

/*
 * For DEC Alphas running OSF.  from Dave Hill <ddhill@zk3.dec.com>.
 */
#if __alpha

#include <c_asm.h>

#define SET_RET_ADDR(file, line)	\
  do { \
    if (file == DMALLOC_DEFAULT_FILE) \
      file = (char *)asm("bis %ra,%ra,%v0"); \
  } while(0)

#endif /* __alpha */

/*
 * for Data General workstations running DG/UX 5.4R3.00 from Joerg
 * Wunsch <joerg_wunsch@julia.tcd-dresden.de>.
 */
#ifdef __m88k__

/*
 * I have no ideas about the syntax of Motorola SVR[34] assemblers.
 * Also, there may be occasions where gcc does not set up a stack
 * frame for some function, so the returned value should be taken with
 * a grain of salt. For the ``average'' function calls it proved to be
 * correct anyway -- jw
 */
#if !__DGUX__ || _DGUXCOFF_TARGET
# define M88K_RET_ADDR	"ld %0,r30,4"
#else  /* __DGUX__ && !_DGUXCOFF_TARGET: DG/UX ELF with version 3 assembler */
# define M88K_RET_ADDR	"ld %0,#r30,4"
#endif

#define SET_RET_ADDR(file, line)	\
  do { \
    if (file == DMALLOC_DEFAULT_FILE) \
      asm(M88K_RET_ADDR : \
	  "=r" (file) : \
	  /* no inputs */); \
  } while(0)

#endif /* m88k */

#endif /* USE_RET_ADDRESS */

/********************************** default **********************************/

/* for all others, do nothing */
#ifndef SET_RET_ADDR
#define SET_RET_ADDR(file, line)
#endif

#endif /* ! __RETURN_H__ */
