#!/bin/sh
set -eu

base=$(CDPATH='' cd -- "$(dirname "$0")/.." && pwd)
helper=${base}/files/Kontrol-upgrade-direct
upgrade=${base}/files/Kontrol-upgrade
repo_port=$(CDPATH='' cd -- "${base}/../pfSense-repo" && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/check-repos.XXXXXX")
trap 'rm -rf "${work}"' EXIT HUP INT TERM

fail() { echo "not ok - $*" >&2; exit 1; }
pass() { echo "ok - $*"; }

mkdir -p "${work}/bin" "${work}/repos" "${work}/tmp"
cat > "${work}/bin/pkg-static" <<'EOF'
#!/bin/sh
: "${PKG_LOG:?}"
printf 'ENV ABI=%s ALTABI=%s OSVERSION=%s ARGS=%s\n' "${ABI-unset}" "${ALTABI-unset}" "${OSVERSION-unset}" "$*" >> "${PKG_LOG}"
conf=""
if [ "${1-}" = -C ]; then conf=$2; shift 2; fi
case "${1-}" in
update)
	cp "${conf}" "${CAPTURE_CONFIG}"
	cat "${conf}" >> "${PKG_LOG}"
	repos=$(sed -n 's/^REPOS_DIR: \[ "\(.*\)" \];/\1/p' "${conf}")
	template=$(find "${repos}" -name '*.conf' -maxdepth 1 | head -1)
	case "$(basename "${template}")" in *unavailable*) exit 1;; esac
	exit 0
	;;
query)
	echo "2.8.1"
	;;
rquery)
	echo "${REMOTE_VERSION:-2.9.0}"
	;;
version)
	[ "${2}" = -t ] && shift
	python3 - "$2" "$3" <<'PY'
import sys
def v(s): return tuple(int(x) for x in s.split('.')[:3])
print('<' if v(sys.argv[1]) < v(sys.argv[2]) else '>' if v(sys.argv[1]) > v(sys.argv[2]) else '=')
PY
	;;
*) exit 1;;
esac
EOF
chmod +x "${work}/bin/pkg-static"

make_target()
{
	name=$1 version=$2 eligible=${3:-yes} policy=${4:-fingerprints}
	cat > "${work}/repos/${name}.target" <<EOF
managed_by=Kontrol-repo
eligible=${eligible}
supported_direct=yes
channel=release
version=${version}
template=${name}
signature_policy=${policy}
EOF
	cp "${repo_port}/files/Kontrol-repo-upgrade.conf" "${work}/repos/${name}.conf"
	sed -i 's/%%SIGNATURE_TYPE%%/fingerprints/g' "${work}/repos/${name}.conf"
	echo FreeBSD:16:amd64 > "${work}/repos/${name}.abi"
	echo 1600000 > "${work}/repos/${name}.osversion"
}

run_helper()
{
	PKG_LOG=${work}/pkg.log CAPTURE_CONFIG=${work}/captured.conf \
	PKG_STATIC=${work}/bin/pkg-static REPO_DIR=${work}/repos TMPDIR=${work}/tmp \
	ABI=FreeBSD:15:amd64 ALTABI=freebsd:15:x86:64 OSVERSION=1500000 \
	REMOTE_VERSION=${REMOTE_VERSION:-2.9.0} "${helper}"
}

make_target Kontrol-repo-upgrade 2.9.0
set +e
output=$(run_helper 2>"${work}/err")
rc=$?
set -e
[ "${rc}" -eq 2 ] || fail "direct update status was ${rc}"
[ "${output}" = '2.9.0 version of Kontrol is available' ] || fail "direct update output changed: ${output}"
pass "active 2.8.1 discovers exact direct 2.9.0 output/status"

REMOTE_VERSION=2.8.1
export REMOTE_VERSION
set +e
output=$(run_helper 2>/dev/null); rc=$?
set -e
if [ "${rc}" -ne 0 ] || [ -n "${output}" ]; then fail "no-update helper result"; fi
unset REMOTE_VERSION
pass "no direct package update returns zero without stdout"

rm -f "${work}/repos"/*
make_target Kontrol-repo-invalid 9.9.9 no
echo 'administrator repository' > "${work}/repos/random.conf"
set +e
output=$(run_helper 2>/dev/null); rc=$?
set -e
if [ "${rc}" -ne 0 ] || [ -n "${output}" ]; then fail "ineligible marker accepted"; fi
pass "unmanaged configuration and ineligible marker are ignored"

rm -f "${work}/repos"/*
make_target Kontrol-repo-unavailable 9.9.9
make_target Kontrol-repo-upgrade 2.9.0
set +e
output=$(run_helper 2>/dev/null); rc=$?
set -e
if [ "${rc}" -ne 2 ] || [ "${output}" != '2.9.0 version of Kontrol is available' ]; then fail "failed target stopped discovery"; fi
pass "unavailable authorized target does not stop eligible release discovery"

rm -f "${work}/repos"/*
make_target Kontrol-repo-first 2.9.0
make_target Kontrol-repo-second 2.10.0
set +e
output=$(run_helper 2>/dev/null); rc=$?
set -e
if [ "${rc}" -ne 2 ] || [ "${output}" != '2.10.0 version of Kontrol is available' ]; then fail "highest target not selected"; fi
pass "multiple targets select the greatest pkg version"

grep -q 'ENV ABI=unset ALTABI=unset OSVERSION=unset' "${work}/pkg.log" || fail "ABI environment leaked"
! grep '^ENV ' "${work}/pkg.log" | grep -qv 'ABI=unset ALTABI=unset OSVERSION=unset' || fail "an isolated child inherited ABI state"
grep -q '^ABI: "FreeBSD:16:amd64";' "${work}/captured.conf" || fail "temporary ABI missing"
grep -q '^OSVERSION: 1600000;' "${work}/captured.conf" || fail "temporary OSVERSION missing"
! grep -q ALTABI "${work}/captured.conf" || fail "temporary ALTABI present"
pass "isolated pkg children clear inherited ABI state and use FreeBSD 16 config"

rm -f "${work}/repos"/*
make_target Kontrol-repo-unsigned 2.9.0 yes none
before=$(sha256sum "${work}/repos/Kontrol-repo-unsigned.conf")
set +e
run_helper >/dev/null 2>/dev/null; rc=$?
set -e
[ "${rc}" -eq 2 ] || fail "authorized unsigned target failed"
[ "$(sha256sum "${work}/repos/Kontrol-repo-unsigned.conf")" = "${before}" ] || fail "installed template changed"
[ "$(grep -c 'signature_type: "fingerprints"' "${work}/repos/Kontrol-repo-unsigned.conf")" -eq 2 ] || fail "installed signatures changed"
pass "authorized unsigned policy rewrites both entries only in disposable copy"

grep -q 'exit 75' "${upgrade}" || fail "lock status 75 missing"
! grep -q 'gnid' "${upgrade}" || fail "GNID remains"
grep -q 'start_progress_listener' "${upgrade}" || fail "early GUI socket listener missing"
grep -q 'select the .* repository in System Update' "${upgrade}" || fail "branch-selection preflight missing"
pass "caller lock, preflight, GUI listener and GNID invariants"

grep -q 'sbin/Kontrol-repo-setup' "${base}/Makefile" || fail "repo setup removed"
grep -q 'libexec/Kontrol-upgrade-direct' "${base}/Makefile" || fail "helper not packaged"
grep -q 'DIRECT_REPOS=.*Kontrol-repo-upgrade' "${repo_port}/Makefile" || fail "target absent from plist"
grep -q 'PFSENSE_DEFAULT_REPO?=.*pfSense-repo-devel' "${repo_port}/Makefile" || fail "default repository changed"
! grep -Eq 'ln -sf.*upgrade|pkg_repo_conf_path.*upgrade' "${repo_port}/files/pkg-install.in" || fail "post-install selects target"
pass "packaging preserves setup/current default and does not select 2.9.0"

echo "All repository upgrade checks passed."
