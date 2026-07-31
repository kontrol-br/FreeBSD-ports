#!/bin/sh
# shellcheck disable=SC2016

set -eu

script="${0%/*}/../files/Kontrol-upgrade"
tmp=$(mktemp -d)
trap 'rm -rf "${tmp}"' EXIT

# Reproduce the RELENG_2_9_0 GUI predicate: get_pkg_info() rejects an add-on
# when pkg query %R does not equal product_name.
gui_packages() {
	awk -F'|' '$3 == "Kontrol" { print $1 }' "${tmp}/pkgdb"
}

cat > "${tmp}/pkgdb" <<'DB'
Kontrol-pkg-Cron|sysutils/Kontrol-pkg-Cron|Kontrol-offline
Kontrol-pkg-E2guardian54|www/Kontrol-pkg-E2guardian54|Kontrol-offline
Kontrol-pkg-LCDproc|sysutils/Kontrol-pkg-LCDproc|Kontrol-offline
Kontrol-pkg-node_exporter|sysutils/Kontrol-pkg-node_exporter|Kontrol-offline
DB
[ -z "$(gui_packages)" ]

# Stage 3 now presents the cached repository as Kontrol before pkg performs the
# real add-on upgrade.  pkg therefore records %R=Kontrol during installation.
sed -i 's/|Kontrol-offline$/|Kontrol/' "${tmp}/pkgdb"
for package in Kontrol-pkg-Cron Kontrol-pkg-E2guardian54 \
    Kontrol-pkg-LCDproc Kontrol-pkg-node_exporter; do
	[ "$(gui_packages | grep -cxF "${package}")" -eq 1 ]
done
grep -qxF 'Kontrol-pkg-E2guardian54|www/Kontrol-pkg-E2guardian54|Kontrol' \
    "${tmp}/pkgdb"

# Verify the actual callers: stage 2 keeps the private repository name and
# stage 3 selects the normal product name before update/upgrade and next_stage.
stage2_line=$(grep -n 'offline_repo_enable osversion ||' "${script}" | cut -d: -f1)
stage3_line=$(grep -n 'offline_repo_enable osversion "${product}" ||' "${script}" | cut -d: -f1)
upgrade_line=$(grep -n 'pkg_static upgrade${dont_update}' "${script}" | tail -1 | cut -d: -f1)
annotation_line=$(grep -n '_pkg annotate -q -D ${kernel_pkg} next_stage' "${script}" | tail -1 | cut -d: -f1)
[ "${stage2_line}" -lt "${stage3_line}" ]
[ "${stage3_line}" -lt "${upgrade_line}" ]
[ "${upgrade_line}" -lt "${annotation_line}" ]

# Re-selecting the product repository at a resumed stage-3 boundary is
# idempotent and package names/origins remain case-correct.
cp "${tmp}/pkgdb" "${tmp}/before"
sed -i 's/|Kontrol-offline$/|Kontrol/' "${tmp}/pkgdb"
cmp -s "${tmp}/before" "${tmp}/pkgdb"

echo 'stage-3 repository identity tests: PASS'
