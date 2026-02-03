# $FreeBSD$

PORTNAME=	pfSense-upgrade
PORTVERSION=	1.2.4
PORTREVISION=	# empty
CATEGORIES=	sysutils
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	coreteam@pfsense.org
COMMENT=	pfSense upgrade script

LICENSE=	APACHE20

NO_MTREE=	yes
NO_BUILD=	yes

PLIST_FILES=	libexec/pfSense-upgrade \
		sbin/pfSense-upgrade \
		sbin/Kontrol-repo-setup

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/sbin
	${MKDIR} ${STAGEDIR}${PREFIX}/libexec
	${MKDIR} ${STAGEDIR}${PREFIX}/share/pfSense
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade \
		${STAGEDIR}${PREFIX}/libexec/pfSense-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade.wrapper \
		${STAGEDIR}${PREFIX}/sbin/pfSense-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-repo-setup \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-repo-setup

.include <bsd.port.mk>
