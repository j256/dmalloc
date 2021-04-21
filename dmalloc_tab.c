/*
 * Memory table routines
 *
 * Copyright 2020 by Gray Watson
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
 * The author may be contacted via http://dmalloc.com/
 */

/*
 * This file contains routines used to maintain and display a memory
 * table by file and line number of memory usage.
 *
 * Inspired by code from PSM.  Thanks much.
 */

#if HAVE_STDLIB_H
# include <stdlib.h>				/* for qsort */
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#include "conf.h"
#include "append.h"
#include "chunk.h"
#include "compat.h"
#include "dmalloc.h"
#include "dmalloc_loc.h"

#include "dmalloc_tab.h"
#include "dmalloc_tab_loc.h"

/*
 * static unsigned int hash
 *
 * Hash a variable-length key into a 32-bit value.  Every bit of the
 * key affects every bit of the return value.  Every 1-bit and 2-bit
 * delta achieves avalanche.  About (6 * len + 35) instructions.  The
 * best hash table sizes are powers of 2.  There is no need to use mod
 * (sooo slow!).  If you need less than 32 bits, use a bitmask.  For
 * example, if you need only 10 bits, do h = (h & hashmask(10)); In
 * which case, the hash table should have hashsize(10) elements.
 *
 * By Bob Jenkins, 1996.  You may use this code any way you wish,
 * private, educational, or commercial.  It's free.  See
 * http://ourworld.compuserve.com/homepages/bob_jenkins/evahash.htm
 * Use for hash table lookup, or anything where one collision in 2^^32
 * is acceptable.  Do NOT use for cryptographic purposes.
 *
 * Returns a 32-bit hash value.
 *
 * ARGUMENTS:
 *
 * key -> Key (the unaligned variable-length array of bytes) that we
 * are hashing.
 *
 * length -> Length of the key in bytes.
 *
 * init_val -> Initialization value of the hash if you need to hash a
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

/*
 * static unsigned int which_bucket
 *
 * Determine the bucket with our file/line and hash function.
 *
 * Returns a bucket number.
 *
 * ARGUMENTS:
 *
 * entry_n -> Number of entries in the memory table.
 *
 * file -> File name or return address of the allocation. 
 *
 * line -> Line number of the allocation.
 */
static	unsigned int	which_bucket(const int entry_n, const char *file,
				     const unsigned int line)
{
  unsigned int	bucket;
  
  if (line == 0) {
    if (file == NULL) {
      bucket = 0;
    }
    else {
      /* we hash on the address in file -- should be a return-address */
      bucket = hash((unsigned char *)&file, sizeof(char *), 0);
    }
  }
  else {
    bucket = hash((unsigned char *)file, strlen(file), 0);
    bucket = hash((unsigned char *)&line, sizeof(line), bucket);
  }
  
  bucket %= entry_n;
  return bucket;
}

/*
 * static int entry_cmp
 *
 * Compare two entries in the memory table for quicksort.
 *
 * Returns -1, 0, or 1 depending if entry1_p is less-than, equal, or
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
  const mem_entry_t	*tab1_p = entry1_p, *tab2_p = entry2_p;
  
  /* if the entry is blank then force the size to be 0 */
  if (tab1_p->me_file == NULL) {
    size1 = 0;
  }
  else {
    size1 = tab1_p->me_total_size;
  }
  
  /* if the entry is blank then force the size to be 0 */
  if (tab2_p->me_file == NULL) {
    size2 = 0;
  }
  else {
    size2 = tab2_p->me_total_size;
  }
  
  /* NOTE: we want the top entries to be ahead so we reverse the sort */
  if (size1 > size2) {
    return -1;
  }
  else if (size1 == size2) {
    return 0;
  }
  else {
    return 1;
  }
}

/*
 * static void swap_bytes
 *
 * Swap the values between two items of a specified size.
 *
 * ARGUMENTS:
 *
 * item1_p -> Pointer to the first item.
 *
 * item2_p -> Pointer to the first item.
 *
 * ele_size -> Size of the two items.
 */
static	void	swap_bytes(unsigned char *item1_p, unsigned char *item2_p,
			   int ele_size)
{
  unsigned char	char_temp;
  
  for (; ele_size > 0; ele_size--) {
    char_temp = *item1_p;
    *item1_p = *item2_p;
    *item2_p = char_temp;
    item1_p++;
    item2_p++;
  }
}

/*
 * static void insert_sort
 *
 * Do an insertion sort which is faster for small numbers of items and
 * better if the items are already sorted.
 *
 * ARGUMENTS:
 *
 * first_p <-> Start of the list that we are splitting.
 *
 * last_p <-> Last entry in the list that we are splitting.
 *
 * holder_p <-> Location of hold area we can store an entry.
 *
 * ele_size -> Size of the each element in the list.
 */
static	void	insert_sort(unsigned char *first_p, unsigned char *last_p,
			    unsigned char *holder_p,
			    const unsigned int ele_size)
{
  unsigned char	*inner_p, *outer_p;
  
  for (outer_p = first_p + ele_size; outer_p <= last_p; ) {
    
    /* look for the place to insert the entry */
    for (inner_p = outer_p - ele_size;
	 inner_p >= first_p && entry_cmp(outer_p, inner_p) < 0;
	 inner_p -= ele_size) {
    }
    inner_p += ele_size;
    
    /* do we need to insert the entry in? */
    if (outer_p != inner_p) {
      /*
       * Now we shift the entry down into its place in the already
       * sorted list.
       */
      memcpy(holder_p, outer_p, ele_size);
      memmove(inner_p + ele_size, inner_p, outer_p - inner_p);
      memcpy(inner_p, holder_p, ele_size);
    }
    
    outer_p += ele_size;
  }
}

/*
 * static void split
 *
 * This sorts an array of longs via the quick sort algorithm (it's pretty quick)
 *
 * ARGUMENTS:
 *
 * first_p -> Start of the list that we are splitting.
 *
 * last_p -> Last entry in the list that we are splitting.
 *
 * ele_size -> Size of the each element in the list.
 */
static	void	split(unsigned char *first_p, unsigned char *last_p,
		      const unsigned int ele_size)
{
  unsigned char	*left_p, *right_p, *pivot_p, *left_last_p, *right_first_p;
  unsigned char	*firsts[MAX_QSORT_SPLITS], *lasts[MAX_QSORT_SPLITS];
  mem_table_t	pivot;
  unsigned int	width, split_c = 0, min_qsort_size, size1, size2;
  
  min_qsort_size = MAX_QSORT_PARTITION * ele_size;
  
  while (1) {
    
    /* find the left, right, and mid point */
    left_p = first_p;
    right_p = last_p;
    /* is there a faster way to find this? */
    width = (last_p - first_p) / ele_size;
    pivot_p = first_p + ele_size * (width >> 1);
    
    /*
     * Find which of the left, middle, and right elements is the
     * median (Knuth vol3 p123).
     */
    if (entry_cmp(first_p, pivot_p) > 0) {
      swap_bytes(first_p, pivot_p, ele_size);
    }
    if (entry_cmp(pivot_p, last_p) > 0) {
      swap_bytes(pivot_p, last_p, ele_size);
      if (entry_cmp(first_p, pivot_p) > 0) {
	swap_bytes(first_p, pivot_p, ele_size);
      }
    }
    
    /*
     * save our pivot so we don't have to worry about hitting and
     * swapping it elsewhere while we iterate across the list below.
     */
    memcpy(&pivot, pivot_p, ele_size);
    
    do {
      
      /* shift the left side up until we reach the pivot value */
      while (entry_cmp(left_p, &pivot) < 0) {
	left_p += ele_size;
      }
      /* shift the right side down until we reach the pivot value */
      while (entry_cmp(&pivot, right_p) < 0) {
	right_p -= ele_size;
      }
      
      /* if we met in the middle then we are done */
      if (left_p == right_p) {
	left_p += ele_size;
	right_p -= ele_size;
	break;
      }
      else if (left_p < right_p) {
	/*
	 * swap the left and right since they both were on the wrong
	 * size of the pivot and continue
	 */
	swap_bytes(left_p, right_p, ele_size);
	left_p += ele_size;
	right_p -= ele_size;
      }
    } while (left_p <= right_p);
    
    /* Rename variables to make more sense.  This will get optimized out. */
    right_first_p = left_p;
    left_last_p = right_p;
    
    /* determine the size of the left and right hand parts */
    size1 = left_last_p - first_p;
    size2 = last_p - right_first_p;
    
    /* is the 1st half small enough to just insert-sort? */
    if (size1 < min_qsort_size) {
      
      /* use the pivot as our temporary space */
      insert_sort(first_p, left_last_p, (unsigned char *)&pivot, ele_size);
      
      /* is the 2nd part small as well? */
      if (size2 < min_qsort_size) {
	
	/* use the pivot as our temporary space */
	insert_sort(right_first_p, last_p, (unsigned char *)&pivot, ele_size);
	
	/* pop a partition off our stack */
	if (split_c == 0) {
	  /* we are done */
	  return;
	}
	split_c--;
	first_p = firsts[split_c];
	last_p = lasts[split_c];
      }
      else {
	/* we can just handle the right side immediately */
	first_p = right_first_p;
	/* last_p = last_p */
      }
    }
    else if (size2 < min_qsort_size) {
      
      /* use the pivot as our temporary space */
      insert_sort(right_first_p, last_p, (unsigned char *)&pivot, ele_size);
      
      /* we can just handle the left side immediately */
      /* first_p = first_p */
      last_p = left_last_p;
    }
    else {
      /*
       * neither partition is small, we'll have to push the larger one
       * of them on the stack
       */
      if (split_c >= MAX_QSORT_SPLITS) {
	/* sanity check here -- we should never get here */
	abort();
      }
      if (size1 > size2) {
	/* push the left partition on the stack */
	firsts[split_c] = first_p;
	lasts[split_c] = left_last_p;
	split_c++;
	/* continue handling the right side */
	first_p = right_first_p;
	/* last_p = last_p */
      }
      else {
	/* push the right partition on the stack */
	firsts[split_c] = right_first_p;
	lasts[split_c] = last_p;
	split_c++;
	/* continue handling the left side */
	/* first_p = first_p */
	last_p = left_last_p;
      }
    }
  }
}

/*
 * static void log_slot
 *
 * Log the information from the memory slot to the logfile.
 *
 * ARGUMENTS:
 *
 * entry_p -> Pointer to the memory table entry we are dumping.
 *
 * in_use_column_b -> Display the in-use numbers in a column.
 *
 * source -> Source description string.
 */
static	void	log_entry(const mem_entry_t *entry_p,
			  const int in_use_column_b, const char *source)
{
  if (in_use_column_b) {
    dmalloc_message("%11lu %6lu %11lu %6lu  %s\n",
		    entry_p->me_total_size, entry_p->me_total_c,
		    entry_p->me_in_use_size, entry_p->me_in_use_c, source);
  }
  else {
    dmalloc_message("%11lu %6lu  %s\n",
		     entry_p->me_total_size, entry_p->me_total_c, source);
  }
}

/*
 * static void add_entry
 *
 * Add a memory entry into our memory info total.
 *
 * ARGUMENTS:
 *
 * total_p <-> Pointer to the memory table entry we are using to total
 * our used size.

 * entry_p -> Pointer to the memory table entry we are adding into our total.
 */
static	void	add_entry(mem_entry_t *total_p, const mem_entry_t *entry_p)
{
  total_p->me_total_size += entry_p->me_total_size;
  total_p->me_total_c += entry_p->me_total_c;
  total_p->me_in_use_size += entry_p->me_in_use_size;
  total_p->me_in_use_c += entry_p->me_in_use_c;
}

/*
 * void _dmalloc_table_init
 *
 * Clear out the allocation information in our table.  We are going to
 * be loading it with other info.
 *
 * ARGUMENTS:
 *
 * mem_table -> Memory table we are working on.
 *
 * mem_entries -> Entries to associate with the table.
 *
 * entry_n -> Number of entries in the mem_entries array.
 */
void	_dmalloc_table_init(mem_table_t *mem_table, mem_entry_t *mem_entries,
			    const int entry_n)
{
  /* clear out our memory table */
  memset(mem_table, 0, sizeof(*mem_table));
  memset(mem_entries, 0, sizeof(*mem_entries) * entry_n);
  
  mem_table->mt_entries = mem_entries;
  mem_table->mt_entry_n = entry_n;
  mem_table->mt_bounds_p = mem_entries + mem_table->mt_entry_n;
}

/*
 * static mem_entry_t *table_find
 *
 * Find an extry in the table.
 *
 * ARGUMENTS:
 *
 * mem_table -> Memory table we are working on.
 *
 * old_file -> File name or return address of the allocation to
 * delete.
 *
 * old_line -> Line number of the allocation to delete.
 */
static mem_entry_t	*table_find(mem_table_t *mem_table,
				    const char *old_file,
				    const unsigned int old_line)
{
  unsigned int	bucket;
  mem_entry_t	*entry_p, *tab_end_p;
  
  bucket = which_bucket(mem_table->mt_entry_n, old_file, old_line);
  entry_p = mem_table->mt_entries + bucket;
  
  /* the end is if we come around to the start again */
  tab_end_p = entry_p;
  
  do {
    if (entry_p->me_file == old_file && entry_p->me_line == old_line) {
      return entry_p;
    }
    else if (entry_p->me_file == NULL) {
      /* blank entry */
      return entry_p;
    }
    entry_p++;
    if (entry_p == mem_table->mt_bounds_p) {
      entry_p = mem_table->mt_entries;
    }
  } while (entry_p != tab_end_p);
  
  return &mem_table->mt_other_pointers;
}

/*
 * void _dmalloc_table_insert
 *
 * Insert a pointer to the table.
 *
 * ARGUMENTS:
 *
 * mem_table -> Memory table we are working on.
 *
 * file -> File name or return address of the allocation.
 *
 * line -> Line number of the allocation.
 *
 * size -> Size in bytes of the allocation.
 */
void	_dmalloc_table_insert(mem_table_t *mem_table,
			      const char *file, const unsigned int line,
			      const unsigned long size)
{
  mem_entry_t	*entry_p;
  
  entry_p = table_find(mem_table, file, line);
  if (entry_p->me_file == NULL
      && mem_table->mt_in_use_c > mem_table->mt_entry_n / 2) {
    /* do we have too many entries in the table?  then put in other bucket. */
    entry_p = &mem_table->mt_other_pointers;
  } else if (entry_p != &mem_table->mt_other_pointers) {
    /* we found an open slot so update the file/line */
    entry_p->me_file = file;
    entry_p->me_line = line;
    mem_table->mt_in_use_c++;
  }
  
  /* update the info for the entry */
  entry_p->me_total_size += size;
  entry_p->me_total_c++;
  entry_p->me_in_use_size += size;
  entry_p->me_in_use_c++;
  entry_p->me_entry_pos_p = entry_p;
}

/*
 * void _dmalloc_table_delete
 *
 * Remove a pointer from the table.
 *
 * ARGUMENTS:
 *
 * mem_table -> Memory table we are working on.
 *
 * old_file -> File name or return address of the allocation to
 * delete.
 *
 * old_line -> Line number of the allocation to delete.
 *
 * size -> Size in bytes of the allocation.
 */
void	_dmalloc_table_delete(mem_table_t *mem_table, const char *old_file,
			      const unsigned int old_line,
			      const DMALLOC_SIZE size)
{
  mem_entry_t	*entry_p;
  
  entry_p = table_find(mem_table, old_file, old_line);
  if (entry_p->me_file == NULL) {
    /* if we didn't find it, account for it in the other_pointers?? */
    entry_p = &mem_table->mt_other_pointers;
  }
  /* update our pointer info if we can */
  if (entry_p->me_in_use_size >= size && entry_p->me_in_use_c > 0) {
    entry_p->me_in_use_size -= size;
    entry_p->me_in_use_c--;
  }
}

/*
 * void _dmalloc_table_log_info
 *
 * Log information from the memory table to the log file.
 *
 * ARGUMENTS:
 *
 * mem_table -> Memory table we are working on.
 *
 * log_n -> Number of entries to log to the file.  Set to 0 to
 * display all entries in the table.
 *
 * in_use_column_b -> Display the in-use numbers in a column.
 */
void	_dmalloc_table_log_info(mem_table_t *mem_table, const int log_n,
				const int in_use_column_b)
{
  mem_entry_t	*entry_p, total;
  int		entry_c;
  char		source[64];
  
  /* is the table empty */
  if (mem_table->mt_in_use_c == 0) {
    dmalloc_message(" memory table is empty");
    return;
  }
  
  /* sort the entries by their total-size */
  split((unsigned char *)mem_table->mt_entries,
	(unsigned char *)(mem_table->mt_bounds_p - 1),
	sizeof(*mem_table->mt_entries));
  
  /* display the column headers */  
  if (in_use_column_b) {
    dmalloc_message(" total-size  count in-use-size  count  source");
  }
  else {
    dmalloc_message(" total-size  count  source");
  }
  
  memset(&total, 0, sizeof(total));
  
  entry_c = 0;
  for (entry_p = mem_table->mt_entries;
       entry_p < mem_table->mt_bounds_p;
       entry_p++) {
    if (entry_p->me_file != NULL) {
      entry_c++;
      /* can we still print the pointer information? */
      if (log_n == 0 || entry_c < log_n) {
	(void)_dmalloc_chunk_desc_pnt(source, sizeof(source),
				      entry_p->me_file, entry_p->me_line);
	log_entry(entry_p, in_use_column_b, source);
      }
      add_entry(&total, entry_p);
    }
  }
  if (mem_table->mt_other_pointers.me_total_c) {
    strncpy(source, "Other pointers", sizeof(source));
    source[sizeof(source) - 1] = '\0';
    log_entry(&mem_table->mt_other_pointers, in_use_column_b, source);
    add_entry(&total, &mem_table->mt_other_pointers);
  }
  
  /* dump our total */
  (void)loc_snprintf(source, sizeof(source), "Total of %d", entry_c);
  log_entry(&total, in_use_column_b, source);
  
  /*
   * If we sorted the array, we have to put it back the way it was if
   * we want to continue and handle memory transactions.  We should be
   * able to do this in O(N).
   */
  for (entry_p = mem_table->mt_entries; entry_p < mem_table->mt_bounds_p;) {
    mem_entry_t		swap_entry;
    
    /*
     * If we have a blank slot or if the entry is in the right
     * position then we can move on to the next one
     */
    if (entry_p->me_file == NULL
	|| entry_p->me_entry_pos_p == entry_p) {
      entry_p++;
      continue;
    }
    
    /* swap this entry for where it should go and do _not_ increment entry_p */
    swap_entry = *entry_p->me_entry_pos_p;
    *entry_p->me_entry_pos_p = *entry_p;
    *entry_p = swap_entry;
  }
}
