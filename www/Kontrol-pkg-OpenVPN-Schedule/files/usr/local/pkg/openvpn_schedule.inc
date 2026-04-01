<?php
/*
 * openvpn_schedule.inc
 * Kontrol OpenVPN schedule-based login control.
 */

require_once('config.inc');
require_once('util.inc');
require_once('/usr/local/pkg/openvpn_schedule_hook.inc');

const OVPN_SCHEDULE_LOG_TAG = '[openvpn_schedule]';
const OVPN_SCHEDULE_STATE_DIR = '/var/db/openvpn_schedule';
const OVPN_SCHEDULE_MANIFEST = '/var/db/openvpn_schedule/manifest.json';
const OVPN_SCHEDULE_TARGET = '/etc/inc/openvpn.auth-user.php';
const OVPN_SCHEDULE_REQUIRE_MARK = "require_once('/usr/local/pkg/openvpn_schedule_hook.inc');";
const OVPN_SCHEDULE_CALL_MARK = "openvpn_schedule_enforce_or_exit();";
const OVPN_SCHEDULE_CRON_COMMAND = '/usr/local/bin/php -f /usr/local/pkg/openvpn_schedule_cron.php';

function openvpn_schedule_log($message) {
	log_error(OVPN_SCHEDULE_LOG_TAG . ' ' . $message);
}

function openvpn_schedule_normalized_package_key($key) {
	return preg_replace('/[^a-z0-9]/', '', strtolower((string)$key));
}

function openvpn_schedule_detect_package_key() {
	$installed = config_get_path('installedpackages', []);
	if (is_array($installed)) {
		foreach (array_keys($installed) as $key) {
			if (openvpn_schedule_normalized_package_key($key) === 'openvpnschedule') {
				return (string)$key;
			}
		}
	}
	return 'openvpn_schedule';
}

function openvpn_schedule_package_base_path() {
	return 'installedpackages/' . openvpn_schedule_detect_package_key();
}

function openvpn_schedule_user_schedule_entries($pkgcfg) {
	$user_schedule = $pkgcfg['user_schedule'] ?? [];
	if (is_array($user_schedule) && isset($user_schedule['item']) && is_array($user_schedule['item'])) {
		return $user_schedule['item'];
	}
	return is_array($user_schedule) ? $user_schedule : [];
}

function openvpn_schedule_sync() {
	openvpn_schedule_apply_runtime_patch();
	openvpn_schedule_setup_cron(true);
}

function openvpn_schedule_install() {
	openvpn_schedule_log('install start');
	if (!openvpn_schedule_apply_runtime_patch()) {
		openvpn_schedule_log('install failed: patch apply unsuccessful');
		return;
	}
	openvpn_schedule_setup_cron(true);
	openvpn_schedule_log('install done');
}

function openvpn_schedule_upgrade() {
	openvpn_schedule_log('upgrade start');
	openvpn_schedule_migrate_config();
	if (!openvpn_schedule_apply_runtime_patch()) {
		openvpn_schedule_log('upgrade failed: patch apply unsuccessful');
		return;
	}
	openvpn_schedule_setup_cron(true);
	openvpn_schedule_log('upgrade done');
}

function openvpn_schedule_install_command() {
	openvpn_schedule_install();
}

function openvpn_schedule_upgrade_command() {
	openvpn_schedule_upgrade();
}

function openvpn_schedule_deinstall() {
	openvpn_schedule_log('deinstall start');
	$runtime_restored = openvpn_schedule_restore_runtime_patch();
	if (!$runtime_restored) {
		openvpn_schedule_log('deinstall warning: could not restore original auth file. Manual recovery may be required from backup in ' . OVPN_SCHEDULE_STATE_DIR);
	}

	/*
	 * Always continue package cleanup even when runtime patch rollback fails.
	 * This prevents stale config.xml/package cron leftovers after uninstall.
	 */
	openvpn_schedule_setup_cron(false);
	config_del_path(openvpn_schedule_package_base_path());
	write_config('Removed OpenVPN schedule package configuration');
	@unlink(OVPN_SCHEDULE_MANIFEST);
	if ($runtime_restored) {
		openvpn_schedule_log('deinstall done');
	} else {
		openvpn_schedule_log('deinstall done with warnings');
	}
}

function openvpn_schedule_deinstall_command() {
	openvpn_schedule_deinstall();
}

function openvpn_schedule_migrate_config() {
	$cfg = config_get_path(openvpn_schedule_package_base_path(), []);
	if (!is_array($cfg)) {
		config_set_path(openvpn_schedule_package_base_path(), []);
		write_config('Migrated OpenVPN schedule package configuration');
		return;
	}

	$pkgcfg = $cfg['config'][0] ?? [];
	$entries = openvpn_schedule_user_schedule_entries($pkgcfg);

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
		foreach ($entries as $entry) {
			if (($entry['username'] ?? '') === $username) {
				$already_set = true;
				break;
			}
		}
		if (!$already_set) {
			$entries[] = [
				'username' => $username,
				'schedule' => $schedule,
			];
		}
	}

	$pkgcfg['user_schedule'] = ['item' => array_values($entries)];

	// Keep defaults open (none) for users without explicit schedule.
	unset($pkgcfg['default_policy']);
	unset($pkgcfg['rule']);

	$cfg['config'][0] = $pkgcfg;
	config_set_path(openvpn_schedule_package_base_path(), $cfg);
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

function openvpn_schedule_insert_before_first($content, $needle, $insert, &$insert_count) {
	$pos = strpos($content, $needle);
	if ($pos === false) {
		$insert_count = 0;
		return $content;
	}
	$insert_count = 1;
	return substr($content, 0, $pos) . $insert . $needle . substr($content, $pos + strlen($needle));
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

	$before_sha = hash_file('sha256', OVPN_SCHEDULE_TARGET);
	$backup_file = OVPN_SCHEDULE_STATE_DIR . '/openvpn.auth-user.php.' . gmdate('YmdHis') . '.bak';
	if (!@copy(OVPN_SCHEDULE_TARGET, $backup_file)) {
		openvpn_schedule_log('failed to create backup file: ' . $backup_file);
		return false;
	}
	$backup_sha = hash_file('sha256', $backup_file);

	$patched = $current;

	if (strpos($patched, OVPN_SCHEDULE_REQUIRE_MARK) === false) {
		$require_count = 0;
		$require_anchors = [
			'require_once("auth.inc");',
			"require_once('auth.inc');",
		];
		foreach ($require_anchors as $needle) {
			$patched = openvpn_schedule_insert_before_first($patched, $needle, OVPN_SCHEDULE_REQUIRE_MARK . "\n", $require_count);
			if ((int)$require_count > 0) {
				break;
			}
		}
		if ((int)$require_count === 0) {
			$patched = preg_replace(
				"/(require_once\\((['\"])auth\\.inc\\2\\);\\s*)/m",
				"$1" . OVPN_SCHEDULE_REQUIRE_MARK . "\n",
				$patched,
				1,
				$require_count
			);
		}
		if ((int)$require_count === 0) {
			openvpn_schedule_log('compatibility error: auth include anchor not found, refusing to patch');
			return false;
		}
	}

	if (strpos($patched, OVPN_SCHEDULE_CALL_MARK) === false) {
		$call_count = 0;
		$call_needles = [
			// Most common pfSense 2.7.x format.
			"foreach (\$authmodes as \$authmode) {",
			// Older variants.
			'$authcfg = array();',
			'$authcfg = [];',
			// Additional fallback anchors.
			'$authenticated = false;',
		];
		foreach ($call_needles as $needle) {
			$patched = openvpn_schedule_insert_before_first($patched, $needle, OVPN_SCHEDULE_CALL_MARK . "\n\n", $call_count);
			if ((int)$call_count > 0) {
				break;
			}
		}
		if ((int)$call_count === 0) {
			$call_anchors = [
				"/(^\\s*foreach\\s*\\(\\s*\\$authmodes\\s+as\\s+\\$authmode\\s*\\)\\s*\\{)/m" => OVPN_SCHEDULE_CALL_MARK . "\n\n$1",
				"/(\\$authcfg\\s*=\\s*(?:array\\s*\\(\\s*\\)|\\[\\s*\\])\\s*;)/m" => OVPN_SCHEDULE_CALL_MARK . "\n\n$1",
				"/(^\\s*\\$authenticated\\s*=\\s*false\\s*;)/m" => OVPN_SCHEDULE_CALL_MARK . "\n\n$1",
				"/(^\\s*\\$authenticated\\s*=\\s*authenticate_user\\s*\\()/m" => OVPN_SCHEDULE_CALL_MARK . "\n\n$1",
			];
			foreach ($call_anchors as $pattern => $replacement) {
				$patched = preg_replace($pattern, $replacement, $patched, 1, $call_count);
				if ((int)$call_count > 0) {
					break;
				}
			}
		}
		if ((int)$call_count === 0) {
			openvpn_schedule_log('compatibility error: no supported runtime auth anchor found, refusing to patch');
			return false;
		}
	}

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

function openvpn_schedule_has_enforced_user_schedules() {
	$pkgcfg = config_get_path(openvpn_schedule_package_base_path() . '/config/0', []);
	foreach (openvpn_schedule_user_schedule_entries($pkgcfg) as $entry) {
		$schedule = trim((string)($entry['schedule'] ?? ''));
		if ($schedule !== '' && strcasecmp($schedule, 'none') !== 0) {
			return true;
		}
	}
	return false;
}

function openvpn_schedule_setup_cron($active = true) {
	$should_enable = (bool)$active && openvpn_schedule_has_enforced_user_schedules();
	if ($should_enable) {
		install_cron_job(OVPN_SCHEDULE_CRON_COMMAND, true, '*/5', '*', '*', '*', '*', 'root');
		openvpn_schedule_log('cron watcher enabled (every 5 minutes)');
		return;
	}
	install_cron_job(OVPN_SCHEDULE_CRON_COMMAND, false);
	openvpn_schedule_force_remove_cron_entries();
	openvpn_schedule_log('cron watcher disabled');
}

function openvpn_schedule_force_remove_cron_entries() {
	$items = config_get_path('cron/item', []);
	if (!is_array($items) || empty($items)) {
		return;
	}

	$filtered = [];
	$removed = 0;
	foreach ($items as $item) {
		$command = (string)($item['command'] ?? '');
		if (strpos($command, '/usr/local/pkg/openvpn_schedule_cron.php') !== false) {
			$removed++;
			continue;
		}
		$filtered[] = $item;
	}

	if ($removed > 0) {
		config_set_path('cron/item', $filtered);
		write_config('Removed OpenVPN schedule cron watcher entries');
		if (function_exists('configure_cron')) {
			configure_cron();
		}
		openvpn_schedule_log("cron watcher cleanup removed {$removed} stale entr" . ($removed === 1 ? 'y' : 'ies'));
	}
}

function openvpn_schedule_collect_connected_users_from_status($status_file) {
	$users = [];
	$inside_client_list = false;
	$lines = @file($status_file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
	if (!is_array($lines)) {
		return [];
	}

	foreach ($lines as $line) {
		$line = trim((string)$line);
		if ($line === '') {
			continue;
		}
		if (strpos($line, 'Common Name,') === 0) {
			$inside_client_list = true;
			continue;
		}
		if (!$inside_client_list) {
			continue;
		}
		if (strpos($line, 'ROUTING TABLE') === 0 || $line === 'GLOBAL STATS' || $line === 'END') {
			break;
		}

		$columns = str_getcsv($line);
		$username = trim((string)($columns[0] ?? ''));
		if ($username !== '' && strcasecmp($username, 'UNDEF') !== 0) {
			$users[$username] = true;
		}
	}

	return array_keys($users);
}

function openvpn_schedule_collect_connected_users() {
	$users = [];
	$status_files = array_merge(
		glob('/var/etc/openvpn/*.status') ?: [],
		glob('/var/etc/openvpn/status*') ?: [],
		glob('/tmp/openvpn-status*') ?: []
	);
	$status_files = array_unique($status_files);

	foreach ($status_files as $status_file) {
		foreach (openvpn_schedule_collect_connected_users_from_status($status_file) as $username) {
			$users[$username] = true;
		}
	}

	foreach (openvpn_schedule_collect_connected_users_from_management() as $username) {
		$users[$username] = true;
	}

	return array_keys($users);
}

function openvpn_schedule_collect_connected_users_from_management() {
	$users = [];
	$sockets = glob('/var/etc/openvpn/server*.sock') ?: [];

	foreach ($sockets as $socket_file) {
		$socket = @stream_socket_client('unix://' . $socket_file, $errno, $errstr, 1);
		if ($socket === false) {
			continue;
		}

		stream_set_timeout($socket, 1);
		fwrite($socket, "status 2\n");
		fwrite($socket, "quit\n");

		while (!feof($socket)) {
			$line = fgets($socket);
			if ($line === false) {
				break;
			}
			$line = trim((string)$line);
			if (strpos($line, 'CLIENT_LIST,') !== 0) {
				continue;
			}

			$columns = str_getcsv($line);
			$common_name = trim((string)($columns[1] ?? ''));
			$username = trim((string)($columns[9] ?? ''));

			if ($username !== '' && strcasecmp($username, 'UNDEF') !== 0) {
				$users[$username] = true;
			}
			if ($common_name !== '' && strcasecmp($common_name, 'UNDEF') !== 0) {
				$users[$common_name] = true;
			}
		}

		fclose($socket);
	}

	return array_keys($users);
}

function openvpn_schedule_disconnect_user_from_openvpn($username) {
	$sockets = glob('/var/etc/openvpn/server*.sock') ?: [];
	$disconnected = false;

	foreach ($sockets as $socket_file) {
		$socket = @stream_socket_client('unix://' . $socket_file, $errno, $errstr, 1);
		if ($socket === false) {
			continue;
		}
		fwrite($socket, "kill {$username}\n");
		fwrite($socket, "quit\n");
		fclose($socket);
		$disconnected = true;
	}

	return $disconnected;
}

function openvpn_schedule_disconnect_out_of_schedule_users() {
	$connected_users = openvpn_schedule_collect_connected_users();
	if (empty($connected_users)) {
		openvpn_schedule_log('cron watcher: no connected users found');
		return;
	}

	foreach ($connected_users as $username) {
		$schedule = openvpn_schedule_find_policy($username);
		if (strcasecmp($schedule, 'none') === 0) {
			continue;
		}
		if (openvpn_schedule_now_matches($schedule)) {
			continue;
		}

		if (openvpn_schedule_disconnect_user_from_openvpn($username)) {
			openvpn_schedule_log("cron watcher: disconnected user '{$username}' outside schedule '{$schedule}'");
		} else {
			openvpn_schedule_log("cron watcher: unable to disconnect user '{$username}' outside schedule '{$schedule}' (management socket unavailable)");
		}
	}
}
