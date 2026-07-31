# Kontrol 2.7.2 upgrade-check caller contract

This note records the read-only review of `kontrol-br/pfsense`, branch
`RELENG_2_7_2` (`src/etc/inc/pkg-utils.inc`, function
`get_system_pkg_version()`).  That source is deliberately not vendored or
modified by these ports changes.

* The existing PHP caller executes `/usr/local/sbin/Kontrol-upgrade -c` with no
  additional arguments. PHP's `exec()` retains the last stdout line in
  `$output` and the process status in `$rc`.
* Status 2 means an update is available. The caller splits the retained stdout
  line on spaces and treats its first token as the available version. Thus the
  compatible output remains `<version> version of Kontrol is available`.
* Status 0 means no update. Any status other than 0 or 2 (including the
  wrapper's lock-contention status 75) makes the function return `false` and
  is not interpreted as a version.
* The GUI cache consists of the configured version-cache file and its `.rc`
  companion. If a sufficiently fresh `.rc` contains 2, stdout is read from the
  version-cache file; a cached 0 avoids executing the command. Other values
  cause a fresh check. Successful 0/2 results are written only when the caller
  requested a cache update. Errors do not refresh either cache file.

Consequently, direct-release discovery is part of `-c`, preserves statuses 0
and 2 and the one-line version format, and sends alternate-repository failures
to stderr. The wrapper remains responsible for `lockf(1)` and preserves its
`EX_TEMPFAIL` (75) behavior.
