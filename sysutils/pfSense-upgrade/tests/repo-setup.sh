#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
SETUP=${HERE}/../files/Kontrol-repo-setup
ROOT=$(mktemp -d /tmp/Kontrol-repo-setup-test.XXXXXX)
trap 'rm -rf "${ROOT}"' EXIT HUP INT TERM
mkdir -p "${ROOT}/templates" "${ROOT}/active"

repo()
{
	name=$1 abi=$2 osversion=$3
	: > "${ROOT}/templates/${name}.conf"
	printf '%s\n' "${abi}" > "${ROOT}/templates/${name}.abi"
	printf '%s\n' "${osversion}" > "${ROOT}/templates/${name}.osversion"
}
repo Kontrol-repo-previous FreeBSD:15:amd64 1500000
repo Kontrol-repo FreeBSD:16:amd64 1600000
: > "${ROOT}/templates/Kontrol-repo-previous.conf.default"

run_setup()
{
	REPO_SELECTION=$1 PRODUCT=Kontrol TEMPLATE_DIR=${ROOT}/templates \
		ACTIVE_DIR=${ROOT}/active PKG_CONF=${ROOT}/pkg.conf "${SETUP}" -U
}

# An empty saved selection resolves to the package default (2.8.1).
run_setup ''
[ "$(readlink "${ROOT}/active/Kontrol.conf")" = \
	"${ROOT}/templates/Kontrol-repo-previous.conf" ]
grep -qx 'ABI=FreeBSD:15:amd64' "${ROOT}/pkg.conf"
grep -qx 'OSVERSION=1500000' "${ROOT}/pkg.conf"
! grep -q ALTABI "${ROOT}/pkg.conf"

# The PHP selector posts "Default" for Kontrol-repo and "Previous" for the
# rollback/current-system template. Both names must resolve without repoc.
run_setup Previous
[ "$(readlink "${ROOT}/active/Kontrol.conf")" = \
	"${ROOT}/templates/Kontrol-repo-previous.conf" ]
run_setup Default
[ "$(readlink "${ROOT}/active/Kontrol.conf")" = \
	"${ROOT}/templates/Kontrol-repo.conf" ]
grep -qx 'ABI=FreeBSD:16:amd64' "${ROOT}/pkg.conf"
grep -qx 'OSVERSION=1600000' "${ROOT}/pkg.conf"

# Never use RELENG_2_8_1's legacy pkg_repos_path under etc. When no explicit
# test override is present, templates are package data below LOCALBASE/share.
mkdir -p "${ROOT}/prefix/share/Kontrol/pkg"
mv "${ROOT}/templates" "${ROOT}/prefix/share/Kontrol/pkg/repos"
REPO_SELECTION=Previous PRODUCT=Kontrol LOCALBASE=${ROOT}/prefix \
	ACTIVE_DIR=${ROOT}/active PKG_CONF=${ROOT}/pkg.conf "${SETUP}" -U
[ "$(readlink "${ROOT}/active/Kontrol.conf")" = \
	"${ROOT}/prefix/share/Kontrol/pkg/repos/Kontrol-repo-previous.conf" ]

# Existing installations may have persisted the pre-DATADIR absolute path.
# Its basename still identifies the selected branch and must not fall back to
# Previous, or confirmation would silently remain on 2.8.1.
REPO_SELECTION=/usr/local/etc/Kontrol/pkg/Kontrol-repo.conf PRODUCT=Kontrol \
	LOCALBASE=${ROOT}/prefix ACTIVE_DIR=${ROOT}/active \
	PKG_CONF=${ROOT}/pkg.conf "${SETUP}" -U
[ "$(readlink "${ROOT}/active/Kontrol.conf")" = \
	"${ROOT}/prefix/share/Kontrol/pkg/repos/Kontrol-repo.conf" ]
grep -qx 'ABI=FreeBSD:16:amd64' "${ROOT}/pkg.conf"

printf 'all repo-setup tests passed\n'
