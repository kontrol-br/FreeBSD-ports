#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
PORT=${HERE}/..
MAKEFILE=${PORT}/Makefile
INSTALL=${PORT}/files/pkg-install.in

grep -Fq 'PORTREVISION=' "${MAKEFILE}"
grep -Fq 'Kontrol-upgrade>=2.9.0_12:sysutils/pfSense-upgrade' "${MAKEFILE}"
grep -Fq 'PKG_REPO_VERSION_UPGRADE is required' "${MAKEFILE}"
grep -Fq 'PKG_REPO_OSMAJOR_UPGRADE is required' "${MAKEFILE}"
grep -Fq 'PKG_REPO_OSVERSION_UPGRADE is required' "${MAKEFILE}"
grep -Fq 'PFSENSE_PKG_SET_VERSION=' "${MAKEFILE}"
grep -Fq 'Kontrol-repo-upgrade.target' "${MAKEFILE}"

# Shipping the maintenance revision must not advertise an unfinished target.
default_repos=$(awk -F= '/^PFSENSE_REPOS\?=/{print $2; exit}' "${MAKEFILE}")
case " ${default_repos} " in
	*' Kontrol-repo-upgrade '*) echo 'upgrade channel enabled by default' >&2; exit 1 ;;
esac

for suffix in conf descr abi altabi target; do
	test -s "${PORT}/files/Kontrol-repo-upgrade.${suffix}"
done
grep -Fq 'signature_policy=fingerprints' "${PORT}/files/Kontrol-repo-upgrade.target"
grep -Fq 'write_config("Normalize package repository after Kontrol-repo update.")' "${INSTALL}"
grep -Fq 'mv -f "${PKG_CONF_TMP}" "${PKG_CONF}"' "${INSTALL}"

echo 'upgrade-channel tests: PASS'
