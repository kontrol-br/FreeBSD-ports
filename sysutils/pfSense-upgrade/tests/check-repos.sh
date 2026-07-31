#!/bin/sh
set -eu
HERE=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
CHECK=${HERE}/../files/Kontrol-upgrade-check-repos
ROOT=$(mktemp -d /tmp/Kontrol-upgrade-test.XXXXXX)
trap 'rm -rf "${ROOT}"' EXIT HUP INT TERM
MOCK=${ROOT}/pkg-static

# The packaged marker must name the product-renamed installed templates.  A
# source-tree pfSense name makes every real Kontrol check fail before pkg runs.
grep -q '^template=%%PRODUCT_NAME%%-repo-upgrade$' \
	"${HERE}/../../pfSense-repo/files/Kontrol-repo-upgrade.target"
cat > "${MOCK}" <<'MOCKEOF'
#!/bin/sh
[ -z "${ABI:-}" ] && [ -z "${ALTABI:-}" ] && [ -z "${OSVERSION:-}" ] || {
	echo "inherited ABI environment reached pkg-static" >&2
	exit 1
}
printf '%s\n' "$*" >> "${CALL_LOG}"
conf=; if [ "$1" = -C ]; then conf=$2; shift 2; fi
if [ "$1" = version ]; then
	echo 'pkg-static: Warning: Major OS version upgrade detected.' >&2
	[ "$4" = "$3" ] && echo = && exit 0
	awk -v a="$3" -v b="$4" 'BEGIN {
		na=split(a, av, "."); nb=split(b, bv, "."); n=(na>nb?na:nb)
		for (i=1; i<=n; i++) { if ((av[i]+0)<(bv[i]+0)) { print "<"; exit }; if ((av[i]+0)>(bv[i]+0)) { print ">"; exit } }
		print "="
	}'
	exit 0
fi
case "$1:$2" in
	query:%v)
		case "$3" in Kontrol|Kontrol-base|Kontrol-kernel-Kontrol) echo 2.8.1;; *) exit 1;; esac ;;
	update:-f)
		echo 'Updating mock repository catalogue...'
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
		IFS=, read -r template version eligible supported available signature_policy <<SPEC
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
		echo 1600000 > "${dir}/repos/${template}.osversion"
		eval "export AVAILABLE_${template}=${available} VERSION_${template}=${version}"
	done
	before=$(find "${dir}" -type f -exec sha256sum {} + | sort | sha256sum)
	set +e
	mkdir -p "${dir}/snap"
	output=$(ABI=FreeBSD:15:amd64 ALTABI=freebsd:15:x86:64 OSVERSION=1500000 \
	    CALL_LOG=${ROOT}/calls SNAP_DIR=${dir}/snap PKG_STATIC=${MOCK} REPO_TEMPLATE_DIR=${dir}/repos \
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
		if grep -q ALTABI "${dir}"/snap/pkg.*.conf; then exit 1; fi
	fi
}

run_case direct_290 2 2.9.0 upgrade290,2.9.0,yes,yes,yes
run_case highest 2 2.10.0 upgrade290,2.9.0,yes,yes,yes upgrade2100,2.10.0,yes,yes,yes
run_case unmarked_281 0 '' upgrade281,2.8.1,no,no,yes
run_case no_stages 2 2.9.0 upgrade281,2.8.1,yes,yes,yes upgrade290,2.9.0,yes,yes,yes
run_case no_fallback 1 '' upgrade281,2.8.1,yes,no,yes upgrade290,2.9.0,yes,yes,no
run_case irrelevant_failure 2 2.9.0 broken281,2.8.1,no,no,no upgrade290,2.9.0,yes,yes,yes

# An unsigned query is allowed only by an explicit, strictly valid policy.
run_case unsigned_not_authorized 1 '' upgrade290,2.9.0,yes,yes,yes,invalid
run_case signed_target 2 2.9.0 upgrade290,2.9.0,yes,yes,yes,fingerprints

set +e
mkdir -p "${ROOT}/template-snap"
template_output=$(ABI=FreeBSD:15:amd64 ALTABI=freebsd:15:x86:64 OSVERSION=1500000 \
	CALL_LOG=${ROOT}/calls SNAP_DIR=${ROOT}/template-snap PKG_STATIC=${MOCK} \
	REPO_TEMPLATE_DIR=${ROOT}/direct_290/repos REAL_PKG_DBDIR=${ROOT}/realdb \
	TMPDIR=${ROOT}/direct_290/tmp KONTROL_UPGRADE_OUTPUT=template "${CHECK}" 2>/dev/null)
template_rc=$?
set -e
[ "${template_rc}" -eq 2 ]
[ "${template_output}" = '2.9.0|upgrade290' ]

# Starting the real upgrade (not -c) refuses to reinstall from the old repo.
REQUIRE=${ROOT}/require.sh
awk '/^require_direct_upgrade_repo\(\)/,/^}/' \
	"${HERE}/../files/Kontrol-upgrade" > "${ROOT}/require-function"
mkdir -p "${ROOT}/require/templates" "${ROOT}/require/active"
: > "${ROOT}/require/templates/current.conf"
: > "${ROOT}/require/templates/upgrade290.conf"
ln -s "${ROOT}/require/templates/current.conf" \
	"${ROOT}/require/active/Kontrol.conf"
cat > "${ROOT}/target-helper" <<'TARGET_HELPER'
#!/bin/sh
echo '2.9.0|upgrade290'
exit 2
TARGET_HELPER
chmod +x "${ROOT}/target-helper"
cat > "${REQUIRE}" <<REQUIRE_HEAD
#!/bin/sh
product=Kontrol
logfile=/dev/null
KONTROL_CHECK_REPOS=${ROOT}/target-helper
KONTROL_REPO_TEMPLATE_DIR=${ROOT}/require/templates
KONTROL_REPO_ACTIVE_DIR=${ROOT}/require/active
export KONTROL_CHECK_REPOS KONTROL_REPO_TEMPLATE_DIR KONTROL_REPO_ACTIVE_DIR
_echo() { echo "\$*"; }
_exit() { exit "\$1"; }
REQUIRE_HEAD
cat "${ROOT}/require-function" >> "${REQUIRE}"
printf '%s\n' 'require_direct_upgrade_repo' >> "${REQUIRE}"
chmod +x "${REQUIRE}"
set +e
require_output=$("${REQUIRE}")
require_rc=$?
set -e
[ "${require_rc}" -eq 1 ]
[ "${require_output}" = "ERROR: Kontrol 2.9.0 is available, but the active repository is not set to 2.9.0.
Select the Kontrol 2.9.0 repository on the upgrade page and try again.
No packages were changed." ]
[ "$(readlink "${ROOT}/require/active/Kontrol.conf")" = \
	"${ROOT}/require/templates/current.conf" ]
ln -sfn "${ROOT}/require/templates/upgrade290.conf" \
	"${ROOT}/require/active/Kontrol.conf"
[ -z "$("${REQUIRE}")" ]

# The GUI socket exists before an early preflight error, allowing the legacy
# page to poll the text log instead of remaining on its initialization text.
START_PROGRESS=${ROOT}/start-progress.sh
awk '/^start_progress_listener\(\)/,/^}/' \
	"${HERE}/../files/Kontrol-upgrade" > "${ROOT}/start-progress-function"
cat > "${ROOT}/nc" <<'MOCK_NC'
#!/bin/sh
socket=$2
: > "${socket}"
sleep 30
MOCK_NC
chmod +x "${ROOT}/nc"
cat > "${START_PROGRESS}" <<PROGRESS_HEAD
#!/bin/sh
PATH=${ROOT}:\${PATH}
progress_socket=${ROOT}/gui.sock
progress_file=${ROOT}/gui.json
nc_pid=
PROGRESS_HEAD
cat "${ROOT}/start-progress-function" >> "${START_PROGRESS}"
cat >> "${START_PROGRESS}" <<'PROGRESS_TAIL'
start_progress_listener
[ -e "${progress_socket}" ]
kill "${nc_pid}"
wait "${nc_pid}" 2>/dev/null || :
PROGRESS_TAIL
chmod +x "${START_PROGRESS}"
"${START_PROGRESS}"
[ "$(grep -c '^[[:space:]]*start_progress_listener$' "${HERE}/../files/Kontrol-upgrade")" -ge 1 ]
grep -q 'signature_type: none' "${ROOT}/direct_290/snap"/repo.*.conf
[ "$(grep -c 'signature_type: none' "${ROOT}/direct_290/snap"/repo.*.conf)" -eq 2 ]
grep -q 'signature_type: "fingerprints"' "${ROOT}/signed_target/snap"/repo.*.conf
grep -q 'signature_type: "fingerprints"' "${ROOT}/direct_290/repos/upgrade290.conf"

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

if grep -q 'gnid' "${HERE}/../files/Kontrol-upgrade"; then exit 1; fi
grep -q 'exit 75' "${HERE}/../files/Kontrol-upgrade"
if grep -q '/etc/platform' "${HERE}/../files/Kontrol-upgrade"; then exit 1; fi
grep -q 'sbin/Kontrol-repo-setup' "${HERE}/../Makefile"
grep -q 'libexec/Kontrol-upgrade-check-repos' "${HERE}/../Makefile"
printf 'all check-repos tests passed\n'
