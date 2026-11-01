#!/usr/local/bin/php -q
<?php

/* Native HTTP Basic authentication helper for Kontrol-ID.
 * The engine sends URL-escaped "username password" records on standard input.
 * Passwords are validated by the selected Kontrol authentication server and
 * are never written to disk or logs by this helper.
 *
 * Successful credentials are cached briefly as SHA-256 digests.  This avoids
 * repeating the expensive local password hash or LDAP bind for every object
 * fetched by a browser, while keeping password and group revocation bounded.
 * Failed credentials are deliberately never cached. */

error_reporting(0);
ini_set('display_errors', '0');

require_once('/etc/inc/config.inc');
require_once('/etc/inc/auth.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

stream_set_blocking(STDIN, true);
$positive_cache = array();
$positive_cache_ttl = 60;
$positive_cache_limit = 256;
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
	$now = time();
	$cache_key = hash('sha256', $username . "\0" . $password);
	$authenticated = isset($positive_cache[$cache_key]) &&
		$positive_cache[$cache_key] >= $now;
	if (!$authenticated) {
		unset($positive_cache[$cache_key]);
		$authenticated = kontrol_id_authenticate_basic($username, $password);
		if ($authenticated) {
			if (count($positive_cache) >= $positive_cache_limit) {
				foreach ($positive_cache as $key => $expires_at) {
					if ($expires_at < $now) {
						unset($positive_cache[$key]);
					}
				}
				if (count($positive_cache) >= $positive_cache_limit) {
					array_shift($positive_cache);
				}
			}
			$positive_cache[$cache_key] = $now + $positive_cache_ttl;
		}
	}
	if ($authenticated) {
		echo 'OK user=' . rawurlencode($username) . "\n";
	} else {
		echo "ERR message=Authentication%20failed\n";
	}
	flush();
}

?>
