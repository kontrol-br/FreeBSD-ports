# $FreeBSD$

PORTNAME=	Kontrol-pkg-Kontrol-ID
PORTVERSION=	0.5.0
PORTREVISION=	12
CATEGORIES=	www
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@kontrol.com.br
COMMENT=	Standalone authenticated secure web gateway for Kontrol

LICENSE=	APACHE20

RUN_DEPENDS=	Kontrol-pkg-Kontrol-ID-Policy>=1.0.0_5:www/Kontrol-pkg-Kontrol-ID-Policy \
		kontrol-id>=5.4.23_6:www/kontrol-id \
		samba416>=4.16:net/samba416
LIB_DEPENDS=	libcurl.so:ftp/curl \
		libsqlite3.so:databases/sqlite3 \
		libxml2.so:textproc/libxml2

CONFLICTS_INSTALL=	pfSense-pkg-squid Kontrol-pkg-KontrolSquid \
		Kontrol-pkg-E2guardian5 Kontrol-pkg-E2guardian54

USES=		compiler:c++17-lang localbase pkgconfig
USE_CXXSTD=	c++17
NO_WRKSUBDIR=	yes
NO_MTREE=	yes

SUB_FILES=	pkg-install pkg-deinstall kontrol_id_winbind
SUB_LIST=	PORTNAME=${PORTNAME}

do-extract:
	${MKDIR} ${WRKSRC}
	${CP} ${FILESDIR}/src/kontrol_identity_helper.cpp ${WRKSRC}/

do-build:
	${CXX} ${CXXFLAGS} -fvisibility=hidden \
		`${LOCALBASE}/bin/pkgconf --cflags libcurl sqlite3 libxml-2.0` \
		${WRKSRC}/kontrol_identity_helper.cpp \
		-o ${WRKSRC}/kontrol-identity-helper \
		${LDFLAGS} `${LOCALBASE}/bin/pkgconf --libs libcurl sqlite3 libxml-2.0`

do-install:
	${MKDIR} ${STAGEDIR}${PREFIX}/libexec
	${MKDIR} ${STAGEDIR}${PREFIX}/etc/rc.d
	${MKDIR} ${STAGEDIR}${PREFIX}/pkg
	${MKDIR} ${STAGEDIR}${PREFIX}/www/kontrol-id
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID
	${MKDIR} ${STAGEDIR}/etc/inc/priv
	${INSTALL_PROGRAM} ${WRKSRC}/kontrol-identity-helper \
		${STAGEDIR}${PREFIX}/libexec/kontrol-identity-helper
	${INSTALL_SCRIPT} ${FILESDIR}${PREFIX}/libexec/kontrol-basic-auth.php \
		${STAGEDIR}${PREFIX}/libexec/kontrol-basic-auth
	${INSTALL_SCRIPT} ${FILESDIR}${PREFIX}/libexec/kontrol-id-policy-sync.php \
		${STAGEDIR}${PREFIX}/libexec/kontrol-id-policy-sync.php
	${INSTALL_SCRIPT} ${WRKDIR}/kontrol_id_winbind \
		${STAGEDIR}${PREFIX}/etc/rc.d
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol_id.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol_id.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol_id_sync.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_status.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_domain.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_policy.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_portal.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_enrollment.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol-id/index.html \
		${STAGEDIR}${PREFIX}/www/kontrol-id
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol-id/Kontrol-ID-Provisioner.exe \
		${STAGEDIR}${PREFIX}/www/kontrol-id
	${INSTALL_DATA} ${FILESDIR}/etc/inc/priv/kontrol_id.priv.inc \
		${STAGEDIR}/etc/inc/priv
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID/info.xml \
		${STAGEDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID
	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID/info.xml

.include <bsd.port.mk>
