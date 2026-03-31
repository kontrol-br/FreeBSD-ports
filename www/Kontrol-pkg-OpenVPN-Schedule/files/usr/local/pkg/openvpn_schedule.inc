<?php
/*
 * openvpn_schedule.inc
 * Kontrol OpenVPN schedule-based login control.
 */

require_once('config.inc');
require_once('util.inc');

const OVPN_SCHEDULE_LOG_TAG = '[openvpn_schedule]';
const OVPN_SCHEDULE_STATE_DIR = '/var/db/openvpn_schedule';
const OVPN_SCHEDULE_MANIFEST = '/var/db/openvpn_schedule/manifest.json';
const OVPN_SCHEDULE_TARGET = '/etc/inc/openvpn.auth-user.php';
const OVPN_SCHEDULE_REQUIRE_MARK = "require_once('/usr/local/pkg/openvpn_schedule_hook.inc');";
const OVPN_SCHEDULE_CALL_MARK = "openvpn_schedule_enforce_or_exit();";

function openvpn_schedule_log($message) {
	log_error(OVPN_SCHEDULE_LOG_TAG . ' ' . $message);
}

function openvpn_schedule_sync() {
	openvpn_schedule_apply_runtime_patch();
}

function openvpn_schedule_install() {
	openvpn_schedule_log('install start');
	if (!openvpn_schedule_apply_runtime_patch()) {
		openvpn_schedule_log('install failed: patch apply unsuccessful');
		return;
	}
	openvpn_schedule_log('install done');
}

function openvpn_schedule_upgrade() {
	openvpn_schedule_log('upgrade start');
	openvpn_schedule_migrate_config();
	if (!openvpn_schedule_apply_runtime_patch()) {
		openvpn_schedule_log('upgrade failed: patch apply unsuccessful');
		return;
	}
	openvpn_schedule_log('upgrade done');
}

function openvpn_schedule_deinstall() {
	openvpn_schedule_log('deinstall start');
	if (!openvpn_schedule_restore_runtime_patch()) {
		openvpn_schedule_log('deinstall failed: could not restore original auth file. Manual recovery required from backup in ' . OVPN_SCHEDULE_STATE_DIR);
		return;
	}
	config_del_path('installedpackages/openvpn_schedule');
	write_config('Removed OpenVPN schedule package configuration');
	@unlink(OVPN_SCHEDULE_MANIFEST);
	@rmdir(OVPN_SCHEDULE_STATE_DIR);
	openvpn_schedule_log('deinstall done');
}

function openvpn_schedule_migrate_config() {
	$cfg = config_get_path('installedpackages/openvpn_schedule', []);
	if (!is_array($cfg)) {
		config_set_path('installedpackages/openvpn_schedule', []);
		write_config('Migrated OpenVPN schedule package configuration');
		return;
	}

	$pkgcfg = $cfg['config'][0] ?? [];
	if (!isset($pkgcfg['user_schedule']) || !is_array($pkgcfg['user_schedule'])) {
		$pkgcfg['user_schedule'] = [];
	}

	// Backward compatibility: import legacy user rules into per-user schedules.
	foreach (($pkgcfg['rule'] ?? []) as $rule) {
		if (($rule['type'] ?? '') !== 'user') {
			continue;
		}
		$username = trim($rule['name'] ?? '');
		$schedule = trim($rule['schedule'] ?? '');
		if ($username === '' || $schedule === '' || strcasecmp($schedule, 'none') === 0) {
			continue;
		}
		$already_set = false;
		foreach ($pkgcfg['user_schedule'] as $entry) {
			if (($entry['username'] ?? '') === $username) {
				$already_set = true;
				break;
			}
		}
		if (!$already_set) {
			$pkgcfg['user_schedule'][] = [
				'username' => $username,
				'schedule' => $schedule,
			];
		}
	}

	// Keep defaults open (none) for users without explicit schedule.
	unset($pkgcfg['default_policy']);
	unset($pkgcfg['rule']);

	$cfg['config'][0] = $pkgcfg;
	config_set_path('installedpackages/openvpn_schedule', $cfg);
	write_config('Migrated OpenVPN schedule package configuration');
}

function openvpn_schedule_build_manifest($backup_file, $target_sha256, $backup_sha256) {
	return [
		'target' => OVPN_SCHEDULE_TARGET,
		'backup' => $backup_file,
		'target_sha256_before' => $target_sha256,
		'backup_sha256' => $backup_sha256,
		'patched_at' => gmdate('c')
	];
}

function openvpn_schedule_read_manifest() {
	if (!file_exists(OVPN_SCHEDULE_MANIFEST)) {
		return null;
	}
	$data = json_decode(file_get_contents(OVPN_SCHEDULE_MANIFEST), true);
	return is_array($data) ? $data : null;
}

function openvpn_schedule_apply_runtime_patch() {
	safe_mkdir(OVPN_SCHEDULE_STATE_DIR);
	if (!file_exists(OVPN_SCHEDULE_TARGET)) {
		openvpn_schedule_log('compatibility error: target file not found ' . OVPN_SCHEDULE_TARGET);
		return false;
	}

	$current = file_get_contents(OVPN_SCHEDULE_TARGET);
	if (strpos($current, OVPN_SCHEDULE_REQUIRE_MARK) !== false && strpos($current, OVPN_SCHEDULE_CALL_MARK) !== false) {
		openvpn_schedule_log('runtime patch already present, nothing to do');
		return true;
	}

	$has_anchor_require = strpos($current, "require_once('auth.inc');") !== false;
	$has_anchor_call = strpos($current, '$authcfg = array();') !== false;
	if (!$has_anchor_require || !$has_anchor_call) {
		openvpn_schedule_log('compatibility error: unsupported openvpn auth file version, refusing to patch');
		return false;
	}

	$before_sha = hash_file('sha256', OVPN_SCHEDULE_TARGET);
	$backup_file = OVPN_SCHEDULE_STATE_DIR . '/openvpn.auth-user.php.' . gmdate('YmdHis') . '.bak';
	if (!@copy(OVPN_SCHEDULE_TARGET, $backup_file)) {
		openvpn_schedule_log('failed to create backup file: ' . $backup_file);
		return false;
	}
	$backup_sha = hash_file('sha256', $backup_file);

	$patched = str_replace(
		"require_once('auth.inc');",
		"require_once('auth.inc');\n" . OVPN_SCHEDULE_REQUIRE_MARK,
		$current
	);
	$patched = str_replace(
		'$authcfg = array();',
		OVPN_SCHEDULE_CALL_MARK . "\n\n" . '$authcfg = array();',
		$patched
	);

	if ($patched === $current) {
		openvpn_schedule_log('idempotency warning: generated patch unchanged, keeping existing file');
		return true;
	}

	if (@file_put_contents(OVPN_SCHEDULE_TARGET, $patched) === false) {
		openvpn_schedule_log('failed to write patched target, attempting rollback');
		@copy($backup_file, OVPN_SCHEDULE_TARGET);
		return false;
	}

	$manifest = openvpn_schedule_build_manifest($backup_file, $before_sha, $backup_sha);
	file_put_contents(OVPN_SCHEDULE_MANIFEST, json_encode($manifest, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES));
	openvpn_schedule_log('runtime patch applied with backup ' . $backup_file);
	return true;
}

function openvpn_schedule_restore_runtime_patch() {
	$manifest = openvpn_schedule_read_manifest();
	if (empty($manifest)) {
		openvpn_schedule_log('no manifest found, trying inline rollback');
		if (!file_exists(OVPN_SCHEDULE_TARGET)) {
			return true;
		}
		$current = file_get_contents(OVPN_SCHEDULE_TARGET);
		$current = str_replace("\n" . OVPN_SCHEDULE_REQUIRE_MARK, '', $current);
		$current = str_replace(OVPN_SCHEDULE_CALL_MARK . "\n\n", '', $current);
		return @file_put_contents(OVPN_SCHEDULE_TARGET, $current) !== false;
	}

	$backup = $manifest['backup'] ?? '';
	if (empty($backup) || !file_exists($backup)) {
		openvpn_schedule_log('deinstall abort: backup file is missing: ' . $backup);
		return false;
	}
	$backup_sha = hash_file('sha256', $backup);
	if ($backup_sha !== ($manifest['backup_sha256'] ?? '')) {
		openvpn_schedule_log('deinstall abort: backup checksum mismatch for ' . $backup);
		return false;
	}
	if (!@copy($backup, OVPN_SCHEDULE_TARGET)) {
		openvpn_schedule_log('deinstall abort: failed restoring original file from backup');
		return false;
	}
	@unlink($backup);
	@unlink(OVPN_SCHEDULE_MANIFEST);
	openvpn_schedule_log('runtime patch restored from backup');
	return true;
}
