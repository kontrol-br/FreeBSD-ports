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
	function openvpn_schedule_now_matches($schedule_name) {
		$schedule_name = trim((string)$schedule_name);
		if ($schedule_name === '' || strcasecmp($schedule_name, 'none') === 0) {
			return true;
		}

		// Reuse core schedule engine when available.
		if (function_exists('filter_schedule_in_effect')) {
			return filter_schedule_in_effect($schedule_name);
		}
		if (function_exists('is_schedule_in_time')) {
			return is_schedule_in_time($schedule_name);
		}

		// Safe fallback: if schedule engine is unavailable, do not block logins.
		openvpn_schedule_log_runtime("schedule engine not available for '{$schedule_name}', fallback allow");
		return true;
	}
}

if (!function_exists('openvpn_schedule_find_policy')) {
	function openvpn_schedule_get_entries($pkg) {
		$user_schedule = $pkg['user_schedule'] ?? [];
		if (is_array($user_schedule) && isset($user_schedule['item']) && is_array($user_schedule['item'])) {
			return $user_schedule['item'];
		}
		return is_array($user_schedule) ? $user_schedule : [];
	}

	function openvpn_schedule_find_policy($username) {
		$pkg = config_get_path('installedpackages/openvpn_schedule/config/0', []);

		foreach (openvpn_schedule_get_entries($pkg) as $entry) {
			if (($entry['username'] ?? '') === $username) {
				return trim($entry['schedule'] ?? 'none');
			}
		}

		// Backward compatibility with the previous user/group rule format.
		foreach (($pkg['rule'] ?? []) as $rule) {
			if (($rule['type'] ?? '') === 'user' && ($rule['name'] ?? '') === $username) {
				return trim($rule['schedule'] ?? 'none');
			}
		}

		return 'none';
	}
}

if (!function_exists('openvpn_schedule_enforce_or_exit')) {
	function openvpn_schedule_enforce_or_exit() {
		$username = $_POST['username'] ?? $_ENV['username'] ?? '';
		if (empty($username)) {
			return;
		}

		$schedule = openvpn_schedule_find_policy($username);
		if (openvpn_schedule_now_matches($schedule)) {
			return;
		}

		openvpn_schedule_log_runtime("OpenVPN login denied for user '{$username}' outside allowed schedule '{$schedule}'");
		exit(1);
	}
}
