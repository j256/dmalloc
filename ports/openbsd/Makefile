#
# BSD /usr/ports Makefile
#
# $Id: Makefile,v 1.6 2003/09/08 14:50:43 gray Exp $
# Original from: Gray Watson http://256.com/gray/email.html
#

DISTNAME=		dmalloc-5.2.2
#PKGNAME=		???
CATEGORIES=     	devel

HOMEPAGE=		http://dmalloc.com/

# where to send bitches about this port
MAINTAINER=		http://256.com/gray/email.html

# Licensing:  This determines what we can distribute.
PERMIT_PACKAGE_CDROM=	Yes
PERMIT_PACKAGE_FTP=	Yes
PERMIT_DISTFILES_CDROM=	Yes
PERMIT_DISTFILES_FTP=	Yes

# where the source files and patches can be fetched
MASTER_SITES=		http://dmalloc.com/releases/ http://download.sourceforge.net/dmalloc/
#MASTER_SITE_SUBDIR=	???
#DISTFILES=		???
EXTRACT_SUFX=		.tgz

# build/configuration variables
# requires bsd.port.mk 1.89 or later
#SEPARATE_BUILD=	concurrent  (build simultaneously on all arches)
SEPARATE_BUILD=		simple  #(configure/build still recreates files in source)
USE_AUTOCONF=		Yes
CONFIGURE_ARGS+=	--enable-threads --enable-shlib

# NOTE: we make the test program first because otherwise it screws up
# and tried to use the .so instead of the .a for some stupid reason
ALL_TARGET=		dmalloc_t all light
INSTALL_TARGET=		install installinfo installdocs

.include <bsd.port.mk>
