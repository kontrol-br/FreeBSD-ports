# Codex prompt: preserve add-on registration during the 2.7.2 to 2.9.0 upgrade

Work in the Kontrol FreeBSD ports repository on the branch that builds the
final Kontrol 2.9.0 release.  Audit the implementation against the exact
`RELENG_2_9_0` base before editing it.  Do not assume that a successful pkg(8)
transaction means that the Package Manager GUI registration is intact.

## Observed failure

A system starts on Kontrol 2.7.2 with these add-ons installed:

* `Kontrol-pkg-Cron`
* `Kontrol-pkg-E2guardian54` (the spelling and capitalization are exact)

The direct 2.7.2 to 2.9.0 upgrade reaches the second stage.  The upgrader locks
the `Kontrol-pkg-*` packages, upgrades the base/core packages, unlocks the
add-ons, and later reports that the new add-on packages were installed
successfully.  After booting 2.9.0:

* `pkg-static` still reports the add-on packages as installed;
* their Services menu entries remain present and the services can leave files
  and configuration behind;
* **System > Package Manager > Installed Packages** says that no packages are
  installed;
* there is consequently no GUI uninstall action for either add-on.

This occurred independently with Cron and E2guardian54.  Treat it as a
systemic stage-2 add-on registration/migration defect, not an E2guardian-only
exception.  Do not solve it by requiring users to remove all add-ons before
the OS upgrade unless investigation proves that correct migration is
impossible and documents why.

## Required investigation

1. Trace the complete second- and third-stage upgrade flow in the 2.9.0
   `Kontrol-upgrade`, especially:
   * locking and unlocking `Kontrol-pkg-*`;
   * `pkg-static upgrade -r Kontrol-core`;
   * `pkg-static upgrade -r Kontrol`;
   * the final unrestricted `pkg-static upgrade`;
   * package scripts executed on old-package deinstall and new-package install;
   * reboots and `next_stage`/`new_major` annotations.
2. Determine exactly how the 2.9.0 Package Manager builds its Installed
   Packages list.  Compare, before and after each stage:
   * the real pkg database (`pkg-static info`, origins, automatic flags and
     annotations);
   * `/cf/conf/config.xml`, particularly every relevant element below
     `<installedpackages>`;
   * package XML descriptors and any generated package metadata/cache;
   * menu/service registration and package install/deinstall script effects.
3. Establish which operation removes or fails to recreate the GUI package
   registration.  Check whether the old package's deinstall script deletes the
   registration after the new package has written it, whether package locking
   changes script order, whether `-r Kontrol` excludes add-ons from the proper
   install path, and whether install scripts run while the old PHP/config API
   is still active.
4. Audit the ports and package scripts for both exact packages named above,
   plus at least one other representative `Kontrol-pkg-*` add-on.  A generic
   fix must not rely on case-folded package names.

## Required behavior

After upgrading 2.7.2 to 2.9.0 with supported add-ons installed:

* every add-on that remains installed in the pkg database must appear exactly
  once in **Installed Packages**;
* the GUI entry must refer to the installed 2.9.0 package/version and provide a
  working uninstall action;
* Services menu entries must correspond to registered, installed packages;
* add-on configuration must be preserved unless that add-on explicitly
  documents an incompatible migration;
* packages removed from the 2.9.0 repository must be removed cleanly, including
  stale GUI/menu/service registration;
* rerunning the repair/synchronization logic must be idempotent;
* package names and origins must remain case-correct;
* the fix must not fabricate GUI registrations for arbitrary packages merely
  because their names share a prefix.

Prefer fixing package lifecycle ordering or the authoritative registration
mechanism over reconstructing config.xml heuristically.  If a reconciliation
step is necessary, derive its allowlist and descriptors from installed,
package-managed 2.9.0 metadata and make it transactional with a backup and
clear diagnostics.

## Tests and evidence

Add an automated harness that simulates or exercises the actual stage-2 and
stage-3 caller, not only a helper.  Cover at minimum:

1. Cron installed on 2.7.2 remains installed and GUI-registered on 2.9.0.
2. E2guardian54 installed on 2.7.2 remains installed and GUI-registered on
   2.9.0.
3. Both packages installed together remain two distinct GUI entries.
4. A representative third add-on follows the same behavior.
5. An add-on unavailable in 2.9.0 is removed without stale menu/service/XML.
6. A failed add-on upgrade does not silently erase its previous registration.
7. Reboot/resume at every `next_stage` boundary is idempotent.
8. Running the final reconciliation twice creates no duplicates.
9. GUI uninstall after upgrade removes the package and its registration.
10. The pkg database and config.xml are captured before/after each stage so
    the test proves where registration is preserved or repaired.

Run `sh -n` and `shellcheck` on changed scripts, the repository's upgrade test
suite, `git diff --check`, and appropriate port/package checks.  In the final
report include the proven root cause, command ordering before and after the
fix, config/pkg database evidence, tests, migration risk, and rollback plan.

