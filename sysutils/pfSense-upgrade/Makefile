# $FreeBSD$

PORTNAME=	Kontrol-upgrade
PORTVERSION=	1.3.35
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

PLIST_FILES=	libexec/Kontrol-upgrade \
		sbin/Kontrol-upgrade \
		sbin/Kontrol-repo-setup

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

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
