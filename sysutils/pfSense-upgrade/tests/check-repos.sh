#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
CHECK=${HERE}/../files/Kontrol-upgrade-check-repos
ROOT=$(mktemp -d /tmp/Kontrol-upgrade-test.XXXXXX)
trap 'rm -rf "${ROOT}"' EXIT HUP INT TERM
MOCK=${ROOT}/pkg-static

cat > "${MOCK}" <<'MOCKEOF'
#!/bin/sh
[ -z "${ABI:-}" ] && [ -z "${ALTABI:-}" ] && [ -z "${OSVERSION:-}" ] || exit 90
conf=; if [ "$1" = -C ]; then conf=$2; shift 2; fi
case "$1:$2" in
	version:-t)
		[ "$3" = "$4" ] && { echo =; exit 0; }
		case "$3:$4" in
			2.9.2:*) echo '>' ;;
			2.9.1:2.9.0|2.9.1:2.8.*) echo '>' ;;
			*) echo '<' ;;
		esac
		;;
	query:%v)
		case "$3" in Kontrol|Kontrol-base|Kontrol-kernel-Kontrol) echo 2.9.0;; *) exit 1;; esac
		;;
	update:-f)
		cp "$conf" "${SNAP_DIR}/pkg.conf"
		repodir=$(sed -n 's@REPOS_DIR: \[ "\([^"]*\)" \];@\1@p' "$conf")
		template=$(basename "$(find "$repodir" -name '*.conf' | head -1)" .conf)
		eval "available=\${AVAILABLE_${template}:-yes}"
		[ "$available" = yes ]
		;;
	rquery:-U)
		repodir=$(sed -n 's@REPOS_DIR: \[ "\([^"]*\)" \];@\1@p' "$conf")
		template=$(basename "$(find "$repodir" -name '*.conf' | head -1)" .conf)
		eval "version=\${VERSION_${template}:-}"
		[ -n "$version" ] && echo "$version"
		;;
	*) exit 1 ;;
esac
MOCKEOF
chmod +x "${MOCK}"

run_case()
{
	name=$1 expected_rc=$2 expected=$3 osversion=$4
	shift 4
	dir=${ROOT}/${name}; mkdir -p "${dir}/repos" "${dir}/tmp" "${dir}/snap"
	for spec in "$@"; do
		IFS=, read template version eligible supported available policy <<SPEC
${spec}
SPEC
		cat > "${dir}/repos/${template}.target" <<MARKER
managed_by=Kontrol-repo
eligible=${eligible}
supported_direct=${supported}
channel=release
version=${version}
template=${template}
signature_policy=${policy}
MARKER
		cat > "${dir}/repos/${template}.conf" <<CONF
Kontrol-core: { signature_type: "fingerprints" }
Kontrol: { signature_type: "fingerprints" }
CONF
		echo FreeBSD:16:amd64 > "${dir}/repos/${template}.abi"
		echo "${osversion}" > "${dir}/repos/${template}.osversion"
		eval "export AVAILABLE_${template}=${available} VERSION_${template}=${version}"
	done
	set +e
	output=$(ABI=FreeBSD:16:amd64 ALTABI=freebsd:16:x86:64 OSVERSION=1600014 \
	    SNAP_DIR=${dir}/snap PKG_STATIC=${MOCK} REPO_TEMPLATE_DIR=${dir}/repos \
	    REAL_PKG_DBDIR=${dir}/realdb TMPDIR=${dir}/tmp "${CHECK}" 2>/dev/null)
	rc=$?
	set -e
	[ "${rc}" -eq "${expected_rc}" ] || { echo "${name}: rc=${rc}" >&2; exit 1; }
	[ "${output}" = "${expected}" ] || { echo "${name}: output=${output}" >&2; exit 1; }
	[ -z "$(find "${dir}/tmp" -mindepth 1 -print -quit)" ] || exit 1
	if [ -f "${dir}/snap/pkg.conf" ]; then
		grep -q 'ABI: "FreeBSD:16:amd64";' "${dir}/snap/pkg.conf"
		grep -q "OSVERSION: ${osversion};" "${dir}/snap/pkg.conf"
		! grep -q ALTABI "${dir}/snap/pkg.conf"
	fi
}

run_case direct_291 2 2.9.1 1600014 upgrade291,2.9.1,yes,yes,yes,fingerprints
run_case highest 2 2.9.2 1600014 upgrade291,2.9.1,yes,yes,yes,fingerprints upgrade292,2.9.2,yes,yes,yes,fingerprints
run_case unmarked 0 '' 1600014 upgrade291,2.9.1,no,no,yes,fingerprints
run_case unavailable 1 '' 1600014 upgrade291,2.9.1,yes,yes,no,fingerprints
run_case bad_policy 1 '' 1600014 upgrade291,2.9.1,yes,yes,yes,invalid
run_case wrong_major 1 '' 1700000 upgrade291,2.9.1,yes,yes,yes,fingerprints

# Starting a real upgrade must not change the active repository automatically.
awk '/^require_direct_upgrade_repo\(\)/,/^}/' \
	"${HERE}/../files/Kontrol-upgrade" > "${ROOT}/require-function"
mkdir -p "${ROOT}/require/templates" "${ROOT}/require/active"
: > "${ROOT}/require/templates/current.conf"
: > "${ROOT}/require/templates/upgrade291.conf"
ln -s "${ROOT}/require/templates/current.conf" "${ROOT}/require/active/Kontrol.conf"
cat > "${ROOT}/target-helper" <<'TARGET'
#!/bin/sh
echo '2.9.1|upgrade291'
exit 2
TARGET
chmod +x "${ROOT}/target-helper"
cat > "${ROOT}/require.sh" <<REQUIRE
#!/bin/sh
product=Kontrol
KONTROL_CHECK_REPOS=${ROOT}/target-helper
KONTROL_REPO_TEMPLATE_DIR=${ROOT}/require/templates
KONTROL_REPO_ACTIVE_DIR=${ROOT}/require/active
export KONTROL_CHECK_REPOS KONTROL_REPO_TEMPLATE_DIR KONTROL_REPO_ACTIVE_DIR
_echo() { echo "\$*"; }
_exit() { exit "\$1"; }
REQUIRE
cat "${ROOT}/require-function" >> "${ROOT}/require.sh"
echo require_direct_upgrade_repo >> "${ROOT}/require.sh"
chmod +x "${ROOT}/require.sh"
set +e
output=$("${ROOT}/require.sh")
rc=$?
set -e
[ "${rc}" -eq 1 ]
echo "${output}" | grep -q 'active repository is not set to 2.9.1'
[ "$(readlink "${ROOT}/require/active/Kontrol.conf")" = \
	"${ROOT}/require/templates/current.conf" ]
ln -sfn "${ROOT}/require/templates/upgrade291.conf" "${ROOT}/require/active/Kontrol.conf"
[ -z "$("${ROOT}/require.sh")" ]

echo 'check-repos tests: PASS'
