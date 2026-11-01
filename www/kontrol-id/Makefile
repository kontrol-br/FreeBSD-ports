PORTNAME=	kontrol-id
PORTVERSION=	5.4.23
PORTREVISION=	11
DISTVERSIONPREFIX=	v
CATEGORIES=	www
GH_ACCOUNT=	kontrol-br
GH_PROJECT=	e2guardian
GH_TAGNAME=	v5.4.23


MAINTAINER=	contato@kontrol.com.br
COMMENT=	Kontrol-ID standalone secure web gateway derived from E2Guardian
WWW=		https://kontrol.com.br

LICENSE=	GPLv2

# Kontrol-ID intentionally installs the proven E2Guardian runtime layout so
# existing policy data can be migrated safely.  The engines must never be
# installed together because they own the same binary and configuration paths.
CONFLICTS_INSTALL=	e2guardian* Kontrol-pkg-E2guardian54*

RUN_DEPENDS+=   openssl111>=1.1.1w_2:security/openssl111

LIB_DEPENDS=	libpcre.so:devel/pcre

USES=		autoreconf compiler:c++11-lang gssapi:mit,flags libtool localbase \
		pkgconfig python
USE_GITHUB=	yes
#BINARY_ALIAS=	autom4te2.72=autom4te-2.72

USE_CXXSTD=	c++11
USE_RC_SUBR=	kontrol_id
GNU_CONFIGURE=	yes
CONFIGURE_ARGS=	--with-logdir=/var/log \
		--with-piddir=/var/run
CONFIGURE_ENV+=	LIBS="${GSSAPILIBS}"

OPTIONS_DEFINE=	NTLM DNS EMAIL DEBUG DOCS SSL_MITM
OPTIONS_DEFAULT=DNS NTLM SSL_MITM
OPTIONS_SUB=	yes

NTLM_DESC=	Include NTLM authentication plugin
DNS_DESC=	Include DNS authetication plugin
EMAIL_DESC=	Enable e-mail reporting support
DEBUG_DESC=	Enable debug build mode
SSL_MITM_DESC=	Enable support for the SSL MITM plugin

ETCDIR=		${PREFIX}/etc/e2guardian
DATADIR=	${PREFIX}/share/e2guardian
DOCSDIR=	${PREFIX}/share/doc/e2guardian

NTLM_CONFIGURE_ENABLE=		ntlm
NTLM_USES=			iconv
DNS_CONFIGURE_ENABLE=		dnsauth
EMAIL_CONFIGURE_ENABLE=		email
DEBUG_CONFIGURE_OFF=		--with-e2debug=off --with-newdebug=off
DEBUG_CONFIGURE_ON=		--with-e2debug=on --with-newdebug=on

# Flags de debug reabilitadas para troubleshooting
DEBUG_CFLAGS=		-g -O0 -fno-omit-frame-pointer
DEBUG_CXXFLAGS=		-g -O0 -fno-omit-frame-pointer
DEBUG_VARS=		STRIP=

#SSL_MITM_USES=                 ssl
SSL_MITM_CONFIGURE_ENABLE=      sslmitm
#DEFAULT_VERSIONS+=                     ssl=openssl
BROKEN_SSL=                     openssl30 openssl31 openssl32
BROKEN_SSL_REASON= required openssl111 semantics for peer certificate

OPENSSLBASE=    /usr/local
OPENSSLINC=     ${OPENSSLBASE}/include
OPENSSLLIB=     ${OPENSSLBASE}/lib

SSL_MITM_CONFIGURE_ENV=         OPENSSL_LIBS="${OPENSSLLIB}/libssl.so ${OPENSSLLIB}/libcrypto.so" \
                                OPENSSL_CFLAGS="-I${OPENSSLINC}" \
                                OPENSSL_LDFLAGS="-L${OPENSSLLIB}"
CFLAGS+=        -I${OPENSSLINC}
CPPFLAGS+=      -I${OPENSSLINC}
LDFLAGS+=       -L${OPENSSLLIB}

.include <bsd.port.options.mk>

# These switches disable content-scanner plugins. They do not disable the
# native E2Guardian ICAP service consumed by Squid.
CONFIGURE_ARGS+=	--disable-commandline --disable-clamd --disable-icap \
			--disable-kavd --disable-avastd
PLIST_SUB+=	SCANNERS="@comment " CLAMD="@comment " ICAP="@comment " \
		CLISCAN="@comment " KAV="@comment " AVAST="@comment "

SUB_FILES=	pkg-message

post-patch:
	@${CP} ${FILESDIR}/kontrol_negotiate.cpp \
		${WRKSRC}/src/authplugins/kontrol_negotiate.cpp
	@${CP} ${FILESDIR}/kontrol_ntlm.cpp \
		${WRKSRC}/src/authplugins/kontrol_ntlm.cpp
	@${CP} ${FILESDIR}/kontrol_basic.cpp \
		${WRKSRC}/src/authplugins/kontrol_basic.cpp
	@${CP} ${FILESDIR}/kontrol_identity.cpp \
		${WRKSRC}/src/authplugins/kontrol_identity.cpp

post-install:
	@${MV} ${STAGEDIR}${PREFIX}/sbin/e2guardian \
		${STAGEDIR}${PREFIX}/sbin/kontrol-id
	@${LN} -sf kontrol-id ${STAGEDIR}${PREFIX}/sbin/e2guardian
	@${INSTALL_DATA} ${FILESDIR}/kontrol-negotiate.conf \
		${STAGEDIR}${ETCDIR}/authplugins/kontrol-negotiate.conf.sample
	@${INSTALL_DATA} ${FILESDIR}/kontrol-ntlm.conf \
		${STAGEDIR}${ETCDIR}/authplugins/kontrol-ntlm.conf.sample
	@${INSTALL_DATA} ${FILESDIR}/kontrol-basic.conf \
		${STAGEDIR}${ETCDIR}/authplugins/kontrol-basic.conf.sample
	@${INSTALL_DATA} ${FILESDIR}/kontrol-identity.conf \
		${STAGEDIR}${ETCDIR}/authplugins/kontrol-identity.conf.sample
	@${FIND} ${STAGEDIR}${ETCDIR} -type f \
		\( -name '*.conf' -or -name '*list' -or -name '*.story' \) \
		-exec ${MV} {} {}.sample \;
	@${MV} ${STAGEDIR}${ETCDIR}/lists/authplugins/ipgroups \
		${STAGEDIR}${ETCDIR}/lists/authplugins/ipgroups.sample
	@${MV} ${STAGEDIR}${ETCDIR}/lists/authplugins/portgroups \
		${STAGEDIR}${ETCDIR}/lists/authplugins/portgroups.sample
	@${MV} ${STAGEDIR}${ETCDIR}/lists/bannedrooms/default \
		${STAGEDIR}${ETCDIR}/lists/bannedrooms/default.sample
	@${MV} ${STAGEDIR}${ETCDIR}/lists/domainsnobypass \
		${STAGEDIR}${ETCDIR}/lists/domainsnobypass.sample
	@${MV} ${STAGEDIR}${ETCDIR}/lists/ipnobypass \
		${STAGEDIR}${ETCDIR}/lists/ipnobypass.sample
	@${MV} ${STAGEDIR}${ETCDIR}/lists/urlnobypass \
		${STAGEDIR}${ETCDIR}/lists/urlnobypass.sample
	@${RM} -r ${STAGEDIR}${ETCDIR}/contentscanners
	@${RM} ${STAGEDIR}${ETCDIR}/lists/exceptionvirus*.sample

.include <bsd.port.mk>
