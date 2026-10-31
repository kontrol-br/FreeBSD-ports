#!/usr/local/bin/php -q
<?php

/* Squid Basic authentication helper for Kontrol-ID.
 * Squid sends URL-escaped "username password" records on standard input.
 * Passwords are validated by the selected Kontrol authentication server and
 * are never written to disk or logs by this helper. */

error_reporting(0);
ini_set('display_errors', '0');

require_once('/etc/inc/config.inc');
require_once('/etc/inc/auth.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

stream_set_blocking(STDIN, true);
while (($line = fgets(STDIN)) !== false) {
	$line = rtrim($line, "\r\n");
	$parts = preg_split('/\s+/', $line, 2);
	if (count($parts) !== 2) {
		echo "ERR message=Invalid%20credentials\n";
		flush();
		continue;
	}

	$username = rawurldecode($parts[0]);
	$password = rawurldecode($parts[1]);
	if (kontrol_id_authenticate_basic($username, $password)) {
		echo 'OK user=' . rawurlencode($username) . "\n";
	} else {
		echo "ERR message=Authentication%20failed\n";
	}
	flush();
}

?>
