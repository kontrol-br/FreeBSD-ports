#!/usr/local/bin/php
<?php

function fail($message) {
	fwrite(STDERR, "FAIL: {$message}\n");
	exit(1);
}

function assert_same($expected, $actual, $message) {
	if ($expected !== $actual) {
		fail($message . ': expected ' . var_export($expected, true) .
			', got ' . var_export($actual, true));
	}
}

$config_fixture = array();
function config_get_path($path, $default = null) {
	global $config_fixture;
	return array_key_exists($path, $config_fixture) ? $config_fixture[$path] : $default;
}

$source = file_get_contents(__DIR__ . '/../files/usr/local/pkg/sarg.inc');
$start = strpos($source, 'function sarg_native_gateway_enabled()');
$end = strpos($source, '// STATIC VARS');
if ($start === false || $end === false || $end <= $start) {
	fail('could not load log source helpers');
}
eval(substr($source, $start, $end - $start));

$temporary = sys_get_temp_dir() . '/sarg-log-source-' . getmypid();
if (!mkdir($temporary, 0700)) {
	fail('could not create temporary directory');
}

try {
	$empty = $temporary . '/empty.log';
	$records = $temporary . '/access.log.1';
	file_put_contents($empty, '');
	file_put_contents($records, "record\n");

	assert_same(array(), sarg_readable_log_files(''), 'empty pattern');
	assert_same(array(), sarg_readable_log_files($temporary . '/missing.log'), 'missing log');
	assert_same(array(), sarg_readable_log_files($empty), 'empty log');
	assert_same(array($records), sarg_readable_log_files($records), 'exact log');
	assert_same(array($records), sarg_readable_log_files($temporary . '/*.log*'), 'rotated wildcard');

	$config_fixture = array(
		'installedpackages/kontrolid/config/0' => array('enable' => '')
	);
	assert_same(false, sarg_native_gateway_enabled(), 'disabled Kontrol-ID');
	$config_fixture['installedpackages/kontrolid/config/0']['enable'] = 'on';
	assert_same(true, sarg_native_gateway_enabled(), 'enabled Kontrol-ID');

	$config_fixture = array(
		'installedpackages/e2guardian/config/0' => array('enable_dg' => 'on')
	);
	assert_same(true, sarg_native_gateway_enabled(), 'legacy E2Guardian');
	$config_fixture = array();
	assert_same(null, sarg_native_gateway_enabled(), 'unknown gateway state');
} finally {
	@unlink($temporary . '/empty.log');
	@unlink($temporary . '/access.log.1');
	@rmdir($temporary);
}

echo "PASS: Sarg log source smoke tests\n";
