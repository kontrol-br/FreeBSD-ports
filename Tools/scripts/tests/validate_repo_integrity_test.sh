#!/bin/sh

set -eu

validator="${0%/*}/../validate_repo_integrity.sh"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/kontrol-repo-test.XXXXXX")
trap 'rm -rf "${tmp}"' EXIT HUP INT TERM
repo="${tmp}/repo"
mkdir -p "${repo}/All"

make_catalog() {
	local catalog_size=$1
	local catalog_hash=$2
	local catalog_path=$3
	printf '{"name":"sample","version":"1.0","path":"%s","pkgsize":%s,"sum":"%s"}\n' \
	    "${catalog_path}" "${catalog_size}" "${catalog_hash}" > "${tmp}/packagesite.yaml"
	printf 'test signature\n' > "${tmp}/packagesite.yaml.sig"
	printf 'test public key\n' > "${tmp}/packagesite.yaml.pub"
	tar -czf "${repo}/packagesite.pkg" -C "${tmp}" \
	    packagesite.yaml packagesite.yaml.sig packagesite.yaml.pub
}

printf 'valid package payload\n' > "${repo}/All/sample-1.0.pkg"
size=$(stat -f %z "${repo}/All/sample-1.0.pkg")
hash=$(sha256 -q "${repo}/All/sample-1.0.pkg")
manifest="${tmp}/manifest.tsv"

make_catalog "${size}" "${hash}" All/sample-1.0.pkg
"${validator}" -m "${manifest}" "${repo}" >/dev/null
grep -q "$(printf '\t')All/sample-1.0.pkg$" "${manifest}"

tar -czf "${repo}/packagesite.pkg" -C "${tmp}" packagesite.yaml
if "${validator}" "${repo}" >"${tmp}/out" 2>"${tmp}/err"; then
	echo "unsigned catalog was accepted" >&2
	exit 1
fi
grep -q 'unsigned repository catalog' "${tmp}/err"
make_catalog "${size}" "${hash}" All/sample-1.0.pkg

rm "${repo}/All/sample-1.0.pkg"
if "${validator}" "${repo}" >"${tmp}/out" 2>"${tmp}/err"; then
	echo "missing package was accepted" >&2
	exit 1
fi
grep -q 'missing: All/sample-1.0.pkg' "${tmp}/err"

printf 'valid package payload\n' > "${repo}/All/sample-1.0.pkg"
make_catalog 1 "${hash}" All/sample-1.0.pkg
if "${validator}" "${repo}" >"${tmp}/out" 2>"${tmp}/err"; then
	echo "size mismatch was accepted" >&2
	exit 1
fi
grep -q 'size mismatch' "${tmp}/err"

make_catalog "${size}" 0000000000000000000000000000000000000000000000000000000000000000 All/sample-1.0.pkg
if "${validator}" "${repo}" >"${tmp}/out" 2>"${tmp}/err"; then
	echo "checksum mismatch was accepted" >&2
	exit 1
fi
grep -q 'package checksum mismatch' "${tmp}/err"

make_catalog "${size}" "${hash}" All/../sample-1.0.pkg
if "${validator}" "${repo}" >"${tmp}/out" 2>"${tmp}/err"; then
	echo "path traversal was accepted" >&2
	exit 1
fi
grep -q 'unsafe package path' "${tmp}/err"

echo 'repository integrity validator tests: PASS'
