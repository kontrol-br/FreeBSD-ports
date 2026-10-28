#!/bin/sh

set -eu

upgrade_script="${0%/*}/../files/Kontrol-upgrade"
repo_script="${0%/*}/../files/Kontrol-repo-setup"
tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT HUP INT TERM

extract_abi_setup() {
	awk '
		/^abi_setup\(\) \{/ { found = 1 }
		found && !/^abi_setup\(\) \{/ && /^[A-Za-z_][A-Za-z0-9_]*\(\) \{/ { exit }
		found { print }
	' "$1"
}

assert_modern_transition() {
	script=$1
	name=$2
	function_file="${tmp}/${name}.abi_setup"
	extract_abi_setup "${script}" > "${function_file}"

	grep -Fq 'OSVERSION="${_target_osmajor}00000"' "${function_file}"
	grep -Fq '[ "${_cur_osmajor}" != "${_target_osmajor}" ]' "${function_file}"
	grep -Fq 'ABI=${ABI}' "${function_file}"
	grep -Fq 'OSVERSION=${OSVERSION}' "${function_file}"
	grep -Fq 'ALTABI=${ALTABI}' "${function_file}"
	grep -Fq 'mv -f "${_pkg_conf_tmp}"' "${function_file}"

	major_line=$(grep -n 'NEW_MAJOR=1' "${function_file}" | head -1 | cut -d: -f1)
	format_line=$(grep -nF 'if [ -n "${NEW_MAJOR}" ] || pkg_uses_osversion; then' "${function_file}" | head -1 | cut -d: -f1)
	legacy_line=$(grep -nF 'ALTABI=${ALTABI}' "${function_file}" | head -1 | cut -d: -f1)
	[ "${major_line}" -lt "${format_line}" ]
	[ "${format_line}" -lt "${legacy_line}" ]

	# Direct truncation exposed partially-written pkg.conf to concurrent GUI
	# readers.  Only the temporary file may be written before atomic mv.
	if grep -Eq '>+ */usr/local/etc/pkg.conf' "${function_file}"; then
		echo "${name}: direct pkg.conf write found" >&2
		exit 1
	fi
}

assert_modern_transition "${upgrade_script}" upgrade
assert_modern_transition "${repo_script}" repo-setup

echo 'cross-major ABI configuration tests: PASS'
