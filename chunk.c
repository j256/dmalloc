/*
 * memory chunk low-level allocation routines
 *
 * Copyright 1991 by the Antaire Corporation
 *
 * Written by Gray Watson
 *
 * chunk.c 3.16 12/23/91
 */

/*
 * algorithm level for the malloc routines.  these routines handle the
 * manipulation and administration of chunks of memory.
 */

#define CHUNK_MAIN

#include <fcntl.h>				/* for O_WRONLY */
#include <varargs.h>				/* for message vsprintf */

#include "useful.h"

#if defined(ANTAIRE)
#include "logger.h"
#include "ourstring.h"
#endif

#include "alloc.h"
#include "chunk.h"
#include "chunk_loc.h"
#include "heap.h"
#include "malloc_dbg.h"
#include "malloc_err.h"
#include "malloc_loc.h"

SCCS_ID("@(#)chunk.c	3.16 GRAY@ANTAIRE.COM 12/23/91");

/* for formating */
#define FREE_COLUMN		    5		/* for dump_free formating */

/* checking information */
#define MIN_FILE_LENGTH		    3		/* min "[a-zA-Z].c" length */
#define MAX_FILE_LENGTH		   40		/* max __FILE__ length */
#define MAX_LINE_NUMBER		10000		/* max __LINE__ value */

#define FREE_CHAR		'\305'		/* to blank free space with */

/*
 * exported variables
 *
 * NOTE: these are here so the user won't see them in alloc.h
 */
/* global debug level number */
EXPORT	int		_malloc_debug_level	= MALLOC_DEBUG_OFF;
EXPORT	int		_malloc_errno		= 0;	/* error number */

/* free lists of bblocks and dblocks */
LOCAL	bblock_t	*free_bblock[LARGEST_BLOCK + 1];
LOCAL	dblock_t	*free_dblock[BASIC_BLOCK];

/* administrative structures */
LOCAL	bblock_adm_t	*bblock_adm_head = NULL; /* pointer to 1st bb_admin */
LOCAL	bblock_adm_t	*bblock_adm_tail = NULL; /* pointer to last bb_admin */

/* memory stats */
LOCAL	long		alloc_current	= 0;	/* current memory usage */
LOCAL	long		alloc_maximum	= 0;	/* maximum memory usage  */
LOCAL	long		alloc_total	= 0;	/* total allocation */
LOCAL	long		alloc_one_max	= 0;	/* maximum at once */

/* pointer stats */
LOCAL	long		alloc_cur_pnts	= 0;	/* current pointers */
LOCAL	long		alloc_max_pnts	= 0;	/* maximum pointers */
LOCAL	long		alloc_tot_pnts	= 0;	/* current pointers */

/* admin counts */
LOCAL	int		bblock_adm_count = 0;	/* count of bblock_admin */
LOCAL	int		dblock_adm_count = 0;	/* count of dblock_admin */
LOCAL	int		bblock_count	 = 0;	/* count of basic-blocks */
LOCAL	int		dblock_count	 = 0;	/* count of divided-blocks */

/* alloc counts */
EXPORT	int		_calloc_count	= 0;	/* # callocs, done in alloc */
LOCAL	int		free_count	= 0;	/* count the frees */
LOCAL	int		malloc_count	= 0;	/* count the mallocs */
LOCAL	int		realloc_count	= 0;	/* count the reallocs */

/***************************** debug functions *******************************/

/*
 * Internal message writter
 * NOTE: this is here so the users will not see it in alloc.h
 */
EXPORT	void	_malloc_message(format, va_alist)
  char		*format;
  va_dcl
{
  static int	outfile = -1;
  va_list	args;
  int		len;
  char		str[1024];
  
  /* no logpath then no workie */
  if (malloc_logpath == NULL)
    return;
  
  /* do we need to open the outfile? */
  if (outfile < 0 &&
      (outfile = open(malloc_logpath, O_WRONLY | O_CREAT | O_TRUNC, 0666)) <
      0) {
    (void)sprintf(str, "%s:%d: could not open '%s': ",
		  __FILE__, __LINE__, malloc_logpath);
    (void)write(STDERR, str, strlen(str));
    perror("");
    exit(1);
  }
  
  /* write the format + info into str */
  va_start(args);
  (void)vsprintf(str, format, args);
  va_end(args);
  
  /* find the length of str, if empty then return */
  if ((len = strlen(str)) == 0)
    return;
  
  /* tack on a '\n' if necessary */
  if (str[len - 1] != '\n') {
    str[len] = '\n';
    str[++len] = '\0';
  }
  
  /* write str to the outfile */
  (void)write(outfile, str, len);
}

/* malloc version of perror of an error in STR */
EXPORT	void	_malloc_perror(str)
  char		*str;
{
  /* if debug level is not high enough, return */
  if (_malloc_debug_level >= DEBUG_LOG_PERROR) {
    
    /* default str value */
    if (str == NULL)
      str = "malloc_perror";
    
    /* print the malloc error message */
    _malloc_message("ERROR: %s: %s(%d)",
		    str, malloc_errlist[_malloc_errno], _malloc_errno);
    
    /* should I kill myself? */
    if (malloc_kill_sig != 0)
      kill(getpid(), malloc_kill_sig);
  }
}

/************************* fence-post error functions ************************/

/* check PNT of SIZE for fence-post magic numbers, returns ERROR or NOERROR */
LOCAL	int	fence_read(pnt, size, file, line)
  char		*pnt;
  unsigned int	size;
  char		*file;
  int		line;
{
  long		top, *longp;
  
  /*
   * write magic numbers into block in bottom of allocation
   *
   * WARNING: assuming a word-boundary here
   */
  for (longp = (long *)pnt; longp < (long *)(pnt + FENCE_BOTTOM); longp++)
    if (*longp != FENCE_MAGIC_BASE) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
			pnt, file, line);
      _malloc_errno = MALLOC_UNDER_FENCE;
      _malloc_perror("fence_read");
      return ERROR;
    }
  
  /*
   * write magic numbers into block in top of allocation
   *
   * WARNING: not guaranteed a word-boundary here
   */
  for (longp = (long *)(pnt + size - FENCE_TOP); longp < (long *)(pnt + size);
       longp++) {
    bcopy(longp, &top, sizeof(long));
    if (top != FENCE_MAGIC_TOP) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
			pnt, file, line);
      _malloc_errno = MALLOC_OVER_FENCE;
      _malloc_perror("fence_read");
      return ERROR;
    }
  }
  
  return NOERROR;
}

/* load PNT of SIZE bytes with fence-post magic numbers */
LOCAL	void	fence_write(pnt, size)
  char		*pnt;
  unsigned int	size;
{
  long		top = FENCE_MAGIC_TOP;
  long		*longp;
  
  /* write magic numbers into block in bottom of allocation */
  for (longp = (long *)pnt; longp < (long *)(pnt + FENCE_BOTTOM); longp++)
    *longp = FENCE_MAGIC_BASE;
  
  /* write magic numbers into block in top of allocation */
  /* WARNING: not guaranteed a word-boundary here */
  for (longp = (long *)(pnt + size - FENCE_TOP); longp < (long *)(pnt + size);
       longp++)
    bcopy(&top, longp, sizeof(long));
}

/************************** administration functions *************************/

/* startup the low level malloc routines */
EXPORT	int	_chunk_startup()
{
  int	binc;
  
  /* align the base pointer */
  if (_heap_align_base(BLOCK_SIZE) == MALLOC_ERROR) {
    _malloc_errno = MALLOC_BAD_SETUP;
    _malloc_perror("_chunk_startup");
    return ERROR;
  }
  
  /* initialize free bins */
  for (binc = 0; binc <= LARGEST_BLOCK; binc++)
    free_bblock[binc] = NULL;
  for (binc = 0; binc < BASIC_BLOCK; binc++)
    free_dblock[binc] = NULL;
  
  /* verify some conditions */
  if (BB_PER_ADMIN <= 2 ||
      sizeof(bblock_adm_t) > BLOCK_SIZE ||
      DB_PER_ADMIN < (1 << (BASIC_BLOCK - SMALLEST_BLOCK)) ||
      sizeof(dblock_adm_t) > BLOCK_SIZE) {
    _malloc_errno = MALLOC_BAD_SETUP;
    _malloc_perror("_chunk_startup");
    return ERROR;
  }
  
  return NOERROR;
}

/* return the number of bits in number SIZE */
LOCAL	int	num_bits(size)
  unsigned int	size;
{
  unsigned int	tmp = size;
  int		bitc;
  
  if (size < 0
#if defined(MALLOC_NO_ZERO_SIZE)
      || size == 0
#endif
      ) {
    _malloc_errno = MALLOC_BAD_SIZE;
    _malloc_perror("num_bits");
    return ERROR;
  }
  
  /* shift right until 0, 2 ^ 0 == 1 */
  for (bitc = -1; tmp > 0; bitc++)
    tmp >>= 1;
  
  /* are we not a base 2 number? */
  if (size > (1 << bitc))
    bitc++;
  
  return bitc;
}

/*
 * "allocate" another bblock administrative block
 * NOTE: some logistic problems with getting from free list
 */
LOCAL	bblock_adm_t	*get_bblock_admin()
{
  bblock_adm_t	*new;
  bblock_t	*bblockp;
  
  bblock_adm_count++;
  bblock_count++;
  
  /* get some more space for a bblock_admin structure */
  if ((new = (bblock_adm_t *)_heap_alloc(BLOCK_SIZE)) ==
      (bblock_adm_t *)HEAP_ALLOC_ERROR)
    return NULL;
  
  /* do we need to print admin info? */
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("new bblock admin alloced for another %d admin slots",
		    BB_PER_ADMIN);
  
  /*
   * initialize the new bblock_admin block
   */
  new->ba_magic1 = CHUNK_MAGIC_BASE;
  if (bblock_adm_tail == NULL)
    new->ba_count = 0;
  else
    new->ba_count = bblock_adm_tail->ba_count + BB_PER_ADMIN;
  
  /* initialize the bblocks in the bblock_admin */
  for (bblockp = new->ba_block; bblockp < new->ba_block + BB_PER_ADMIN;
       bblockp++)
    bblockp->bb_flags = 0;
  
  new->ba_next = NULL;
  new->ba_magic2 = CHUNK_MAGIC_TOP;
  
  /*
   * continue bblock_admin linked-list
   */
  if (bblock_adm_tail == NULL)
    bblock_adm_head = new;
  else
    bblock_adm_tail->ba_next = new;
  bblock_adm_tail = new;
  
  return new;
}

/* get MANY of bblocks, return a pointer to the first one */
LOCAL	bblock_t	*get_bblocks(bitc)
  int		bitc;
{
  static int	free_slots = 0;		/* free slots in last bb_admin */
  int		newc, mark;
  bblock_adm_t	*bblock_admp;
  bblock_t	*bblockp;
  
  if (bitc < BASIC_BLOCK) {
    _malloc_errno = MALLOC_BAD_SIZE;
    _malloc_perror("get_bblocks");
    return NULL;
  }
  
  /* do we need to print admin info? */
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("need bblocks for %d bytes or %d bits",
		    1 << bitc, bitc);
  
  /* is there anything on the free list? */
  if ((bblockp = free_bblock[bitc]) != NULL) {
    free_bblock[bitc] = bblockp->bb_next;
    return bblockp;
  }
  
  bblock_count += 1 << (bitc - BASIC_BLOCK);
  
  /* point at first free bblock entry (may be out of range) */
  bblock_admp = bblock_adm_tail;
  mark = BB_PER_ADMIN - free_slots;
  
  /* loop until we have enough slots */
  for (newc = 0; free_slots < 1 << (bitc - BASIC_BLOCK); newc++) {
    if (get_bblock_admin() == NULL)
      return NULL;
    
    /* do we need to move to the next bblock admin? */
    if (mark == BB_PER_ADMIN) {
      if (bblock_admp == NULL)
	bblock_admp = bblock_adm_tail;
      else
	bblock_admp = bblock_admp->ba_next;
      mark = 0;
    }
    
    /* write a bblock entry for the bblock admin */
    bblockp = &bblock_admp->ba_block[mark];
    
    bblockp->bb_flags	= BBLOCK_ADMIN;
    bblockp->bb_count	= bblock_adm_tail->ba_count;
    bblockp->bb_adminp	= bblock_adm_tail;
    
    /* add one to mark to pass by bblock admin header */
    mark++;
    
    /* adjust free_slot count for addition of new bblock admin */
    free_slots += BB_PER_ADMIN - 1;
  }
  
  /* do we need to move to the next bblock admin? */
  if (mark == BB_PER_ADMIN) {
    bblock_admp = bblock_admp->ba_next;
    mark = 0;
  }
  
  /* adjust free_slot count for the allocation */
  free_slots -= 1 << (bitc - BASIC_BLOCK);
  
  return &bblock_admp->ba_block[mark];
}

/* get MANY of contiguous dblock administrative slots */
LOCAL	dblock_t	*get_dblock_admin(many)
  int		many;
{
  static int		free_slots = 0;
  static dblock_adm_t	*dblock_admp;
  dblock_t		*dblockp;
  bblock_t		*bblockp;
  
  /* do we need to print admin info? */
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("need %d dblock slots", many);
  
  /* do we have anough right now */
  if (free_slots >= many) {
    free_slots -= many;
    return &dblock_admp->da_block[DB_PER_ADMIN - (free_slots + many)];
  }
  
  /*
   * allocate a new bblock of dblock admin slots, should use free list
   */
  if ((bblockp = get_bblocks(BASIC_BLOCK)) == NULL)
    return NULL;
  
  dblock_adm_count++;
  
  /* allocate the block if needed */
  if (IS_BIT_SET(bblockp->bb_flags, BBLOCK_ALLOCATED))
    dblock_admp = (dblock_adm_t *)bblockp->bb_mem;
  else
    if ((dblock_admp = (dblock_adm_t *)_heap_alloc(BLOCK_SIZE)) ==
	(dblock_adm_t *)HEAP_ALLOC_ERROR)
      return NULL;
  bblockp->bb_flags = BBLOCK_DBLOCK_ADMIN;
  bblockp->bb_slotp = dblock_admp;
  
  /* do we need to print admin info? */
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("opened another %d dblock slots", DB_PER_ADMIN);
  
  dblock_admp->da_magic1 = CHUNK_MAGIC_BASE;
  
  /* initialize the db_slots */
  for (dblockp = dblock_admp->da_block;
       dblockp < dblock_admp->da_block + DB_PER_ADMIN; dblockp++) {
    dblockp->db_bblock = NULL;
    dblockp->db_next = NULL;
  }
  
  dblock_admp->da_magic2 = CHUNK_MAGIC_TOP;
  
  free_slots = DB_PER_ADMIN - many;
  
  return dblock_admp->da_block;
}

/* get a dblock of 1<<BITC sized chunks, also asked for the slot memory */
LOCAL	char	*get_dblock(bitc, admp)
  int		bitc;
  dblock_t	**admp;
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  char		*mem;
  
  /* is there anything on the dblock free list? */
  if ((dblockp = free_dblock[bitc]) != NULL) {
    free_dblock[bitc] = dblockp->db_next;
    *admp = dblockp;
    
    /* find pointer to memory chunk */
    mem = dblockp->db_bblock->bb_mem +
      (dblockp - dblockp->db_bblock->bb_dblock) * (1 << bitc);
    
    /* do we need to print admin info? */
    if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
      _malloc_message("found a %d byte dblock entry", 1 << bitc);
    
    return mem;
  }
  
  /* do we need to print admin info? */
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("need to create a dblock for %dx %d byte blocks",
		    1 << (BASIC_BLOCK - bitc), 1 << bitc);
  
  /* get some dblock admin slots */
  if ((dblockp = get_dblock_admin(1 << (BASIC_BLOCK - bitc))) == NULL)
    return NULL;
  *admp = dblockp;
  
  dblock_count++;
  
  /* get a bblock from free list */
  if ((bblockp = get_bblocks(BASIC_BLOCK)) == NULL)
    return NULL;
  
  /* allocate the block if needed */
  if (IS_BIT_SET(bblockp->bb_flags, BBLOCK_ALLOCATED))
    mem = bblockp->bb_mem;
  else
    if ((mem = _heap_alloc(BLOCK_SIZE)) == HEAP_ALLOC_ERROR)
      return NULL;
  
  /* setup bblock information */
  bblockp->bb_flags	= BBLOCK_DBLOCK;
  bblockp->bb_bitc	= bitc;
  bblockp->bb_dblock	= dblockp;
  bblockp->bb_mem	= mem;
  
  /* add the rest to the free list */
  free_dblock[bitc] = ++dblockp;
  for (; dblockp < *admp + (1 << (BASIC_BLOCK - bitc)) - 1; dblockp++) {
    dblockp->db_next	= dblockp + 1;
    dblockp->db_bblock	= bblockp;
  }
  
  /* last one points to NULL */
  dblockp->db_next	= NULL;
  dblockp->db_bblock	= bblockp;
  
  /* set the block with our FREE_CHAR */
  if (_malloc_debug_level >= DEBUG_FREE_BLANK)
    memset(mem, FREE_CHAR, BLOCK_SIZE);
  
  return mem;
}

/*
 * find the bblock entry for PNT, return bblock admin in *BB_ADMIN (if != NULL)
 * and return the block number in BLOCK_NUM (if non NULL)
 */
LOCAL	bblock_t	*find_bblock(pnt, block_num, bb_admin)
  char		*pnt;
  int		*block_num;
  bblock_adm_t	**bb_admin;
{
  int		bblockc;
  bblock_adm_t	*bblock_admp;
  
  /*
   * check validity of the pointer
   */
  if (! IS_IN_HEAP(pnt)) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_POINTER_NOT_IN_HEAP;
    _malloc_perror("find_bblock");
    return NULL;
  }
  
  /* find which block it is in */
  bblockc = WHICH_BLOCK(pnt);
  
  /* do we need to return the block number */
  if (block_num != NULL)
    *block_num = bblockc;
  
  /* find right bblock admin */
  for (bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bblock_admp = bblock_admp->ba_next) {
    if (bblockc < BB_PER_ADMIN)
      break;
    
    bblockc -= BB_PER_ADMIN;
  }
  
  if (bblock_admp == NULL) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_POINTER_NOT_FOUND;
    _malloc_perror("find_bblock");
    return NULL;
  }
  
  /* should we return bblock admin info? */
  if (bb_admin != NULL)
    *bb_admin = bblock_admp;
  
  return &bblock_admp->ba_block[bblockc];
}

/* run extensive tests on the entire heap depending on TYPE */
EXPORT	int	_chunk_heap_check(type)
  int		type;		/* check all or use only level */
{
  bblock_adm_t	*this, *last_admp;
  bblock_t	*bblockp, *last_bblockp;
  dblock_t	*dblockp;
  int		undef = 0, start = 0;
  char		*bytep;
  char		*mem;
  int		bitc, dblockc, bblockc = 0, freec = 0;
  int		bbc = 0, len;
  
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("checking heap");
  
  if (bblock_adm_head == NULL)
    return NOERROR;
  
  /* start pointers */
  this = bblock_adm_head;
  last_admp = NULL;
  last_bblockp = NULL;
  
  /* test admin pointer validity */
  if (! IS_IN_HEAP(this)) {
    _malloc_errno = MALLOC_BAD_ADMINP;
    _malloc_perror("_chunk_heap_check");
    return ERROR;
  }
  
  /* test structure validity */
  if (this->ba_magic1 != CHUNK_MAGIC_BASE ||
      this->ba_magic2 != CHUNK_MAGIC_TOP) {
    _malloc_errno = MALLOC_BAD_ADMIN_MAGIC;
    _malloc_perror("_chunk_heap_check");
    return ERROR;
  }
  
  /* verify count value */
  if (this->ba_count != bbc) {
    _malloc_errno = MALLOC_BAD_ADMIN_COUNT;
    _malloc_perror("_chunk_heap_check");
    return ERROR;
  }
  
  /* check out the basic blocks */
  for (bblockp = this->ba_block;; last_bblockp = bblockp++) {
    
    /* are we at the end of the bb_admin section */
    if (bblockp >= this->ba_block + BB_PER_ADMIN) {
      this = this->ba_next;
      bbc += BB_PER_ADMIN;
      
      /* are we done? */
      if (this == NULL)
	break;
      
      /* test admin pointer validity */
      if (! IS_IN_HEAP(this)) {
	_malloc_errno = MALLOC_BAD_ADMINP;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* test structure validity */
      if (this->ba_magic1 != CHUNK_MAGIC_BASE ||
	  this->ba_magic2 != CHUNK_MAGIC_TOP) {
	_malloc_errno = MALLOC_BAD_ADMIN_MAGIC;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify count value */
      if (this->ba_count != bbc) {
	_malloc_errno = MALLOC_BAD_ADMIN_COUNT;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      bblockp = this->ba_block;
    }
    
    /* check for no-allocation */
    if (IS_BIT_NOT_SET(bblockp->bb_flags, BBLOCK_ALLOCATED)) {
      undef = 1;
      continue;
    }
    
    /* we better not have has a no allocation block before */
    if (undef == 1) {
      _malloc_errno = MALLOC_BAD_BLOCK_ORDER;
      _malloc_perror("_chunk_heap_check");
      return ERROR;
    }
    
    start = 0;
    
    /*
     * check for different types
     */
    switch (bblockp->bb_flags) {
      
      /* check a starting user-block */
    case BBLOCK_START_USER:
      /* check X blocks in a row */
      if (bblockc != 0) {
	_malloc_errno = MALLOC_USER_NON_CONTIG;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* mark the size in bits */
      if ((bitc = num_bits(bblockp->bb_size)) == ERROR)
	return ERROR;
      bblockc = 1 << (bitc - BASIC_BLOCK);
      start = 1;
      
      /* check fence-posts for memory chunk */
      if (_malloc_debug_level >= DEBUG_CHECK_FENCE) {
	mem = BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block));
	if (fence_read(mem, bblockp->bb_size, bblockp->bb_file,
		       bblockp->bb_line) != NOERROR)
	  return ERROR;
      }
      
      /* NOTE: NO BREAK HERE ON PURPOSE */
      
    case BBLOCK_USER:
      
      /* check line number */
      if (bblockp->bb_line < 0 || bblockp->bb_line > MAX_LINE_NUMBER) {
	if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	  _malloc_message("bad pointer alloced in '%s:%d'",
			  bblockp->bb_file, bblockp->bb_line);
	_malloc_errno = MALLOC_BAD_LINE;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
      if (bblockp->bb_size <= BLOCK_SIZE / 2 ||
	  bblockp->bb_size > (1 << LARGEST_BLOCK)) {
	if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	  _malloc_message("bad pointer alloced in '%s:%d'",
			  bblockp->bb_file, bblockp->bb_line);
	_malloc_errno = MALLOC_BAD_SIZE;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* check file pointer */
      if (bblockp->bb_file == NULL) {
	len = strlen(bblockp->bb_file);
	if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	  _malloc_errno = MALLOC_BAD_FILEP;
	  _malloc_perror("_chunk_heap_check");
	  return ERROR;
	}
      }
      
      /* check X blocks in a row */
      if (bblockc == 0) {
	_malloc_errno = MALLOC_USER_NON_CONTIG;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      else
	if (start == 0 &&
	    (last_bblockp == NULL ||
	     (last_bblockp->bb_flags != BBLOCK_START_USER && 
	      last_bblockp->bb_flags != BBLOCK_USER) ||
	     bblockp->bb_file != last_bblockp->bb_file ||
	     bblockp->bb_line != last_bblockp->bb_line ||
	     bblockp->bb_size != last_bblockp->bb_size)) {
	  _malloc_errno = MALLOC_USER_NON_CONTIG;
	  _malloc_perror("_chunk_heap_check");
	  return ERROR;
	}
      
      bblockc--;
      
      /* NOTE: we should check above the allocated space if free_blank on */
      
      break;
      
    case BBLOCK_ADMIN:
      
      /* check the bblock_admin linked-list */
      if ((last_admp == NULL && bblockp->bb_adminp != bblock_adm_head) ||
	  (last_admp != NULL && last_admp->ba_next != bblockp->bb_adminp)) {
	_malloc_errno = MALLOC_BAD_BLOCK_ADMINP;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      last_admp = bblockp->bb_adminp;
      
      /* check count against admin count */
      if (bblockp->bb_count != bblockp->bb_adminp->ba_count) {
	_malloc_errno = MALLOC_BAD_BLOCK_ADMINC;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      break;
      
    case BBLOCK_DBLOCK:
      
      /* check out bitc */
      if (bblockp->bb_bitc < 0 || bblockp->bb_bitc >= BASIC_BLOCK) {
	_malloc_errno = MALLOC_BAD_DBLOCK_SIZE;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblockp->bb_dblock)) {
	_malloc_errno = MALLOC_BAD_DBLOCK_POINTER;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify mem pointer */
      if (bblockp->bb_mem !=
	  BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block))) {
	_malloc_errno = MALLOC_BAD_DBLOCK_MEM;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* check fence-posts for memory chunk */
      if (_malloc_debug_level >= DEBUG_FENCE_DBLOCK ||
	  (type == CHUNK_CHECK_ALL &&
	   _malloc_debug_level >= DEBUG_CHECK_FENCE)) {
	for (dblockc = 0, dblockp = bblockp->bb_dblock;
	     dblockp < bblockp->bb_dblock +
	     (1 << (BASIC_BLOCK - bblockp->bb_bitc));
	     dblockc++, dblockp++) {
	  
	  /* check out dblock entry to see if it is not free */
	  if (dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next))
	    continue;
	  
	  /*
	   * check out size, better be less than BLOCK_SIZE / 2
	   * I have to check this twice :-(
	   */
	  if (dblockp->db_size < SMALLEST_BLOCK ||
	      dblockp->db_size > BLOCK_SIZE / 2) {
	    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	      _malloc_message("bad pointer alloced in '%s:%d'",
			      dblockp->db_file, dblockp->db_line);
	    _malloc_errno = MALLOC_BAD_DBADMIN_SLOT;
	    _malloc_perror("_chunk_heap_check");
	    return ERROR;
	  }
	  
	  mem = bblockp->bb_mem + dblockc * (1 << bblockp->bb_bitc);
	  if (fence_read(mem, dblockp->db_size, dblockp->db_file,
			 dblockp->db_line) != NOERROR)
	    return ERROR;
	}
      }
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      /* check out dblock pointer */
      if (! IS_IN_HEAP(bblockp->bb_slotp)) {
	_malloc_errno = MALLOC_BAD_DBADMIN_POINTER;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify magic numbers */
      if (bblockp->bb_slotp->da_magic1 != CHUNK_MAGIC_BASE ||
	  bblockp->bb_slotp->da_magic2 != CHUNK_MAGIC_TOP) {
	_malloc_errno = MALLOC_BAD_DBADMIN_MAGIC;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* check out each dblock_admin struct? */
      if (_malloc_debug_level >= DEBUG_CHECK_DBLOCK ||
	  type == CHUNK_CHECK_ALL) {
	for (dblockp = bblockp->bb_slotp->da_block;
	     dblockp < bblockp->bb_slotp->da_block + DB_PER_ADMIN; dblockp++) {
	  
	  /* verify if we have a good bblock pointer and good back pointer */
	  if (dblockp->db_bblock == NULL && dblockp->db_next == NULL)
	    continue;
	  
	  /* check out dblock pointer and next pointer (if free) */
	  if (dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next)) {
	    
	    /* find pointer to memory chunk */
	    mem = dblockp->db_bblock->bb_mem +
	      (dblockp - dblockp->db_bblock->bb_dblock) *
		(1 << dblockp->db_bblock->bb_bitc);
	    
	    /* should we verify that we have a block of FREE_CHAR? */
	    if (_malloc_debug_level >= DEBUG_CHECK_FREE ||
		(type == CHUNK_CHECK_ALL &&
		 _malloc_debug_level >= DEBUG_FREE_BLANK))
	      for (bytep = (char *)mem;
		   bytep < (char *)mem + (1 << dblockp->db_bblock->bb_bitc);
		   bytep++)
		if (*bytep != FREE_CHAR) {
		  if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
		    _malloc_message("bad free memory at '%#lx'", mem);
		  _malloc_errno = MALLOC_FREE_NON_BLANK;
		  _malloc_perror("_chunk_heap_check");
		  return ERROR;
		}
	    
	    continue;
	  }
	  
	  /* check out size, better be less than BLOCK_SIZE / 2 */
	  if (dblockp->db_size < SMALLEST_BLOCK ||
	      dblockp->db_size > BLOCK_SIZE / 2) {
	    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	      _malloc_message("bad pointer alloced in '%s:%d'",
			      dblockp->db_file, dblockp->db_line);
	    _malloc_errno = MALLOC_BAD_DBADMIN_SLOT;
	    _malloc_perror("_chunk_heap_check");
	    return ERROR;
	  }
	  
	  /* check line number */
	  if (dblockp->db_line < 0 || dblockp->db_line > MAX_LINE_NUMBER) {
	    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	      _malloc_message("bad pointer alloced in '%s:%d'",
			      dblockp->db_file, dblockp->db_line);
	    _malloc_errno = MALLOC_BAD_DBADMIN_SLOT;
	    _malloc_perror("_chunk_heap_check");
	    return ERROR;
	  }
	  
	  len = strlen(dblockp->db_file);
	  if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	    _malloc_errno = MALLOC_BAD_DBADMIN_SLOT;
	    _malloc_perror("_chunk_heap_check");
	    return ERROR;
	  }
	}
      }
      break;
      
    case BBLOCK_FREE:
      
      /* NOTE: check out free_lists, depending on debug level? */
      
      /* check out bitc which is the free-lists size */
      if (bblockp->bb_bitc < BASIC_BLOCK ||
	  bblockp->bb_bitc > LARGEST_BLOCK) {
	_malloc_errno = MALLOC_BAD_SIZE;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify linked list pointer */
      if (bblockp->bb_next != NULL && ! IS_IN_HEAP(bblockp->bb_next)) {
	_malloc_errno = MALLOC_BAD_FREE_LIST;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* verify mem pointer */
      if (bblockp->bb_mem !=
	  BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block))) {
	_malloc_errno = MALLOC_BAD_FREE_MEM;
	_malloc_perror("_chunk_heap_check");
	return ERROR;
      }
      
      /* check X blocks in a row */
      if (freec == 0)
	freec = 1 << (bblockp->bb_bitc - BASIC_BLOCK);
      else
	if (last_bblockp == NULL || last_bblockp->bb_flags != BBLOCK_FREE ||
	    bblockp->bb_bitc != last_bblockp->bb_bitc) {
	  _malloc_errno = MALLOC_FREE_NON_CONTIG;
	  _malloc_perror("_chunk_heap_check");
	  return ERROR;
	}
      freec--;
      
      /* should we verify that we have a block of FREE_CHAR? */
      if (_malloc_debug_level >= DEBUG_CHECK_FREE ||
	  (type == CHUNK_CHECK_ALL && _malloc_debug_level >= DEBUG_FREE_BLANK))
	for (bytep = (char *)bblockp->bb_mem;
	     bytep < (char *)bblockp->bb_mem + BLOCK_SIZE; bytep++)
	  if (*bytep != FREE_CHAR) {
	    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	      _malloc_message("bad free memory at '%#lx'", bblockp->bb_mem);
	    _malloc_errno = MALLOC_FREE_NON_BLANK;
	    _malloc_perror("_chunk_heap_check");
	    return ERROR;
	  }
      break;
      
    default:
      _malloc_errno = MALLOC_BAD_FLAG;
      _malloc_perror("_chunk_heap_check");
      return ERROR;
      break;
    }
  }
  
  /*
   * any left over contiguous counters?
   */
  if (bblockc > 0) {
    _malloc_errno = MALLOC_USER_NON_CONTIG;
    _malloc_perror("_chunk_heap_check");
    return ERROR;
  }
  if (freec > 0) {
    _malloc_errno = MALLOC_FREE_NON_CONTIG;
    _malloc_perror("_chunk_heap_check");
    return ERROR;
  }
  
  return NOERROR;
}

/* run extensive tests on PNT */
EXPORT	int	_chunk_pnt_check(pnt)
  char		*pnt;
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  int		len;
  
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("checking pointer %#lx", pnt);
  
  /* adjust the pointer down if fence-posting */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    pnt -= FENCE_BOTTOM;
  
  /* find which block it is in */
  if ((bblockp = find_bblock(pnt, NULL, NULL)) == NULL)
    return ERROR;
  
  if (IS_BIT_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if ((pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_ON_BLOCK;
      _malloc_perror("_chunk_pnt_check");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + (pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_ALREADY_FREE;
      _malloc_perror("_chunk_pnt_check");
      return ERROR;
    }
    
    /* check line number */
    if (dblockp->db_line < 0 || dblockp->db_line > MAX_LINE_NUMBER) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
			pnt, dblockp->db_file, dblockp->db_line);
      _malloc_errno = MALLOC_BAD_LINE;
      _malloc_perror("_chunk_pnt_check");
      return ERROR;
    }
    
    /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
    if (dblockp->db_size < SMALLEST_BLOCK ||
	dblockp->db_size > BLOCK_SIZE / 2) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
			pnt, dblockp->db_file, dblockp->db_line);
      _malloc_errno = MALLOC_BAD_DBADMIN_SLOT;
      _malloc_perror("_chunk_pnt_check");
      return ERROR;
    }
    
    /* check file pointer */
    if (dblockp->db_file == NULL) {
      len = strlen(dblockp->db_file);
      if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
	if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	  _malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
			  pnt, dblockp->db_file, dblockp->db_line);
	_malloc_errno = MALLOC_BAD_FILEP;
	_malloc_perror("_chunk_pnt_check");
	return ERROR;
      }
    }
    
    /* check out the fence-posts */
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
      if (fence_read(pnt, dblockp->db_size, dblockp->db_file,
		     dblockp->db_line) != NOERROR)
	return ERROR;
    
    return NOERROR;
  }
  
  /* on a block boundary? */
  if (! ON_BLOCK(pnt)) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_NOT_ON_BLOCK;
    _malloc_perror("_chunk_pnt_check");
    return ERROR;
  }
  
  /* are we on a normal block */
  if (IS_BIT_NOT_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_NOT_START_USER;
    _malloc_perror("_chunk_pnt_check");
    return ERROR;
  }
  
  /* check line number */
  if (bblockp->bb_line < 0 || bblockp->bb_line > MAX_LINE_NUMBER) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
		      pnt, bblockp->bb_file, bblockp->bb_line);
    _malloc_errno = MALLOC_BAD_LINE;
    _malloc_perror("_chunk_pnt_check");
    return ERROR;
  }
  
  /* check out size, BLOCK_SIZE / 2 == 512 when dblock allocs take over */
  if (bblockp->bb_size <= BLOCK_SIZE / 2 ||
      bblockp->bb_size > (1 << LARGEST_BLOCK)) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
		      pnt, bblockp->bb_file, bblockp->bb_line);
    _malloc_errno = MALLOC_BAD_SIZE;
    _malloc_perror("_chunk_pnt_check");
    return ERROR;
  }
  
  /* check file pointer */
  if (bblockp->bb_file == NULL) {
    len = strlen(bblockp->bb_file);
    if (len < MIN_FILE_LENGTH || len > MAX_FILE_LENGTH) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx' alloced in '%s:%d'",
			pnt, bblockp->bb_file, bblockp->bb_line);
      _malloc_errno = MALLOC_BAD_FILEP;
      _malloc_perror("_chunk_pnt_check");
      return ERROR;
    }
  }
  
  /* check out the fence-posts */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    if (fence_read(pnt, bblockp->bb_size, bblockp->bb_file,
		   bblockp->bb_line) != NOERROR)
      return ERROR;
  
  return NOERROR;
}

/**************************** information routines ***************************/

/* return some information associated with PNT, ERROR on error */
EXPORT	int	chunk_read_info(pnt, size, file, line)
  char		*pnt;
  unsigned int	*size;
  char		**file;
  unsigned int	*line;
{
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("reading info about pointer %#lx", pnt);
  
  /* find which block it is in */
  if ((bblockp = find_bblock(pnt, NULL, NULL)) == NULL) {
    /* find block already set error */
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (IS_BIT_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if ((pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_ON_BLOCK;
      _malloc_perror("chunk_read_info");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + (pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_ALREADY_FREE;
      _malloc_perror("chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      *size = dblockp->db_size;
    if (file != NULL)
      *file = dblockp->db_file;
    if (line != NULL)
      *line = dblockp->db_line;
  }
  else {
    
    /* verify that the pointer is either dblock or user allocated */
    if (IS_BIT_NOT_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_USER;
      _malloc_perror("chunk_read_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      *size = bblockp->bb_size;
    if (file != NULL)
      *file = bblockp->bb_file;
    if (line != NULL)
      *line = bblockp->bb_line;
  }
  
  return NOERROR;
}

/* write new FILE, LINE, SIZE info into PNT */
LOCAL	int	chunk_write_info(pnt, size, file, line)
  char		*pnt;
  unsigned int	size;
  char		*file;
  unsigned int	line;
{
  int		bitc, bblockc;
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  bblock_adm_t	*bblock_admp;
  
  /* find which block it is in */
  if ((bblockp = find_bblock(pnt, NULL, &bblock_admp)) == NULL) {
    /* find block already set error */
    return ERROR;
  }
  
  /* are we looking in a DBLOCK */
  if (IS_BIT_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    /* on a mini-block boundary? */
    if ((pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_ON_BLOCK;
      _malloc_perror("chunk_write_info");
      return ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + (pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_USER;
      _malloc_perror("chunk_write_info");
      return ERROR;
    }
    
    /* write info back to user space */
    if (size != NULL)
      dblockp->db_size = size;
    if (file != NULL)
      dblockp->db_file = file;
    if (line != NULL)
      dblockp->db_line = line;
  }
  else {
    
    /* verify that the pointer is user allocated */
    if (IS_BIT_NOT_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_USER;
      _malloc_perror("chunk_write_info");
      return ERROR;
    }
    
    /* count the bits */
    if ((bitc = num_bits(bblockp->bb_size)) == ERROR)
      return ERROR;
    if (bitc < BASIC_BLOCK) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_BAD_SIZE_INFO;
      _malloc_perror("chunk_write_info");
      return ERROR;
    }
    
    /*
     * reset values in the bblocks
     */
    for (bblockc = 0; bblockc < (1 << (bitc - BASIC_BLOCK));
	 bblockc++, bblockp++) {
      
      /* do we need to hop to a new bblock_admp header? */
      if (bblockp == &bblock_admp->ba_block[BB_PER_ADMIN]) {
	if ((bblock_admp = bblock_admp->ba_next) == NULL) {
	  if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	    _malloc_message("bad pointer '%#lx'", pnt);
	  _malloc_errno = MALLOC_BAD_ADMIN_LIST;
	  _malloc_perror("chunk_write_info");
	  return ERROR;
	}
	bblockp = bblock_admp->ba_block;
      }
      
      /* set bblock info */
      bblockp->bb_size = size;
      bblockp->bb_file = file;
      bblockp->bb_line = line;
    }
  }
  
  return NOERROR;
}

/************************** low-level user functions *************************/

/* get a SIZE chunk of memory for FILE at LINE */
EXPORT	char	*_chunk_malloc(file, line, size)
  char		*file;
  unsigned int	line;
  unsigned int	size;
{
  int		bitc, bblockc;
  bblock_t	*bblockp;
  bblock_adm_t	*bblock_admp;
  dblock_t	*dblockp;
  char		*mem;
  
  malloc_count++;				/* counts calls to malloc */
  
  /* do we need to check the levels */
  if (_malloc_debug_level >= DEBUG_CHECK_HEAP)
    (void)_chunk_heap_check(CHUNK_CHECK_LEVEL);
  
  if (size < 0
#if defined(MALLOC_NO_ZERO_SIZE)
      || size == 0
#endif
      ) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad allocation of '%d' bytes from '%s:%d'",
		      size, file, line);
    _malloc_errno = MALLOC_BAD_SIZE;
    _malloc_perror("_chunk_malloc");
    return MALLOC_ERROR;
  }
  if (file == NULL) {
    _malloc_errno = MALLOC_BAD_FILEP;
    _malloc_perror("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* adjust the size */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    size += FENCE_BOTTOM + FENCE_TOP;
  
  /* monitor current allocation level */
  alloc_current += size;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_total += size;
  alloc_one_max = MAX(alloc_one_max, size);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  /* count the bits */
  if ((bitc = num_bits(size)) == ERROR)
    return MALLOC_ERROR;
  
  /* have we exceeded the upper bounds */
  if (bitc > LARGEST_BLOCK) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad allocation of '%d' bytes from '%s:%d'",
		      size, file, line);
    _malloc_errno = MALLOC_TOO_BIG;
    _malloc_perror("_chunk_malloc");
    return MALLOC_ERROR;
  }
  
  /* normalize to SMALLEST_BLOCK.  No use spending 16 bytes to admin 1 byte */
  if (bitc < SMALLEST_BLOCK)
    bitc = SMALLEST_BLOCK;
  
  /* allocate divided block if small */
  if (bitc < BASIC_BLOCK) {
    if ((mem = get_dblock(bitc, &dblockp)) == NULL)
      return MALLOC_ERROR;
    
    dblockp->db_line = (unsigned short)line;
    dblockp->db_size = (unsigned short)size;
    dblockp->db_file = file;
  }
  else {
    
    /*
     * allocate some bblock space
     */
    
    /* handle blocks */
    if ((bblockp = get_bblocks(bitc)) == NULL)
      return MALLOC_ERROR;
    
    /* calculate current bblock admin pointer and memory pointer */
    bblock_admp = (bblock_adm_t *)WHAT_BLOCK(bblockp);
    mem = BLOCK_POINTER(bblock_admp->ba_count +
			(bblockp - bblock_admp->ba_block));
    
    /*
     * initialize the bblocks
     */
    for (bblockc = 0; bblockc < (1 << (bitc - BASIC_BLOCK));
	 bblockc++, bblockp++) {
      
      /* do we need to hop to a new bblock admin header? */
      if (bblockp == &bblock_admp->ba_block[BB_PER_ADMIN]) {
	if ((bblock_admp = bblock_admp->ba_next) == NULL)
	  return MALLOC_ERROR;
	
	bblockp = bblock_admp->ba_block;
      }
      
      /* allocate the block if needed */
      if (IS_BIT_NOT_SET(bblockp->bb_flags, BBLOCK_ALLOCATED))
	if (_heap_alloc(BLOCK_SIZE) == HEAP_ALLOC_ERROR)
	  return MALLOC_ERROR;
      
      if (bblockc == 0)
	bblockp->bb_flags = BBLOCK_START_USER;
      else
	bblockp->bb_flags = BBLOCK_USER;
      
      bblockp->bb_line	= (unsigned short)line;
      bblockp->bb_size	= (unsigned int)size;
      bblockp->bb_file	= file;
    }
  }
  
  /* set our extra memory with FREE_CHAR */
  if (_malloc_debug_level >= DEBUG_FREE_BLANK)
    memset(mem + size, FREE_CHAR, (1 << bitc) - size);
  
  /* write fence post info if needed */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE) {
    fence_write(mem, size);
    mem += FENCE_BOTTOM;
  }
  
  /* do we need to print transaction info? */
  if (_malloc_debug_level >= DEBUG_LOG_TRANS)
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
      _malloc_message("*** alloc: asking %u bytes (%d bits) file '%s:%u', got '%#lx'",
		      size - (FENCE_BOTTOM + FENCE_TOP), bitc, file, line,
		      mem);
    else
      _malloc_message("*** alloc: asking %u bytes (%d bits) file '%s:%u', got '%#lx'",
		      size, bitc, file, line, mem);
  
  return mem;
}

/* frees PNT from the heap, returns FREE_ERROR or FREE_NOERROR */
EXPORT	int	_chunk_free(file, line, pnt)
  char		*file;
  unsigned int	line;
  char		*pnt;
{
  int		bblockc, bitc;
  bblock_t	*bblockp, *first;
  bblock_adm_t	*bblock_admp;
  dblock_t	*dblockp;
  
  free_count++;					/* counts calls to free */
  alloc_cur_pnts--;
  
  /* do we need to check the levels */
  if (_malloc_debug_level >= DEBUG_CHECK_HEAP)
    (void)_chunk_heap_check(CHUNK_CHECK_LEVEL);
  
  /* adjust the pointer down if fence-posting */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    pnt -= FENCE_BOTTOM;
  
  /* find which block it is in */
  if ((bblockp = find_bblock(pnt, NULL, &bblock_admp)) == NULL)
    return FREE_ERROR;
  
  /* are we free'ing a dblock entry? */
  if (IS_BIT_SET(bblockp->bb_flags, BBLOCK_DBLOCK)) {
    
    /* on a mini-block boundary? */
    if ((pnt - bblockp->bb_mem) % (1 << bblockp->bb_bitc) != 0) {
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_NOT_ON_BLOCK;
      _malloc_perror("_chunk_free");
      return FREE_ERROR;
    }
    
    /* find correct dblockp */
    dblockp = bblockp->bb_dblock + (pnt - bblockp->bb_mem) /
      (1 << bblockp->bb_bitc);
    
    if (dblockp->db_bblock == bblockp) {
      /* NOTE: we should run through free list here */
      if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
	_malloc_message("bad pointer '%#lx'", pnt);
      _malloc_errno = MALLOC_ALREADY_FREE;
      _malloc_perror("_chunk_free");
      return FREE_ERROR;
    }
    
    /* check fence-post, probably again */
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
      if (fence_read(pnt, dblockp->db_size, dblockp->db_file,
		     dblockp->db_line) != NOERROR)
	return FREE_ERROR;
    
    /* do we need to print transaction info? */
    if (_malloc_debug_level >= DEBUG_LOG_TRANS)
      if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
	_malloc_message("*** free: freeing dblock pnter '%#lx': size %u, file '%s:%u'",
			pnt + FENCE_BOTTOM,
			dblockp->db_size - (FENCE_BOTTOM + FENCE_TOP),
			dblockp->db_file, dblockp->db_line);
      else
	_malloc_message("*** free: freeing dblock pnter '%#lx': size %u, file '%s:%u'",
			pnt, dblockp->db_size,
			dblockp->db_file, dblockp->db_line);
    
    /* monitor current allocation level */
    alloc_current -= dblockp->db_size;
    
    /* count the bits */
    bitc = bblockp->bb_bitc;
    
    /* rearrange info */
    dblockp->db_bblock	= bblockp;
    dblockp->db_next	= free_dblock[bitc];
    free_dblock[bitc]	= dblockp;
    
    /* should we set free memory with FREE_CHAR? */
    if (_malloc_debug_level >= DEBUG_FREE_BLANK)
      memset(pnt, FREE_CHAR, 1 << bitc);
    
    return FREE_NOERROR;
  }
  
  /* on a block boundary? */
  if (! ON_BLOCK(pnt)) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_NOT_ON_BLOCK;
    _malloc_perror("_chunk_free");
    return FREE_ERROR;
  }
  
  /* are we on a normal block */
  if (IS_BIT_NOT_SET(bblockp->bb_flags, BBLOCK_START_USER)) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_NOT_START_USER;
    _malloc_perror("_chunk_free");
    return FREE_ERROR;
  }
  
  /* check fence-post, probably again */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    if (fence_read(pnt, bblockp->bb_size, bblockp->bb_file,
		   bblockp->bb_line) != NOERROR)
      return FREE_ERROR;
  
  /* do we need to print transaction info? */
  if (_malloc_debug_level >= DEBUG_LOG_TRANS)
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
      _malloc_message("*** free: freeing bblock pnter '%#lx': size %u, file '%s:%u'",
		      pnt + FENCE_BOTTOM,
		      bblockp->bb_size - (FENCE_BOTTOM + FENCE_TOP),
		      bblockp->bb_file, bblockp->bb_line);
    else
      _malloc_message("*** free: freeing bblock pnter '%#lx': size %u, file '%s:%u'",
		      pnt, bblockp->bb_size,
		      bblockp->bb_file, bblockp->bb_line);
  
  /* monitor current allocation level */
  alloc_current -= bblockp->bb_size;
  
  /* count the bits */
  if ((bitc = num_bits(bblockp->bb_size)) == ERROR)
    return FREE_ERROR;
  
  if (bitc < BASIC_BLOCK) {
    if (_malloc_debug_level >= DEBUG_LOG_BAD_POINTER)
      _malloc_message("bad pointer '%#lx'", pnt);
    _malloc_errno = MALLOC_BAD_SIZE_INFO;
    _malloc_perror("_chunk_free");
    return FREE_ERROR;
  }
  
  /* setup free linked-list */
  first = free_bblock[bitc];
  free_bblock[bitc] = bblockp;
  
  bblockp->bb_next = first;
  
  /*
   * initialize the bblocks
   */
  for (bblockc = 0; bblockc < (1 << (bitc - BASIC_BLOCK));
       bblockc++, bblockp++, pnt += BLOCK_SIZE) {
    
    /* do we need to hop to a new bblock_admp header? */
    if (bblockp == &bblock_admp->ba_block[BB_PER_ADMIN]) {
      if ((bblock_admp = bblock_admp->ba_next) == NULL) {
	_malloc_errno = MALLOC_BAD_ADMIN_LIST;
	_malloc_perror("_chunk_free");
	return FREE_ERROR;
      }
      bblockp = bblock_admp->ba_block;
    }
    
    /* set bblock info */
    bblockp->bb_flags	= BBLOCK_FREE;
    bblockp->bb_bitc	= bitc;			/* num bblocks in this chunk */
    bblockp->bb_mem	= pnt;			/* pointer to real memory */
    bblockp->bb_next	= first;
    
    /* should we set free memory with FREE_CHAR? */
    if (_malloc_debug_level >= DEBUG_FREE_BLANK)
      memset(pnt, FREE_CHAR, BLOCK_SIZE);
  }
  
  return FREE_NOERROR;
}

/* reallocate a section of memory */
EXPORT	char	*_chunk_realloc(file, line, oldp, new_size)
  char		*file;
  unsigned int	line;
  char		*oldp;
  unsigned int	new_size;
{
  char		*newp;
  unsigned int	old_size, size;
  int		old_bitc, new_bitc;
  
  realloc_count++;				/* counts calls to realloc */
  
  /* do we need to check the levels */
  if (_malloc_debug_level >= DEBUG_CHECK_HEAP)
    (void)_chunk_heap_check(CHUNK_CHECK_LEVEL);
  
  /* adjust the pointer down if fence-posting */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE) {
    oldp -= FENCE_BOTTOM;
    new_size += FENCE_BOTTOM + FENCE_TOP;
  }
  
  /* get info about old pointer */
  if (chunk_read_info(oldp, &old_size, NULL, NULL) != NOERROR)
    return REALLOC_ERROR;
  
  /* adjust the pointer down if fence-posting */
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    if (fence_read(oldp, old_size, file, line) != NOERROR)
      return REALLOC_ERROR;
  
  /* monitor current allocation level */
  alloc_current += new_size - old_size;
  alloc_maximum = MAX(alloc_maximum, alloc_current);
  alloc_total += new_size;
  alloc_one_max = MAX(alloc_one_max, new_size);
  
  /* monitor pointer usage */
  alloc_cur_pnts++;
  alloc_max_pnts = MAX(alloc_max_pnts, alloc_cur_pnts);
  alloc_tot_pnts++;
  
  /* get the old and new bit sizes */
  if ((old_bitc = num_bits(old_size)) == ERROR)
    return REALLOC_ERROR;
  if ((new_bitc = num_bits(new_size)) == ERROR)
    return REALLOC_ERROR;
  
  /* if we are not realloc copying and the size is the same */
  if (_malloc_debug_level >= DEBUG_REALLOC_COPY || old_bitc != new_bitc) {
    
    /* readjust info */
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE) {
      oldp += FENCE_BOTTOM;
      old_size -= FENCE_BOTTOM + FENCE_TOP;
      new_size -= FENCE_BOTTOM + FENCE_TOP;
    }
    
    /* allocate space for new chunk */
    if ((newp = _chunk_malloc(file, line, new_size)) == MALLOC_ERROR)
      return REALLOC_ERROR;
    
    /*
     * NOTE: _chunk_malloc() already took care of the fence stuff...
     */
    
    /* copy stuff into new section of memory */
    size = MIN(new_size, old_size);
    if (size > 0)
      bcopy(oldp, newp, size);
    
    /* free old pointer */
    if (_chunk_free(file, line, oldp) != FREE_NOERROR)
      return REALLOC_ERROR;
  }
  else {
    /* reuse the old-pointer */
    newp = oldp;
    
    /* rewrite size information */
    if (chunk_write_info(newp, new_size, file, line) != NOERROR)
      return REALLOC_ERROR;
    
    /* write in fence-post info */
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
      fence_write(newp, new_size);
    
    /* set out non-used space above allocation with FREE_CHAR */
    if (_malloc_debug_level >= DEBUG_FREE_BLANK)
      memset(newp + new_size, FREE_CHAR, (1 << new_bitc) - new_size);
    
    /* adjust new pointer forward over fence info */
    if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
      newp += FENCE_BOTTOM;
  }
  
  /* do we need to print transaction info? */
  if (_malloc_debug_level >= DEBUG_LOG_TRANS)
    _malloc_message("*** realloced from %#lx (%u bytes) to %#lx (%u bytes), file '%s:%u'",
		    oldp, old_size, newp, new_size, file, line);
  
  return newp;
}

/***************************** diagnostic routines ***************************/

/* log present free and used lists */
EXPORT	void	_chunk_count()
{
  int		bitc, count;
  char		info[256], tmp[80];
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  
  /* print header bit-counts */
  info[0] = NULLC;
  for (bitc = SMALLEST_BLOCK; bitc <= LARGEST_BLOCK; bitc++) {
    (void)sprintf(tmp, "%*d", FREE_COLUMN, bitc);
    (void)strcat(info, tmp);
  }
  
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "bits: %s", info);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("bits: %s", info);
#endif
  
  /* dump the free (and later used) list counts */
  info[0] = NULLC;
  for (bitc = SMALLEST_BLOCK; bitc <= LARGEST_BLOCK; bitc++) {
    if (bitc < BASIC_BLOCK)
      for (count = 0, dblockp = free_dblock[bitc]; dblockp != NULL;
	   count++, dblockp = dblockp->db_next);
    else
      for (count = 0, bblockp = free_bblock[bitc]; bblockp != NULL;
	   count++, bblockp = bblockp->bb_next);
    
    (void)sprintf(tmp, "%*d", FREE_COLUMN, count);
    (void)strcat(info, tmp);
  }
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "free: %s", info);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("free: %s", info);
#endif
}

/* log statistics on the heap */
EXPORT	void	_chunk_stats()
{
  long		overhead;
  
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("getting chunk statistics");
  
  /* general heap information */
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "heap start %#lx, heap end %#lx, heap size %ld bytes",
	 _heap_base, _heap_last, HEAP_SIZE());
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("heap start %#lx, heap end %#lx, heap size %ld bytes",
		    _heap_base, _heap_last, HEAP_SIZE());
#endif
  
  /* log user allocation information */
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "alloc calls: malloc %d, realloc %d, calloc %d, free %d",
	 malloc_count, realloc_count, _calloc_count, free_count);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("alloc calls: malloc %d, realloc %d, calloc %d, free %d",
		    malloc_count, realloc_count, _calloc_count, free_count);
#endif
  
#if defined(ANTAIRE)
  logger(LOGGER_INFO,
	 "total amount of memory allocated:   %ld bytes (%ld pnts)",
	 alloc_total, alloc_tot_pnts);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("total amount of memory allocated:   %ld bytes (%ld pnts)",
		    alloc_total, alloc_tot_pnts);
#endif
  
#if defined(ANTAIRE)
  logger(LOGGER_INFO,
	 "maximum memory in use at one time:  %ld bytes (%ld pnts)",
	 alloc_maximum, alloc_max_pnts);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("maximum memory in use at one time:  %ld bytes (%ld pnts)",
		    alloc_maximum, alloc_max_pnts);
#endif
  
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "maximum memory alloced with 1 call: %ld bytes",
	 alloc_one_max);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("maximum memory alloced with 1 call: %ld bytes",
		    alloc_one_max);
#endif
  
  /* log administration information */
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "basic-blocks %d, divided blocks %d",
	 bblock_count, dblock_count);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("basic-blocks %d, divided blocks %d",
		    bblock_count, dblock_count);
#endif
  
  if (_malloc_debug_level >= DEBUG_CHECK_FENCE)
    overhead = alloc_max_pnts * (FENCE_BOTTOM + FENCE_TOP) +
      bblock_adm_count * BLOCK_SIZE + dblock_adm_count * BLOCK_SIZE;
  else
    overhead = bblock_adm_count * BLOCK_SIZE + dblock_adm_count * BLOCK_SIZE;
  
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "final heap administration overhead: %ld bytes (%d%%)",
	 overhead, (overhead * 100) / alloc_maximum);
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("final heap administration overhead: %ld bytes (%d%%)",
		    overhead, (overhead * 100) / alloc_maximum);
#endif
}

/* dump the unfreed memory, logs the unfreed information to logger */
EXPORT	void	_chunk_dump_not_freed()
{
  bblock_adm_t	*this;
  bblock_t	*bblockp;
  dblock_t	*dblockp;
  char		*mem;
  int		unknown_sizec = 0, unknownc = 0;
  int		sizec = 0, dblockc = 0, bblockc = 0;
  
  /* check the heap? */
  if (_malloc_debug_level >= DEBUG_CHECK_HEAP)
    (void)_chunk_heap_check(CHUNK_CHECK_LEVEL);
  
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("dumping the unfreed pointers");
  
  /* has anything been alloced yet? */
  if ((this = bblock_adm_head) == NULL)
    return;
  
  /* check out the basic blocks */
  for (bblockp = this->ba_block;; bblockp++) {
    
    /* are we at the end of the bb_admin section */
    if (bblockp >= this->ba_block + BB_PER_ADMIN) {
      this = this->ba_next;
      
      /* are we done? */
      if (this == NULL)
	break;
      
      bblockp = this->ba_block;
    }
    
    /*
     * check for different types
     */
    switch (bblockp->bb_flags) {
      
    case BBLOCK_ADMIN:
    case BBLOCK_DBLOCK:
    case BBLOCK_FREE:
    case BBLOCK_USER:
      break;
      
    case BBLOCK_START_USER:
      /* find pointer to memory chunk */
      mem = BLOCK_POINTER(this->ba_count + (bblockp - this->ba_block));
      
      /* unknown pointer? */
      if (strcmp(DEFAULT_FILE, bblockp->bb_file) == 0 &&
	  bblockp->bb_line == DEFAULT_LINE) {
	unknownc++;
	unknown_sizec += bblockp->bb_size;
      }
      else {
#if defined(ANTAIRE)
	logger(LOGGER_INFO,
	       "bblock: %#9lx (%8d bytes) from '%s:%d' not freed",
	       mem, bblockp->bb_size, bblockp->bb_file,
	       bblockp->bb_line);
#else
	if (_malloc_debug_level >= DEBUG_LOG_STATS)
	  _malloc_message("bblock: %#9lx (%8d bytes) from '%s:%d' not freed",
			  mem, bblockp->bb_size, bblockp->bb_file,
			  bblockp->bb_line);
#endif
      }
      
      sizec += bblockp->bb_size;
      bblockc++;
      break;
      
    case BBLOCK_DBLOCK_ADMIN:
      
      for (dblockp = bblockp->bb_slotp->da_block;
	   dblockp < bblockp->bb_slotp->da_block + DB_PER_ADMIN; dblockp++) {
	
	/* verify if we have a good bblock pointer and good back pointer */
	if (dblockp->db_bblock == NULL && dblockp->db_next == NULL)
	  continue;
	
	/* check out dblock pointer and next pointer (if free) */
	if (dblockp->db_next == NULL || IS_IN_HEAP(dblockp->db_next))
	  continue;
	
	/* find pointer to memory chunk */
	mem = bblockp->bb_mem +
	  (dblockp - bblockp->bb_dblock) * (1 << bblockp->bb_bitc);
	
	/* unknown pointer? */
	if (strcmp(DEFAULT_FILE, dblockp->db_file) == 0 &&
	    dblockp->db_line == DEFAULT_LINE) {
	  unknownc++;
	  unknown_sizec += dblockp->db_size;
	}
	else {
#if defined(ANTAIRE)
	  logger(LOGGER_INFO,
		 "dblock: %#9lx (%8d bytes) from '%s:%d' not freed",
		 mem, dblockp->db_size, dblockp->db_file,
		 dblockp->db_line);
#else
	  if (_malloc_debug_level >= DEBUG_LOG_STATS)
	    _malloc_message("dblock: %#9lx (%8d bytes) from '%s:%d' not freed",
			    mem, dblockp->db_size, dblockp->db_file,
			    dblockp->db_line);
#endif
	}
	
	sizec += dblockp->db_size;
	dblockc++;
      }
      break;
    }
  }
  
  /* copy out size of pointers */
  if (bblockc + dblockc > 0) {
#if defined(ANTAIRE)
    logger(LOGGER_INFO,
	   "total not freed: %d bblock, %d dblock, %d byte%s",
	   bblockc, dblockc, sizec, (sizec == 1 ? "" : "s"));
#else
    if (_malloc_debug_level >= DEBUG_LOG_STATS)
      _malloc_message("total not freed: %d bblock, %d dblock, %d byte%s",
		      bblockc, dblockc, sizec, (sizec == 1 ? "" : "s"));
#endif
#if defined(ANTAIRE)
    logger(LOGGER_INFO, "unknown not freed: %d pointers, %d byte%s",
	   unknownc, unknown_sizec,
	   (unknown_sizec == 1 ? "" : "s"));
#else
    if (_malloc_debug_level >= DEBUG_LOG_STATS)
      _malloc_message("unknown not freed: %d pointers, %d byte%s",
		      unknownc, unknown_sizec,
		      (unknown_sizec == 1 ? "" : "s"));
#endif
  }
}

/* log an entry for the heap structure */
EXPORT	void	_chunk_log_heap_map()
{
  bblock_adm_t	*bblock_admp;
  char		line[BB_PER_ADMIN + 10];
  int		linec, bblockc, bb_adminc;
  
  if (_malloc_debug_level >= DEBUG_LOG_ADMIN)
    _malloc_message("logging heap map information");
  
  /* do we need to check the levels? */
  if (_malloc_debug_level >= DEBUG_CHECK_HEAP)
    (void)_chunk_heap_check(CHUNK_CHECK_LEVEL);
  
#if defined(ANTAIRE)
  logger(LOGGER_INFO, "heap-base = %#lx, heap-end = %#lx, size = %ld bytes",
	 _heap_base, _heap_last, HEAP_SIZE());
#else
  if (_malloc_debug_level >= DEBUG_LOG_STATS)
    _malloc_message("heap-base = %#lx, heap-end = %#lx, size = %ld bytes",
		    _heap_base, _heap_last, HEAP_SIZE());
#endif
  
  for (bb_adminc = 0, bblock_admp = bblock_adm_head; bblock_admp != NULL;
       bb_adminc++, bblock_admp = bblock_admp->ba_next) {
    linec = 0;
    
    for (bblockc = 0; bblockc < BB_PER_ADMIN; bblockc++) {
      if (IS_BIT_NOT_SET(bblock_admp->ba_block[bblockc].bb_flags,
			 BBLOCK_ALLOCATED)) {
	line[linec++] = '_';
	continue;
      }
      
      if (IS_BIT_SET(bblock_admp->ba_block[bblockc].bb_flags,
		     BBLOCK_START_USER)) {
	line[linec++] = 'S';
	continue;
      }
      
      if (IS_BIT_SET(bblock_admp->ba_block[bblockc].bb_flags,
		     BBLOCK_USER)) {
	line[linec++] = 'U';
	continue;
      }
      
      if (IS_BIT_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_ADMIN)) {
	line[linec++] = 'A';
	continue;
      }
      
      if (IS_BIT_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_DBLOCK)) {
	line[linec++] = 'd';
	continue;
      }
      
      if (IS_BIT_SET(bblock_admp->ba_block[bblockc].bb_flags,
		     BBLOCK_DBLOCK_ADMIN)) {
	line[linec++] = 'a';
	continue;
      }
      
      if (IS_BIT_SET(bblock_admp->ba_block[bblockc].bb_flags, BBLOCK_FREE)) {
	line[linec++] = 'F';
	continue;
      }
    }
    
    /* dumping a line to the logfile */
    if (linec > 0) {
      line[linec] = NULLC;
#if defined(ANTAIRE)
      logger(LOGGER_INFO, "S%d:%s", bb_adminc, line);
#else
      if (_malloc_debug_level >= DEBUG_LOG_STATS)
	_malloc_message("S%d:%s", bb_adminc, line);
#endif
    }
  }
}
