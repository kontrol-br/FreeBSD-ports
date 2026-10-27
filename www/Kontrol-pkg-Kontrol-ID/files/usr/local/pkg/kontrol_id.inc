<?php

require_once('config.inc');
require_once('functions.inc');
require_once('util.inc');
require_once('auth.inc');
require_once('certs.inc');
require_once('xmlrpc_client.inc');

define('KONTROL_ID_CONF', '/usr/local/etc/kontrol-identity.conf');
define('KONTROL_ID_DB_DIR', '/var/db/kontrol-identity');
define('KONTROL_ID_DB', KONTROL_ID_DB_DIR . '/identity.db');
define('KONTROL_ID_PKI_DIR', '/var/etc/kontrol-id');
define('KONTROL_ID_CA_FILE', KONTROL_ID_PKI_DIR . '/ca.pem');
define('KONTROL_ID_CLIENT_CERT', KONTROL_ID_PKI_DIR . '/client.crt');
define('KONTROL_ID_CLIENT_KEY', KONTROL_ID_PKI_DIR . '/client.key');
define('KONTROL_ID_GROUP', 'kontrol-internet-users');
define('KONTROL_ID_BEGIN', '# BEGIN KONTROL-ID');
define('KONTROL_ID_END', '# END KONTROL-ID');

function kontrol_id_defaults() {
	return array(
		'enable' => '',
		'squid_integration' => 'on',
		'e2guardian_integration' => 'on',
		'winrm_port' => '5986',
		'connect_timeout_ms' => '1200',
		'request_timeout_ms' => '2500',
		'positive_ttl' => '300',
		'negative_ttl' => '30',
		'stale_ttl' => '900',
		'portal_ttl' => '28800',
		'unknown_user' => 'unknown',
		'unknown_action' => 'portal',
		'portal_url' => '',
		'admin_contact' => 'Entre em contato com o administrador da rede para solicitar sua credencial.',
		'auth_server' => 'Local Database',
		'local_group' => KONTROL_ID_GROUP,
		'ad_groups' => 'Domain Users',
		'verify_peer' => 'on',
		'verify_host' => 'on',
		'debug' => '',
		'enrollment_token_hash' => '',
		'enrollment_token_expires' => '0',
		'enrollment_token_uses' => '0',
		'ca_ref' => '',
		'client_cert_ref' => '',
		'pki_owned' => '',
		'e2g_proxy_basic_added' => '',
		'previous_squid_auth_method' => '',
		'dependency_defaults_initialized' => ''
	);
}

function kontrol_id_authserver_options() {
	$options = array(array('name' => 'Local Database', 'value' => 'Local Database'));
	foreach (config_get_path('system/authserver', array()) as $server) {
		if (!is_array($server) || ($server['type'] ?? '') !== 'ldap' || empty($server['name'])) {
			continue;
		}
		$options[] = array(
			'name' => $server['name'] . ' (LDAP/Active Directory)',
			'value' => $server['name']
		);
	}
	return $options;
}

function kontrol_id_get_settings() {
	$settings = config_get_path('installedpackages/kontrolid/config/0', array());
	if (!is_array($settings)) {
		$settings = array();
	}
	return array_merge(kontrol_id_defaults(), $settings);
}

function kontrol_id_set_settings($settings) {
	config_set_path('installedpackages/kontrolid/config/0', $settings);
}

function kontrol_id_extract_item($lookup) {
	if (is_array($lookup) && isset($lookup['item'])) {
		return $lookup['item'];
	}
	return is_array($lookup) ? $lookup : array();
}

function kontrol_id_ensure_defaults() {
	$current = config_get_path('installedpackages/kontrolid/config/0', array());
	if (!is_array($current)) {
		$current = array();
	}
	$merged = array_merge(kontrol_id_defaults(), $current);
	if ($merged != $current) {
		kontrol_id_set_settings($merged);
		return true;
	}
	return false;
}

function kontrol_id_prepare_dependency_defaults() {
	$settings = kontrol_id_get_settings();
	if (($settings['dependency_defaults_initialized'] ?? '') === 'on') {
		return false;
	}

	$changed = false;
	$squid = config_get_path('installedpackages/squid/config/0', array());
	if (!is_array($squid)) {
		$squid = array();
	}
	$squid_defaults = array(
		'enable_squid' => '',
		'active_interface' => 'lan',
		'proxy_port' => '3128',
		'transparent_proxy' => 'on',
		'transparent_active_interface' => 'lan',
		'allow_interface' => 'on'
	);
	foreach ($squid_defaults as $key => $value) {
		if (!array_key_exists($key, $squid)) {
			$squid[$key] = $value;
			$changed = true;
		}
	}
	config_set_path('installedpackages/squid/config/0', $squid);

	$cache = config_get_path('installedpackages/squidcache/config/0', array());
	if (!is_array($cache)) {
		$cache = array();
	}
	$cache_defaults = array(
		'harddisk_cache_system' => 'null',
		'harddisk_cache_size' => '100',
		'harddisk_cache_location' => '/var/squid/cache',
		'level1_subdirs' => '16',
		'minimum_object_size' => '0',
		'maximum_object_size' => '4',
		'memory_cache_size' => '64',
		'maximum_objsize_in_mem' => '256'
	);
	foreach ($cache_defaults as $key => $value) {
		if (!array_key_exists($key, $cache)) {
			$cache[$key] = $value;
			$changed = true;
		}
	}
	config_set_path('installedpackages/squidcache/config/0', $cache);

	$auth = config_get_path('installedpackages/squidauth/config/0', array());
	if (!is_array($auth)) {
		$auth = array();
	}
	if (empty($auth['auth_method'])) {
		$auth['auth_method'] = 'none';
		config_set_path('installedpackages/squidauth/config/0', $auth);
		$changed = true;
	}

	$settings['dependency_defaults_initialized'] = 'on';
	kontrol_id_set_settings($settings);
	return $changed;
}

function kontrol_id_ensure_group() {
	foreach (config_get_path('system/group', array()) as $group) {
		if ($group['name'] === KONTROL_ID_GROUP) {
			return false;
		}
	}

	$highest_gid = 1999;
	foreach (config_get_path('system/group', array()) as $group) {
		if (isset($group['gid']) && is_numeric($group['gid'])) {
			$highest_gid = max($highest_gid, (int)$group['gid']);
		}
	}
	$group = array(
		'name' => KONTROL_ID_GROUP,
		'description' => 'Users allowed to authenticate on the Kontrol-ID portal',
		'scope' => 'local',
		'gid' => (string)($highest_gid + 1),
		'member' => array()
	);
	config_set_path('system/group/', $group);
	local_group_set($group);
	return true;
}

function kontrol_id_ensure_pki() {
	$settings = kontrol_id_get_settings();
	$changed = false;
	$ca_lookup = !empty($settings['ca_ref']) ? lookup_ca($settings['ca_ref']) : array();
	$ca = kontrol_id_extract_item($ca_lookup);
	if (empty($ca['crt']) || empty($ca['prv'])) {
		$ca = array(
			'refid' => uniqid(),
			'descr' => 'Kontrol-ID Internal CA',
			'serial' => 0
		);
		$dn = array(
			'organizationName' => 'Kontrol',
			'organizationalUnitName' => 'Kontrol-ID',
			'commonName' => 'Kontrol-ID Internal CA'
		);
		if (!ca_create($ca, '3072', '3650', $dn, 'sha256')) {
			throw new Exception('Unable to create the Kontrol-ID certificate authority.');
		}
		config_set_path('ca/', $ca);
		$settings['ca_ref'] = $ca['refid'];
		$settings['pki_owned'] = 'on';
		$changed = true;
	}

	$cert_lookup = !empty($settings['client_cert_ref']) ? lookup_cert($settings['client_cert_ref']) : array();
	$client = kontrol_id_extract_item($cert_lookup);
	if (empty($client['crt']) || empty($client['prv'])) {
		$client = array(
			'refid' => uniqid(),
			'descr' => 'Kontrol-ID WinRM Client'
		);
		$dn = array(
			'organizationName' => 'Kontrol',
			'organizationalUnitName' => 'Kontrol-ID',
			'commonName' => 'Kontrol-ID-WinRM-Client'
		);
		if (!cert_create($client, $settings['ca_ref'], '3072', '1825', $dn, 'user', 'sha256')) {
			throw new Exception('Unable to create the Kontrol-ID WinRM client certificate.');
		}
		config_set_path('cert/', $client);
		$settings['client_cert_ref'] = $client['refid'];
		$changed = true;
	}

	if ($changed) {
		kontrol_id_set_settings($settings);
	}
	return $changed;
}

function kontrol_id_ensure_storage() {
	safe_mkdir(KONTROL_ID_DB_DIR, 0770);
	safe_mkdir(KONTROL_ID_PKI_DIR, 0750);
	@chown(KONTROL_ID_DB_DIR, 'squid');
	@chgrp(KONTROL_ID_DB_DIR, 'proxy');
	@chmod(KONTROL_ID_DB_DIR, 0770);
	@chown(KONTROL_ID_PKI_DIR, 'root');
	@chgrp(KONTROL_ID_PKI_DIR, 'proxy');
	@chmod(KONTROL_ID_PKI_DIR, 0750);

	if (!class_exists('SQLite3')) {
		throw new Exception('The PHP SQLite3 extension is required by Kontrol-ID.');
	}
	$db = new SQLite3(KONTROL_ID_DB);
	$db->busyTimeout(1000);
	$db->exec('PRAGMA journal_mode=WAL');
	$db->exec('CREATE TABLE IF NOT EXISTS identities (' .
		'ip TEXT PRIMARY KEY, username TEXT NOT NULL, source TEXT NOT NULL, state TEXT NOT NULL, ' .
		'updated_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, stale_until INTEGER NOT NULL, ' .
		"detail TEXT NOT NULL DEFAULT '')");
	$db->exec('CREATE TABLE IF NOT EXISTS portal_sessions (' .
		'ip TEXT PRIMARY KEY, username TEXT NOT NULL, backend TEXT NOT NULL, created_at INTEGER NOT NULL, ' .
		'expires_at INTEGER NOT NULL, revoked INTEGER NOT NULL DEFAULT 0)');
	$db->exec('CREATE TABLE IF NOT EXISTS endpoints (' .
		"ip TEXT PRIMARY KEY, hostname TEXT NOT NULL, fqdn TEXT NOT NULL DEFAULT '', " .
		"cert_sha256 TEXT NOT NULL DEFAULT '', enrolled_at INTEGER NOT NULL, " .
		'last_seen INTEGER NOT NULL DEFAULT 0, revoked INTEGER NOT NULL DEFAULT 0)');
	$db->exec('CREATE TABLE IF NOT EXISTS auth_attempts (' .
		'ip TEXT NOT NULL, attempted_at INTEGER NOT NULL, success INTEGER NOT NULL DEFAULT 0)');
	$db->close();
	@chown(KONTROL_ID_DB, 'squid');
	@chgrp(KONTROL_ID_DB, 'proxy');
	@chmod(KONTROL_ID_DB, 0660);
}

function kontrol_id_write_pki() {
	$settings = kontrol_id_get_settings();
	$ca = kontrol_id_extract_item(lookup_ca($settings['ca_ref']));
	$client = kontrol_id_extract_item(lookup_cert($settings['client_cert_ref']));
	if (empty($ca['crt']) || empty($client['crt']) || empty($client['prv'])) {
		throw new Exception('Kontrol-ID PKI is incomplete.');
	}
	file_put_contents(KONTROL_ID_CA_FILE, base64_decode($ca['crt']), LOCK_EX);
	file_put_contents(KONTROL_ID_CLIENT_CERT, base64_decode($client['crt']), LOCK_EX);
	file_put_contents(KONTROL_ID_CLIENT_KEY, base64_decode($client['prv']), LOCK_EX);
	foreach (array(KONTROL_ID_CA_FILE, KONTROL_ID_CLIENT_CERT, KONTROL_ID_CLIENT_KEY) as $file) {
		@chown($file, 'root');
		@chgrp($file, 'proxy');
		@chmod($file, 0640);
	}
}

function kontrol_id_ini_value($value) {
	return str_replace(array("\r", "\n"), '', (string)$value);
}

function kontrol_id_write_helper_config() {
	$settings = kontrol_id_get_settings();
	$contents = "# Automatically generated by Kontrol-ID. Do not edit.\n[general]\n";
	$values = array(
		'enabled' => ($settings['enable'] === 'on' ? 'true' : 'false'),
		'winrm_port' => $settings['winrm_port'],
		'connect_timeout_ms' => $settings['connect_timeout_ms'],
		'request_timeout_ms' => $settings['request_timeout_ms'],
		'positive_ttl' => $settings['positive_ttl'],
		'negative_ttl' => $settings['negative_ttl'],
		'stale_ttl' => $settings['stale_ttl'],
		'unknown_user' => $settings['unknown_user'],
		'verify_peer' => ($settings['verify_peer'] === 'on' ? 'true' : 'false'),
		'verify_host' => ($settings['verify_host'] === 'on' ? 'true' : 'false'),
		'debug' => ($settings['debug'] === 'on' ? 'true' : 'false'),
		'database' => KONTROL_ID_DB,
		'ca_file' => KONTROL_ID_CA_FILE,
		'client_cert' => KONTROL_ID_CLIENT_CERT,
		'client_key' => KONTROL_ID_CLIENT_KEY
	);
	foreach ($values as $key => $value) {
		$contents .= $key . '=' . kontrol_id_ini_value($value) . "\n";
	}
	$tmp = KONTROL_ID_CONF . '.tmp';
	file_put_contents($tmp, $contents, LOCK_EX);
	@chown($tmp, 'root');
	@chgrp($tmp, 'proxy');
	@chmod($tmp, 0640);
	rename($tmp, KONTROL_ID_CONF);
}

function kontrol_id_portal_url() {
	$settings = kontrol_id_get_settings();
	if (!empty($settings['portal_url'])) {
		return $settings['portal_url'];
	}
	$scheme = config_get_path('system/webgui/protocol', 'https');
	$host = config_get_path('system/hostname', 'kontrol');
	$domain = config_get_path('system/domain', 'localdomain');
	if (!empty($domain)) {
		$host .= '.' . $domain;
	}
	$port = config_get_path('system/webgui/port');
	$default_port = ($scheme === 'https') ? '443' : '80';
	$port_part = (!empty($port) && (string)$port !== $default_port) ? ':' . (int)$port : '';
	return $scheme . '://' . $host . $port_part . '/kontrol_id_portal.php';
}

function kontrol_id_remove_squid_block($value) {
	$pattern = '/' . preg_quote(KONTROL_ID_BEGIN, '/') . ';.*?' . preg_quote(KONTROL_ID_END, '/') . ';?/s';
	$value = preg_replace($pattern, '', (string)$value);
	$value = preg_replace('/;{2,}/', ';', $value);
	return trim($value, "; \t\r\n");
}

function kontrol_id_squid_block() {
	$settings = kontrol_id_get_settings();
	$portal = preg_replace('/[\s;]/', '', kontrol_id_portal_url());
	$unknown = preg_replace('/[^A-Za-z0-9_.@-]/', '', $settings['unknown_user']);
	$lines = array(
		KONTROL_ID_BEGIN,
		'external_acl_type kontrol_identity children-startup=5 children-max=25 children-idle=5 ttl=5 negative_ttl=5 cache=65536 %SRC /usr/local/libexec/kontrol-identity-helper -c ' . KONTROL_ID_CONF,
		'acl kontrol_identity external kontrol_identity',
		'acl kontrol_identity_unknown ext_user -i ' . $unknown,
		/* Evaluate the helper and fail closed if it is unavailable. A successful
		 * lookup deliberately falls through to the existing Squid ACL policy. */
		'http_access deny !kontrol_identity'
	);
	if ($settings['unknown_action'] === 'portal') {
		$lines[] = 'deny_info 302:' . $portal . ' kontrol_identity_unknown';
		$lines[] = 'http_access deny kontrol_identity kontrol_identity_unknown';
	} elseif ($settings['unknown_action'] === 'deny') {
		$lines[] = 'http_access deny kontrol_identity kontrol_identity_unknown';
	}
	if ($settings['e2guardian_integration'] === 'on') {
		$lines[] = 'cache_peer 127.0.0.1 parent 8080 0 login=*:password no-digest no-netdb-exchange';
		$lines[] = 'always_direct deny all';
		$lines[] = 'never_direct allow all';
	}
	$lines[] = KONTROL_ID_END;
	return implode(';', $lines) . ';';
}

function kontrol_id_squid_conflicts($custom_options, $with_e2guardian) {
	$custom_options = kontrol_id_remove_squid_block($custom_options);
	$directives = str_replace(';', "\n", $custom_options);
	$conflicts = array();
	if (preg_match('/^\s*external_acl_type\s+kontrol_identity\b/im', $directives) ||
		strpos($directives, '/kontrol-identity-helper') !== false ||
		strpos($directives, '/kontrolhelper') !== false) {
		$conflicts[] = 'a manually configured or legacy Kontrol-ID helper';
	}
	if ($with_e2guardian && preg_match('/^\s*cache_peer\s+(127\.0\.0\.1|localhost)\s+parent\s+8080\b/im', $directives)) {
		$conflicts[] = 'an existing E2Guardian cache_peer on 127.0.0.1:8080';
	}
	return $conflicts;
}

function kontrol_id_apply_squid_integration($enabled) {
	$settings = kontrol_id_get_settings();
	$squid = config_get_path('installedpackages/squid/config/0', array());
	$auth = config_get_path('installedpackages/squidauth/config/0', array());
	if (!is_array($squid)) {
		$squid = array();
	}
	if (!is_array($auth)) {
		$auth = array('auth_method' => 'none');
	}

	$squid['custom_options'] = kontrol_id_remove_squid_block($squid['custom_options'] ?? '');
	if ($enabled) {
		$method = $auth['auth_method'] ?? 'none';
		if ($method !== 'none') {
			throw new Exception("Squid authentication method '{$method}' is incompatible with Kontrol-ID.");
		}
		$conflicts = kontrol_id_squid_conflicts(
			$squid['custom_options'],
			$settings['e2guardian_integration'] === 'on'
		);
		if (!empty($conflicts)) {
			throw new Exception('Squid contains ' . implode(' and ', $conflicts) . '. Remove it before enabling managed integration.');
		}
		$squid['custom_options'] = trim($squid['custom_options'], ';') .
			(empty($squid['custom_options']) ? '' : ';') . kontrol_id_squid_block();
	} elseif (!empty($settings['previous_squid_auth_method'])) {
		$settings['previous_squid_auth_method'] = '';
	}

	config_set_path('installedpackages/squid/config/0', $squid);
	config_set_path('installedpackages/squidauth/config/0', $auth);
	kontrol_id_set_settings($settings);
}

function kontrol_id_apply_e2guardian_integration($enabled) {
	$settings = kontrol_id_get_settings();
	$e2g = config_get_path('installedpackages/e2guardianconfig/config/0', array());
	if (!is_array($e2g)) {
		$e2g = array();
	}
	$plugins = array_filter(array_map('trim', explode(',', $e2g['auth_plugin'] ?? '')));
	$has_plugin = in_array('proxy-basic.conf', $plugins, true);
	if ($enabled && !$has_plugin) {
		$plugins[] = 'proxy-basic.conf';
		$settings['e2g_proxy_basic_added'] = 'on';
	} elseif (!$enabled && $settings['e2g_proxy_basic_added'] === 'on') {
		$plugins = array_values(array_diff($plugins, array('proxy-basic.conf')));
		$settings['e2g_proxy_basic_added'] = '';
	}
	$e2g['auth_plugin'] = implode(',', array_unique($plugins));
	config_set_path('installedpackages/e2guardianconfig/config/0', $e2g);
	kontrol_id_set_settings($settings);
}

function kontrol_id_validate($post, &$input_errors) {
	$numeric = array(
		'winrm_port' => array(1, 65535),
		'connect_timeout_ms' => array(200, 30000),
		'request_timeout_ms' => array(500, 60000),
		'positive_ttl' => array(5, 86400),
		'negative_ttl' => array(1, 3600),
		'stale_ttl' => array(0, 86400),
		'portal_ttl' => array(60, 604800)
	);
	foreach ($numeric as $field => $range) {
		if (!isset($post[$field]) || !is_numeric($post[$field]) ||
			(int)$post[$field] < $range[0] || (int)$post[$field] > $range[1]) {
			$input_errors[] = sprintf('%s must be between %d and %d.', $field, $range[0], $range[1]);
		}
	}
	if (!preg_match('/^[A-Za-z0-9_.@-]+$/', $post['unknown_user'] ?? '')) {
		$input_errors[] = 'Unknown user contains invalid characters.';
	}
	if (!empty($post['portal_url']) && filter_var($post['portal_url'], FILTER_VALIDATE_URL) === false) {
		$input_errors[] = 'Portal URL is invalid.';
	}
	if (($post['enable'] ?? '') === 'on') {
		$method = config_get_path('installedpackages/squidauth/config/0/auth_method', 'none');
		if ($method !== 'none') {
			$input_errors[] = "Squid authentication method '{$method}' must be disabled before enabling Kontrol-ID.";
		}
		if (($post['squid_integration'] ?? '') === 'on') {
			$squid = config_get_path('installedpackages/squid/config/0', array());
			if (($squid['enable_squid'] ?? '') !== 'on' || empty($squid['active_interface'])) {
				$input_errors[] = 'Enable Squid and select at least one proxy interface before enabling Kontrol-ID integration.';
			}
			$conflicts = kontrol_id_squid_conflicts(
				$squid['custom_options'] ?? '',
				($post['e2guardian_integration'] ?? '') === 'on'
			);
			if (!empty($conflicts)) {
				$input_errors[] = 'Remove ' . implode(' and ', $conflicts) . ' from Squid custom options before enabling managed integration.';
			}
		}
		if (($post['e2guardian_integration'] ?? '') === 'on') {
			$e2g = config_get_path('installedpackages/e2guardian/config/0', array());
			if (($e2g['enable'] ?? '') !== 'on' && ($e2g['enable_dg'] ?? '') !== 'on') {
				$input_errors[] = 'Enable E2Guardian before enabling Kontrol-ID forwarding.';
			}
		}
		if (auth_get_authserver($post['auth_server'] ?? 'Local Database') === null) {
			$input_errors[] = 'The selected Kontrol authentication server does not exist.';
		}
	}
}

function kontrol_id_resync($sync = true) {
	$changed = kontrol_id_ensure_defaults();
	$changed = kontrol_id_ensure_group() || $changed;
	$changed = kontrol_id_ensure_pki() || $changed;
	kontrol_id_ensure_storage();
	kontrol_id_write_pki();
	kontrol_id_write_helper_config();
	$settings = kontrol_id_get_settings();
	$enabled = ($settings['enable'] === 'on');
	kontrol_id_apply_squid_integration($enabled && $settings['squid_integration'] === 'on');
	kontrol_id_apply_e2guardian_integration($enabled && $settings['e2guardian_integration'] === 'on');
	write_config('Kontrol-ID configuration updated.');

	if (file_exists('/usr/local/pkg/squid.inc')) {
		require_once('/usr/local/pkg/squid.inc');
		if (function_exists('squid_resync')) {
			squid_resync('yes');
		}
	}
	if (file_exists('/usr/local/pkg/e2guardian.inc')) {
		require_once('/usr/local/pkg/e2guardian.inc');
		if (function_exists('sync_package_e2guardian')) {
			sync_package_e2guardian('no', false);
		}
	}
	if ($sync) {
		kontrol_id_sync_on_changes();
	}
}

function kontrol_id_install() {
	try {
		kontrol_id_ensure_defaults();
		kontrol_id_prepare_dependency_defaults();
		kontrol_id_ensure_group();
		kontrol_id_ensure_pki();
		write_config('Kontrol-ID installed with safe defaults.');
		kontrol_id_resync(false);
	} catch (Exception $e) {
		log_error('[Kontrol-ID] Installation failed: ' . $e->getMessage());
	}
}

function kontrol_id_deinstall() {
	try {
		$settings = kontrol_id_get_settings();
		$settings['enable'] = '';
		kontrol_id_set_settings($settings);
		kontrol_id_apply_squid_integration(false);
		kontrol_id_apply_e2guardian_integration(false);
		write_config('Kontrol-ID integrations removed.');
		@unlink(KONTROL_ID_CONF);
		@unlink(KONTROL_ID_CA_FILE);
		@unlink(KONTROL_ID_CLIENT_CERT);
		@unlink(KONTROL_ID_CLIENT_KEY);
	} catch (Exception $e) {
		log_error('[Kontrol-ID] Deinstallation cleanup failed: ' . $e->getMessage());
	}
}

function kontrol_id_generate_token($uses = 1, $ttl = 900) {
	$uses = max(1, min(500, (int)$uses));
	$ttl = max(60, min(86400, (int)$ttl));
	$token = bin2hex(random_bytes(24));
	$settings = kontrol_id_get_settings();
	$settings['enrollment_token_hash'] = hash('sha256', $token);
	$settings['enrollment_token_expires'] = (string)(time() + $ttl);
	$settings['enrollment_token_uses'] = (string)$uses;
	kontrol_id_set_settings($settings);
	write_config('Kontrol-ID enrollment token generated.');
	return $token;
}

function kontrol_id_token_valid($token) {
	$settings = kontrol_id_get_settings();
	if (empty($token) || empty($settings['enrollment_token_hash']) ||
		(int)$settings['enrollment_token_expires'] < time() ||
		(int)$settings['enrollment_token_uses'] < 1) {
		return false;
	}
	return hash_equals($settings['enrollment_token_hash'], hash('sha256', $token));
}

function kontrol_id_consume_token() {
	$settings = kontrol_id_get_settings();
	$settings['enrollment_token_uses'] = (string)max(0, (int)$settings['enrollment_token_uses'] - 1);
	if ((int)$settings['enrollment_token_uses'] === 0) {
		$settings['enrollment_token_hash'] = '';
		$settings['enrollment_token_expires'] = '0';
	}
	kontrol_id_set_settings($settings);
}

function kontrol_id_sync_on_changes() {
	$sync = config_get_path('installedpackages/kontrolidsync/config/0', array());
	if (!is_array($sync)) {
		return;
	}
	$mode = $sync['synconchanges'] ?? 'disabled';
	$targets = array();
	if ($mode === 'manual') {
		$targets = is_array($sync['row'] ?? null) ? $sync['row'] : array();
	} elseif ($mode === 'auto') {
		$ha = config_get_path('hasync', array());
		if (!empty($ha['synchronizetoip'])) {
			$targets[] = array(
				'syncdestinenable' => true,
				'ipaddress' => $ha['synchronizetoip'],
				'username' => $ha['username'] ?? 'admin',
				'password' => $ha['password'] ?? '',
				'syncprotocol' => config_get_path('system/webgui/protocol', 'https'),
				'syncport' => config_get_path('system/webgui/port', '443')
			);
		}
	} else {
		return;
	}

	foreach ($targets as $target) {
		if (empty($target['syncdestinenable'])) {
			continue;
		}
		kontrol_id_do_xmlrpc_sync($target, (int)($sync['synctimeout'] ?? 120));
	}
}

function kontrol_id_do_xmlrpc_sync($target, $timeout) {
	$host = $target['ipaddress'] ?? '';
	$port = $target['syncport'] ?? '';
	$protocol = $target['syncprotocol'] ?? 'https';
	$username = $target['username'] ?? 'admin';
	$password = $target['password'] ?? '';
	if ((!is_ipaddr($host) && !is_hostname($host) && !is_domain($host)) ||
		!is_port($port) || empty($username) || empty($password)) {
		log_error('[Kontrol-ID] XMLRPC synchronization target is invalid.');
		return;
	}
	$xml = array(
		'kontrolid' => config_get_path('installedpackages/kontrolid', array()),
		'kontrolidsync' => config_get_path('installedpackages/kontrolidsync', array())
	);
	$settings = kontrol_id_get_settings();
	$pki = array(
		'ca' => kontrol_id_extract_item(lookup_ca($settings['ca_ref'])),
		'cert' => kontrol_id_extract_item(lookup_cert($settings['client_cert_ref']))
	);
	if (empty($pki['ca']['refid']) || empty($pki['cert']['refid'])) {
		log_error('[Kontrol-ID] XMLRPC synchronization skipped because the package PKI is incomplete.');
		return;
	}
	$pki_payload = base64_encode(json_encode($pki));
	$client = new pfsense_xmlrpc_client();
	$client->setConnectionData($host, $port, $username, $password, $protocol);
	$client->xmlrpc_method('merge_installedpackages_section', $xml, $timeout);
	$command = "require_once('/usr/local/pkg/kontrol_id.inc');\n" .
		"\$kid_pki = json_decode(base64_decode('{$pki_payload}'), true);\n" .
		"foreach (array('ca', 'cert') as \$kid_section) {\n" .
		"  \$kid_item = \$kid_pki[\$kid_section];\n" .
		"  \$kid_items = config_get_path(\$kid_section, array());\n" .
		"  \$kid_found = false;\n" .
		"  foreach (\$kid_items as \$kid_idx => \$kid_current) {\n" .
		"    if ((\$kid_current['refid'] ?? '') === \$kid_item['refid']) { config_set_path(\$kid_section . '/' . \$kid_idx, \$kid_item); \$kid_found = true; break; }\n" .
		"  }\n" .
		"  if (!\$kid_found) { config_set_path(\$kid_section . '/', \$kid_item); }\n" .
		"}\n" .
		"write_config('Kontrol-ID PKI synchronized from XMLRPC master.');\n" .
		"kontrol_id_resync(false);";
	$client->xmlrpc_exec_php($command, $timeout);
	log_error("[Kontrol-ID] XMLRPC configuration synchronized to {$host}.");
}

?>
