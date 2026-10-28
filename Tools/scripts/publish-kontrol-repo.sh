#!/bin/sh

# Publish a validated pkg repository without exposing metadata before packages.
# Example:
#   publish-kontrol-repo.sh /poudriere/data/packages/jail-ports/.latest \
#     root@100.89.4.30:/var/www/html2/packages/2.9.0/amd64 \
#     root@100.89.4.31:/var/www/html2/packages/2.9.0/amd64

set -eu

usage() {
	cat >&2 << EOF
usage: ${0##*/} [-n] [-v validator] repository host:path [host:path ...]

  -n             validate only; do not connect to mirrors
  -v validator   path to validate_repo_integrity.sh
EOF
	exit 64
}

dry_run=""
script_dir=$(CDPATH= cd -- "${0%/*}" && pwd -P)
validator="${script_dir}/validate_repo_integrity.sh"

while getopts "nv:" option; do
	case "${option}" in
	n) dry_run=1 ;;
	v) validator=${OPTARG} ;;
	*) usage ;;
	esac
done
shift $((OPTIND - 1))
if [ -n "${dry_run}" ]; then
	[ "$#" -ge 1 ] || usage
else
	[ "$#" -ge 2 ] || usage
fi

repository=$1
shift
[ -d "${repository}" ] || { echo "repository not found: ${repository}" >&2; exit 66; }
repository=$(CDPATH= cd -- "${repository}" && pwd -P)

for metadata in meta.conf data.pkg packagesite.pkg; do
	[ -f "${repository}/${metadata}" ] || {
		echo "required metadata not found: ${repository}/${metadata}" >&2
		exit 66
	}
done

workdir=$(mktemp -d "${TMPDIR:-/tmp}/kontrol-repo-publish.XXXXXX")
trap 'rm -rf "${workdir}"' EXIT HUP INT TERM
manifest="${workdir}/packages.manifest"

"${validator}" -m "${manifest}" "${repository}"
[ -z "${dry_run}" ] || {
	echo "validation-only run complete; no mirror was modified"
	exit 0
}

case "${repository}" in
*"'"*) echo "repository path may not contain a single quote" >&2; exit 65 ;;
esac

publish_one() {
	destination=$1
	case "${destination}" in
	*:* ) ;;
	* ) echo "destination must use host:path syntax: ${destination}" >&2; return 65 ;;
	esac

	host=${destination%%:*}
	remote_root=${destination#*:}
	case "${host}" in
	""|*[!A-Za-z0-9_.@-]*) echo "unsafe destination host: ${host}" >&2; return 65 ;;
	esac
	case "${remote_root}" in
	/*) ;;
	*) echo "destination path must be absolute: ${remote_root}" >&2; return 65 ;;
	esac
	case "${remote_root}" in
	*[!A-Za-z0-9_./-]*) echo "unsafe destination path: ${remote_root}" >&2; return 65 ;;
	esac

	token="$(date -u +%Y%m%d%H%M%S).$$"
	stage="${remote_root%/}/.kontrol-publish.${token}"
	lock="${remote_root%/}/.kontrol-publish.lock"

	# The lock is deliberately not auto-stolen.  A stale lock requires an
	# administrator to inspect the previous failed publication first.
	ssh "${host}" "set -eu; mkdir -p '${remote_root}'; mkdir '${lock}'; mkdir '${stage}'" || {
		echo "unable to acquire publication lock on ${destination}" >&2
		return 75
	}

	cleanup_remote() {
		ssh "${host}" "rm -rf '${stage}'; rmdir '${lock}' 2>/dev/null || true" >/dev/null 2>&1 || true
	}

	# Never use --delete here: an older catalog may still be cached by clients.
	# Package payloads are complete before any new catalog reaches the mirror.
	if ! rsync -a --partial --delay-updates \
	    --include='*/' --include='*.pkg' --exclude='*' \
	    "${repository}/All/" "${host}:${remote_root%/}/All/"; then
		cleanup_remote
		return 1
	fi

	if ! rsync -a "${manifest}" \
	    "${repository}/meta.conf" \
	    "${repository}/data.pkg" \
	    "${repository}/packagesite.pkg" \
	    "${host}:${stage}/"; then
		cleanup_remote
		return 1
	fi

	# Revalidate the mirror itself, then move the catalog last.  All mv calls
	# remain on the same filesystem, so each visible metadata replacement is
	# atomic and packagesite.pkg is the commit point.
	if ! ssh "${host}" "sh -s -- '${remote_root}' '${stage}' '${lock}'" << 'REMOTE'
set -eu
root=$1
stage=$2
lock=$3
tab=$(printf '\t')

while IFS="${tab}" read -r expected_hash expected_size relative; do
	case "${relative}" in
	All/*.pkg) ;;
	*) echo "unsafe manifest path: ${relative}" >&2; exit 1 ;;
	esac
	package="${root%/}/${relative}"
	[ -f "${package}" ] || { echo "mirror package missing: ${relative}" >&2; exit 1; }
	if stat -f %z "${package}" >/dev/null 2>&1; then
		actual_size=$(stat -f %z "${package}")
	else
		actual_size=$(stat -c %s "${package}")
	fi
	[ "${actual_size}" = "${expected_size}" ] || {
		echo "mirror size mismatch: ${relative}" >&2
		exit 1
	}
	if command -v sha256 >/dev/null 2>&1; then
		actual_hash=$(sha256 -q "${package}")
	else
		actual_hash=$(sha256sum "${package}" | awk '{ print $1 }')
	fi
	[ "${actual_hash}" = "${expected_hash}" ] || {
		echo "mirror SHA256 mismatch: ${relative}" >&2
		exit 1
	}
done < "${stage}/packages.manifest"

mv -f "${stage}/meta.conf" "${root%/}/meta.conf"
mv -f "${stage}/data.pkg" "${root%/}/data.pkg"
mv -f "${stage}/packagesite.pkg" "${root%/}/packagesite.pkg"
rm -rf "${stage}"
rmdir "${lock}"
REMOTE
	then
		cleanup_remote
		return 1
	fi

	echo "published and verified: ${destination}"
}

for destination in "$@"; do
	publish_one "${destination}"
done

echo "all mirrors published successfully"
