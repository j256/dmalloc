/*
 * Memory protection functions
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
 * This file contains memory protection calls which allow dmalloc to
 * protect its administrative information until it needs to use it.
 */

#include <ctype.h>

#if HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#if HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#endif

#define DMALLOC_DISABLE

#include "conf.h"

#include "dmalloc.h"
#include "dmalloc_loc.h"
#include "error.h"
#include "heap.h"
#include "protect.h"

/*
 * void _dmalloc_protect_set_read_only
 *
 * Set the protections on a block to be read-only.
 *
 * ARGUMENTS:
 *
 * mem -> Pointer to block that we are protecting.
 *
 * block_n -> Number of blocks that we are protecting.
 */
void	_dmalloc_protect_set_read_only(void *mem, const int block_n)
{
#if PROTECT_ALLOWED && PROTECT_BLOCKS
  int	size = block_n * BLOCK_SIZE;
  void	*block_pnt;
  
  if (ON_BLOCK(mem)) {
    block_pnt = mem;
  }
  else {
    block_pnt = BLOCK_ROUND(mem);
  }
  
  if (mprotect(block_pnt, size, PROT_READ) != 0) {
    dmalloc_message("mprotect on '%#p' size %d failed", block_pnt, size);
  }
#endif
}

/*
 * void _dmalloc_protect_set_read_write
 *
 * Set the protections on a block to be read-write.
 *
 * ARGUMENTS:
 *
 * mem -> Pointer to block that we are protecting.
 *
 * block_n -> Number of blocks that we are protecting.
 */
void	_dmalloc_protect_set_read_write(void *mem, const int block_n)
{
#if PROTECT_ALLOWED && PROTECT_BLOCKS
  int	prot, size = block_n * BLOCK_SIZE;
  void	*block_pnt;
  
  if (ON_BLOCK(mem)) {
    block_pnt = mem;
  }
  else {
    block_pnt = BLOCK_ROUND(mem);
  }
  
  /*
   * We set executable if possible in case the user has allocated
   * stack space or some such
   */
  prot = PROT_READ | PROT_WRITE;
#ifdef PROT_EXEC
  prot |= PROT_EXEC;
#endif
  if (mprotect(block_pnt, size, prot) != 0) {
    dmalloc_message("mprotect on '%#p' size %d failed", block_pnt, size);
  }
#endif
}

/*
 * void _dmalloc_protect_set_no_access
 *
 * Set the protections on a block to be no-access.
 *
 * ARGUMENTS:
 *
 * mem -> Pointer to block that we are protecting.
 *
 * block_n -> Number of blocks that we are protecting.
 */
void	_dmalloc_protect_set_no_access(void *mem, const int block_n)
{
#if PROTECT_ALLOWED && PROTECT_BLOCKS
  int	size = block_n * BLOCK_SIZE;
  void	*block_pnt;
  
  if (ON_BLOCK(mem)) {
    block_pnt = mem;
  }
  else {
    block_pnt = BLOCK_ROUND(mem);
  }
  
  if (mprotect(block_pnt, size, PROT_NONE) != 0) {
    dmalloc_message("mprotect on '%#p' size %d failed", block_pnt, size);
  }
#endif
}
