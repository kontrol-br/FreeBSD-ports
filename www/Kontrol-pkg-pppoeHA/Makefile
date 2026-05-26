# $FreeBSD$

PORTNAME=	Kontrol-pkg-pppoeHA
PORTVERSION=	0.1.2
CATEGORIES=	www
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@kontrol.com.br
COMMENT=	Kontrol PPPoE high availability package for CARP-based failover
LICENSE=	APACHE20

NO_BUILD=	yes
NO_MTREE=	yes

SUB_FILES=	pkg-install pkg-deinstall
SUB_LIST=	PORTNAME=${PORTNAME}

do-extract:
	${MKDIR} ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/pkg
	${MKDIR} ${STAGEDIR}${PREFIX}/sbin
	${MKDIR} ${STAGEDIR}${PREFIX}/etc/rc.d
	${MKDIR} ${STAGEDIR}${PREFIX}/etc/devd
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol-pkg-pppoeHA
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/pppoe_ha.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/pppoe_ha.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_SCRIPT} ${FILESDIR}${PREFIX}/sbin/pppoe_ha_event \
		${STAGEDIR}${PREFIX}/sbin
	${INSTALL_SCRIPT} ${FILESDIR}${PREFIX}/sbin/pppoe_ha_event.php \
		${STAGEDIR}${PREFIX}/sbin
	${INSTALL_SCRIPT} ${FILESDIR}${PREFIX}/etc/rc.d/pppoe_ha \
		${STAGEDIR}${PREFIX}/etc/rc.d
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/etc/devd/pppoe_ha.conf \
		${STAGEDIR}${PREFIX}/etc/devd
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/share/Kontrol-pkg-pppoeHA/info.xml \
		${STAGEDIR}${PREFIX}/share/Kontrol-pkg-pppoeHA
	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${PREFIX}/pkg/pppoe_ha.xml
	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${PREFIX}/share/Kontrol-pkg-pppoeHA/info.xml

.include <bsd.port.mk>
