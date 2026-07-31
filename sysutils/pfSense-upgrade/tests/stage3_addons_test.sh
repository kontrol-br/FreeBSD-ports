#!/bin/sh

set -eu

SCRIPT="${0%/*}/../files/Kontrol-upgrade"
KONTROL_UPGRADE_FUNCTIONS_ONLY=1 . "${SCRIPT}"

tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT
product=Kontrol
pkg_prefix=Kontrol-pkg-
logfile=/dev/null
config="${tmp}/config.xml"
printf '%s\n' '<installedpackages/>' > "${config}"

# Redirect the production backup paths while preserving the production body.
reconcile_test() {
	sed "s#/cf/conf/config.xml#${config}#g" "${SCRIPT}" |
	    sed -n '/^reconcile_additional_packages()/,/^}/p' > "${tmp}/fn"
	. "${tmp}/fn"
	reconcile_additional_packages
}

installed='Kontrol-pkg-Cron Kontrol-pkg-E2guardian54 Kontrol-pkg-Service_Watchdog'
remote="${installed}"
registrations=''

_pkg() {
	case "$1:$2" in
	query:-e) printf '%s\n' ${installed} ;;
	rquery:-U)
		name=${6}
		for candidate in ${remote}; do
			[ "${candidate}" = "${name}" ] && printf '%s\n' "${name}"
		done
		;;
	query:%Fp)
		printf '/usr/local/share/%s/info.xml\n' "$3"
		;;
	esac
}

_exec() {
	name=${1##* }
	[ "${name}" = "Kontrol-pkg-Fail" ] && return 1
	case " ${registrations} " in
	*" ${name} "*) ;;
	*) registrations="${registrations} ${name}" ;;
	esac
	return 0
}

reconcile_test
reconcile_test
for name in Kontrol-pkg-Cron Kontrol-pkg-E2guardian54 Kontrol-pkg-Service_Watchdog; do
	count=$(printf '%s\n' ${registrations} | grep -cx "${name}")
	[ "${count}" -eq 1 ] || exit 1
done

# Case is exact and a package absent from the 2.9.0 catalogue is not
# fabricated by reconciliation.
installed="${installed} Kontrol-pkg-e2guardian54 Kontrol-pkg-Removed"
reconcile_test
case " ${registrations} " in
*' Kontrol-pkg-e2guardian54 '*|*' Kontrol-pkg-Removed '*) exit 1 ;;
esac

# A lifecycle failure preserves the configuration backup for rollback.
installed='Kontrol-pkg-Fail'
remote='Kontrol-pkg-Fail'
printf '%s\n' '<installedpackages><package>old</package></installedpackages>' > "${config}"
if reconcile_test; then
	exit 1
fi
grep -q '<package>old</package>' "${config}"

# Assert that the actual stage-3 caller reconciles before deleting next_stage.
reconcile_line=$(grep -n 'reconcile_additional_packages || _exit 1' "${SCRIPT}" | cut -d: -f1)
annotation_line=$(grep -n '_pkg annotate -q -D ${kernel_pkg} next_stage' "${SCRIPT}" | tail -1 | cut -d: -f1)
[ "${reconcile_line}" -lt "${annotation_line}" ]

echo 'stage-3 add-on registration tests: PASS'
