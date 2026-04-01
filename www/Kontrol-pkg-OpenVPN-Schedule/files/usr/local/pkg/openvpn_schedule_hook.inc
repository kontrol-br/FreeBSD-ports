<?php
/*
 * openvpn_schedule_hook.inc
 * Loaded only from OpenVPN authentication flow.
 */

require_once('auth.inc');
require_once('config.inc');
require_once('util.inc');
if (file_exists('/etc/inc/filter.inc')) {
	require_once('/etc/inc/filter.inc');
} else {
	include_once('filter.inc');
}

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

	function openvpn_schedule_eval_manual_timerange($timerange, DateTimeImmutable $now_local) {
		if (!is_array($timerange)) {
			return false;
		}

		$now_month = (int)$now_local->format('n');
		$now_day = (int)$now_local->format('j');
		$now_weekday = (int)$now_local->format('N'); // 1..7 (Mon..Sun)
		$now_minutes = ((int)$now_local->format('G') * 60) + (int)$now_local->format('i');
		$now_hm = sprintf('%02d:%02d', (int)$now_local->format('G'), (int)$now_local->format('i'));

		$month_raw = trim((string)($timerange['month'] ?? ''));
		$day_raw = trim((string)($timerange['day'] ?? ''));
		$position_raw = trim((string)($timerange['position'] ?? ''));
		$hour_raw = trim((string)($timerange['hour'] ?? ''));
		if ($hour_raw === '') {
			$from = trim((string)($timerange['timerange_from'] ?? ($timerange['from'] ?? '')));
			$to = trim((string)($timerange['timerange_to'] ?? ($timerange['to'] ?? '')));
			if ($from !== '' && $to !== '') {
				$hour_raw = $from . '-' . $to;
			}
		}

		$match_month = true;
		if ($month_raw !== '') {
			$match_month = in_array($now_month, array_map('intval', explode(',', $month_raw)), true);
		}

		$match_day = true;
		if ($day_raw !== '') {
			$match_day = in_array($now_day, array_map('intval', explode(',', $day_raw)), true);
		}

		$match_position = true;
		if ($position_raw !== '') {
			$match_position = in_array($now_weekday, array_map('intval', explode(',', $position_raw)), true);
		}

		$match_hour = true;
		if ($hour_raw !== '') {
			$parts = explode('-', $hour_raw, 2);
			$start = isset($parts[0]) ? trim((string)$parts[0]) : '';
			$end = isset($parts[1]) ? trim((string)$parts[1]) : '';
			$match_hour = false;
			if ($start !== '' && $end !== '') {
				$start_chunks = explode(':', $start, 2);
				$end_chunks = explode(':', $end, 2);
				if (isset($start_chunks[0], $start_chunks[1], $end_chunks[0], $end_chunks[1])) {
					$start_minutes = ((int)$start_chunks[0] * 60) + (int)$start_chunks[1];
					$end_minutes = ((int)$end_chunks[0] * 60) + (int)$end_chunks[1];
					if ($end_minutes === $start_minutes) {
						$match_hour = true;
					} elseif ($end_minutes > $start_minutes) {
						$match_hour = ($now_minutes >= $start_minutes && $now_minutes < $end_minutes);
					} else {
						$match_hour = ($now_minutes >= $start_minutes || $now_minutes < $end_minutes);
					}
				}
			}
		}

		openvpn_schedule_log_debug(
			'manual evaluator compare: now(local=' . $now_local->format('c') . ', month=' . $now_month . ', day=' . $now_day .
			', weekday=' . $now_weekday . ', hour=' . $now_hm . ') vs schedule(month=' . ($month_raw === '' ? '*' : $month_raw) .
			', day=' . ($day_raw === '' ? '*' : $day_raw) . ', position=' . ($position_raw === '' ? '*' : $position_raw) .
			', hour=' . ($hour_raw === '' ? '*' : $hour_raw) . ') => month=' . ($match_month ? 'match' : 'no') .
			', day=' . ($match_day ? 'match' : 'no') . ', position=' . ($match_position ? 'match' : 'no') .
			', hour=' . ($match_hour ? 'match' : 'no')
		);

		return $match_month && $match_day && $match_position && $match_hour;
	}

	function openvpn_schedule_eval_with_manual_fallback($schedule_name, array $schedule_cfg) {
		$timeranges = $schedule_cfg['timerange'] ?? [];
		if (isset($timeranges['from']) || isset($timeranges['timerange_from'])) {
			$timeranges = [$timeranges];
		}
		if (!is_array($timeranges) || empty($timeranges)) {
			openvpn_schedule_log_debug("manual evaluator: schedule '{$schedule_name}' has no timeranges");
			return null;
		}

		$now_local = new DateTimeImmutable('now');
		openvpn_schedule_log_debug("manual evaluator: schedule '{$schedule_name}' raw timeranges=" . json_encode($timeranges));
		foreach ($timeranges as $idx => $timerange) {
			$in_range = openvpn_schedule_eval_manual_timerange($timerange, $now_local);
			openvpn_schedule_log_debug("manual evaluator: schedule '{$schedule_name}' range #" . ($idx + 1) . ' => ' . ($in_range ? 'match' : 'no-match'));
			if ($in_range) {
				return true;
			}
		}

		return false;
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

		openvpn_schedule_log_debug("checking schedule '{$schedule_name}' at local " . date('c') . ' (tz=' . date_default_timezone_get() . ', utc=' . gmdate('c') . ')');

		$core_match = openvpn_schedule_eval_with_core_engine($schedule_name, $schedule_cfg);
		if ($core_match !== null) {
			return (bool)$core_match;
		}

		$manual_match = openvpn_schedule_eval_with_manual_fallback($schedule_name, $schedule_cfg);
		if ($manual_match !== null) {
			openvpn_schedule_log_debug("schedule '{$schedule_name}' evaluated by manual fallback => " . ($manual_match ? 'allow' : 'deny'));
			return (bool)$manual_match;
		}

		$known = [
			'filter_schedule_in_effect' => function_exists('filter_schedule_in_effect') ? 'yes' : 'no',
			'is_schedule_in_time' => function_exists('is_schedule_in_time') ? 'yes' : 'no',
		];
		openvpn_schedule_log_runtime("schedule engine not available for '{$schedule_name}', denying login (functions: " . json_encode($known) . ')');
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

	function openvpn_schedule_hook_field_text($row, $field, $default = '') {
		if (!is_array($row)) {
			return (string)$default;
		}
		return openvpn_schedule_hook_text_value($row[$field] ?? $default, $default);
	}

	function openvpn_schedule_find_policy($username) {
		$pkg = config_get_path(openvpn_schedule_hook_package_base_path() . '/config/0', []);

		foreach (openvpn_schedule_get_entries($pkg) as $entry) {
			$entry_username = openvpn_schedule_hook_field_text($entry, 'username', '');
			if ($entry_username !== '' && strcasecmp($entry_username, $username) === 0) {
				return openvpn_schedule_hook_field_text($entry, 'schedule', 'none');
			}
		}

		// Backward compatibility with the previous user/group rule format.
		foreach (($pkg['rule'] ?? []) as $rule) {
			$rule_type = openvpn_schedule_hook_field_text($rule, 'type', '');
			$rule_name = openvpn_schedule_hook_field_text($rule, 'name', '');
			if (strcasecmp($rule_type, 'user') === 0 && $rule_name !== '' && strcasecmp($rule_name, $username) === 0) {
				return openvpn_schedule_hook_field_text($rule, 'schedule', 'none');
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
