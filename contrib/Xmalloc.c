/*
 * Provide malloc library replacements for X Window System heap routines
 * 	XtCalloc
 * 	XtFree
 * 	XtMalloc
 * 	XtRealloc
 * so that we can get accurate caller data.
 *
 * ddhill@zk3.dec.com
 */
#define DMALLOC_DISABLE

#include "dmalloc.h"
#include "conf.h"

#include "dmalloc_lp.h"
#include "return.h"

#define DO_XT_ENTRY_POINTS 1
#if DO_XT_ENTRY_POINTS

static _XtAllocError(char * name)
{
  write(2,"Xt Error: ",10);
  write(2,name,strlen(name));
  write(1,"\n");
  exit(1);
}

EXPORT	char	*XtMalloc(unsigned size)
{
  char	*ptr;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  ptr = malloc(size > 0 ? size : 1);
  if (ptr == NULL)
    _XtAllocError("malloc");
  
  return ptr;
}

/*
 * release PNT in the heap, returning FREE_ERROR, FREE_NOERROR or void
 * depending on whether STDC is defined by your compiler.
 */
EXPORT	void	XtFree(char * pnt)
{
  char	*ptr;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (ptr)
    free(pnt);
}

EXPORT	char	*XtCalloc(unsigned num_elements, unsigned size)
{
  char	*ptr;
  
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  ptr = calloc(num_elements, size ? size : 1);
  if (ptr == NULL)
    _XtAllocError("calloc");
  
  return ptr;
}

/*
 * resizes OLD_PNT to SIZE bytes and return the new space after either copying
 * all of OLD_PNT to the new area or truncating.  returns 0L on error.
 */
EXPORT	char	*XtRealloc(char * ptr, unsigned size)
{
  SET_RET_ADDR(_dmalloc_file, _dmalloc_line);
  
  if (ptr == NULL)
    return malloc(size > 0 ? size : 1);
  
  ptr = realloc(ptr, size > 0 ? size : 1);
  if (ptr == NULL)
    _XtAllocError("realloc");
  
  return ptr;
}

#endif /* DO_XT_ENTRY_POINTS */
