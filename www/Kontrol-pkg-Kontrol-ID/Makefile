# $FreeBSD$

PORTNAME=	Kontrol-pkg-Kontrol-ID
PORTVERSION=	0.5.0
PORTREVISION=	66
CATEGORIES=	www
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@kontrol.com.br
COMMENT=	Standalone authenticated secure web gateway for Kontrol

LICENSE=	APACHE20

RUN_DEPENDS=	kontrol-id>=5.4.23_18:www/kontrol-id \
		${LOCALBASE}/share/certs/ca-root-nss.crt:security/ca_root_nss \
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
	${MKDIR} ${STAGEDIR}${PREFIX}/lib
	${MKDIR} ${STAGEDIR}${PREFIX}/etc/e2guardian
	${MKDIR} ${STAGEDIR}${PREFIX}/www
	${MKDIR} ${STAGEDIR}${PREFIX}/www/kontrol-id
	${MKDIR} ${STAGEDIR}${PREFIX}/www/widgets/widgets
	${MKDIR} ${STAGEDIR}${PREFIX}/www/widgets/include
	${MKDIR} ${STAGEDIR}${PREFIX}/www/shortcuts
	${MKDIR} ${STAGEDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID
	${MKDIR} ${STAGEDIR}/etc/inc/priv
	${MKDIR} ${STAGEDIR}/root
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
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol-id-login-base.css \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol-id-login-kontrol-dark.css \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol-id-login-aurora.css \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol-id-login-corporate.css \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/pkg/kontrol-id-login-signal.css \
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
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_api_authorize.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_provisioner.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol_id_unenrollment.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol-id/index.html \
		${STAGEDIR}${PREFIX}/www/kontrol-id
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/www/kontrol-id/Kontrol-ID-Provisioner.exe \
		${STAGEDIR}${PREFIX}/www/kontrol-id
	${INSTALL_DATA} ${FILESDIR}/etc/inc/priv/kontrol_id.priv.inc \
		${STAGEDIR}/etc/inc/priv
	${INSTALL_DATA} ${FILESDIR}/etc/inc/priv/e2guardian.priv.inc \
		${STAGEDIR}/etc/inc/priv
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_blacklist.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_config.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_content_acl.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_file_acl.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_groups.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_gateways.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_header_acl.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_ldap.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_limits.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_log.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_search_acl.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_sync.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_site_acl.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_url_acl.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/pkg_e2guardian.inc \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian.conf.template \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_ips_header.template \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_users_footer.template \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_users_header.template \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_story.template \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/common.story.default \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/preauth.story.default \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/site.story.default \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/default.conf.default \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/kontrol-id-block-template.html \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/kontrol-id-block-template-aurora.html \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/kontrol-id-block-template-corporate.html \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/kontrol-id-block-template-signal.html \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardianfx.conf.template \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_ips.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/pkg/e2guardian_users.xml \
		${STAGEDIR}${PREFIX}/pkg
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2gerror.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_monitor_data.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_monitor.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_log_parser.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_clone.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_scheds.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_logrotate.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/www/e2guardian_ldap.php \
		${STAGEDIR}${PREFIX}/www
	${INSTALL_DATA} -m 0755 ${FILESDIR}${PREFIX}/etc/e2guardian/squidparent.conf \
		${STAGEDIR}${PREFIX}/etc/e2guardian
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/www/widgets/widgets/kontrol_id.widget.php \
		${STAGEDIR}${PREFIX}/www/widgets/widgets
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/www/widgets/include/kontrol_id.inc \
		${STAGEDIR}${PREFIX}/www/widgets/include
	${INSTALL_DATA} -m 0644 ${FILESDIR}${PREFIX}/www/shortcuts/pkg_e2guardian.inc \
		${STAGEDIR}${PREFIX}/www/shortcuts
	${INSTALL_DATA} ${FILESDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID/info.xml \
		${STAGEDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID
	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${PREFIX}/share/Kontrol-pkg-Kontrol-ID/info.xml
	@${REINPLACE_CMD} -i '' -e "s|%%PKGVERSION%%|${PKGVERSION}|" \
		${STAGEDIR}${PREFIX}/pkg/e2guardian.xml

.include <bsd.port.mk>
