/*
 * Memory table routines
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
 * $Id: dmalloc_tab.c,v 1.6 1999/03/11 01:01:33 gray Exp $
 */

/*
 * This file contains routines used to maintain and display a memory
 * table by file and line number of memory usage.
 *
 * Inspired by code from PSM <psm @ sics.se>.  Thanks much.
 */

#if HAVE_STDLIB_H
# include <stdlib.h>				/* for qsort */
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#include "conf.h"
#include "chunk.h"
#include "compat.h"
#include "dmalloc.h"
#include "dmalloc_loc.h"
#include "error.h"
#include "error_val.h"

#include "dmalloc_tab.h"
#include "dmalloc_tab_loc.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: dmalloc_tab.c,v 1.6 1999/03/11 01:01:33 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: dmalloc_tab.c,v 1.6 1999/03/11 01:01:33 gray Exp $";
#endif
#endif

/*
 * local variables
 */
static	mem_table_t	other_pointers;		/* info of other pointers */
static	int		table_entry_c = 0;	/* num entries in table */
/* memory table entries */
static	mem_table_t	memory_table[MEM_ENTRIES_N];

/*
 * static unsigned int hash
 *
 * DESCRIPTION:
 *
 * Hash a variable-length key into a 32-bit value.  Every bit of the
 * key affects every bit of the return value.  Every 1-bit and 2-bit
 * delta achieves avalanche.  About (6 * len + 35) instructions.  The
 * best hash table sizes are powers of 2.  There is no need to use mod
 * (sooo slow!).  If you need less than 32 bits, use a bitmask.  For
 * example, if you need only 10 bits, do h = (h & hashmask(10)); In
 * which case, the hash table should have hashsize(10) elements.
 *
 * By Bob Jenkins, 1996.  bob_jenkins@compuserve.com.  You may use
 * this code any way you wish, private, educational, or commercial.
 * It's free.  See
 * http://ourworld.compuserve.com/homepages/bob_jenkins/evahash.htm
 * Use for hash table lookup, or anything where one collision in 2^^32
 * is acceptable.  Do NOT use for cryptographic purposes.
 *
 * RETURNS:
 *
 * Returns a 32-bit hash value.
 *
 * ARGUMENTS:
 *
 * key - Key (the unaligned variable-length array of bytes) that we
 * are hashing.
 *
 * length - Length of the key in bytes.
 *
 * init_val - Initialization value of the hash if you need to hash a
 * number of strings together.  For instance, if you are hashing N
 * strings (unsigned char **)keys, do it like this:
 *
 * for (i=0, h=0; i<N; ++i) h = hash( keys[i], len[i], h);
 */
static	unsigned int	hash(const unsigned char *key,
			     const unsigned int length,
			     const unsigned int init_val)
{
  const unsigned char	*key_p = key;
  unsigned int		a, b, c, len;
  
  /* set up the internal state */
  a = 0x9e3779b9;	/* the golden ratio; an arbitrary value */
  b = 0x9e3779b9;
  c = init_val;		/* the previous hash value */
  
  /* handle most of the key */
  for (len = length; len >= 12; len -= 12) {
    a += (key_p[0]
	  + ((unsigned int)key_p[1] << 8)
	  + ((unsigned int)key_p[2] << 16)
	  + ((unsigned int)key_p[3] << 24));
    b += (key_p[4]
	  + ((unsigned int)key_p[5] << 8)
	  + ((unsigned int)key_p[6] << 16)
	  + ((unsigned int)key_p[7] << 24));
    c += (key_p[8]
	  + ((unsigned int)key_p[9] << 8)
	  + ((unsigned int)key_p[10] << 16)
	  + ((unsigned int)key_p[11] << 24));
    HASH_MIX(a,b,c);
    key_p += 12;
  }
  
  c += length;
  
  /* all the case statements fall through to the next */
  switch(len) {
  case 11:
    c += ((unsigned int)key_p[10] << 24);
  case 10:
    c += ((unsigned int)key_p[9] << 16);
  case 9:
    c += ((unsigned int)key_p[8] << 8);
    /* the first byte of c is reserved for the length */
  case 8:
    b += ((unsigned int)key_p[7] << 24);
  case 7:
    b += ((unsigned int)key_p[6] << 16);
  case 6:
    b += ((unsigned int)key_p[5] << 8);
  case 5:
    b += key_p[4];
  case 4:
    a += ((unsigned int)key_p[3] << 24);
  case 3:
    a += ((unsigned int)key_p[2] << 16);
  case 2:
    a += ((unsigned int)key_p[1] << 8);
  case 1:
    a += key_p[0];
    /* case 0: nothing left to add */
  }
  HASH_MIX(a, b, c);
  
  return c;
}

#if QSORT_OKAY
/*
 * static int entry_cmp
 *
 * DESCRIPTION:
 *
 * Compare two entries in the memory table for quicksort.
 *
 * RETURNS:
 *
 * -1, 0, or 1 depending if entry1_p is less-than, equal, or
 * greater-than entry2_p.
 *
 * ARGUMENTS:
 *
 * entry1_p -> Pointer to the 1st entry.
 *
 * entry2_p -> Pointer to the 2nd entry.
 */
static	int	entry_cmp(const void *entry1_p, const void *entry2_p)
{
  unsigned long		size1, size2;
  const mem_table_t	*tab1_p = entry1_p, *tab2_p = entry2_p;
  
  /* if the entry is blank then force the size to be 0 */
  if (tab1_p->mt_file == NULL) {
    size1 = 0;
  }
  else {
    size1 = tab1_p->mt_total_size;
  }
  
  /* if the entry is blank then force the size to be 0 */
  if (tab2_p->mt_file == NULL) {
    size2 = 0;
  }
  else {
    size2 = tab2_p->mt_total_size;
  }
  
  /* NOTE: we want the top entries to be ahead so we reverse the sort */
  return size2 - size1;
}
#endif

/*
 * static void log_slot
 *
 * DESCRIPTION:
 *
 * Log the information from the memory slot to the logfile.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * tab_p -> Pointer to the memory table entry we are dumping.
 *
 * source -> Source description string.
 */
static	void	log_entry(const mem_table_t *tab_p, const int in_use_b,
			  const char *source)
{
  if (in_use_b) {
    _dmalloc_message("%11lu %6lu %11lu %6lu  %s\n",
		     tab_p->mt_total_size, tab_p->mt_total_c,
		     tab_p->mt_in_use_size, tab_p->mt_in_use_c, source);
  }
  else {
    _dmalloc_message("%11lu %6lu  %s\n",
		     tab_p->mt_total_size, tab_p->mt_total_c, source);
  }
}

/*
 * static void add_entry
 *
 * DESCRIPTION:
 *
 * Add a memory entry into our memory info total.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * total_p <-> Pointer to the memory table entry we are using to total
 * our used size.

 * tab_p -> Pointer to the memory table entry we are adding into our total.
 */
static	void	add_entry(mem_table_t *total_p, const mem_table_t *tab_p)
{
  total_p->mt_total_size += tab_p->mt_total_size;
  total_p->mt_total_c += tab_p->mt_total_c;
  total_p->mt_in_use_size += tab_p->mt_in_use_size;
  total_p->mt_in_use_c += tab_p->mt_in_use_c;
}

/*
 * void _table_clear
 *
 * DESCRIPTION:
 *
 * Clear out the allocation information in our table.  We are going to
 * be loading it with other info.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * None.
 */
void	_table_clear(void)
{
  /* clear out our memory table */
  memset(memory_table, 0, sizeof(mem_table_t) * MEM_ENTRIES_N);
  memset(&other_pointers, 0, sizeof(other_pointers));
  table_entry_c = 0;
}

/*
 * void _table_alloc
 *
 * DESCRIPTION:
 *
 * Register an allocation with the memory table.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * file -> File name or return address of the allocation. 
 *
 * line -> Line number of the allocation.
 *
 * size -> Size in bytes of the allocation.
 */
void	_table_alloc(const char *file, const unsigned int line,
		     const unsigned long size)
{
  unsigned int	bucket;
  mem_table_t	*tab_p, *tab_end_p, *tab_bounds_p;
  
  /*
   * Get our start bucket.  We calculate the hash value by referencing
   * the file/line information directly because of problems with
   * putting them in a structure.
   */
  bucket = hash((unsigned char *)file, strlen(file), 0);
  bucket = hash((unsigned char *)&line, sizeof(line), bucket);
  bucket %= MEM_ENTRIES_N;
  tab_p = memory_table + bucket;
  
  /* the end is if we come around to the start again */
  tab_end_p = tab_p;
  tab_bounds_p = memory_table + MEM_ENTRIES_N;
  
  while (1) {
    if (tab_p->mt_file == file && tab_p->mt_line == line) {
      /* we found it */
      break;
    }
    else if (tab_p->mt_file == NULL) {
      /* we didn't find it */
      break;
    }
    tab_p++;
    if (tab_p == tab_bounds_p) {
      tab_p = memory_table;
    }
    /* we shouldn't have gone through the entire table */
    if (tab_p == tab_end_p) {
      dmalloc_errno = ERROR_TABLE_CORRUPT;
      dmalloc_error("check_debug_vars");
      return;
    }
  }
  
  /* did we not find the pointer? */
  if (tab_p->mt_file == NULL) {
    
    if (table_entry_c >= MEMORY_TABLE_SIZE) {
      /* if the table is too full then add info into other_pointers bucket */
      tab_p = &other_pointers;
    }
    else {
      /* we found an open slot */
      tab_p->mt_file = file;
      tab_p->mt_line = line;
#if QSORT_OKAY
      /* remember its position in the table so we can unsort it later */
      tab_p->mt_entry_pos_p = tab_p;
#endif
      table_entry_c++;
    }
  }
  
  /* update the info for the entry */
  tab_p->mt_total_size += size;
  tab_p->mt_total_c++;
  tab_p->mt_in_use_size += size;
  tab_p->mt_in_use_c++;
}

/*
 * void _table_free
 *
 * DESCRIPTION:
 *
 * Register an allocation with the memory table.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * old_file -> File name or return address of the allocation _not_ the
 * free itself.
 *
 * old_line -> Line number of the allocation _not_ the free itself.
 *
 * size -> Size in bytes of the allocation.
 */
void	_table_free(const char *old_file, const unsigned int old_line,
		    const DMALLOC_SIZE size)
{
  unsigned int	bucket;
  int		found_b = 0;
  mem_table_t	*tab_p, *tab_end_p, *tab_bounds_p;
  
  /*
   * Get our start bucket.  We calculate the hash value by referencing
   * the file/line information directly because of problems with
   * putting them in a structure.
   */
  bucket = hash((unsigned char *)old_file, strlen(old_file), 0);
  bucket = hash((unsigned char *)&old_line, sizeof(old_line), bucket);
  bucket %= MEM_ENTRIES_N;
  tab_p = memory_table + bucket;
  
  /* the end is if we come around to the start again */
  tab_end_p = tab_p;
  tab_bounds_p = memory_table + MEM_ENTRIES_N;
  
  do {
    if (tab_p->mt_file == old_file && tab_p->mt_line == old_line) {
      /* we found it */
      found_b = 1;
      break;
    }
    else if (tab_p->mt_file == NULL) {
      /* we didn't find it */
      break;
    }
    tab_p++;
    if (tab_p == tab_bounds_p) {
      tab_p = memory_table;
    }
  } while (tab_p != tab_end_p);
  
  /* did we find the pointer? */
  if (! found_b) {
    
    /* assume that we should take it out of the other_pointers bucket */
    if (other_pointers.mt_in_use_size < size) {
      /* just skip the pointer, no use going negative */
      return;
    }
    
    /* we found an open slot */
    tab_p = &other_pointers;
  }
  
  /* update our pointer info if we can */
  if (tab_p->mt_in_use_size >= size && tab_p->mt_in_use_c > 0) {
    tab_p->mt_in_use_size -= size;
    tab_p->mt_in_use_c--;
  }
}

/*
 * void _table_log_info
 *
 * DESCRIPTION:
 *
 * Log information from the memory table to the log file.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * entry_n -> Number of entries to log to the file.  Set to 0 to
 * display all entries in the table.
 *
 * in_use_b -> Display the in-use numbers.
 */
void	_table_log_info(const int entry_n, const int in_use_b)
{
  mem_table_t	*tab_p, *tab_bounds_p, total;
  int		entry_c;
  char		source[64];
  
  /* is the table empty */
  if (table_entry_c == 0) {
    _dmalloc_message("memory table is empty");
    return;
  }
  
  /* sort the entry by the total size if we have qsort */
#if QSORT_OKAY
  qsort(memory_table, MEM_ENTRIES_N, sizeof(mem_table_t), entry_cmp);
#else
  _dmalloc_message("qsort not available, displaying first entries");
#endif
  
  /* display the column headers */  
  if (in_use_b) {
    _dmalloc_message(" total-size  count in-use-size  count  source");
  }
  else {
    _dmalloc_message(" total-size  count  source");
  }
  
  memset(&total, 0, sizeof(total));
  
  tab_bounds_p = memory_table + MEM_ENTRIES_N;
  entry_c = 0;
  for (tab_p = memory_table; tab_p < tab_bounds_p; tab_p++) {
    if (tab_p->mt_file != NULL) {
      entry_c++;
      /* can we still print the pointer information? */
      if (entry_n == 0 || entry_c < entry_n) {
	(void)_chunk_desc_pnt(source, sizeof(source),
			      tab_p->mt_file, tab_p->mt_line);
	log_entry(tab_p, in_use_b, source);
      }
      add_entry(&total, tab_p);
    }
  }
  if (table_entry_c >= MEMORY_TABLE_SIZE) {
    strncpy(source, "Other pointers", sizeof(source));
    source[sizeof(source) - 1] = '\0';
    log_entry(&other_pointers, in_use_b, source);
    add_entry(&total, &other_pointers);
  }
  
  /* dump our total */
  (void)loc_snprintf(source, sizeof(source), "Total of %d", entry_c);
  log_entry(&total, in_use_b, source);
  
#if QSORT_OKAY
  /*
   * If we sorted the array, we have to put it back the way it was if
   * we want to continue and handle memory transactions.  We should be
   * able to do this in O(N).
   */
  tab_bounds_p = memory_table + MEM_ENTRIES_N;
  for (tab_p = memory_table; tab_p < tab_bounds_p;) {
    mem_table_t		swap_entry;
    
    /*
     * If we have a blank slot or if the entry is in the right
     * position then we can move on to the next one
     */
    if (tab_p->mt_file == NULL
	|| tab_p->mt_entry_pos_p == tab_p) {
      tab_p++;
      continue;
    }
    
    /* swap this entry for where it should go and do _not_ increment tab_p */
    swap_entry = *tab_p->mt_entry_pos_p;
    *tab_p->mt_entry_pos_p = *tab_p;
    *tab_p = swap_entry;
  }
#endif
}
