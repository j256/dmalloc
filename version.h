/*
 * version string for the library
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
 * $Id: version.h,v 1.100 2000/07/24 20:04:02 gray Exp $
 */

#ifndef __VERSION_H__
#define __VERSION_H__

/*
 * NOTE to gray: whenever this is changed, a corresponding entry
 * should be entered in:
 *
 *	Changlog
 *	RELEASE.html
 *	NEWS
 *	dmalloc.spec
 *	dmalloc.texi (2 places)
 *	dmalloc.h.3 (DMALLOC_VERSION defines at top of file)
 *
 * Make sure to also cvs tag the release.  dmalloc_release_X_X_X[_bX]
 */
static	char	*dmalloc_version = "4.7.0";

/* Version Date: $Date: 2000/07/24 20:04:02 $ */

#endif /* ! __VERSION_H__ */
