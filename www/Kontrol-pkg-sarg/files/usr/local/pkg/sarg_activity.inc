<?php
/*
 * Estimated active-time analytics for Kontrol Sarg reports.
 *
 * A session remains active while requests are no more than the configured
 * inactivity timeout apart. One minute is added after the last request so a
 * session containing a single request is still represented.
 */

function sarg_activity_parse_date($date, $time) {
	$value = DateTime::createFromFormat('!Y.m.d H:i:s', $date . ' ' . $time);
	return ($value instanceof DateTime) ? $value->getTimestamp() : false;
}

function sarg_activity_identity($user, $ip) {
	$user = trim((string)$user);
	$ip = trim((string)$ip);
	$anonymous = array('', '-', 'unknown', 'unknow', 'anonymous');
	if (in_array(strtolower($user), $anonymous, true)) {
		$user = $ip;
	}
	if ($user === '' || preg_match('/[\x00-\x1f\x7f]/', $user)) {
		return false;
	}
	return $user;
}

function sarg_activity_parse_e2guardian($line) {
	$line = trim($line);
	if ($line === '') {
		return false;
	}

	if (strpos($line, "\t") !== false || $line[0] === '"') {
		$fields = ($line[0] === '"') ? str_getcsv($line) : explode("\t", $line);
		if (count($fields) < 3 || !preg_match('/^(\d{4}\.\d{2}\.\d{2})\s+(\d{2}:\d{2}:\d{2})/', trim($fields[0]), $match)) {
			return false;
		}
		$timestamp = sarg_activity_parse_date($match[1], $match[2]);
		$identity = sarg_activity_identity($fields[1], $fields[2]);
		return ($timestamp !== false && $identity !== false) ? array($timestamp, $identity) : false;
	}

	$fields = preg_split('/\s+/', $line);
	if (count($fields) < 5 || !preg_match('/^\d{4}\.\d{2}\.\d{2}$/', $fields[0])) {
		return false;
	}
	$base = (isset($fields[2]) && preg_match('/^\d+\.\d+$/', $fields[2])) ? 1 : 0;
	if (!isset($fields[3 + $base])) {
		return false;
	}
	$timestamp = sarg_activity_parse_date($fields[0], $fields[1]);
	$identity = sarg_activity_identity($fields[2 + $base], $fields[3 + $base]);
	return ($timestamp !== false && $identity !== false) ? array($timestamp, $identity) : false;
}

function sarg_activity_parse_squid($line) {
	$fields = preg_split('/\s+/', trim($line), 10);
	if (count($fields) < 8 || !is_numeric($fields[0])) {
		return false;
	}
	$timestamp = (int)floor((float)$fields[0]);
	$identity = sarg_activity_identity($fields[7], $fields[2]);
	return ($timestamp > 0 && $identity !== false) ? array($timestamp, $identity) : false;
}

function sarg_activity_period_from_dir($directory) {
	$name = basename($directory);
	if (preg_match('/^(\d{4}[A-Za-z]{3}\d{2})-(\d{4}[A-Za-z]{3}\d{2})$/', $name, $match)) {
		$format = '!YMd';
	} elseif (preg_match('/^(\d{2}[A-Za-z]{3}\d{4})-(\d{2}[A-Za-z]{3}\d{4})$/', $name, $match)) {
		$format = '!dMY';
	} else {
		return false;
	}
	$start = DateTime::createFromFormat($format, $match[1]);
	$end = DateTime::createFromFormat($format, $match[2]);
	if (!($start instanceof DateTime) || !($end instanceof DateTime)) {
		return false;
	}
	return array(
		'directory' => $directory,
		'label' => $name,
		'start' => $start->getTimestamp(),
		'end' => $end->modify('+1 day')->getTimestamp()
	);
}

function sarg_activity_add_interval(&$user, $start, $end) {
	if ($end <= $start) {
		return;
	}
	$user['active_seconds'] += $end - $start;
	$cursor = $start;
	while ($cursor < $end) {
		$next = strtotime(date('Y-m-d H:00:00', $cursor) . ' +1 hour');
		if ($next === false || $next <= $cursor) {
			$next = $cursor + 3600;
		}
		$slice_end = min($end, $next);
		$user['hours'][(int)date('G', $cursor)] += $slice_end - $cursor;
		$cursor = $slice_end;
	}
}

function sarg_activity_close_session(&$user, $period_end) {
	if (!isset($user['_session_start'])) {
		return;
	}
	$end = min($period_end, $user['_session_last'] + 60);
	sarg_activity_add_interval($user, $user['_session_start'], $end);
	$user['sessions']++;
	unset($user['_session_start'], $user['_session_last']);
}

function sarg_activity_add_event(&$period, $timestamp, $identity, $timeout_seconds) {
	if ($timestamp < $period['start'] || $timestamp >= $period['end']) {
		return;
	}
	if (!isset($period['users'][$identity])) {
		$period['users'][$identity] = array(
			'identity' => $identity,
			'active_seconds' => 0,
			'sessions' => 0,
			'first' => $timestamp,
			'last' => $timestamp,
			'hours' => array_fill(0, 24, 0),
			'_session_start' => $timestamp,
			'_session_last' => $timestamp
		);
		return;
	}

	$user =& $period['users'][$identity];
	if ($timestamp < $user['_session_last']) {
		return;
	}
	if ($timestamp - $user['_session_last'] > $timeout_seconds) {
		sarg_activity_close_session($user, $period['end']);
		$user['_session_start'] = $timestamp;
	}
	$user['_session_last'] = $timestamp;
	$user['first'] = min($user['first'], $timestamp);
	$user['last'] = max($user['last'], $timestamp);
}

function sarg_activity_log_files($pattern) {
	$files = glob($pattern);
	if ($files === false) {
		$files = array();
	}
	$files = array_values(array_filter($files, 'is_file'));
	usort($files, function ($left, $right) {
		$left_time = @filemtime($left);
		$right_time = @filemtime($right);
		if ($left_time === $right_time) {
			return strcmp($left, $right);
		}
		return ($left_time < $right_time) ? -1 : 1;
	});
	return $files;
}

function sarg_activity_read_log($file, $callback) {
	$compressed = (substr($file, -3) === '.gz');
	$handle = $compressed ? @gzopen($file, 'rb') : @fopen($file, 'rb');
	if ($handle === false) {
		return;
	}
	while (($line = $compressed ? gzgets($handle) : fgets($handle)) !== false) {
		$callback($line);
	}
	$compressed ? gzclose($handle) : fclose($handle);
}

function sarg_generate_activity_reports($log_pattern, $output_dir, $proxy, $timeout_minutes = 15, $generated_after = 0) {
	$timeout_minutes = max(1, min(120, (int)$timeout_minutes));
	$periods = array();
	$directories = glob(rtrim($output_dir, '/') . '/*', GLOB_ONLYDIR);
	if ($directories === false) {
		return 0;
	}
	foreach ($directories as $directory) {
		$period = sarg_activity_period_from_dir($directory);
		if ($period === false) {
			continue;
		}
		$index = is_file($directory . '/index.html') ? $directory . '/index.html' : $directory . '/index.html.gz';
		if (!is_file($index) || ($generated_after > 0 && filemtime($index) < $generated_after)) {
			continue;
		}
		$period['users'] = array();
		$periods[] = $period;
	}
	if (empty($periods)) {
		return 0;
	}

	$parser = ($proxy === 'squid') ? 'sarg_activity_parse_squid' : 'sarg_activity_parse_e2guardian';
	$timeout_seconds = $timeout_minutes * 60;
	foreach (sarg_activity_log_files($log_pattern) as $file) {
		sarg_activity_read_log($file, function ($line) use (&$periods, $parser, $timeout_seconds) {
			$entry = call_user_func($parser, $line);
			if ($entry === false) {
				return;
			}
			foreach ($periods as &$period) {
				sarg_activity_add_event($period, $entry[0], $entry[1], $timeout_seconds);
			}
			unset($period);
		});
	}

	$written = 0;
	foreach ($periods as &$period) {
		foreach ($period['users'] as &$user) {
			sarg_activity_close_session($user, $period['end']);
		}
		unset($user);
		uasort($period['users'], function ($left, $right) {
			if ($left['active_seconds'] === $right['active_seconds']) {
				return strcmp($left['identity'], $right['identity']);
			}
			return ($left['active_seconds'] > $right['active_seconds']) ? -1 : 1;
		});
		$top = array_slice(array_values($period['users']), 0, 10);
		$summary = array('identities' => count($period['users']), 'active_seconds' => 0, 'sessions' => 0);
		foreach ($period['users'] as $user) {
			$summary['active_seconds'] += $user['active_seconds'];
			$summary['sessions'] += $user['sessions'];
		}
		$document = array(
			'version' => 1,
			'method' => 'estimated-session-activity',
			'timeout_minutes' => $timeout_minutes,
			'generated_at' => date(DATE_ATOM),
			'period' => array('label' => $period['label'], 'start' => $period['start'], 'end' => $period['end'] - 1),
			'summary' => $summary,
			'top' => $top,
			'users' => $period['users']
		);
		$json = json_encode($document, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
		if ($json === false) {
			continue;
		}
		$target = $period['directory'] . '/.sarg-activity.json';
		$temporary = $target . '.tmp.' . getmypid();
		if (file_put_contents($temporary, $json, LOCK_EX) !== false && @rename($temporary, $target)) {
			@chmod($target, 0644);
			$written++;
		} else {
			@unlink($temporary);
		}
	}
	unset($period);
	return $written;
}
