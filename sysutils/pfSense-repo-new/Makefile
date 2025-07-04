# $FreeBSD$

PORTNAME=	Kontrol-repo
DISTVERSION=	2.6.0
PORTREVISION=	14
CATEGORIES=	sysutils
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@konntrol.com.br
COMMENT=	Setup Kontrol pkg(8) repositories

LICENSE=	APACHE20

RUN_DEPENDS=	${LOCALBASE}/sbin/Kontrol-upgrade:sysutils/Kontrol-upgrade

NO_MTREE=	yes

SUB_FILES=	pkg-install
SUB_LIST=	PORTNAME=${PORTNAME} \
		PRODUCT_NAME=${PRODUCT_NAME} \
		DEFAULT_REPO=${DEFAULT_REPO}

DATADIR=	${PREFIX}/share/Kontrol

# Flag used to detect when we want to reinstall all packages during upgrade
PFSENSE_PKG_SET_VERSION?=	${DISTVERSION}

.include <bsd.port.pre.mk>

MIRROR_TYPE?=	srv
PFSENSE_REPOS?=	Kontrol-repo Kontrol-repo-previous
PFSENSE_REPOS_${ARCH}?=	${PFSENSE_REPOS}
PFSENSE_DEFAULT_REPO?=	Kontrol-repo
PFSENSE_DEFAULT_REPO_${ARCH}?=	${PFSENSE_DEFAULT_REPO}
SIGNATURE_TYPE?=	fingerprints
DEFAULT_REPO=	${DATADIR}/pkg/repos/${PFSENSE_DEFAULT_REPO_${ARCH}}.conf
TMP_PLIST_FILES=${PFSENSE_REPOS_${ARCH}:C/$/.conf/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.descr/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.abi/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.altabi/}
PLIST_FILES=	${TMP_PLIST_FILES:C/^/%%DATADIR%%\/pkg\/repos\//} \
		${DEFAULT_REPO}.default \
		${DATADIR}/next_pkg_set_version
DEFAULT_ABI=	${OPSYS}:${OSREL:C/\..*//}:${ARCH}

.if ${ARCH} == "aarch64"
ALTABI_ARCH=	aarch64:64
.elif ${ARCH} == "amd64"
ALTABI_ARCH=	x86:64
.elif ${ARCH} == "i386"
ALTABI_ARCH=	x86:32
.elif ${ARCH} == "armv6"
ALTABI_ARCH=	32:el:eabi:hardfp
.elif ${ARCH} == "armv7"
ALTABI_ARCH=	32:el:eabi:softfp
.else
.error "Invalid arch: ${ARCH}"
.endif

.if ${ARCH} == "armv7"
ARCH_OLD=	armv6
ALTABI_ARCH_OLD=32:el:eabi:hardfp
.else
ARCH_OLD=	${ARCH}
ALTABI_ARCH_OLD=${ALTABI_ARCH}
.endif

DEFAULT_ALTABI=	${OPSYS:tl}:${OSREL:C/\..*//}:${ALTABI_ARCH}

.if defined(PKG_REPO_BRANCH_DEVEL_${ARCH}) && !empty(PKG_REPO_BRANCH_DEVEL_${ARCH})
PKG_REPO_BRANCH_DEVEL=${PKG_REPO_BRANCH_DEVEL_${ARCH}}
.endif
.if defined(PKG_REPO_BRANCH_RELEASE_${ARCH}) && !empty(PKG_REPO_BRANCH_RELEASE_${ARCH})
PKG_REPO_BRANCH_RELEASE=${PKG_REPO_BRANCH_RELEASE_${ARCH}}
.endif
.if defined(PKG_REPO_SERVER_DEVEL_${ARCH}) && !empty(PKG_REPO_SERVER_DEVEL_${ARCH})
PKG_REPO_SERVER_DEVEL=${PKG_REPO_SERVER_DEVEL_${ARCH}}
.endif
.if defined(PKG_REPO_SERVER_RELEASE_${ARCH}) && !empty(PKG_REPO_SERVER_RELEASE_${ARCH})
PKG_REPO_SERVER_RELEASE=${PKG_REPO_SERVER_RELEASE_${ARCH}}
.endif

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

do-build:
.for f in ${PFSENSE_REPOS_${ARCH}}
	@${REINPLACE_CMD} -i '' \
		-e "s,%%ARCH%%,${ARCH},g" \
		-e "s,%%ARCH_OLD%%,${ARCH_OLD},g" \
		-e "s/%%MIRROR_TYPE%%/${MIRROR_TYPE}/" \
		-e "s,%%PKG_REPO_BRANCH_DEVEL%%,${PKG_REPO_BRANCH_DEVEL},g" \
		-e "s,%%PKG_REPO_BRANCH_RELEASE%%,${PKG_REPO_BRANCH_RELEASE},g" \
		-e "s,%%PKG_REPO_SERVER_DEVEL%%,${PKG_REPO_SERVER_DEVEL},g" \
		-e "s,%%PKG_REPO_SERVER_RELEASE%%,${PKG_REPO_SERVER_RELEASE},g" \
		-e "s,%%POUDRIERE_PORTS_NAME%%,${POUDRIERE_PORTS_NAME},g" \
		-e "s,%%PRODUCT_NAME%%,${PRODUCT_NAME},g" \
		-e "s,%%REPO_BRANCH_PREFIX%%,${REPO_BRANCH_PREFIX},g" \
		-e "s,%%SIGNATURE_TYPE%%,${SIGNATURE_TYPE},g" \
		${WRKSRC}/${f}.conf
	@if [ -f ${WRKSRC}/${f}.abi ]; then \
		${REINPLACE_CMD} -i '' \
			-e "s,%%ARCH%%,${ARCH},g" \
			-e "s,%%ARCH_OLD%%,${ARCH_OLD},g" \
			${WRKSRC}/${f}.abi; \
	fi
	@if [ -f ${WRKSRC}/${f}.altabi ]; then \
		${REINPLACE_CMD} -i '' \
			-e "s,%%ARCH%%,${ALTABI_ARCH},g" \
			-e "s,%%ARCH_OLD%%,${ALTABI_ARCH_OLD},g" \
			${WRKSRC}/${f}.altabi; \
	fi
.endfor

do-install:
	${MKDIR} ${STAGEDIR}${DATADIR}/pkg/repos
	${TOUCH} ${STAGEDIR}${DEFAULT_REPO}.default
	${MKDIR} ${STAGEDIR}/usr/local/share/Kontrol/keys/pkg/trusted/
	${INSTALL_DATA} ${FILESDIR}/usr/local/share/Kontrol/keys/pkg/trusted/pkg.kontrol.com.br.20180421 ${STAGEDIR}/usr/local/share/Kontrol/keys/pkg/trusted/pkg.kontrol.com.br.20180421
.for f in ${PFSENSE_REPOS_${ARCH}}
	${INSTALL_DATA} ${WRKSRC}/${f}.conf ${STAGEDIR}${DATADIR}/pkg/repos
	${INSTALL_DATA} ${WRKSRC}/${f}.descr ${STAGEDIR}${DATADIR}/pkg/repos
	if [ -f ${WRKSRC}/${f}.abi ]; then \
		${INSTALL_DATA} ${WRKSRC}/${f}.abi ${STAGEDIR}${DATADIR}/pkg/repos; \
	else \
		${ECHO_CMD} ${DEFAULT_ABI} > ${STAGEDIR}${DATADIR}/pkg/repos/${f}.abi; \
	fi
	if [ -f ${WRKSRC}/${f}.altabi ]; then \
		${INSTALL_DATA} ${WRKSRC}/${f}.altabi ${STAGEDIR}${DATADIR}/pkg/repos; \
	else \
		${ECHO_CMD} ${DEFAULT_ALTABI} > ${STAGEDIR}${DATADIR}/pkg/repos/${f}.altabi; \
	fi
.endfor
	${ECHO_CMD} ${PFSENSE_PKG_SET_VERSION} \
		> ${STAGEDIR}${DATADIR}/next_pkg_set_version

.include <bsd.port.post.mk>
