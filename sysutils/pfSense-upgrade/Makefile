# $FreeBSD$

PORTNAME=	Kontrol-upgrade
PORTVERSION=	2.3.2
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
		sbin/Kontrol-upgrade

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/sbin
	${MKDIR} ${STAGEDIR}${PREFIX}/libexec
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade \
		${STAGEDIR}${PREFIX}/libexec
	${INSTALL_SCRIPT} ${WRKSRC}/Kontrol-upgrade.wrapper \
		${STAGEDIR}${PREFIX}/sbin/Kontrol-upgrade

.include <bsd.port.mk>
