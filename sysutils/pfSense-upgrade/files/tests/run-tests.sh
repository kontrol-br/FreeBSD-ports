#!/bin/sh
set -eu

BASE_DIR="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
ENGINE="${BASE_DIR}/Kontrol-upgrade"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

export KONTROL_UPGRADE_TEST_MODE=1

pass=0
fail=0

run_test() {
	name="$1"
	shift
	if "$@"; then
		echo "ok - ${name}"
		pass=$((pass+1))
	else
		echo "not ok - ${name}"
		fail=$((fail+1))
	fi
}

mk_repo_profile() {
	p="$1"
	cat > "${p}" <<EOF_CONF
Kontrol: {
  url: "pkg+https://repo.example.invalid"
}
EOF_CONF
	echo "FreeBSD:14:amd64" > "${p%.conf}.abi"
	echo "freebsd:14:x86:64" > "${p%.conf}.altabi"
}

test_parser_abi() {
	repo="${TMP}/repo.conf"
	mk_repo_profile "${repo}"
	out="$(${ENGINE} --state-file "${TMP}/state1.json" --repo-profile "${repo}" check >/dev/null 2>&1; ${ENGINE} --state-file "${TMP}/state1.json" status 2>/dev/null | tr -d '\n')"
	echo "${out}" | grep -q 'FreeBSD:14:amd64'
}

test_state_machine_normal() {
	repo="${TMP}/repo2.conf"
	mk_repo_profile "${repo}"
	${ENGINE} --state-file "${TMP}/state2.json" --repo-profile "${repo}" apply --assume-yes --no-reboot >/dev/null 2>&1 || true
	${ENGINE} --state-file "${TMP}/state2.json" resume --assume-yes >/dev/null 2>&1
	${ENGINE} --state-file "${TMP}/state2.json" status | grep -q '"current_state": "DONE"'
}

test_power_loss_resume() {
	repo="${TMP}/repo3.conf"
	mk_repo_profile "${repo}"
	${ENGINE} --state-file "${TMP}/state3.json" --repo-profile "${repo}" prepare --assume-yes >/dev/null 2>&1
	${ENGINE} --state-file "${TMP}/state3.json" --repo-profile "${repo}" apply --assume-yes --no-reboot >/dev/null 2>&1 || true
	${ENGINE} --state-file "${TMP}/state3.json" resume --assume-yes >/dev/null 2>&1
	${ENGINE} --state-file "${TMP}/state3.json" status | grep -q '"current_state": "DONE"'
}

test_major_repo_switch() {
	repo="${TMP}/repo4.conf"
	mk_repo_profile "${repo}"
	echo "FreeBSD:15:amd64" > "${repo%.conf}.abi"
	${ENGINE} --state-file "${TMP}/state4.json" --repo-profile "${repo}" check >/dev/null 2>&1
	${ENGINE} --state-file "${TMP}/state4.json" status | grep -q 'FreeBSD:15:amd64'
}

test_controlled_rollback() {
	repo="${TMP}/repo5.conf"
	mk_repo_profile "${repo}"
	${ENGINE} --state-file "${TMP}/state5.json" --repo-profile "${repo}" check >/dev/null 2>&1
	${ENGINE} --state-file "${TMP}/state5.json" abort >/dev/null 2>&1 || true
	${ENGINE} --state-file "${TMP}/state5.json" status | grep -q '"current_state": "ROLLBACK_NEEDED"'
}

run_test "parser ABI" test_parser_abi
run_test "upgrade normal" test_state_machine_normal
run_test "power loss resume" test_power_loss_resume
run_test "major repo switch" test_major_repo_switch
run_test "controlled rollback" test_controlled_rollback

[ ${fail} -eq 0 ]
