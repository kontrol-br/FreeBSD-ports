# $FreeBSD$

PORTNAME=	Kontrol-pkg-OpenVPN-Schedule
PORTVERSION=	0.1.0
CATEGORIES=	www
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@kontrol.com.br
COMMENT=	Kontrol OpenVPN login schedule enforcement package
LICENSE=	APACHE20

NO_BUILD=	yes
NO_MTREE=	yes

SUB_FILES=	pkg-install pkg-deinstall
SUB_LIST=	PORTNAME=${PORTNAME}

do-extract:
	${MKDIR} ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/pkg
	${MKDIR} ${STAGEDIR}${PREFIX}/www
	${MKDIR} ${STAGEDIR}${PREFIX}/share/doc/${PORTNAME}
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/openvpn_schedule.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/openvpn_schedule.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/openvpn_schedule_hook.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/www/openvpn_schedule.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/share/doc/${PORTNAME}/README.md \
		${STAGEDIR}${PREFIX}/share/doc/${PORTNAME}
	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${PREFIX}/pkg/openvpn_schedule.xml

.include <bsd.port.mk>
