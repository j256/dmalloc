/*
 * Memory protection functions
 *
 * Copyright 2000 by Gray Watson
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
 *
 * $Id: protect.c,v 1.1 2000/05/15 15:46:40 gray Exp $
 */

/*
 * This file contains memory protection calls which allow dmalloc to
 * protect its administrative information until it needs to use it.
 */

#include <ctype.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#include <sys/mman.h>

#define DMALLOC_DISABLE

#include "conf.h"

#include "dmalloc.h"
#include "dmalloc_loc.h"

#if INCLUDE_RCS_IDS
#ifdef __GNUC__
#ident "$Id: protect.c,v 1.1 2000/05/15 15:46:40 gray Exp $";
#else
static	char	*rcs_id =
  "$Id: protect.c,v 1.1 2000/05/15 15:46:40 gray Exp $";
#endif
#endif

/*
 * void protect_read_only
 *
 * DESCRIPTION:
 *
 * Set the protections on a block to be read-only.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * block_pnt -> Pointer to block that we are protecting.
 *
 * block_n -> Number of blocks that we are protecting.
 */
void	protect_read_only(void *block_pnt, const int block_n)
{
#if PROTECT_ALLOWED && PROTECT_BLOCKS
  int	size = block_n * BLOCK_SIZE;
  
  if (mprotect(block_pnt, size, PROT_READ) != 0) {
    _dmalloc_message("mprotect on '%#lx' size %d failed", block_pnt, size);
  }
#endif
}

/*
 * void protect_read_write
 *
 * DESCRIPTION:
 *
 * Set the protections on a block to be read-write.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * block_pnt -> Pointer to block that we are protecting.
 *
 * block_n -> Number of blocks that we are protecting.
 */
void	protect_read_write(void *block_pnt, const int block_n)
{
#if PROTECT_ALLOWED && PROTECT_BLOCKS
  int	prot, size = block_n * BLOCK_SIZE;
  
  /*
   * We set executable if possible in case the user has allocated
   * stack space or some such
   */
  prot = PROT_READ | PROT_WRITE;
#ifdef PROT_EXEC
  prot |= PROT_EXEC;
#endif
  if (mprotect(block_pnt, size, prot) != 0) {
    _dmalloc_message("mprotect on '%#lx' size %d failed", block_pnt, size);
  }
#endif
}

/*
 * void protect_no_access
 *
 * DESCRIPTION:
 *
 * Set the protections on a block to be no-access.
 *
 * RETURNS:
 *
 * None.
 *
 * ARGUMENTS:
 *
 * block_pnt -> Pointer to block that we are protecting.
 *
 * block_n -> Number of blocks that we are protecting.
 */
void	protect_no_access(void *block_pnt, const int block_n)
{
#if PROTECT_ALLOWED && PROTECT_BLOCKS
  int	size = block_n * BLOCK_SIZE;
  
  if (mprotect(block_pnt, size, PROT_NONE) != 0) {
    _dmalloc_message("mprotect on '%#lx' size %d failed", block_pnt, size);
  }
#endif
}
