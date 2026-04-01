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

if (!function_exists('openvpn_schedule_hook_debug_enabled')) {
	function openvpn_schedule_hook_debug_enabled() {
		$pkg = config_get_path(openvpn_schedule_hook_package_base_path() . '/config/0', []);
		$debug_flag = trim((string)($pkg['debug_auth'] ?? ''));
		return in_array(strtolower($debug_flag), ['1', 'yes', 'on', 'true'], true);
	}

	function openvpn_schedule_log_debug($message) {
		if (openvpn_schedule_hook_debug_enabled()) {
			openvpn_schedule_log_runtime('[debug] ' . $message);
		}
	}
}

if (!function_exists('openvpn_schedule_now_matches')) {
	function openvpn_schedule_hook_eval_to_bool($value) {
		if (is_bool($value)) {
			return $value;
		}

		if (is_int($value) || is_float($value)) {
			if ((int)$value === 0) {
				return false;
			}
			if ((int)$value === 1) {
				return true;
			}
		}

		if (is_string($value)) {
			$normalized = strtolower(trim($value));
			if (in_array($normalized, ['0', 'no', 'off', 'false'], true)) {
				return false;
			}
			if (in_array($normalized, ['1', 'yes', 'on', 'true'], true)) {
				return true;
			}
		}

		return null;
	}

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
					$raw = filter_schedule_in_effect($arg);
					$normalized = openvpn_schedule_hook_eval_to_bool($raw);
					if ($normalized !== null) {
						openvpn_schedule_log_debug("schedule '{$schedule_name}' evaluated by filter_schedule_in_effect(" . gettype($arg) . ') => ' . ($normalized ? 'allow' : 'deny'));
						return $normalized;
					}
					openvpn_schedule_log_debug("schedule '{$schedule_name}' returned non-boolean from filter_schedule_in_effect(" . gettype($arg) . '), trying fallback signatures');
				} catch (Throwable $e) {
					/* Ignore and continue trying compatibility signatures. */
					openvpn_schedule_log_debug("filter_schedule_in_effect(" . gettype($arg) . ') failed for schedule ' . "'{$schedule_name}': " . $e->getMessage());
				}
			}
		}

		if (function_exists('is_schedule_in_time')) {
			foreach ([$schedule_cfg, $schedule_name] as $arg) {
				try {
					$raw = is_schedule_in_time($arg);
					$normalized = openvpn_schedule_hook_eval_to_bool($raw);
					if ($normalized !== null) {
						openvpn_schedule_log_debug("schedule '{$schedule_name}' evaluated by is_schedule_in_time(" . gettype($arg) . ') => ' . ($normalized ? 'allow' : 'deny'));
						return $normalized;
					}
					openvpn_schedule_log_debug("schedule '{$schedule_name}' returned non-boolean from is_schedule_in_time(" . gettype($arg) . '), trying fallback signatures');
				} catch (Throwable $e) {
					/* Ignore and continue trying compatibility signatures. */
					openvpn_schedule_log_debug("is_schedule_in_time(" . gettype($arg) . ') failed for schedule ' . "'{$schedule_name}': " . $e->getMessage());
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

		// Fail closed when a schedule is configured but schedule engine is unavailable.
		openvpn_schedule_log_runtime("schedule engine not available for '{$schedule_name}', denying login");
		return false;
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

	function openvpn_schedule_hook_text_value($value, $default = '') {
		while (is_array($value)) {
			$value = reset($value);
			if ($value === false) {
				$value = $default;
				break;
			}
		}
		$text = trim((string)$value);
		return $text === '' ? (string)$default : $text;
	}

	function openvpn_schedule_find_policy($username) {
		$pkg = config_get_path(openvpn_schedule_hook_package_base_path() . '/config/0', []);

		foreach (openvpn_schedule_get_entries($pkg) as $entry) {
			if (strcasecmp((string)($entry['username'] ?? ''), $username) === 0) {
				return openvpn_schedule_hook_text_value($entry['schedule'] ?? 'none', 'none');
			}
		}

		// Backward compatibility with the previous user/group rule format.
		foreach (($pkg['rule'] ?? []) as $rule) {
			if (($rule['type'] ?? '') === 'user' && strcasecmp((string)($rule['name'] ?? ''), $username) === 0) {
				return openvpn_schedule_hook_text_value($rule['schedule'] ?? 'none', 'none');
			}
		}

		return 'none';
	}
}

if (!function_exists('openvpn_schedule_enforce_or_exit')) {
	function openvpn_schedule_decode_username_candidate($value) {
		$username = trim((string)$value);
		if ($username === '') {
			return '';
		}

		/*
		 * Some auth flows can pass username in base64 (e.g. "am9hbw==" for "joao").
		 * Decode only when the input clearly looks like base64 and the decoded value
		 * is a non-empty printable token.
		 */
		if (preg_match('/^[A-Za-z0-9+\/]+={0,2}$/', $username) === 1 && (strlen($username) % 4) === 0) {
			$decoded = base64_decode($username, true);
			if ($decoded !== false) {
				$decoded = trim((string)$decoded);
				if ($decoded !== '' && preg_match('/^[[:print:]]+$/', $decoded) === 1) {
					openvpn_schedule_log_debug("decoded base64 username '{$username}' => '{$decoded}'");
					return $decoded;
				}
			}
		}

		return $username;
	}

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
			$username = openvpn_schedule_decode_username_candidate($lines[0] ?? '');
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
			$GLOBALS['authcfg']['username'] ?? '',
			$GLOBALS['authcfg']['common_name'] ?? '',
			$GLOBALS['upauth']['username'] ?? '',
			$GLOBALS['vpnuser'] ?? '',
			$GLOBALS['username'] ?? '',
		];

		foreach ($candidates as $candidate) {
			$username = openvpn_schedule_decode_username_candidate($candidate);
			if ($username !== '') {
				return $username;
			}
		}

		return openvpn_schedule_extract_username_from_argv_file();
	}

	function openvpn_schedule_enforce_or_exit() {
		$username = openvpn_schedule_extract_username();
		if (empty($username)) {
			openvpn_schedule_log_runtime('username is empty at auth hook, denying login to avoid schedule bypass');
			exit(1);
		}

		$schedule = openvpn_schedule_find_policy($username);
		$allowed = openvpn_schedule_now_matches($schedule);
		openvpn_schedule_log_debug("auth intercept user='{$username}' schedule='{$schedule}' decision=" . ($allowed ? 'allow' : 'deny'));
		if ($allowed) {
			if (strcasecmp($schedule, 'none') !== 0) {
				openvpn_schedule_log_runtime("OpenVPN login allowed for user '{$username}' inside schedule '{$schedule}'");
			}
			return;
		}

		openvpn_schedule_log_runtime("OpenVPN login denied for user '{$username}' outside allowed schedule '{$schedule}' (reason: schedule not active at authentication time)");
		exit(1);
	}
}

if (!defined('OVPN_SCHEDULE_DISABLE_AUTO_ENFORCE')) {
	$script_name = basename((string)($_SERVER['SCRIPT_FILENAME'] ?? ''));
	if ($script_name === 'openvpn.auth-user.php') {
		openvpn_schedule_enforce_or_exit();
	}
}
