#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
CHECK=${HERE}/../files/Kontrol-upgrade-check-repos
ROOT=$(mktemp -d /tmp/Kontrol-upgrade-test.XXXXXX)
trap 'rm -rf "${ROOT}"' EXIT HUP INT TERM
MOCK=${ROOT}/pkg-static
cat > "${MOCK}" <<'MOCKEOF'
#!/bin/sh
printf '%s\n' "$*" >> "${CALL_LOG}"
if [ "$1" = version ]; then
	[ "$4" = "$3" ] && echo = && exit 0
	awk -v a="$3" -v b="$4" 'BEGIN { print ((a+0)<(b+0))?"<":">" }'
	exit 0
fi
conf=; if [ "$1" = -C ]; then conf=$2; shift 2; fi
case "$1:$2" in
	query:%v)
		case "$3" in Kontrol|Kontrol-base|Kontrol-kernel-Kontrol) echo 2.7.2;; *) exit 1;; esac ;;
	update:-f)
		repodir=$(sed -n 's@REPOS_DIR: \[ "\([^"]*\)" \];@\1@p' "$conf")
		template=$(basename "$(find "$repodir" -name '*.conf' | head -1)" .conf)
		eval "available=\${AVAILABLE_${template}:-yes}"
		[ "$available" = yes ] || exit 1 ;;
	rquery:-U)
		repodir=$(sed -n 's@REPOS_DIR: \[ "\([^"]*\)" \];@\1@p' "$conf")
		template=$(basename "$(find "$repodir" -name '*.conf' | head -1)" .conf)
		eval "version=\${VERSION_${template}:-}"
		[ -n "$version" ] || exit 1
		echo "$version" ;;
	*) exit 1 ;;
esac
MOCKEOF
chmod +x "${MOCK}"

run_case()
{
	name=$1 expected_rc=$2 expected=$3
	shift 3
	dir=${ROOT}/${name}; mkdir -p "${dir}/repos" "${dir}/tmp"
	: > "${ROOT}/calls"
	for spec in "$@"; do
		# template,version,eligible,supported,available
		IFS=, read template version eligible supported available <<SPEC
${spec}
SPEC
		cat > "${dir}/repos/${template}.target" <<MARKER
managed_by=Kontrol-repo
eligible=${eligible}
supported_direct=${supported}
channel=release
version=${version}
template=${template}
MARKER
		: > "${dir}/repos/${template}.conf"
		echo FreeBSD:16:amd64 > "${dir}/repos/${template}.abi"
		echo freebsd:16:x86:64 > "${dir}/repos/${template}.altabi"
		eval "export AVAILABLE_${template}=${available} VERSION_${template}=${version}"
	done
	before=$(find "${dir}" -type f -exec sha256sum {} + | sort | sha256sum)
	set +e
	output=$(CALL_LOG=${ROOT}/calls PKG_STATIC=${MOCK} REPO_TEMPLATE_DIR=${dir}/repos \
	    REAL_PKG_DBDIR=${dir}/realdb TMPDIR=${dir}/tmp "${CHECK}" 2>"${dir}/stderr")
	rc=$?
	set -e
	[ "$rc" -eq "$expected_rc" ] || { echo "$name: rc $rc" >&2; exit 1; }
	[ "$output" = "$expected" ] || { echo "$name: output '$output'" >&2; exit 1; }
	after=$(find "${dir}" -type f ! -name stderr -exec sha256sum {} + | sort | sha256sum)
	# stderr is the harness capture, not a system file; all fixtures stay intact.
	[ "$before" = "$after" ] || { echo "$name: fixture changed" >&2; exit 1; }
	[ -z "$(find "${dir}/tmp" -mindepth 1 -print -quit)" ] || { echo "$name: temporary leak" >&2; exit 1; }
	! grep -Eq '(^| )(bootstrap|install|upgrade|annotate|set|delete)( |$)' "${ROOT}/calls" || exit 1
}

run_case direct_290 2 2.9.0 upgrade290,2.9.0,yes,yes,yes
run_case highest 2 2.9.0 upgrade281,2.8.1,yes,yes,yes upgrade290,2.9.0,yes,yes,yes
run_case unmarked_281 0 '' upgrade281,2.8.1,no,no,yes
run_case no_stages 2 2.9.0 upgrade281,2.8.1,yes,yes,yes upgrade290,2.9.0,yes,yes,yes
run_case no_fallback 1 '' upgrade281,2.8.1,yes,no,yes upgrade290,2.9.0,yes,yes,no
run_case irrelevant_failure 2 2.9.0 broken281,2.8.1,no,no,no upgrade290,2.9.0,yes,yes,yes
printf 'all check-repos tests passed\n'
