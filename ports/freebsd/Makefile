#
# $Id: Makefile,v 1.11 2004/01/28 14:51:37 gray Exp $
#

PORTNAME=	dmalloc
PORTVERSION=	5.3.0
CATEGORIES=	devel
MASTER_SITES=	${MASTER_SITE_SOURCEFORGE}
MASTER_SITE_SUBDIR=	dmalloc
EXTRACT_SUFX=	.tgz

# Thanks much to Jeremy Shaffner <jeremy@external.org>
MAINTAINER=	http://256.com/gray/email.html
COMMENT=	Portable heap memory debugging library.

INSTALLS_SHLIB=	YES
GNU_CONFIGURE=	YES
USE_REINPLACE=	yes
CONFIGURE_ARGS+=--enable-threads --enable-shlib

# NOTE: we make the test program first because otherwise it screws up
# and tried to use the .so instead of the .a for some stupid reason
ALL_TARGET=	dmalloc_t all light

pre-patch:
	@${REINPLACE_CMD} -e 's|-lpthread|${PTHREAD_LIBS}|g' ${WRKSRC}/configure

do-install:
	${INSTALL_PROGRAM} ${WRKSRC}/dmalloc ${PREFIX}/bin
	${INSTALL_DATA} ${WRKSRC}/dmalloc.h ${PREFIX}/include
	${INSTALL_DATA} ${WRKSRC}/libdmallocthcxx.so ${PREFIX}/lib/libdmallocthcxx.so.1
	@${LN} -sf ${PREFIX}/lib/libdmallocthcxx.so.1 ${PREFIX}/lib/libdmallocthcxx.so
	${INSTALL_DATA} ${WRKSRC}/libdmallocthcxx.a ${PREFIX}/lib
	${INSTALL_DATA} ${WRKSRC}/libdmallocth.so ${PREFIX}/lib/libdmallocth.so.1
	@${LN} -sf ${PREFIX}/lib/libdmallocth.so.1 ${PREFIX}/lib/libdmallocth.so
	${INSTALL_DATA} ${WRKSRC}/libdmallocth.a ${PREFIX}/lib
	${INSTALL_DATA} ${WRKSRC}/libdmalloc.so ${PREFIX}/lib/libdmalloc.so.1
	@${LN} -sf ${PREFIX}/lib/libdmalloc.so.1 ${PREFIX}/lib/libdmalloc.so
	${INSTALL_DATA} ${WRKSRC}/libdmalloc.a ${PREFIX}/lib
.if !defined(NOPORTDOCS)
	@${MKDIR} ${PREFIX}/share/doc/dmalloc
	${INSTALL_DATA} ${WRKSRC}/docs/dmalloc.html ${PREFIX}/share/doc/dmalloc
.endif

post-install:
	${INSTALL_DATA} ${WRKSRC}/docs/dmalloc.info ${PREFIX}/info
	@install-info ${PREFIX}/info/dmalloc.info ${PREFIX}/info/dir

.include <bsd.port.mk>
