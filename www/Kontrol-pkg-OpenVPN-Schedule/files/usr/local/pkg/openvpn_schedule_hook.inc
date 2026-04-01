<?php
/*
 * openvpn_schedule_hook.inc
 * Loaded only from OpenVPN authentication flow.
 */

require_once('auth.inc');
require_once('config.inc');
require_once('util.inc');

if (!function_exists('openvpn_schedule_log_runtime')) {
	function openvpn_schedule_log_runtime($message) {
		log_error('[openvpn_schedule] ' . $message);
	}
}

if (!function_exists('openvpn_schedule_now_matches')) {
	function openvpn_schedule_get_schedule_config($schedule_name) {
		foreach (config_get_path('schedules/schedule', []) as $schedule) {
			if (strcasecmp(trim((string)($schedule['name'] ?? '')), $schedule_name) === 0) {
				return is_array($schedule) ? $schedule : [];
			}
		}
		return [];
	}

	function openvpn_schedule_eval_with_core_engine($schedule_name, array $schedule_cfg) {
		if (function_exists('filter_schedule_in_effect')) {
			foreach ([$schedule_cfg, $schedule_name] as $arg) {
				try {
					return (bool)filter_schedule_in_effect($arg);
				} catch (Throwable $e) {
					/* Ignore and continue trying compatibility signatures. */
				}
			}
		}

		if (function_exists('is_schedule_in_time')) {
			foreach ([$schedule_cfg, $schedule_name] as $arg) {
				try {
					return (bool)is_schedule_in_time($arg);
				} catch (Throwable $e) {
					/* Ignore and continue trying compatibility signatures. */
				}
			}
		}

		return null;
	}

	function openvpn_schedule_now_matches($schedule_name) {
		$schedule_name = trim((string)$schedule_name);
		if ($schedule_name === '' || strcasecmp($schedule_name, 'none') === 0) {
			return true;
		}

		$schedule_cfg = openvpn_schedule_get_schedule_config($schedule_name);
		if (empty($schedule_cfg)) {
			openvpn_schedule_log_runtime("schedule '{$schedule_name}' not found, denying login");
			return false;
		}

		$core_match = openvpn_schedule_eval_with_core_engine($schedule_name, $schedule_cfg);
		if ($core_match !== null) {
			return (bool)$core_match;
		}

		// Safe fallback: if schedule engine is unavailable, do not block logins.
		openvpn_schedule_log_runtime("schedule engine not available for '{$schedule_name}', fallback allow");
		return true;
	}
}

if (!function_exists('openvpn_schedule_find_policy')) {
	function openvpn_schedule_hook_normalized_package_key($key) {
		return preg_replace('/[^a-z0-9]/', '', strtolower((string)$key));
	}

	function openvpn_schedule_hook_package_base_path() {
		$installed = config_get_path('installedpackages', []);
		if (is_array($installed)) {
			foreach (array_keys($installed) as $key) {
				if (openvpn_schedule_hook_normalized_package_key($key) === 'openvpnschedule') {
					return 'installedpackages/' . $key;
				}
			}
		}
		return 'installedpackages/openvpn_schedule';
	}

	function openvpn_schedule_get_entries($pkg) {
		$user_schedule = $pkg['user_schedule'] ?? [];
		if (is_array($user_schedule) && isset($user_schedule['item']) && is_array($user_schedule['item'])) {
			return $user_schedule['item'];
		}
		return is_array($user_schedule) ? $user_schedule : [];
	}

	function openvpn_schedule_find_policy($username) {
		$pkg = config_get_path(openvpn_schedule_hook_package_base_path() . '/config/0', []);

		foreach (openvpn_schedule_get_entries($pkg) as $entry) {
			if (strcasecmp((string)($entry['username'] ?? ''), $username) === 0) {
				return trim($entry['schedule'] ?? 'none');
			}
		}

		// Backward compatibility with the previous user/group rule format.
		foreach (($pkg['rule'] ?? []) as $rule) {
			if (($rule['type'] ?? '') === 'user' && strcasecmp((string)($rule['name'] ?? ''), $username) === 0) {
				return trim($rule['schedule'] ?? 'none');
			}
		}

		return 'none';
	}
}

if (!function_exists('openvpn_schedule_enforce_or_exit')) {
	function openvpn_schedule_extract_username_from_argv_file() {
		$argv = $_SERVER['argv'] ?? [];
		if (!is_array($argv)) {
			return '';
		}

		foreach ($argv as $idx => $arg) {
			if ($idx === 0) {
				continue;
			}
			$path = trim((string)$arg);
			if ($path === '' || !is_file($path) || !is_readable($path)) {
				continue;
			}
			$lines = @file($path, FILE_IGNORE_NEW_LINES);
			if (!is_array($lines) || !isset($lines[0])) {
				continue;
			}
			$username = trim((string)$lines[0]);
			if ($username !== '') {
				return $username;
			}
		}

		return '';
	}

	function openvpn_schedule_extract_username() {
		$candidates = [
			$_POST['username'] ?? '',
			$_REQUEST['username'] ?? '',
			$_ENV['username'] ?? '',
			$_SERVER['username'] ?? '',
			$_SERVER['common_name'] ?? '',
			getenv('username') ?: '',
			$GLOBALS['username'] ?? '',
		];

		foreach ($candidates as $candidate) {
			$username = trim((string)$candidate);
			if ($username !== '') {
				return $username;
			}
		}

		return openvpn_schedule_extract_username_from_argv_file();
	}

	function openvpn_schedule_enforce_or_exit() {
		$username = openvpn_schedule_extract_username();
		if (empty($username)) {
			openvpn_schedule_log_runtime('username is empty at auth hook, skipping schedule check');
			return;
		}

		$schedule = openvpn_schedule_find_policy($username);
		$allowed = openvpn_schedule_now_matches($schedule);
		if ($allowed) {
			if (strcasecmp($schedule, 'none') !== 0) {
				openvpn_schedule_log_runtime("OpenVPN login allowed for user '{$username}' inside schedule '{$schedule}'");
			}
			return;
		}

		openvpn_schedule_log_runtime("OpenVPN login denied for user '{$username}' outside allowed schedule '{$schedule}'");
		exit(1);
	}
}

if (!defined('OVPN_SCHEDULE_DISABLE_AUTO_ENFORCE')) {
	$script_name = basename((string)($_SERVER['SCRIPT_FILENAME'] ?? ''));
	if ($script_name === 'openvpn.auth-user.php') {
		openvpn_schedule_enforce_or_exit();
	}
}
