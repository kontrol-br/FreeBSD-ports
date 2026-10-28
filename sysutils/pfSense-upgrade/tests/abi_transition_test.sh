#!/bin/sh

set -eu

upgrade_script="${0%/*}/../files/Kontrol-upgrade"
repo_script="${0%/*}/../files/Kontrol-repo-setup"
tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT HUP INT TERM

check_upgrade() {
	function_file="${tmp}/abi_setup"
	awk '
		/^abi_setup\(\) \{/ { found = 1 }
		found && !/^abi_setup\(\) \{/ && /^[A-Za-z_][A-Za-z0-9_]*\(\) \{/ { exit }
		found { print }
	' "${upgrade_script}" > "${function_file}"

	grep -Fq 'OSVERSION="${_target_osmajor}00000"' "${function_file}"
	grep -Fq '[ "${_cur_osmajor}" != "${_target_osmajor}" ]' "${function_file}"
	grep -Fq 'OSVERSION=${OSVERSION}' "${function_file}"
	grep -Fq 'ALTABI=${ALTABI}' "${function_file}"
	grep -Fq 'mv -f "${_pkg_conf_tmp}" "${_pkg_conf}"' "${function_file}"

	major_line=$(grep -n 'NEW_MAJOR=1' "${function_file}" | head -1 | cut -d: -f1)
	modern_line=$(grep -nF 'if [ -n "${NEW_MAJOR}" ]; then' "${function_file}" | head -1 | cut -d: -f1)
	legacy_line=$(grep -nF 'ALTABI=${ALTABI}' "${function_file}" | head -1 | cut -d: -f1)
	[ "${major_line}" -lt "${modern_line}" ]
	[ "${modern_line}" -lt "${legacy_line}" ]

	if grep -Eq '>+ */usr/local/etc/pkg.conf' "${function_file}"; then
		echo 'direct pkg.conf write found' >&2
		exit 1
	fi
}

check_repo_setup() {
	grep -Fq 'resolve_repo_conf() {' "${repo_script}"
	grep -Fq 'repo_candidate=${repo_target%%.conf/*}.conf' "${repo_script}"
	grep -Fq 'recovered legacy repository link:' "${repo_script}"
	grep -Fq 'target_osversion="${target_major}00000"' "${repo_script}"
	grep -Fq 'if [ -n "${new_major}" ] || pkg_uses_osversion; then' "${repo_script}"
	grep -Fq 'OSVERSION=${target_osversion}' "${repo_script}"
	grep -Fq 'ALTABI=${target_altabi}' "${repo_script}"
	grep -Fq 'mv -f "${pkg_conf_tmp}" "${pkg_conf}"' "${repo_script}"
	grep -Fq '/usr/bin/env IGNORE_OSVERSION=yes /etc/rc.update_pkg_metadata now' "${repo_script}"
}

check_upgrade
check_repo_setup

echo 'FreeBSD 14 to 16 ABI configuration tests: PASS'
