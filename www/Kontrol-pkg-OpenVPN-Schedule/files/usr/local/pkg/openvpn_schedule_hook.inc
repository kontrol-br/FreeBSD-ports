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
		if (empty($schedule_name)) {
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

if (!function_exists('openvpn_schedule_user_groups')) {
	function openvpn_schedule_user_groups($username) {
		$groups = [];
		foreach (config_get_path('system/group', []) as $group) {
			foreach (($group['member'] ?? []) as $member) {
				if ($member === $username) {
					$groups[] = $group['name'];
				}
			}
		}
		return $groups;
	}
}

if (!function_exists('openvpn_schedule_find_policy')) {
	function openvpn_schedule_find_policy($username) {
		$pkg = config_get_path('installedpackages/openvpn_schedule/config/0', []);
		$rules = $pkg['rule'] ?? [];
		$default = $pkg['default_policy'] ?? 'allow';

		foreach ($rules as $rule) {
			if (($rule['type'] ?? '') === 'user' && ($rule['name'] ?? '') === $username) {
				return ['match' => 'user', 'schedule' => ($rule['schedule'] ?? ''), 'default' => $default];
			}
		}

		$user_groups = openvpn_schedule_user_groups($username);
		foreach ($rules as $rule) {
			if (($rule['type'] ?? '') === 'group' && in_array(($rule['name'] ?? ''), $user_groups, true)) {
				return ['match' => 'group', 'schedule' => ($rule['schedule'] ?? ''), 'default' => $default];
			}
		}

		return ['match' => 'default', 'schedule' => '', 'default' => $default];
	}
}

if (!function_exists('openvpn_schedule_enforce_or_exit')) {
	function openvpn_schedule_enforce_or_exit() {
		$username = $_POST['username'] ?? $_ENV['username'] ?? '';
		if (empty($username)) {
			return;
		}

		$policy = openvpn_schedule_find_policy($username);
		if ($policy['match'] === 'default') {
			if ($policy['default'] === 'deny') {
				openvpn_schedule_log_runtime("OpenVPN login denied for user '{$username}' by default deny policy");
				exit(1);
			}
			return;
		}

		if (openvpn_schedule_now_matches($policy['schedule'])) {
			return;
		}

		openvpn_schedule_log_runtime("OpenVPN login denied for user '{$username}' outside allowed schedule '{$policy['schedule']}' via {$policy['match']} rule");
		exit(1);
	}
}
