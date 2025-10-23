PORTNAME=	Kontrol-upgrade
PORTVERSION=	1.3.14
CATEGORIES=	sysutils
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	coreteam@pfsense.org
COMMENT=	Kontrol upgrade script

LICENSE=	APACHE20

NO_MTREE=	yes
NO_BUILD=	yes

PRODUCT_NAME=	${PORTNAME:S/-upgrade//}

PLIST_FILES=	libexec/install-boot.sh \
		libexec/${PRODUCT_NAME}-upgrade \
		sbin/install-boot \
		sbin/${PRODUCT_NAME}-repo-setup \
		sbin/${PRODUCT_NAME}-upgrade

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/sbin
	${MKDIR} ${STAGEDIR}${PREFIX}/libexec
	${MKDIR} ${STAGEDIR}${PREFIX}/share/${PRODUCT_NAME}
	${INSTALL_SCRIPT} ${WRKSRC}/${PRODUCT_NAME}-upgrade \
		${STAGEDIR}${PREFIX}/libexec
	${INSTALL_SCRIPT} ${WRKSRC}/${PRODUCT_NAME}-upgrade.wrapper \
		${STAGEDIR}${PREFIX}/sbin/${PRODUCT_NAME}-upgrade
	${INSTALL_SCRIPT} ${WRKSRC}/install-boot \
		${STAGEDIR}${PREFIX}/sbin
	${INSTALL_SCRIPT} ${WRKSRC}/install-boot.sh \
		${STAGEDIR}${PREFIX}/libexec
	${INSTALL_SCRIPT} ${WRKSRC}/${PRODUCT_NAME}-repo-setup \
		${STAGEDIR}${PREFIX}/sbin/${PRODUCT_NAME}-repo-setup

.include <bsd.port.mk>
