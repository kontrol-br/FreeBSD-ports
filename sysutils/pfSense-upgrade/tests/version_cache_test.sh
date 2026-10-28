#!/bin/sh

set -eu

SCRIPT=${1:-$(dirname "$0")/../files/Kontrol-repo-setup}
INSTALLER=${2:-$(dirname "$0")/../../pfSense-repo/files/pkg-install.in}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/Kontrol-version-cache.XXXXXX")
trap 'rm -rf "${tmp}"' EXIT HUP INT TERM

sed -n '/^invalidate_version_cache()/,/^}/p' "${SCRIPT}" > "${tmp}/function"
[ -s "${tmp}/function" ]
. "${tmp}/function"

PRODUCT=Kontrol
VERSION_CACHE_DIR=${tmp}
dbdir=""
touch "${tmp}/Kontrol_version" "${tmp}/Kontrol_version.rc"
invalidate_version_cache
[ ! -e "${tmp}/Kontrol_version" ]
[ ! -e "${tmp}/Kontrol_version.rc" ]

# Isolated/destdir calls must not invalidate the running system cache.
touch "${tmp}/Kontrol_version" "${tmp}/Kontrol_version.rc"
dbdir=${tmp}/isolated-db
invalidate_version_cache
[ -e "${tmp}/Kontrol_version" ]
[ -e "${tmp}/Kontrol_version.rc" ]

# The repository package must also invalidate both files after installation.
grep -q '%%PRODUCT_NAME%%_version"' "${INSTALLER}"
grep -q '%%PRODUCT_NAME%%_version.rc"' "${INSTALLER}"

echo "version cache invalidation tests: PASS"
