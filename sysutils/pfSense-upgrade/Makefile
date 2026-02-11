# $FreeBSD$

PORTNAME=	Kontrol-upgrade
PORTVERSION=	1.3.50
PORTREVISION=	# empty
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

PLIST_FILES=	libexec/Kontrol-upgrade \
		sbin/Kontrol-upgrade \
		sbin/Kontrol-repo-setup

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
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade \
		${STAGEDIR}${PREFIX}/libexec/Kontrol-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade.wrapper \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-repo-setup \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-repo-setup

.include <bsd.port.mk>
