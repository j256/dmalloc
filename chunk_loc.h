/*
 * local malloc defines
 *
 * Copyright 1991 by the Antaire Corporation
 *
 * @(#)chunk_loc.h	1.12 GRAY@ANTAIRE.COM 12/7/91
 */

/* defines for the malloc subsystem */
#define BASIC_BLOCK		12	/* basic block size in bits */
#define SMALLEST_BLOCK		 4	/* smallest block size in bits */
#define LARGEST_BLOCK		24	/* allowable allocation in bits */
#define LARGEST_NORMAL_BLOCK	16	/* largest normal allocation in bits */

#define BLOCK_SIZE		(1 << BASIC_BLOCK)

/* number of blocks in the administrative structures */
#define BB_PER_ADMIN	((BLOCK_SIZE - \
			  (sizeof(long) + sizeof(int) + sizeof(char *) + \
			   sizeof(long))) / sizeof(bblock_t))
#define DB_PER_ADMIN	((BLOCK_SIZE - (sizeof(long) + sizeof(long))) \
			 / sizeof(dblock_t))

#define CHUNK_MAGIC_BASE	0xDEA007	/* base magic number */
#define CHUNK_MAGIC_TOP		0x976DEAD	/* top magic number */

#define WHAT_BLOCK(pnt)		(((long)(pnt) / BLOCK_SIZE) * BLOCK_SIZE)

/* bb_flags values */
#define BBLOCK_ALLOCATED	0x3F		/* block has been allocated */
#define BBLOCK_START_USER	0x01		/* start of some user space */
#define BBLOCK_USER		0x02		/* allocated by user space */
#define BBLOCK_ADMIN		0x04		/* pointing to bblock admin */
#define BBLOCK_DBLOCK		0x08		/* pointing to divided block */
#define BBLOCK_DBLOCK_ADMIN	0x10		/* pointing to dblock admin */
#define BBLOCK_FREE		0x20		/* block is free */

/*
 * single divided-block administrative structure
 */
struct dblock_st {
  union {
    struct {
      unsigned short	nu_size;		/* size of contiguous area */
      unsigned short	nu_line;		/* line where it was alloced */
    } in_nums;
    
    struct bblock_st	*in_bblock;		/* pointer to the bblock */
  } db_info;
  
  /* to reference union and struct elements as db elements */
#define db_bblock	db_info.in_bblock	/* F */
#define db_size		db_info.in_nums.nu_size	/* U */
#define db_line		db_info.in_nums.nu_line	/* U */
  
  union {
    struct dblock_st	*pn_next;		/* next in the free list */
    char		*pn_file;		/* .c filename where alloced */
  } db_pnt;

  /* to reference union elements as db elements */
#define db_next		db_pnt.pn_next		/* F */
#define db_file		db_pnt.pn_file		/* U */

};
typedef struct dblock_st	dblock_t;

/*
 * single basic-block administrative structure
 */
struct bblock_st {
  unsigned short	bb_flags;		/* what it is */
  
  union {
    unsigned short	nu_bitc;		/* chunk size */
    unsigned short	nu_line;		/* line where it was alloced */
  } bb_num;
  
  /* to reference union elements as bb elements */
#define bb_bitc		bb_num.nu_bitc		/* DF */
#define bb_line		bb_num.nu_line		/* U */
  
  union {
    unsigned int	in_count;		/* admin count number */
    dblock_t		*in_dblock;		/* pointer to dblock info */
    struct bblock_st	*in_next;		/* next in free list */
    unsigned int	in_size;		/* size of allocation */
  } bb_info;
  
  /* to reference union elements as bb elements */
#define bb_count	bb_info.in_count	/* A */
#define	bb_dblock	bb_info.in_dblock	/* D */
#define	bb_next		bb_info.in_next		/* F */
#define	bb_size		bb_info.in_size		/* U */
  
  union {
    struct dblock_adm_st	*pn_slotp;	/* pointer to db_admin block */
    struct bblock_adm_st	*pn_adminp;	/* pointer to bb_admin block */
    char			*pn_mem;	/* memory associated to it */
    char			*pn_file;	/* .c filename where alloced */
  } bb_pnt;
  
  /* to reference union elements as bb elements */
#define	bb_slotp	bb_pnt.pn_slotp		/* a */
#define	bb_adminp	bb_pnt.pn_adminp	/* A */
#define	bb_mem		bb_pnt.pn_mem		/* DF */
#define	bb_file		bb_pnt.pn_file		/* U */
  
};
typedef struct bblock_st	bblock_t;

/*
 * collection of bblock admin structures
 */
struct bblock_adm_st {
  long			ba_magic1;		/* bottom magic number */
  int			ba_count;		/* position in bblock array */
  bblock_t		ba_block[BB_PER_ADMIN];	/* bblock admin info */
  struct bblock_adm_st	*ba_next;		/* next bblock adm struct */
  long			ba_magic2;		/* top magic number */
};
typedef struct bblock_adm_st	bblock_adm_t;

/*
 * collection of dblock admin structures
 */
struct dblock_adm_st {
  long			da_magic1;		/* bottom magic number */
  dblock_t		da_block[DB_PER_ADMIN];	/* dblock admin info */
  long			da_magic2;		/* top magic number */
};
typedef struct dblock_adm_st	dblock_adm_t;
