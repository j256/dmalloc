/*
 * Local defines for the memory table.
 *
 * Copyright 1999 by Gray Watson
 *
 * This file is part of the dmalloc package.
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
 * The author may be contacted via http://www.dmalloc.com/
 *
 * $Id: dmalloc_tab_loc.h,v 1.3 1999/03/11 00:57:28 gray Exp $
 */

#ifndef __DMALLOC_TAB_LOC_H__
#define __DMALLOC_TAB_LOC_H__

#include "conf.h"

#define MEM_ENTRIES_N	(MEMORY_TABLE_SIZE * 2)

/* entry in our memory table */
typedef struct mem_table_st {
  const char		*mt_file;		/* filename of alloc or ra */
  unsigned int		mt_line;		/* line number of alloc */
  unsigned long		mt_total_size;		/* size bytes alloced */
  unsigned long		mt_total_c;		/* total pointers allocated */
  unsigned long		mt_in_use_size;		/* size currently alloced */
  unsigned long		mt_in_use_c;		/* pointers currently in use */
#if HAVE_QSORT
  /* we use this so we can easily un-sort the list */
  struct mem_table_st	*mt_entry_pos_p;	/* pos of entry in table */
#endif
} mem_table_t;

/*
 * void HASH_MIX
 *
 * DESCRIPTION:
 *
 * Mix 3 32-bit values reversibly.  For every delta with one or two
 * bits set, and the deltas of all three high bits or all three low
 * bits, whether the original value of a,b,c is almost all zero or is
 * uniformly distributed.
 *
 * If HASH_MIX() is run forward or backward, at least 32 bits in a,b,c
 * have at least 1/4 probability of changing.  If mix() is run
 * forward, every bit of c will change between 1/3 and 2/3 of the
 * time.  (Well, 22/100 and 78/100 for some 2-bit deltas.)
 *
 * HASH_MIX() takes 36 machine instructions, but only 18 cycles on a
 * superscalar machine (like a Pentium or a Sparc).  No faster mixer
 * seems to work, that's the result of my brute-force search.  There
 * were about 2^68 hashes to choose from.  I only tested about a
 * billion of those.
 */
#define HASH_MIX(a, b, c) \
 do { \
   a -= b; a -= c; a ^= (c >> 13); \
   b -= c; b -= a; b ^= (a << 8); \
   c -= a; c -= b; c ^= (b >> 13); \
   a -= b; a -= c; a ^= (c >> 12); \
   b -= c; b -= a; b ^= (a << 16); \
   c -= a; c -= b; c ^= (b >> 5); \
   a -= b; a -= c; a ^= (c >> 3); \
   b -= c; b -= a; b ^= (a << 10); \
   c -= a; c -= b; c ^= (b >> 15); \
 } while(0)

#endif /* ! __DMALLOC_TAB_LOC_H__ */
