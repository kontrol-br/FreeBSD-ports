#!/bin/sh

# Validate packagesite.pkg against the package payloads in All/.  This script
# intentionally uses only FreeBSD base utilities so it can run as a Poudriere
# post-build gate without installing Python, jq, or another package.

set -eu

usage() {
	cat >&2 << EOF
usage: ${0##*/} [-nu] [-m manifest] repository

  -m manifest   write SHA256, size, and path as a tab-separated manifest
  -n            skip package SHA256 calculation (existence and size only)
  -u            allow an unsigned catalog (development/test repositories only)
EOF
	exit 64
}

check_hash=1
require_signature=1
output_manifest=""

file_size() {
	if stat -f %z "$1" >/dev/null 2>&1; then
		stat -f %z "$1"
	else
		stat -c %s "$1"
	fi
}

file_sha256() {
	if command -v sha256 >/dev/null 2>&1; then
		sha256 -q "$1"
	else
		sha256sum "$1" | awk '{ print $1 }'
	fi
}

while getopts "m:nu" option; do
	case "${option}" in
	m) output_manifest=${OPTARG} ;;
	n) check_hash="" ;;
	u) require_signature="" ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
[ "$#" -eq 1 ] || usage

repository=$1
[ -d "${repository}" ] || { echo "repository not found: ${repository}" >&2; exit 66; }
repository=$(CDPATH= cd -- "${repository}" && pwd -P)
[ -d "${repository}/All" ] || { echo "package directory not found: ${repository}/All" >&2; exit 66; }
[ -f "${repository}/packagesite.pkg" ] || { echo "catalog not found: ${repository}/packagesite.pkg" >&2; exit 66; }

workdir=$(mktemp -d "${TMPDIR:-/tmp}/kontrol-repo-validate.XXXXXX")
trap 'rm -rf "${workdir}"' EXIT HUP INT TERM
catalog="${workdir}/packagesite.yaml"
raw_manifest="${workdir}/packages.raw"
verified_manifest="${workdir}/packages.verified"
archive_members="${workdir}/packagesite.members"

if ! tar -tf "${repository}/packagesite.pkg" > "${archive_members}" || \
    ! tar -xOf "${repository}/packagesite.pkg" packagesite.yaml > "${catalog}"; then
	echo "unable to extract packagesite.yaml from ${repository}/packagesite.pkg" >&2
	exit 65
fi
if [ -n "${require_signature}" ]; then
	for member in packagesite.yaml.sig packagesite.yaml.pub; do
		grep -qx "${member}" "${archive_members}" || {
			echo "unsigned repository catalog: missing ${member}" >&2
			exit 65
		}
	done
fi

# packagesite.yaml is JSON-lines.  Extract only the three fields needed for
# publication integrity.  Package paths cannot contain quotes or tabs.
if ! awk '
function string_value(key, value) {
	pattern = "\"" key "\"[[:space:]]*:[[:space:]]*\""
	if (!match($0, pattern))
		return ""
	value = substr($0, RSTART + RLENGTH)
	sub(/".*/, "", value)
	return value
}
function number_value(key, value) {
	pattern = "\"" key "\"[[:space:]]*:[[:space:]]*"
	if (!match($0, pattern))
		return ""
	value = substr($0, RSTART + RLENGTH)
	sub(/[^0-9].*/, "", value)
	return value
}
NF {
	path = string_value("path")
	if (path == "")
		path = string_value("repopath")
	sum = string_value("sum")
	size = number_value("pkgsize")
	if (path == "" || sum == "" || size == "") {
		printf "packagesite.yaml:%d: missing path, pkgsize, or sum\n", NR > "/dev/stderr"
		exit 1
	}
	printf "%s\t%s\t%s\n", sum, size, path
}
' "${catalog}" > "${raw_manifest}"; then
	echo "unable to parse repository catalog" >&2
	exit 65
fi

[ -s "${raw_manifest}" ] || { echo "repository catalog is empty" >&2; exit 65; }

tab=$(printf '\t')
duplicate=$(cut -f3 "${raw_manifest}" | LC_ALL=C sort | uniq -d | head -1)
[ -z "${duplicate}" ] || { echo "duplicate catalog path: ${duplicate}" >&2; exit 65; }

failures=0
count=0
: > "${verified_manifest}"
while IFS="${tab}" read -r expected_hash expected_size relative; do
	count=$((count + 1))
	case "${relative}" in
	All/*.pkg)
		package_name=${relative#All/}
		case "${package_name}" in
		""|*/*|.|..) echo "unsafe package path: ${relative}" >&2; failures=$((failures + 1)); continue ;;
		esac
		;;
	*) echo "unsafe package path: ${relative}" >&2; failures=$((failures + 1)); continue ;;
	esac

	case "${expected_size}" in
	""|*[!0-9]*) echo "invalid pkgsize for ${relative}: ${expected_size}" >&2; failures=$((failures + 1)); continue ;;
	esac
	if printf '%s\n' "${expected_hash}" | grep -Eq '^[0-9A-Fa-f]{64}$'; then
		hash_type=sha256_hex_raw
	elif printf '%s\n' "${expected_hash}" | grep -Eq '^1\$[0-9A-Fa-f]{64}$'; then
		hash_type=sha256_hex
	elif printf '%s\n' "${expected_hash}" | grep -Eq '^2\$[a-z0-9]+$'; then
		hash_type=blake2_base32
	else
		echo "invalid package checksum for ${relative}: ${expected_hash}" >&2
		failures=$((failures + 1))
		continue
	fi

	package="${repository}/${relative}"
	if [ ! -f "${package}" ]; then
		echo "missing: ${relative}" >&2
		failures=$((failures + 1))
		continue
	fi
	actual_size=$(file_size "${package}")
	if [ "${actual_size}" != "${expected_size}" ]; then
		echo "size mismatch: ${relative}: catalog=${expected_size}, file=${actual_size}" >&2
		failures=$((failures + 1))
		continue
	fi
	if [ -n "${check_hash}" ]; then
		case "${hash_type}" in
		sha256_hex_raw) actual_hash=$(file_sha256 "${package}") ;;
		sha256_hex) actual_hash=$(pkg-static checksum -q -t sha256_hex "${package}") ;;
		blake2_base32) actual_hash=$(pkg-static checksum -q -t blake2_base32 "${package}") ;;
		esac
		if [ "${actual_hash}" != "${expected_hash}" ]; then
			echo "package checksum mismatch: ${relative}: catalog=${expected_hash}, file=${actual_hash}" >&2
			failures=$((failures + 1))
			continue
		fi
	fi
	# Mirrors may not have pkg installed.  Normalize the verified catalogue
	# checksum to SHA256 for the remote post-rsync verification manifest.
	manifest_hash=$(file_sha256 "${package}")
	printf '%s\t%s\t%s\n' "${manifest_hash}" "${expected_size}" "${relative}" >> "${verified_manifest}"
done < "${raw_manifest}"

if [ "${failures}" -ne 0 ]; then
	echo "repository validation failed: ${failures} catalog/file divergence(s)" >&2
	exit 1
fi

if [ -n "${output_manifest}" ]; then
	manifest_tmp="${output_manifest}.tmp.$$"
	LC_ALL=C sort -t "${tab}" -k3,3 "${verified_manifest}" > "${manifest_tmp}"
	chmod 0644 "${manifest_tmp}"
	mv -f "${manifest_tmp}" "${output_manifest}"
fi

echo "repository integrity OK: ${count} package(s), catalog=${repository}/packagesite.pkg"
