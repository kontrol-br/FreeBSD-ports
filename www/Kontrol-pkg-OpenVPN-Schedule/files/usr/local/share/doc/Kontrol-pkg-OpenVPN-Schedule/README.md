# Kontrol OpenVPN Schedule Package

## Overview
This package enforces OpenVPN login access windows using existing pfSense/Kontrol schedules.

## Installation
1. Install the `www/Kontrol-pkg-OpenVPN-Schedule` port via pkg/ports.
2. `POST-INSTALL` calls `/etc/rc.packages` and runs `openvpn_schedule_install()`.
3. The installer validates the target auth file and applies an idempotent patch with backup + checksum.

## Configuration
1. Open **VPN > OpenVPN Schedule**.
2. The page lists non-admin local users.
3. Select one schedule per user, or **None** for unrestricted access.
4. If no schedules exist in **Firewall > Schedules**, all users remain on **None**.

## How to test
1. Create or validate schedules in **Firewall > Schedules**.
2. Assign a schedule to one test user in **VPN > OpenVPN Schedule**.
3. Test OpenVPN authentication inside and outside the allowed window.
4. Review logs tagged with `[openvpn_schedule]`.

## Deinstall / rollback
1. Remove the package.
2. `pkg-deinstall` runs `openvpn_schedule_deinstall()`.
3. Deinstall restores the original auth file from a checksum-validated backup.
4. If backup validation fails, rollback is aborted with explicit logs for manual recovery.

## Upgrade
- `POST-UPGRADE` migrates legacy config to per-user schedule settings.
- Backups and the manifest (`/var/db/openvpn_schedule/manifest.json`) remain consistent.

## Known limitations
- Runtime patching depends on known anchors in `/etc/inc/openvpn.auth-user.php`.
- If the base file changes incompatibly, installation aborts with a compatibility error (safe behavior).
