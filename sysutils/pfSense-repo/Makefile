# $FreeBSD$

PORTNAME=	Kontrol-repo
DISTVERSION=	2.9.0
PORTREVISION=	7
CATEGORIES=	sysutils
MASTER_SITES=	# empty
DISTFILES=	# empty
EXTRACT_ONLY=	# empty

MAINTAINER=	contato@kontrol.com.br
COMMENT=	Setup Kontrol pkg(8) repositories

LICENSE=	APACHE20

RUN_DEPENDS=	Kontrol-upgrade>=2.9.0_12:sysutils/pfSense-upgrade

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
PFSENSE_REPOS?=	Kontrol-repo Kontrol-repo-previous Kontrol-repo-backport

# A future release channel is deliberately opt-in.  Publishing the maintenance
# package must not expose an unfinished target to installed 2.9.0 systems.
# Release builders enable it only when all four target values are known.
.if defined(PKG_REPO_BRANCH_UPGRADE_${ARCH}) && !empty(PKG_REPO_BRANCH_UPGRADE_${ARCH})
PKG_REPO_BRANCH_UPGRADE=	${PKG_REPO_BRANCH_UPGRADE_${ARCH}}
.endif
.if defined(PKG_REPO_BRANCH_UPGRADE) && !empty(PKG_REPO_BRANCH_UPGRADE)
.if !defined(PKG_REPO_VERSION_UPGRADE) || empty(PKG_REPO_VERSION_UPGRADE)
.error "PKG_REPO_VERSION_UPGRADE is required when PKG_REPO_BRANCH_UPGRADE is set"
.endif
.if !defined(PKG_REPO_OSMAJOR_UPGRADE) || empty(PKG_REPO_OSMAJOR_UPGRADE)
.error "PKG_REPO_OSMAJOR_UPGRADE is required when PKG_REPO_BRANCH_UPGRADE is set"
.endif
.if !defined(PKG_REPO_OSVERSION_UPGRADE) || empty(PKG_REPO_OSVERSION_UPGRADE)
.error "PKG_REPO_OSVERSION_UPGRADE is required when PKG_REPO_BRANCH_UPGRADE is set"
.endif
PFSENSE_REPOS+=	Kontrol-repo-upgrade
UPGRADE_TARGET_FILE=	${DATADIR}/pkg/repos/Kontrol-repo-upgrade.target
Kontrol-repo-upgrade_OSVERSION=	${PKG_REPO_OSVERSION_UPGRADE}
PFSENSE_PKG_SET_VERSION=	${PKG_REPO_VERSION_UPGRADE}
.endif

PFSENSE_REPOS_${ARCH}?=	${PFSENSE_REPOS}
PFSENSE_DEFAULT_REPO?=	Kontrol-repo
PFSENSE_DEFAULT_REPO_${ARCH}?=	${PFSENSE_DEFAULT_REPO}
SIGNATURE_TYPE?=	fingerprints
DEFAULT_REPO=	${DATADIR}/pkg/repos/${PFSENSE_DEFAULT_REPO_${ARCH}}.conf
TMP_PLIST_FILES=${PFSENSE_REPOS_${ARCH}:C/$/.conf/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.descr/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.abi/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.altabi/} \
		${PFSENSE_REPOS_${ARCH}:C/$/.osversion/}
PLIST_FILES=	${TMP_PLIST_FILES:C/^/%%DATADIR%%\/pkg\/repos\//} \
		${UPGRADE_TARGET_FILE} \
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

# Repository templates are overlaid by the product builder.  Keep explicit
# target OS versions here as a safe fallback when an overlay omits an
# .osversion file; using the build jail's OSVERSION would make rollback
# repositories advertise the current FreeBSD major.
Kontrol-repo_OSVERSION=		1600014
Kontrol-repo-previous_OSVERSION=	1500000
Kontrol-repo-backport_OSVERSION=	1400000

.if defined(PKG_REPO_BRANCH_BACKPORT_${ARCH}) && !empty(PKG_REPO_BRANCH_BACKPORT_${ARCH})
PKG_REPO_BRANCH_BACKPORT=${PKG_REPO_BRANCH_BACKPORT_${ARCH}}
.endif
.if defined(PKG_REPO_BRANCH_RELEASE_${ARCH}) && !empty(PKG_REPO_BRANCH_RELEASE_${ARCH})
PKG_REPO_BRANCH_RELEASE=${PKG_REPO_BRANCH_RELEASE_${ARCH}}
.endif
PKG_REPO_BRANCH_PREVIOUS?=	v2_8_1
PKG_REPO_BRANCH_BACKPORT?=	v2_7_2
.if defined(PKG_REPO_SERVER_RELEASE_${ARCH}) && !empty(PKG_REPO_SERVER_RELEASE_${ARCH})
PKG_REPO_SERVER_RELEASE=${PKG_REPO_SERVER_RELEASE_${ARCH}}
.endif

do-extract:
	@${MKDIR} ${WRKSRC}
	${CP} -r ${FILESDIR}/* ${WRKSRC}

do-build:
.if defined(PKG_REPO_BRANCH_UPGRADE) && !empty(PKG_REPO_BRANCH_UPGRADE)
	@case "${PKG_REPO_VERSION_UPGRADE}:${PKG_REPO_BRANCH_UPGRADE}" in \
		*[!A-Za-z0-9_.:-]*) \
			${ECHO_MSG} "Invalid future release version or branch" >&2; \
			exit 1 ;; \
		esac
	@case "${PKG_REPO_OSVERSION_UPGRADE}" in \
		${PKG_REPO_OSMAJOR_UPGRADE}[0-9][0-9][0-9][0-9][0-9]) ;; \
		*) \
			${ECHO_MSG} "PKG_REPO_OSVERSION_UPGRADE does not match PKG_REPO_OSMAJOR_UPGRADE" >&2; \
			exit 1 ;; \
		esac
.endif
.for f in ${PFSENSE_REPOS_${ARCH}}
	@${REINPLACE_CMD} -i '' \
		-e "s,%%ARCH%%,${ARCH},g" \
		-e "s,%%ARCH_OLD%%,${ARCH_OLD},g" \
		-e "s/%%MIRROR_TYPE%%/${MIRROR_TYPE}/" \
		-e "s,%%PKG_REPO_BRANCH_BACKPORT%%,${PKG_REPO_BRANCH_BACKPORT},g" \
		-e "s,%%PKG_REPO_BRANCH_RELEASE%%,${PKG_REPO_BRANCH_RELEASE},g" \
		-e "s,%%PKG_REPO_BRANCH_PREVIOUS%%,${PKG_REPO_BRANCH_PREVIOUS},g" \
		-e "s,%%PKG_REPO_BRANCH_UPGRADE%%,${PKG_REPO_BRANCH_UPGRADE},g" \
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
			-e "s,%%PKG_REPO_OSMAJOR_UPGRADE%%,${PKG_REPO_OSMAJOR_UPGRADE},g" \
			${WRKSRC}/${f}.abi; \
	fi
	@if [ -f ${WRKSRC}/${f}.altabi ]; then \
		${REINPLACE_CMD} -i '' \
			-e "s,%%ARCH%%,${ALTABI_ARCH},g" \
			-e "s,%%ARCH_OLD%%,${ALTABI_ARCH_OLD},g" \
			-e "s,%%PKG_REPO_OSMAJOR_UPGRADE%%,${PKG_REPO_OSMAJOR_UPGRADE},g" \
			${WRKSRC}/${f}.altabi; \
	fi
.endfor
.if defined(PKG_REPO_BRANCH_UPGRADE) && !empty(PKG_REPO_BRANCH_UPGRADE)
	@${REINPLACE_CMD} -i '' \
		-e "s,%%PKG_REPO_VERSION_UPGRADE%%,${PKG_REPO_VERSION_UPGRADE},g" \
		-e "s,%%PKG_REPO_OSMAJOR_UPGRADE%%,${PKG_REPO_OSMAJOR_UPGRADE},g" \
		${WRKSRC}/Kontrol-repo-upgrade.descr \
		${WRKSRC}/Kontrol-repo-upgrade.target
.endif

do-install:
	${MKDIR} ${STAGEDIR}${DATADIR}/pkg/repos
	${TOUCH} ${STAGEDIR}${DEFAULT_REPO}.default
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
	if [ -f ${WRKSRC}/${f}.osversion ]; then \
		${INSTALL_DATA} ${WRKSRC}/${f}.osversion \
			${STAGEDIR}${DATADIR}/pkg/repos; \
	else \
		${ECHO_CMD} ${${f}_OSVERSION:U${OSVERSION}} \
			> ${STAGEDIR}${DATADIR}/pkg/repos/${f}.osversion; \
	fi
.endfor
.if defined(PKG_REPO_BRANCH_UPGRADE) && !empty(PKG_REPO_BRANCH_UPGRADE)
	${INSTALL_DATA} ${WRKSRC}/Kontrol-repo-upgrade.target \
		${STAGEDIR}${DATADIR}/pkg/repos
.endif
	${ECHO_CMD} ${PFSENSE_PKG_SET_VERSION} \
		> ${STAGEDIR}${DATADIR}/next_pkg_set_version

.include <bsd.port.post.mk>
