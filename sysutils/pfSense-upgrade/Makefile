# $FreeBSD$

PORTNAME=	pfSense-upgrade
PORTVERSION=	1.3.1
PORTREVISION=	6
CATEGORIES=	sysutils
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	coreteam@pfsense.org
COMMENT=	pfSense upgrade script

LICENSE=	APACHE20

NO_MTREE=	yes
NO_BUILD=	yes

PLIST_FILES=	libexec/Kontrol-upgrade \
		sbin/Kontrol-upgrade \
		etc/rc.d/kontrol-upgrade-stage \
		share/Kontrol/upgrade/kontrol_upgrade_status.inc

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/sbin
	${MKDIR} ${STAGEDIR}${PREFIX}/libexec
	${MKDIR} ${STAGEDIR}${PREFIX}/etc/rc.d
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol/upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade \
		${STAGEDIR}${PREFIX}/libexec
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade.wrapper \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/kontrol-upgrade-stage.rc \
		${STAGEDIR}${PREFIX}/etc/rc.d/kontrol-upgrade-stage
	${INSTALL_DATA} ${WRKSRC}/kontrol_upgrade_status.inc \
		${STAGEDIR}${PREFIX}/share/Kontrol/upgrade/kontrol_upgrade_status.inc

.include <bsd.port.mk>
