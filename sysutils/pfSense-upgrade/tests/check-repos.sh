#!/bin/sh
set -eu
HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
CHECK=${HERE}/../files/Kontrol-upgrade-check-repos
ROOT=$(mktemp -d /tmp/Kontrol-upgrade-test.XXXXXX)
trap 'rm -rf "${ROOT}"' EXIT HUP INT TERM
MOCK=${ROOT}/pkg-static

# The packaged marker must name the product-renamed installed templates.  A
# source-tree pfSense name makes every real Kontrol check fail before pkg runs.
grep -q '^template=Kontrol-repo-upgrade$' \
	"${HERE}/../../pfSense-repo/files/pfSense-repo-upgrade.target"
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
		cp "$conf" "${SNAP_DIR}/pkg.$$.conf"
		repodir=$(sed -n 's@REPOS_DIR: \[ "\([^"]*\)" \];@\1@p' "$conf")
		template=$(basename "$(find "$repodir" -name '*.conf' | head -1)" .conf)
		cp "$repodir/$template.conf" "${SNAP_DIR}/repo.$$.conf"
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
		# template,version,eligible,supported,available,signature_policy
		IFS=, read template version eligible supported available signature_policy <<SPEC
${spec}
SPEC
		: "${signature_policy:=none}"
		cat > "${dir}/repos/${template}.target" <<MARKER
managed_by=Kontrol-repo
eligible=${eligible}
supported_direct=${supported}
channel=release
version=${version}
template=${template}
signature_policy=${signature_policy}
MARKER
		cat > "${dir}/repos/${template}.conf" <<CONF
Kontrol-core: {
  signature_type: "fingerprints"
}
Kontrol: {
  signature_type: "fingerprints"
}
CONF
		echo FreeBSD:16:amd64 > "${dir}/repos/${template}.abi"
		eval "export AVAILABLE_${template}=${available} VERSION_${template}=${version}"
	done
	before=$(find "${dir}" -type f -exec sha256sum {} + | sort | sha256sum)
	set +e
	mkdir -p "${dir}/snap"
	output=$(CALL_LOG=${ROOT}/calls SNAP_DIR=${dir}/snap PKG_STATIC=${MOCK} REPO_TEMPLATE_DIR=${dir}/repos \
	    REAL_PKG_DBDIR=${dir}/realdb TMPDIR=${dir}/tmp "${CHECK}" 2>"${dir}/stderr")
	rc=$?
	set -e
	[ "$rc" -eq "$expected_rc" ] || { echo "$name: rc $rc" >&2; exit 1; }
	[ "$output" = "$expected" ] || { echo "$name: output '$output'" >&2; exit 1; }
	after=$(find "${dir}" -type f ! -path '*/snap/*' ! -name stderr -exec sha256sum {} + | sort | sha256sum)
	# stderr is the harness capture, not a system file; all fixtures stay intact.
	[ "$before" = "$after" ] || { echo "$name: fixture changed" >&2; exit 1; }
	[ -z "$(find "${dir}/tmp" -mindepth 1 -print -quit)" ] || { echo "$name: temporary leak" >&2; exit 1; }
	! grep -Eq '(^| )(bootstrap|install|upgrade|annotate|set|delete)( |$)' "${ROOT}/calls" || exit 1
	if find "${dir}/snap" -name 'pkg.*.conf' -print -quit | grep -q .; then
		grep -q 'ABI: "FreeBSD:16:amd64";' "${dir}"/snap/pkg.*.conf
		grep -q 'OSVERSION: 1600000;' "${dir}"/snap/pkg.*.conf
		! grep -q ALTABI "${dir}"/snap/pkg.*.conf
	fi
}

run_case direct_290 2 2.9.0 upgrade290,2.9.0,yes,yes,yes
run_case highest 2 2.9.0 upgrade281,2.8.1,yes,yes,yes upgrade290,2.9.0,yes,yes,yes
run_case unmarked_281 0 '' upgrade281,2.8.1,no,no,yes
run_case no_stages 2 2.9.0 upgrade281,2.8.1,yes,yes,yes upgrade290,2.9.0,yes,yes,yes
run_case no_fallback 1 '' upgrade281,2.8.1,yes,no,yes upgrade290,2.9.0,yes,yes,no
run_case irrelevant_failure 2 2.9.0 broken281,2.8.1,no,no,no upgrade290,2.9.0,yes,yes,yes

# An unsigned query is allowed only by an explicit, strictly valid policy.
run_case unsigned_not_authorized 1 '' upgrade290,2.9.0,yes,yes,yes,invalid
run_case signed_target 2 2.9.0 upgrade290,2.9.0,yes,yes,yes,fingerprints
grep -q 'signature_type: none' "${ROOT}/direct_290/snap"/repo.*.conf
[ "$(grep -c 'signature_type: none' "${ROOT}/direct_290/snap"/repo.*.conf)" -eq 2 ]
grep -q 'signature_type: "fingerprints"' "${ROOT}/signed_target/snap"/repo.*.conf
grep -q 'signature_type: "fingerprints"' "${ROOT}/direct_290/repos/upgrade290.conf"

# Debug mode explains the exact failing phase without contaminating stdout.
DEBUG_LOG=${ROOT}/debug.log
set +e
debug_output=$(CALL_LOG=${ROOT}/calls SNAP_DIR=${ROOT}/no_fallback/snap PKG_STATIC=${MOCK} \
	REPO_TEMPLATE_DIR=${ROOT}/no_fallback/repos REAL_PKG_DBDIR=${ROOT}/realdb \
	TMPDIR=${ROOT}/no_fallback/tmp KONTROL_UPGRADE_DEBUG=yes \
	KONTROL_UPGRADE_DEBUG_LOG=${DEBUG_LOG} AVAILABLE_upgrade290=no \
	"${CHECK}" 2>/dev/null)
debug_rc=$?
set -e
[ "${debug_rc}" -eq 1 ]
[ -z "${debug_output}" ]
grep -q 'authorized target version=2.9.0' "${DEBUG_LOG}"
grep -q 'pkg update failed for target=2.9.0' "${DEBUG_LOG}"
grep -q 'eligible targets failed verification' "${DEBUG_LOG}"

# Exercise the real check_upgrade caller function with its dependencies mocked.
CALLER=${ROOT}/caller.sh
awk '/^check_upgrade\(\)/,/^}/' "${HERE}/../files/Kontrol-upgrade" > "${ROOT}/check-function"
cat > "${CALLER}" <<CALLER_HEAD
#!/bin/sh
product=Kontrol
KONTROL_CHECK_REPOS=${CHECK}
REPO_TEMPLATE_DIR=\$1
TMPDIR=\$2
export KONTROL_CHECK_REPOS REPO_TEMPLATE_DIR TMPDIR
_echo() { echo "\$*"; }
notice() { :; }
get_meta_pkg_name() { echo Kontrol; }
pkg_update() { :; }
pkg_upgrade_repo() { echo 'ERROR: Unable to compare version of Kontrol-repo' >&2; return 1; }
compare_pkg_version() { echo =; }
_pkg() {
	if [ "\$1" = query ]; then echo Kontrol-base Kontrol-kernel-Kontrol; else return 1; fi
}
CALLER_HEAD
cat "${ROOT}/check-function" >> "${CALLER}"
cat >> "${CALLER}" <<'CALLER_TAIL'
check_upgrade
exit $?
CALLER_TAIL
chmod +x "${CALLER}"
set +e
caller_output=$(PKG_STATIC=${MOCK} CALL_LOG=${ROOT}/calls SNAP_DIR=${ROOT}/direct_290/snap \
	REAL_PKG_DBDIR=${ROOT}/realdb "${CALLER}" "${ROOT}/direct_290/repos" "${ROOT}/direct_290/tmp" 2>/dev/null)
caller_rc=$?
set -e
[ "${caller_rc}" -eq 2 ]
[ "${caller_output}" = '2.9.0 version of Kontrol is available' ]

! grep -q 'gnid' "${HERE}/../files/Kontrol-upgrade"
grep -q 'exit 75' "${HERE}/../files/Kontrol-upgrade"
printf 'all check-repos tests passed\n'
