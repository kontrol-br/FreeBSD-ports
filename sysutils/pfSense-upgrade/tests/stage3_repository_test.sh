#!/bin/sh
# shellcheck disable=SC2016

set -eu

script="${0%/*}/../files/Kontrol-upgrade"
tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT

# This models the exact predicate used by RELENG_2_9_0 get_pkg_info(): an
# installed add-on whose pkg %R differs from product_name is not returned.
cat > "${tmp}/pkgdb" <<'DB'
Kontrol-pkg-Cron|sysutils/Kontrol-pkg-Cron|Kontrol-offline
Kontrol-pkg-E2guardian54|www/Kontrol-pkg-E2guardian54|Kontrol-offline
Kontrol-pkg-Service_Watchdog|sysutils/Kontrol-pkg-Service_Watchdog|Kontrol-offline
Kontrol-core|sysutils/Kontrol-core|Kontrol-offline
DB

gui_packages() {
	awk -F'|' '$3 == "Kontrol" { print $1 }' "${tmp}/pkgdb"
}

[ -z "$(gui_packages)" ]
# Simulate pkg-static set -r Kontrol-offline:Kontrol from the real stage-3
# caller, preserving package names and origins byte-for-byte.
sed -i '/^Kontrol-pkg-/s/|Kontrol-offline$/|Kontrol/' "${tmp}/pkgdb"
for package in Kontrol-pkg-Cron Kontrol-pkg-E2guardian54 Kontrol-pkg-Service_Watchdog; do
	[ "$(gui_packages | grep -cxF "${package}")" -eq 1 ]
done
grep -qxF 'Kontrol-pkg-E2guardian54|www/Kontrol-pkg-E2guardian54|Kontrol' "${tmp}/pkgdb"
grep -qxF 'Kontrol-core|sysutils/Kontrol-core|Kontrol-offline' "${tmp}/pkgdb"

# Repeating normalization is idempotent and does not invent registrations.
cp "${tmp}/pkgdb" "${tmp}/before"
sed -i '/^Kontrol-pkg-/s/|Kontrol-offline$/|Kontrol/' "${tmp}/pkgdb"
cmp -s "${tmp}/before" "${tmp}/pkgdb"

set_line=$(grep -n 'restore_additional_package_repositories$' "${script}" | head -1 | cut -d: -f1)
upgrade_line=$(grep -n 'pkg_static upgrade${dont_update}' "${script}" | tail -1 | cut -d: -f1)
annotation_line=$(grep -n '_pkg annotate -q -D ${kernel_pkg} next_stage' "${script}" | tail -1 | cut -d: -f1)
[ "${upgrade_line}" -lt "${set_line}" ]
[ "${set_line}" -lt "${annotation_line}" ]

echo 'stage-3 repository metadata tests: PASS'
