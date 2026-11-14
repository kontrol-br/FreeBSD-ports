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

$source = file_get_contents(__DIR__ . '/../files/usr/local/pkg/kontrol_id.inc');
$start = strpos($source, 'function kontrol_id_secure_webgui_port(');
$end = strpos($source, 'function kontrol_id_prepare_secure_provisioning(', $start);
if ($start === false || $end === false || $end <= $start) {
	fail('could not load the secure WebGUI port helper');
}
eval(substr($source, $start, $end - $start));

assert_same('443', kontrol_id_secure_webgui_port('http', '80'), 'default HTTP port');
assert_same('443', kontrol_id_secure_webgui_port('http', ''), 'implicit HTTP port');
assert_same('8080', kontrol_id_secure_webgui_port('http', '8080'), 'custom HTTP port');
assert_same('443', kontrol_id_secure_webgui_port('https', '443'), 'default HTTPS port');
assert_same('80', kontrol_id_secure_webgui_port('https', '80'), 'existing HTTPS port');
assert_same('8443', kontrol_id_secure_webgui_port('https', 8443), 'custom HTTPS port');

echo "PASS: secure provisioning WebGUI port smoke tests\n";
