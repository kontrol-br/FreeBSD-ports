# $FreeBSD$

PORTNAME=	Kontrol-upgrade
PORTVERSION=	2.0.0
PORTREVISION=	0
CATEGORIES=	sysutils
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@kontrol.com.br
COMMENT=	Kontrol upgrade script

LICENSE=	APACHE20

NO_MTREE=	yes
NO_BUILD=	yes

OPTIONS_DEFINE=	DEBUG
DEBUG_DESC=	Enable verbose debug logging by default
OPTIONS_DEFAULT=DEBUG

PLIST_FILES=	etc/rc.d/kontrol_upgrade \
		libexec/Kontrol-upgrade \
		sbin/Kontrol-upgrade \
		sbin/Kontrol-repo-setup \
		share/doc/Kontrol-upgrade/UPGRADE_ENGINE.md \
		share/Kontrol/tests/run-tests.sh

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

post-patch:
.if defined(PORT_OPTIONS) && ${PORT_OPTIONS:MDEBUG}
	${REINPLACE_CMD} 's/^DEBUG_DEFAULT=0/DEBUG_DEFAULT=1/' \
		${WRKSRC}/Kontrol-upgrade
.endif

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/sbin
	${MKDIR} ${STAGEDIR}${PREFIX}/libexec
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol
	${MKDIR} ${STAGEDIR}${PREFIX}/etc/rc.d
	${MKDIR} ${STAGEDIR}${PREFIX}/share/doc/Kontrol-upgrade
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol/tests
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade \
		${STAGEDIR}${PREFIX}/libexec/Kontrol-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade.wrapper \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-repo-setup \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-repo-setup
	${INSTALL_SCRIPT} ${WRKSRC}/kontrol_upgrade_rc \
		${STAGEDIR}${PREFIX}/etc/rc.d/kontrol_upgrade
	${INSTALL_DATA} ${WRKSRC}/UPGRADE_ENGINE.md \
		${STAGEDIR}${PREFIX}/share/doc/Kontrol-upgrade/UPGRADE_ENGINE.md
	${INSTALL_SCRIPT} ${WRKSRC}/tests/run-tests.sh \
		${STAGEDIR}${PREFIX}/share/Kontrol/tests/run-tests.sh

.include <bsd.port.mk>
