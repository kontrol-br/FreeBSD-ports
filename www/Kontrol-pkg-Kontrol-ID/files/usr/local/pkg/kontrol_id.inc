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
define('KONTROL_ID_PKI_BACKUP', KONTROL_ID_DB_DIR . '/pki-backup.json');
define('KONTROL_ID_PKI_DIR', '/var/etc/kontrol-id');
define('KONTROL_ID_CA_FILE', KONTROL_ID_PKI_DIR . '/ca.pem');
define('KONTROL_ID_CLIENT_CERT', KONTROL_ID_PKI_DIR . '/client.crt');
define('KONTROL_ID_CLIENT_KEY', KONTROL_ID_PKI_DIR . '/client.key');
define('KONTROL_ID_ETC_DIR', '/usr/local/etc/kontrol-id');
define('KONTROL_ID_KRB5_CONF', KONTROL_ID_ETC_DIR . '/krb5.conf');
define('KONTROL_ID_SMB_CONF', KONTROL_ID_ETC_DIR . '/smb4.conf');
define('KONTROL_ID_DOMAIN_DIR', '/var/db/kontrol-id-domain');
define('KONTROL_ID_KEYTAB', KONTROL_ID_DOMAIN_DIR . '/krb5.keytab');
define('KONTROL_ID_ENGINE_AUTH_CONF', '/usr/local/etc/e2guardian/authplugins/kontrol-negotiate.conf');
define('KONTROL_ID_ENGINE_NTLM_AUTH_CONF', '/usr/local/etc/e2guardian/authplugins/kontrol-ntlm.conf');
define('KONTROL_ID_ENGINE_BASIC_AUTH_CONF', '/usr/local/etc/e2guardian/authplugins/kontrol-basic.conf');
define('KONTROL_ID_ENGINE_IDENTITY_CONF', '/usr/local/etc/e2guardian/authplugins/kontrol-identity.conf');
define('KONTROL_ID_LOGIN_TEMPLATE_DIR', '/usr/local/pkg');
define('KONTROL_ID_ENGINE_PID', '/var/run/kontrol-id.pid');
define('KONTROL_ID_WINBIND_MARKER', KONTROL_ID_DOMAIN_DIR . '/winbind.enabled');
define('KONTROL_ID_WINBIND_PID', '/var/run/kontrol-id-samba/winbindd.pid');
define('KONTROL_ID_SAMBA_DIR', KONTROL_ID_DOMAIN_DIR . '/samba');
define('KONTROL_ID_SAMBA_PRIVATE', KONTROL_ID_SAMBA_DIR . '/private');
define('KONTROL_ID_SAMBA_STATE', KONTROL_ID_SAMBA_DIR . '/state');
define('KONTROL_ID_SAMBA_CACHE', KONTROL_ID_SAMBA_DIR . '/cache');
define('KONTROL_ID_SAMBA_LOCK', KONTROL_ID_SAMBA_DIR . '/lock');
define('KONTROL_ID_SAMBA_RUN', '/var/run/kontrol-id-samba');
define('KONTROL_ID_POLICY_CACHE', KONTROL_ID_DOMAIN_DIR . '/policy-members.json');
define('KONTROL_ID_POLICY_STATUS', KONTROL_ID_DOMAIN_DIR . '/policy-status.json');
define('KONTROL_ID_POLICY_LOCK', KONTROL_ID_DOMAIN_DIR . '/policy-sync.lock');
define('KONTROL_ID_GROUP', 'kontrol-internet-users');
define('KONTROL_ID_QUIC_SOURCE_ALIAS', 'KID_QUIC_SRC_BYPASS');
define('KONTROL_ID_QUIC_DESTINATION_ALIAS', 'KID_QUIC_DST_BYPASS');
define('KONTROL_ID_BEGIN', '# BEGIN KONTROL-ID');
define('KONTROL_ID_END', '# END KONTROL-ID');
define('KONTROL_ID_E2G_BEGIN', '# BEGIN KONTROL-ID ICAP SSL BUMP');
define('KONTROL_ID_E2G_END', '# END KONTROL-ID ICAP SSL BUMP');

function kontrol_id_defaults() {
	return array(
		'enable' => '',
		'access_mode' => 'transparent',
		'explicit_auth' => 'kerberos',
		'proxy_fqdn' => '',
		'ad_domain' => '',
		'ad_dc' => '',
		'ad_workgroup' => '',
		'engine_workers' => '256',
		'applyaction' => '-r',
		'http_proxy_port' => '3128',
		'https_intercept_port' => '3129',
		'engine_integration' => 'on',
		'traffic_mode' => 'http_only',
		'block_quic' => 'on',
		'intercept_interfaces' => 'lan',
		'bypass_private_destinations' => '',
		'bypass_sources' => '',
		'bypass_destinations' => '',
		'tls_bypass_sites' => '',
		'download_limiter' => 'none',
		'upload_limiter' => 'none',
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
		'manage_portal_certificate' => 'on',
		'portal_cert_ref' => '',
		'previous_webgui_cert_ref' => '',
		'login_template' => 'kontrol_dark',
		'admin_contact' => '<p><strong>Identificação necessária.</strong></p>' .
			'<p>Esta estação ainda não possui uma identidade válida. Use uma credencial autorizada para liberar temporariamente o acesso à Internet.</p>' .
			'<p>Se você ainda não possui uma credencial, entre em contato com o administrador da rede.</p>',
		'auth_server' => 'Local Database',
		'local_group' => KONTROL_ID_GROUP,
		'ad_groups' => 'Domain Users',
		'ad_policy_enable' => '',
		'ad_policy_cache_ttl' => '300',
		'ad_policy_mappings' => '',
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
		'previous_squid_auth_config' => '',
		'previous_squid_config' => '',
		'previous_squid_cache_config' => '',
		'previous_squid_antivirus_config' => '',
		'previous_squid_remote_config' => '',
		'previous_e2guardian_config' => '',
		'previous_e2guardian_auth_plugin' => '',
		'e2guardian_auth_snapshot_taken' => '',
		'dependency_defaults_initialized' => ''
	);
}

function kontrol_id_login_template_catalog() {
	return array(
		'kontrol_dark' => 'kontrol-id-login-kontrol-dark.css',
		'aurora' => 'kontrol-id-login-aurora.css',
		'corporate' => 'kontrol-id-login-corporate.css',
		'signal' => 'kontrol-id-login-signal.css',
		'custom_message' => 'kontrol-id-login-corporate.css'
	);
}

function kontrol_id_login_template_path($template_id) {
	$catalog = kontrol_id_login_template_catalog();
	if (!isset($catalog[$template_id])) {
		$template_id = 'kontrol_dark';
	}
	return KONTROL_ID_LOGIN_TEMPLATE_DIR . '/' . $catalog[$template_id];
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

function kontrol_id_ca_is_usable($ca) {
	if (!is_array($ca) || empty($ca['refid']) || empty($ca['crt']) || empty($ca['prv'])) {
		return false;
	}
	$certificate = base64_decode($ca['crt'], true);
	$private_key = base64_decode($ca['prv'], true);
	if ($certificate === false || $private_key === false ||
		!function_exists('openssl_x509_check_private_key') ||
		!@openssl_x509_check_private_key($certificate, $private_key)) {
		return false;
	}
	$details = @openssl_x509_parse($certificate);
	if (!is_array($details)) {
		return false;
	}
	$now = time();
	if ((isset($details['validFrom_time_t']) && (int)$details['validFrom_time_t'] > $now) ||
		(isset($details['validTo_time_t']) && (int)$details['validTo_time_t'] <= $now)) {
		return false;
	}
	$constraints = (string)($details['extensions']['basicConstraints'] ?? '');
	return stripos($constraints, 'CA:TRUE') !== false;
}

function kontrol_id_ca_options() {
	$options = array();
	foreach (config_get_path('ca', array()) as $ca) {
		if (!kontrol_id_ca_is_usable($ca)) {
			continue;
		}
		$internal = (($ca['descr'] ?? '') === 'Kontrol-ID Internal CA');
		$options[] = array(
			'name' => ($ca['descr'] ?? $ca['refid']) .
				($internal ? ' (recommended)' : ''),
			'value' => $ca['refid'],
			'internal' => $internal ? 0 : 1
		);
	}
	usort($options, function($left, $right) {
		if ($left['internal'] !== $right['internal']) {
			return $left['internal'] <=> $right['internal'];
		}
		return strnatcasecmp($left['name'], $right['name']);
	});
	foreach ($options as &$option) {
		unset($option['internal']);
	}
	unset($option);
	return $options;
}

function kontrol_id_authenticate_basic($username, $password) {
	if (!is_string($username) || !is_string($password) || $username === '' ||
		strlen($username) > 255 || strlen($password) > 4096 ||
		!preg_match('/^[A-Za-z0-9_.@\\-\\\\]+$/D', $username)) {
		return false;
	}

	$settings = kontrol_id_get_settings();
	$authcfg = auth_get_authserver($settings['auth_server']);
	if (!is_array($authcfg)) {
		return false;
	}
	$attributes = array('nas_identifier' => 'kontrol-id-' . gethostname());
	if (!authenticate_user($username, $password, $authcfg, $attributes)) {
		return false;
	}

	if (($authcfg['type'] ?? '') === 'ldap') {
		$groups = @ldap_get_groups($username, $authcfg);
		$groups = array_map('strtolower', is_array($groups) ? $groups : array());
		foreach (array_filter(array_map('trim', explode(',', $settings['ad_groups']))) as $group) {
			if (in_array(strtolower($group), $groups, true)) {
				return true;
			}
		}
		return false;
	}

	$user = kontrol_id_extract_item(getUserEntry($username));
	$groups = is_array($user) ? local_user_get_groups($user, true) : array();
	return in_array($settings['local_group'], is_array($groups) ? $groups : array(), true);
}

function kontrol_id_authenticate_administrator($username, $password) {
	global $userindex, $groupindex;

	if (!is_string($username) || !is_string($password) || $username === '' ||
		strlen($username) > 255 || strlen($password) > 4096 ||
		!preg_match('/^[A-Za-z0-9_.@\\-\\\\]+$/D', $username)) {
		return false;
	}
	$userindex = index_users();
	$groupindex = array();
	foreach (config_get_path('system/group', array()) as $index => $group) {
		if (!empty($group['name'])) {
			$groupindex[$group['name']] = $index;
		}
	}
	$attributes = array('nas_identifier' => 'kontrol-id-provisioner-' . gethostname());
	$webgui_auth = auth_get_authserver(config_get_path('system/webgui/authmode', 'Local Database'));
	$authenticated_remotely = is_array($webgui_auth) &&
		authenticate_user($username, $password, $webgui_auth, $attributes);
	$effective_auth = $webgui_auth;
	if (!$authenticated_remotely) {
		$effective_auth = auth_get_authserver('Local Database');
		if (!authenticate_user($username, $password, $effective_auth, $attributes)) {
			return false;
		}
	}
	$user_entry = getUserEntry($username);
	$user = is_array($user_entry) ? ($user_entry['item'] ?? array()) : array();
	if ((string)($user['uid'] ?? '') === '0') {
		return true;
	}
	$groups = getUserGroups($username, $effective_auth, $attributes);
	return in_array('admins', is_array($groups) ? $groups : array(), true);
}

function kontrol_id_get_settings() {
	$settings = config_get_path('installedpackages/kontrolid/config/0', array());
	if (!is_array($settings)) {
		$settings = array();
	}
	return array_merge(kontrol_id_defaults(), $settings);
}

function kontrol_id_normalize_tls_bypass_sites($value, &$invalid = null) {
	$invalid_entries = array();
	$normalized = array();
	foreach (preg_split('/[\r\n;,]+/', (string)$value) as $entry) {
		$entry = trim($entry);
		if ($entry === '') {
			continue;
		}
		$host = $entry;
		if (preg_match('#^[a-z][a-z0-9+.-]*://#i', $host)) {
			$host = (string)parse_url($host, PHP_URL_HOST);
		} elseif (strpos($host, '/') !== false) {
			$host = (string)parse_url('https://' . ltrim($host, '/'), PHP_URL_HOST);
		}
		if (strpos($host, '*.') === 0) {
			$host = '.' . substr($host, 2);
		}
		$host = strtolower(trim($host));
		$validation_host = ltrim($host, '.');
		if ($validation_host === '' ||
			(!is_hostname($validation_host) && !is_ipaddr($validation_host) &&
			!is_ipaddrv6($validation_host))) {
			$invalid_entries[] = $entry;
			continue;
		}
		$normalized[] = $host;
	}
	$invalid = array_values(array_unique($invalid_entries));
	return array_values(array_unique($normalized));
}

function kontrol_id_normalize_destination_bypass($value) {
	$normalized = array();
	foreach (explode(';', (string)$value) as $entry) {
		$entry = trim($entry);
		if ($entry === '') {
			continue;
		}
		if (preg_match('#^[a-z][a-z0-9+.-]*://#i', $entry)) {
			$host = (string)parse_url($entry, PHP_URL_HOST);
			$normalized[] = ($host !== '') ? $host : $entry;
		} elseif (strpos($entry, '/') !== false && !is_subnet($entry) &&
			!is_subnetv6($entry)) {
			$host = (string)parse_url('https://' . ltrim($entry, '/'), PHP_URL_HOST);
			$normalized[] = ($host !== '') ? $host : $entry;
		} else {
			$normalized[] = $entry;
		}
	}
	return implode(';', array_values(array_unique($normalized)));
}

function kontrol_id_listener_owners($port) {
	$port = (int)$port;
	if ($port < 1 || $port > 65535) {
		return array();
	}

	$lines = array();
	$status = 0;
	exec('/usr/bin/sockstat -46l 2>/dev/null', $lines, $status);
	if ($status !== 0) {
		return array();
	}

	$owners = array();
	foreach ($lines as $line) {
		$columns = preg_split('/\s+/', trim((string)$line));
		if (count($columns) < 6 || !preg_match('/:(\d+)$/', $columns[5], $match) ||
			(int)$match[1] !== $port) {
			continue;
		}
		$owners[] = (string)$columns[1];
	}
	return array_values(array_unique($owners));
}

function kontrol_id_listener_owned_by_engine($port) {
	$owners = kontrol_id_listener_owners($port);
	if (empty($owners)) {
		return false;
	}
	foreach ($owners as $owner) {
		if (!in_array($owner, array('kontrol-id', 'e2guardian'), true)) {
			return false;
		}
	}
	return true;
}

function kontrol_id_wait_for_listener($port, $timeout = 20) {
	$deadline = microtime(true) + max(1, (int)$timeout);
	do {
		if (kontrol_id_listener_owned_by_engine($port)) {
			return true;
		}
		usleep(100000);
	} while (microtime(true) < $deadline);
	return kontrol_id_listener_owned_by_engine($port);
}

function kontrol_id_transparent_mode($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	return in_array(
		$settings['access_mode'] ?? 'transparent',
		array('transparent', 'transparent_no_identity'),
		true
	);
}

function kontrol_id_transparent_identity_mode($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	return ($settings['access_mode'] ?? 'transparent') === 'transparent';
}

/* The policy package originally registered its own Services menu entry as
 * "E2guardian Proxy". Removing the <menu> block from the package XML prevents
 * new installations from creating it, but pfSense preserves an already
 * imported installedpackages/menu item across package upgrades. Remove only
 * that exact legacy entry; the e2guardian_*.xml policy pages remain installed
 * and are still reached through the unified Kontrol-ID tabs. */
function kontrol_id_remove_legacy_e2guardian_menu() {
	$menus = config_get_path('installedpackages/menu', array());
	if (!is_array($menus)) {
		return false;
	}

	$changed = false;
	$filtered = array();
	foreach ($menus as $menu) {
		$name = trim((string)($menu['name'] ?? ''));
		$url = html_entity_decode((string)($menu['url'] ?? ''), ENT_QUOTES, 'UTF-8');
		$is_legacy_entry = (
			strcasecmp($name, 'E2guardian Proxy') === 0 &&
			strpos($url, 'xml=e2guardian.xml') !== false
		);
		if ($is_legacy_entry) {
			$changed = true;
			continue;
		}
		$filtered[] = $menu;
	}

	if ($changed) {
		config_set_path('installedpackages/menu', array_values($filtered));
	}
	return $changed;
}

/* Package upgrades preserve previous <service> registrations. Kontrol-ID now
 * owns the native service entry, so remove only the obsolete E2Guardian
 * signature that points to the retired e2guardian.sh rc script and migrate
 * the existing Kontrol-ID entry to the renamed native executable. */
function kontrol_id_remove_legacy_e2guardian_service() {
	$services = config_get_path('installedpackages/service', array());
	$legacy_script = '/usr/local/etc/rc.d/e2guardian.sh';
	$changed = false;
	if (is_file($legacy_script) || is_link($legacy_script)) {
		@unlink($legacy_script);
		$changed = true;
	}
	if (!is_array($services) || empty($services)) {
		return $changed;
	}
	if (isset($services['name'])) {
		$services = array($services);
	}

	$filtered = array();
	foreach ($services as $service) {
		if (is_array($service) &&
			($service['name'] ?? '') === 'kontrol_id' &&
			($service['rcfile'] ?? '') === 'kontrol_id' &&
			($service['executable'] ?? '') !== 'kontrol-id') {
			$service['executable'] = 'kontrol-id';
			$changed = true;
		}
		$is_legacy = is_array($service) &&
			($service['name'] ?? '') === 'e2guardian' &&
			($service['rcfile'] ?? '') === 'e2guardian.sh' &&
			($service['executable'] ?? '') === 'e2guardian';
		if ($is_legacy) {
			$changed = true;
			continue;
		}
		$filtered[] = $service;
	}
	if ($changed) {
		config_set_path('installedpackages/service', array_values($filtered));
	}
	return $changed;
}

function kontrol_id_tab_definitions() {
	$tabs = array(
		array('key' => 'general', 'text' => 'General', 'url' => '/pkg_edit.php?xml=kontrol_id.xml&amp;id=0')
	);
	if (kontrol_id_transparent_identity_mode()) {
		$tabs[] = array('key' => 'status', 'text' => 'Status & Enrollment', 'url' => '/kontrol_id_status.php');
	}
	$tabs = array_merge($tabs, array(
		array('key' => 'domain', 'text' => 'Domain & Keytab', 'url' => '/kontrol_id_domain.php'),
		array('key' => 'policy', 'text' => 'AD Policy', 'url' => '/kontrol_id_policy.php'),
		array('key' => 'limits', 'text' => 'Limits', 'url' => '/pkg_edit.php?xml=e2guardian_limits.xml&amp;id=0'),
		array('key' => 'blacklist', 'text' => 'Blacklist', 'url' => '/pkg_edit.php?xml=e2guardian_blacklist.xml&amp;id=0'),
		array('key' => 'acls', 'text' => 'ACLs', 'url' => '/pkg.php?xml=e2guardian_site_acl.xml'),
		array('key' => 'groups', 'text' => 'Groups', 'url' => '/pkg.php?xml=e2guardian_groups.xml'),
		array('key' => 'ips', 'text' => 'IPs', 'url' => '/pkg_edit.php?xml=e2guardian_ips.xml'),
		array('key' => 'users', 'text' => 'Users', 'url' => '/pkg_edit.php?xml=e2guardian_users.xml'),
		array('key' => 'gateways', 'text' => 'Gateways', 'url' => '/pkg.php?xml=e2guardian_gateways.xml'),
		array('key' => 'realtime', 'text' => 'Real Time', 'url' => '/e2guardian_monitor.php'),
		array('key' => 'reports', 'text' => 'Reports & Logs', 'url' => '/pkg_edit.php?xml=e2guardian_log.xml&amp;id=0'),
		array('key' => 'sync', 'text' => 'XMLRPC Sync', 'url' => '/pkg_edit.php?xml=kontrol_id_sync.xml&amp;id=0')
	));
	return $tabs;
}

function kontrol_id_print_general_auth_visibility() {
?>
<script type="text/javascript">
//<![CDATA[
events.push(function() {
	function kontrolIdUpdateExplicitAuthVisibility() {
		var explicitMode = ($('#access_mode').val() === 'explicit');
		var transparentHttps = !explicitMode && ($('#traffic_mode').val() === 'http_https');
		$('#explicit_auth').closest('.form-group').toggle(explicitMode);
		$('#block_quic').closest('.form-group').toggle(transparentHttps);
	}
	$('#access_mode, #traffic_mode').on('change', kontrolIdUpdateExplicitAuthVisibility);
	kontrolIdUpdateExplicitAuthVisibility();
});
//]]>
</script>
<?php
}

function kontrol_id_tabs($active) {
	$tabs = array();
	foreach (kontrol_id_tab_definitions() as $definition) {
		$tabs[] = array(
			$definition['text'],
			$active === $definition['key'],
			$definition['url']
		);
	}
	return $tabs;
}

function kontrol_id_package_active_tab($name) {
	$map = array(
		'kontrolid' => 'general',
		'kontrolidsync' => 'sync',
		'e2guardian' => 'general',
		'e2guardianconfig' => 'general',
		'e2guardianlimits' => 'limits',
		'e2guardianblacklist' => 'blacklist',
		'e2guardiansiteacl' => 'acls',
		'e2guardianurlacl' => 'acls',
		'e2guardianfileacl' => 'acls',
		'e2guardiancontentacl' => 'acls',
		'e2guardianheaderacl' => 'acls',
		'e2guardiansearchacl' => 'acls',
		'e2guardiangroups' => 'groups',
		'e2guardianldap' => 'groups',
		'e2guardianusers' => 'users',
		'e2guardianips' => 'ips',
		'e2guardiangateways' => 'gateways',
		'e2guardianlog' => 'reports',
		'e2guardiansync' => 'sync'
	);
	return $map[$name] ?? 'general';
}

function kontrol_id_adjust_policy_fields(&$pkg) {
	$name = $pkg['name'] ?? '';
	if (strpos($name, 'e2guardian') !== 0) {
		return;
	}
	foreach (array('title', 'delete_string', 'addedit_string') as $label) {
		if (isset($pkg[$label]) && is_string($pkg[$label])) {
			$pkg[$label] = str_ireplace('E2guardian', 'Kontrol-ID', $pkg[$label]);
		}
	}
	if (!is_array($pkg['fields']['field'] ?? null)) {
		return;
	}
	/* These controls belong to E2Guardian features retired by the Kontrol-ID
	 * profile. Keep this defensive filter here as well as in the policy XMLs
	 * so an older policy package cannot expose stale controls after an upgrade. */
	$hidden = array(
		'auth_plugin', 'picsacl', 'phraseacl', 'antivirusacl',
		'weightedphrasemode', 'phrasefiltermode', 'content_scanners',
		'contentscannericapurl', 'contentscannertimeout',
		'contentscanexceptions', 'maxcontentramcachescansize',
		'maxcontentfilecachescansize', 'initialtrickledelay',
		'trickledelay'
	);
	$retired_options = array(
		'scancleancache', 'forcequicksearch', 'deletedownloadedtempfiles',
		'recheckreplacedurls', 'disablecontentscan', 'enablepics',
		'icapscan', 'clamdscan'
	);
	$retired_topics = array(
		'antivirus', 'content scanners', 'phrase lists', 'phrases', 'pics'
	);
	if ($name === 'e2guardian') {
		$hidden = array_merge($hidden, array(
			'icapport', 'parentmode', 'proxyip', 'proxyport', 'proxytimeout', 'watchdog'
		));
		unset($pkg['advanced_options']);
	}
	$fields = array();
	foreach ($pkg['fields']['field'] as $field) {
		$fieldname = $field['fieldname'] ?? '';
		if (in_array($fieldname, $hidden, true)) {
			continue;
		}
		if (($field['type'] ?? '') === 'listtopic' &&
			in_array(strtolower(trim((string)($field['name'] ?? ''))), $retired_topics, true)) {
			continue;
		}
		foreach (array('fielddescr', 'name') as $label) {
			if (isset($field[$label]) && is_string($field[$label])) {
				$field[$label] = str_ireplace('E2guardian', 'Kontrol-ID', $field[$label]);
			}
		}
		foreach (array('description', 'sethelp') as $label) {
			if (isset($field[$label]) && is_string($field[$label])) {
				$field[$label] = str_replace(
					array('E2guardian', 'E2Guardian'),
					'Kontrol-ID',
					$field[$label]
				);
			}
		}
		if (is_array($field['options']['option'] ?? null)) {
			$options = array();
			foreach ($field['options']['option'] as $option) {
				if (in_array((string)($option['value'] ?? ''), $retired_options, true)) {
					continue;
				}
				if (isset($option['name']) && is_string($option['name'])) {
					$option['name'] = str_ireplace('E2guardian', 'Kontrol-ID', $option['name']);
				}
				$options[] = $option;
			}
			$field['options']['option'] = $options;
		}
		if ($name === 'e2guardian' && ($field['name'] ?? '') === 'Advanced Features') {
			$field['name'] = 'Configuration Backup';
			unset($field['advancedfield']);
		}
		if ($name === 'e2guardian' && in_array($fieldname, array(
			'e2g_export_settings_button', 'e2g_import_controls'
		), true)) {
			unset($field['advancedfield']);
			if (isset($field['fielddescr'])) {
				$field['fielddescr'] = str_ireplace('E2guardian', 'Kontrol-ID', $field['fielddescr']);
			}
			if (isset($field['description'])) {
				$field['description'] = str_ireplace('E2guardian', 'Kontrol-ID', $field['description']);
			}
		}
		$fields[] = $field;
	}
	$pkg['fields']['field'] = $fields;
}

function kontrol_id_adjust_package_tabs(&$pkg) {
	$name = $pkg['name'] ?? '';
	if ($name !== 'kontrolid' && $name !== 'kontrolidsync' &&
		strpos($name, 'e2guardian') !== 0) {
		return;
	}
	/* Every unified page shares the policy package shortcut definition. This
	 * gives the consolidated General page native service controls and status. */
	$pkg['shortcut_section'] = 'e2guardian';
	$active = kontrol_id_package_active_tab($name);
	$tabs = array();
	foreach (kontrol_id_tab_definitions() as $definition) {
		$tab = array('text' => $definition['text'], 'url' => $definition['url']);
		if ($active === $definition['key']) {
			$tab['active'] = '';
		}
		$tabs[] = $tab;
	}
	foreach (($pkg['tabs']['tab'] ?? array()) as $tab) {
		if ((int)($tab['tab_level'] ?? 1) > 1) {
			$tabs[] = $tab;
		}
	}
	$pkg['tabs']['tab'] = $tabs;
	kontrol_id_adjust_policy_fields($pkg);
}

/* Keep configuration transfer on the consolidated General page. The policy
 * backend owns the versioned XML format, while Kontrol-ID owns applying the
 * imported identity and engine settings as one managed transaction. */
function kontrol_id_handle_settings_transfer_request() {
	$export = ($_POST['kontrol_id_export_settings'] ?? '') === 'kontrol_id_export_settings';
	$import = ($_POST['kontrol_id_import_settings'] ?? '') === 'kontrol_id_import_settings';
	if (!$export && !$import) {
		return;
	}
	if (!is_file('/usr/local/pkg/e2guardian.inc')) {
		file_notice(
			'Kontrol-ID',
			gettext('The Kontrol-ID policy backend is not installed.'),
			gettext('Configuration transfer is unavailable.'),
			'danger'
		);
		return;
	}
	require_once('/usr/local/pkg/e2guardian.inc');
	if (!function_exists('e2guardian_export_settings_xml') ||
		!function_exists('e2guardian_import_settings_xml')) {
		file_notice(
			'Kontrol-ID',
			gettext('The installed policy backend does not support configuration transfer.'),
			gettext('Configuration transfer is unavailable.'),
			'danger'
		);
		return;
	}

	if ($export) {
		$payload = e2guardian_export_settings_xml();
		$filename = 'kontrol-id-config-' . date('Ymd-His') . '.xml';
		header('Content-Type: application/xml');
		header('Content-Disposition: attachment; filename="' . $filename . '"');
		header('Content-Length: ' . strlen($payload));
		echo $payload;
		exit;
	}

	$errors = array();
	$payload = trim((string)($_POST['kontrol_id_import_payload'] ?? ''));
	if ($payload === '') {
		$errors[] = gettext('Choose a valid Kontrol-ID XML export file before importing.');
	} elseif (e2guardian_import_settings_xml($payload, $errors)) {
		write_config('Kontrol-ID package settings imported from configuration backup.');
		kontrol_id_resync(false);
		file_notice(
			'Kontrol-ID',
			gettext('Import completed successfully.'),
			gettext('Kontrol-ID settings were imported and applied.'),
			'success'
		);
	}
	foreach ($errors as $error) {
		file_notice(
			'Kontrol-ID',
			$error,
			gettext('Failed to import Kontrol-ID settings.'),
			'danger'
		);
	}
}

kontrol_id_handle_settings_transfer_request();

/* pkg.php/pkg_edit.php parse the XML before loading this include file.  This
 * lets the package own a mode-aware tab bar without changing the base GUI. */
if (isset($pkg) && is_array($pkg)) {
	kontrol_id_adjust_package_tabs($pkg);
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
	$defaults = kontrol_id_defaults();
	$engine = config_get_path('installedpackages/e2guardian/config/0', array());
	if (!is_array($engine)) {
		$engine = array();
	}
	/* Preserve the few useful non-duplicated Engine values when upgrading to
	 * the single-page configuration model. General becomes authoritative after
	 * this migration. */
	$migration = array(
		'bypass_private_destinations' => 'private_subnet_proxy_off',
		'bypass_sources' => 'defined_ip_proxy_off',
		'bypass_destinations' => 'defined_ip_proxy_off_dest',
		'download_limiter' => 'inpipe',
		'upload_limiter' => 'outpipe'
	);
	foreach ($migration as $setting => $legacy) {
		if (!array_key_exists($setting, $current) && array_key_exists($legacy, $engine)) {
			$defaults[$setting] = $engine[$legacy];
		}
	}
	if (($current['admin_contact'] ?? '') ===
		'Entre em contato com o administrador da rede para solicitar sua credencial.') {
		$current['admin_contact'] = $defaults['admin_contact'];
	}
	$merged = array_merge($defaults, $current);
	if ($merged != $current) {
		kontrol_id_set_settings($merged);
		return true;
	}
	return false;
}

function kontrol_id_prepare_dependency_defaults() {
	/* Kontrol-ID is now the proxy and policy engine.  Keep this migration
	 * entry point, but never create or mutate conventional Squid settings. */
	return false;

	$settings = kontrol_id_get_settings();
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
		'allow_interface' => 'on',
		'log_sqd' => ''
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
		'nocache' => 'on',
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

	$antivirus = config_get_path('installedpackages/squidantivirus/config/0', array());
	if (!is_array($antivirus)) {
		$antivirus = array();
	}
	foreach (array('enable' => '', 'enable_advanced' => 'disabled') as $key => $value) {
		if (!array_key_exists($key, $antivirus)) {
			$antivirus[$key] = $value;
			$changed = true;
		}
	}
	config_set_path('installedpackages/squidantivirus/config/0', $antivirus);

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

function kontrol_id_snapshot_value($encoded_snapshot, $field) {
	$decoded = base64_decode((string)$encoded_snapshot, true);
	$snapshot = ($decoded === false) ? null : json_decode($decoded, true);
	if (!is_array($snapshot) || !isset($snapshot[$field]) ||
		empty($snapshot[$field]['exists'])) {
		return null;
	}
	return $snapshot[$field]['value'] ?? null;
}

function kontrol_id_certificate_matches_ca($certificate, $ca) {
	if (empty($certificate['crt']) || empty($certificate['prv']) || empty($ca['crt'])) {
		return false;
	}
	$certificate_pem = base64_decode($certificate['crt'], true);
	$ca_pem = base64_decode($ca['crt'], true);
	if ($certificate_pem === false || $ca_pem === false) {
		return false;
	}
	$ca_public_key = @openssl_pkey_get_public($ca_pem);
	return $ca_public_key !== false && @openssl_x509_verify($certificate_pem, $ca_public_key) === 1;
}

function kontrol_id_certificate_covers_host($certificate, $host) {
	$host = strtolower(rtrim(trim((string)$host), '.'));
	if ($host === '' || empty($certificate['crt'])) {
		return false;
	}
	$certificate_pem = base64_decode($certificate['crt'], true);
	$parsed = ($certificate_pem === false) ? false : @openssl_x509_parse($certificate_pem);
	if (!is_array($parsed)) {
		return false;
	}
	$subject_alt_name = (string)($parsed['extensions']['subjectAltName'] ?? '');
	foreach (array_map('trim', explode(',', $subject_alt_name)) as $entry) {
		if (!preg_match('/^(DNS|IP Address):(.+)$/i', $entry, $match)) {
			continue;
		}
		$type = strtolower($match[1]);
		$name = strtolower(rtrim(trim($match[2]), '.'));
		if ($type === 'ip address') {
			if ((is_ipaddr($host) || is_ipaddrv6($host)) && $name === $host) {
				return true;
			}
			continue;
		}
		if ($name === $host) {
			return true;
		}
		if (strpos($name, '*.') === 0) {
			$suffix = substr($name, 1);
			$prefix = substr($host, 0, -strlen($suffix));
			if ($prefix !== '' && strpos($prefix, '.') === false &&
				substr($host, -strlen($suffix)) === $suffix) {
				return true;
			}
		}
	}
	return false;
}

function kontrol_id_portal_certificate_sans($settings, $portal_host) {
	$subject_alt_names = array();
	$add_name = function($type, $value) use (&$subject_alt_names) {
		$value = trim((string)$value);
		if ($value !== '') {
			$subject_alt_names[] = $type . ':' . $value;
		}
	};
	if (is_ipaddr($portal_host) || is_ipaddrv6($portal_host)) {
		$add_name('IP', $portal_host);
	} else {
		$add_name('DNS', $portal_host);
	}
	$hostname = trim((string)config_get_path('system/hostname', ''));
	if ($hostname !== '' && is_hostname($hostname)) {
		$add_name('DNS', $hostname);
	}
	$interfaces = explode(',', kontrol_id_normalize_interfaces(
		$settings['intercept_interfaces'] ?? 'lan'));
	$interfaces[] = 'lan';
	foreach (array_unique(array_filter($interfaces)) as $interface) {
		$address = (string)get_interface_ip($interface);
		if (is_ipaddr($address) || is_ipaddrv6($address)) {
			$add_name('IP', $address);
		}
	}
	return array_values(array_unique($subject_alt_names));
}

function kontrol_id_restore_webgui_certificate(&$settings) {
	$current_ref = (string)config_get_path('system/webgui/ssl-certref', '');
	$portal_ref = (string)($settings['portal_cert_ref'] ?? '');
	$previous_ref = (string)($settings['previous_webgui_cert_ref'] ?? '');
	if ($portal_ref === '' || $current_ref !== $portal_ref || $previous_ref === '') {
		return false;
	}
	$previous = kontrol_id_extract_item(lookup_cert($previous_ref));
	if (empty($previous['crt']) || empty($previous['prv'])) {
		return false;
	}
	config_set_path('system/webgui/ssl-certref', $previous_ref);
	return true;
}

function kontrol_id_ensure_portal_certificate() {
	global $cert_strict_values;

	$settings = kontrol_id_get_settings();
	$result = array('changed' => false, 'restart_webgui' => false);
	$manage = ($settings['manage_portal_certificate'] ?? 'on') === 'on';
	$local_portal = trim((string)($settings['portal_url'] ?? '')) === '';
	$https_webgui = config_get_path('system/webgui/protocol', 'https') === 'https';
	if (!$manage || !$local_portal || !$https_webgui) {
		if ((!$manage || !$local_portal) && kontrol_id_restore_webgui_certificate($settings)) {
			$result['changed'] = true;
			$result['restart_webgui'] = true;
		}
		kontrol_id_set_settings($settings);
		return $result;
	}

	$portal_host = strtolower((string)parse_url(kontrol_id_portal_url(), PHP_URL_HOST));
	if ($portal_host === '' ||
		(!is_hostname($portal_host) && !is_ipaddr($portal_host) && !is_ipaddrv6($portal_host))) {
		throw new Exception('The local Kontrol-ID portal hostname is invalid.');
	}
	$current_ref = (string)config_get_path('system/webgui/ssl-certref', '');
	$current = kontrol_id_extract_item(lookup_cert($current_ref));
	$ca = kontrol_id_extract_item(lookup_ca($settings['ca_ref'] ?? ''));
	if (!kontrol_id_ca_is_usable($ca)) {
		throw new Exception('The selected Kontrol-ID CA cannot issue the local portal certificate.');
	}
	$portal_ref = (string)($settings['portal_cert_ref'] ?? '');
	$current_is_managed = $portal_ref !== '' && $current_ref === $portal_ref;
	$current_parsed = empty($current['crt']) ? false :
		@openssl_x509_parse(base64_decode($current['crt']));
	$current_not_expiring = is_array($current_parsed) &&
		(int)($current_parsed['validTo_time_t'] ?? 0) > time() + 86400;
	if (kontrol_id_certificate_covers_host($current, $portal_host) &&
		$current_not_expiring &&
		(!$current_is_managed || kontrol_id_certificate_matches_ca($current, $ca))) {
		return $result;
	}
	$portal = kontrol_id_extract_item(lookup_cert($portal_ref));
	$portal_valid = kontrol_id_certificate_matches_ca($portal, $ca) &&
		kontrol_id_certificate_covers_host($portal, $portal_host);
	if ($portal_valid) {
		$parsed = @openssl_x509_parse(base64_decode($portal['crt']));
		$portal_valid = is_array($parsed) &&
			(int)($parsed['validTo_time_t'] ?? 0) > time() + 86400;
	}
	if (!$portal_valid) {
		$portal = array(
			'refid' => uniqid(),
			'descr' => 'Kontrol-ID Portal - ' . $portal_host,
			'autorenew' => 'enabled'
		);
		$dn = array(
			'organizationName' => 'Kontrol',
			'organizationalUnitName' => 'Kontrol-ID Portal',
			'commonName' => $portal_host,
			'subjectAltName' => implode(',',
				kontrol_id_portal_certificate_sans($settings, $portal_host))
		);
		$max_lifetime = max(1, (int)($cert_strict_values['max_server_cert_lifetime'] ?? 825));
		if (!cert_create($portal, $settings['ca_ref'], '3072', (string)$max_lifetime,
			$dn, 'server', 'sha256')) {
			throw new Exception('Unable to create the Kontrol-ID portal server certificate.');
		}
		config_set_path('cert/', $portal);
		$settings['portal_cert_ref'] = $portal['refid'];
		$result['changed'] = true;
	}

	if ($current_ref !== ($portal['refid'] ?? '')) {
		if (empty($settings['previous_webgui_cert_ref'])) {
			$settings['previous_webgui_cert_ref'] = $current_ref;
		}
		config_set_path('system/webgui/ssl-certref', $portal['refid']);
		$result['changed'] = true;
		$result['restart_webgui'] = true;
	}
	kontrol_id_set_settings($settings);
	return $result;
}

function kontrol_id_load_pki_backup() {
	if (!file_exists(KONTROL_ID_PKI_BACKUP)) {
		return array();
	}
	$backup = json_decode((string)file_get_contents(KONTROL_ID_PKI_BACKUP), true);
	if (!is_array($backup) || !is_array($backup['ca'] ?? null) ||
		!is_array($backup['client'] ?? null) ||
		!kontrol_id_certificate_matches_ca($backup['client'], $backup['ca'])) {
		return array();
	}
	return $backup;
}

function kontrol_id_append_config_item_if_missing($section, $item) {
	foreach (config_get_path($section, array()) as $current) {
		if (($current['refid'] ?? '') === ($item['refid'] ?? '')) {
			return false;
		}
	}
	config_set_path($section . '/', $item);
	return true;
}

function kontrol_id_save_pki_backup($ca, $client) {
	if (!kontrol_id_certificate_matches_ca($client, $ca) || !is_dir(KONTROL_ID_DB_DIR)) {
		return;
	}
	$tmp = KONTROL_ID_PKI_BACKUP . '.tmp';
	file_put_contents($tmp, json_encode(array('ca' => $ca, 'client' => $client)), LOCK_EX);
	@chown($tmp, 'root');
	@chgrp($tmp, 'wheel');
	@chmod($tmp, 0600);
	rename($tmp, KONTROL_ID_PKI_BACKUP);
}

function kontrol_id_ensure_pki() {
	$settings = kontrol_id_get_settings();
	$changed = false;
	$ca_lookup = !empty($settings['ca_ref']) ? lookup_ca($settings['ca_ref']) : array();
	$ca = kontrol_id_extract_item($ca_lookup);
	if (empty($ca['crt']) || empty($ca['prv'])) {
		$backup = kontrol_id_load_pki_backup();
		if (!empty($backup)) {
			kontrol_id_append_config_item_if_missing('ca', $backup['ca']);
			kontrol_id_append_config_item_if_missing('cert', $backup['client']);
			$ca = $backup['ca'];
			$settings['ca_ref'] = $ca['refid'];
			$settings['client_cert_ref'] = $backup['client']['refid'];
			$changed = true;
			log_error('[Kontrol-ID] Restored package PKI from the protected runtime backup.');
		}
	}
	if (empty($ca['crt']) || empty($ca['prv'])) {
		$preferred_refs = array(
			config_get_path('installedpackages/squid/config/0/dca', ''),
			kontrol_id_snapshot_value($settings['previous_squid_config'] ?? '', 'dca')
		);
		foreach (array_unique(array_filter($preferred_refs)) as $preferred_ref) {
			$preferred_ca = kontrol_id_extract_item(lookup_ca($preferred_ref));
			if (!empty($preferred_ca['crt']) && !empty($preferred_ca['prv'])) {
				$ca = $preferred_ca;
				$settings['ca_ref'] = $preferred_ref;
				$changed = true;
				log_error('[Kontrol-ID] Reusing the pre-existing Squid certificate authority.');
				break;
			}
		}
	}
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
	if (!kontrol_id_certificate_matches_ca($client, $ca)) {
		$client = array();
		foreach (config_get_path('cert', array()) as $candidate) {
			if (($candidate['descr'] ?? '') === 'Kontrol-ID WinRM Client' &&
				kontrol_id_certificate_matches_ca($candidate, $ca)) {
				$client = $candidate;
				$settings['client_cert_ref'] = $candidate['refid'];
				$changed = true;
				break;
			}
		}
	}
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
	safe_mkdir(KONTROL_ID_ETC_DIR, 0750);
	safe_mkdir(KONTROL_ID_DOMAIN_DIR, 0750);
	@chown(KONTROL_ID_DOMAIN_DIR, 'root');
	@chgrp(KONTROL_ID_DOMAIN_DIR, 'proxy');
	@chmod(KONTROL_ID_DOMAIN_DIR, 0750);
	foreach (array(KONTROL_ID_SAMBA_DIR, KONTROL_ID_SAMBA_PRIVATE) as $directory) {
		safe_mkdir($directory, 0750);
		@chown($directory, 'root');
		@chgrp($directory, 'proxy');
		@chmod($directory, 0750);
	}
	/* Samba requires these directories to be searchable by its own helper
	 * processes. Their parent remains root:proxy 0750, so this does not expose
	 * the domain state outside the package-owned tree. */
	foreach (array(KONTROL_ID_SAMBA_STATE, KONTROL_ID_SAMBA_CACHE,
		KONTROL_ID_SAMBA_LOCK) as $directory) {
		safe_mkdir($directory, 0755);
		@chown($directory, 'root');
		@chgrp($directory, 'proxy');
		@chmod($directory, 0755);
	}
	foreach (array(KONTROL_ID_SAMBA_RUN, KONTROL_ID_SAMBA_RUN . '/winbindd') as $directory) {
		safe_mkdir($directory, 0750);
		@chown($directory, 'root');
		@chgrp($directory, 'proxy');
		@chmod($directory, 0750);
	}
	@chown(KONTROL_ID_DB_DIR, 'squid');
	@chgrp(KONTROL_ID_DB_DIR, 'proxy');
	@chmod(KONTROL_ID_DB_DIR, 0770);
	@chown(KONTROL_ID_PKI_DIR, 'root');
	@chgrp(KONTROL_ID_PKI_DIR, 'proxy');
	@chmod(KONTROL_ID_PKI_DIR, 0750);
	@chown(KONTROL_ID_ETC_DIR, 'root');
	@chgrp(KONTROL_ID_ETC_DIR, 'proxy');
	@chmod(KONTROL_ID_ETC_DIR, 0750);

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
	$db->exec('CREATE TABLE IF NOT EXISTS provisioner_auth_attempts (' .
		'ip TEXT NOT NULL, attempted_at INTEGER NOT NULL, success INTEGER NOT NULL DEFAULT 0)');
	$db->exec('CREATE TABLE IF NOT EXISTS enrollment_authorizations (' .
		'code_hash TEXT PRIMARY KEY, pkce_challenge TEXT NOT NULL, hostname TEXT NOT NULL, ' .
		"fqdn TEXT NOT NULL DEFAULT '', approved_by TEXT NOT NULL, created_at INTEGER NOT NULL, " .
		'expires_at INTEGER NOT NULL, consumed INTEGER NOT NULL DEFAULT 0)');
	/* Removed stations are not an inventory or revocation list. A signed
	 * unenrollment deletes the row, and this migration clears legacy rows. */
	$db->exec('DELETE FROM endpoints WHERE revoked<>0');
	$db->close();
	@chown(KONTROL_ID_DB, 'squid');
	@chgrp(KONTROL_ID_DB, 'proxy');
	@chmod(KONTROL_ID_DB, 0660);
}

function kontrol_id_atomic_write($path, $contents, $mode = 0640, $group = 'proxy') {
	$directory = dirname($path);
	if (!is_dir($directory)) {
		safe_mkdir($directory, 0750);
	}
	$tmp = tempnam($directory, '.kontrol-id-');
	if ($tmp === false || file_put_contents($tmp, $contents, LOCK_EX) === false) {
		if (is_string($tmp) && file_exists($tmp)) {
			@unlink($tmp);
		}
		throw new Exception('Unable to write managed configuration ' . basename($path) . '.');
	}
	@chown($tmp, 'root');
	@chgrp($tmp, $group);
	@chmod($tmp, $mode);
	if (!@rename($tmp, $path)) {
		@unlink($tmp);
		throw new Exception('Unable to install managed configuration ' . basename($path) . '.');
	}
}

function kontrol_id_policy_mappings($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	$encoded = (string)($settings['ad_policy_mappings'] ?? '');
	$decoded = base64_decode($encoded, true);
	$rows = ($decoded === false) ? null : json_decode($decoded, true);
	if (!is_array($rows)) {
		return array();
	}
	$mappings = array();
	foreach ($rows as $row) {
		if (!is_array($row)) {
			continue;
		}
		$ad_group = trim((string)($row['ad_group'] ?? ''));
		$e2guardian_group = trim((string)($row['e2guardian_group'] ?? ''));
		if ($ad_group === '' || $e2guardian_group === '') {
			continue;
		}
		$mappings[] = array(
			'ad_group' => $ad_group,
			'e2guardian_group' => $e2guardian_group
		);
	}
	return $mappings;
}

function kontrol_id_policy_enabled($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	return ($settings['ad_policy_enable'] ?? '') === 'on' &&
		!empty(kontrol_id_policy_mappings($settings));
}

function kontrol_id_e2guardian_group_options() {
	$options = array();
	$groups = config_get_path('installedpackages/e2guardiangroups/config', array());
	foreach (is_array($groups) ? $groups : array() as $index => $group) {
		if (!is_array($group) || empty($group['name'])) {
			continue;
		}
		$description = (string)($group['description'] ?? $group['name']);
		if ($index === 0) {
			$description .= ' — default/fallback policy';
		}
		$options[(string)$group['name']] = $description;
	}
	return $options;
}

function kontrol_id_policy_cache() {
	if (!is_file(KONTROL_ID_POLICY_CACHE)) {
		return array();
	}
	$cache = json_decode((string)@file_get_contents(KONTROL_ID_POLICY_CACHE), true);
	return is_array($cache) ? $cache : array();
}

function kontrol_id_policy_status() {
	if (!is_file(KONTROL_ID_POLICY_STATUS)) {
		return array();
	}
	$status = json_decode((string)@file_get_contents(KONTROL_ID_POLICY_STATUS), true);
	return is_array($status) ? $status : array();
}

function kontrol_id_ldap_filter_escape($value) {
	return preg_replace_callback('/[\\\\*()\x00]/', function ($match) {
		return sprintf('\\%02x', ord($match[0]));
	}, (string)$value);
}

function kontrol_id_parse_ads_search($output) {
	$records = array();
	$current = array();
	$last_attribute = '';
	foreach (preg_split('/\R/', (string)$output) as $line) {
		if ($line === '') {
			if (!empty($current)) {
				$records[] = $current;
				$current = array();
			}
			$last_attribute = '';
			continue;
		}
		if (preg_match('/^\s+(.+)$/', $line, $continuation) && $last_attribute !== '') {
			$current[$last_attribute] .= trim($continuation[1]);
			continue;
		}
		if (!preg_match('/^([A-Za-z][A-Za-z0-9;-]*):\s?(.*)$/', $line, $matches)) {
			continue;
		}
		$last_attribute = strtolower($matches[1]);
		$current[$last_attribute] = $matches[2];
	}
	if (!empty($current)) {
		$records[] = $current;
	}
	return $records;
}

function kontrol_id_ads_search($filter, array $attributes) {
	if (!is_file(KONTROL_ID_SAMBA_PRIVATE . '/secrets.tdb')) {
		throw new Exception('This appliance has no local Active Directory machine trust.');
	}
	$command = array(
		'/usr/local/bin/net',
		'--configfile=' . KONTROL_ID_SMB_CONF,
		'--machine-pass',
		'--use-kerberos=required',
		'ads',
		'search',
		(string)$filter
	);
	foreach ($attributes as $attribute) {
		$command[] = (string)$attribute;
	}
	$result = kontrol_id_run_command($command, '', kontrol_id_domain_environment(), 60);
	if ($result['code'] !== 0) {
		$detail = trim($result['stderr'] ?: $result['stdout']);
		throw new Exception('Kerberos-authenticated Active Directory search failed.' .
			($detail !== '' ? ' ' . substr($detail, 0, 512) : ''));
	}
	return kontrol_id_parse_ads_search($result['stdout']);
}

function kontrol_id_ad_group_names() {
	$records = kontrol_id_ads_search('(objectCategory=group)', array('sAMAccountName'));
	$groups = array();
	foreach ($records as $record) {
		if (!empty($record['samaccountname'])) {
			$groups[] = $record['samaccountname'];
		}
	}
	$groups = array_values(array_unique($groups));
	natcasesort($groups);
	return array_values($groups);
}

function kontrol_id_policy_member_identities($samaccountname, $user_principal_name, $workgroup, $realm) {
	$identities = array(trim((string)$samaccountname));
	if ($workgroup !== '') {
		$identities[] = $workgroup . '\\' . $samaccountname;
	}
	if (trim((string)$user_principal_name) !== '') {
		$identities[] = trim((string)$user_principal_name);
	} elseif ($realm !== '') {
		$identities[] = $samaccountname . '@' . $realm;
	}
	$lowercase_identities = array_map('strtolower', $identities);
	return array_values(array_unique(array_filter(
		array_merge($identities, $lowercase_identities),
		'strlen'
	)));
}

function kontrol_id_rebuild_e2guardian_policy() {
	if (!is_file('/usr/local/pkg/e2guardian.inc')) {
		return;
	}
	require_once('/usr/local/pkg/e2guardian.inc');
	if (function_exists('sync_package_e2guardian')) {
		$had_csrf = isset($_POST['__csrf_magic']);
		$previous_csrf = $_POST['__csrf_magic'] ?? null;
		if (!$had_csrf || empty($previous_csrf)) {
			$_POST['__csrf_magic'] = 'kontrol-id-policy-sync';
		}
		sync_package_e2guardian('no', false);
		if ($had_csrf) {
			$_POST['__csrf_magic'] = $previous_csrf;
		} else {
			unset($_POST['__csrf_magic']);
		}
	}
	if (function_exists('e2guardian_start')) {
		e2guardian_start('no', false, true);
	}
}

function kontrol_id_sync_ad_policies($force = false) {
	$settings = kontrol_id_get_settings();
	$mappings = kontrol_id_policy_mappings($settings);
	if (!kontrol_id_policy_enabled($settings)) {
		return array('ok' => true, 'changed' => false, 'skipped' => true,
			'message' => 'Active Directory policy mapping is disabled or has no mappings.');
	}
	kontrol_id_ensure_storage();
	$ttl = max(60, min(86400, (int)($settings['ad_policy_cache_ttl'] ?? 300)));
	$existing = kontrol_id_policy_cache();
	if (!$force && (int)($existing['generated_at'] ?? 0) + $ttl > time()) {
		return array('ok' => true, 'changed' => false, 'skipped' => true,
			'message' => 'The last successful policy cache is still fresh.');
	}
	$lock = @fopen(KONTROL_ID_POLICY_LOCK, 'c');
	if ($lock === false || !flock($lock, LOCK_EX | ($force ? 0 : LOCK_NB))) {
		if (is_resource($lock)) {
			fclose($lock);
		}
		return array('ok' => true, 'changed' => false, 'skipped' => true,
			'message' => 'Another Active Directory policy synchronization is already running.');
	}
	$attempted_at = time();
	try {
		$domain = kontrol_id_domain_values($settings);
		$cache_mappings = array();
		$claimed_identities = array();
		foreach ($mappings as $mapping) {
			$escaped_group = kontrol_id_ldap_filter_escape($mapping['ad_group']);
			$group_records = kontrol_id_ads_search(
				'(&(objectCategory=group)(sAMAccountName=' . $escaped_group . '))',
				array('distinguishedName', 'sAMAccountName', 'primaryGroupToken')
			);
			if (count($group_records) !== 1 || empty($group_records[0]['distinguishedname'])) {
				throw new Exception('AD group "' . $mapping['ad_group'] . '" was not found uniquely.');
			}
			$group = $group_records[0];
			$membership = '(memberOf:1.2.840.113556.1.4.1941:=' .
				kontrol_id_ldap_filter_escape($group['distinguishedname']) . ')';
			if (!empty($group['primarygrouptoken']) && ctype_digit($group['primarygrouptoken'])) {
				$membership = '(|' . $membership . '(primaryGroupID=' . $group['primarygrouptoken'] . '))';
			}
			$user_records = kontrol_id_ads_search(
				'(&(objectCategory=person)(objectClass=user)' . $membership .
				'(!(userAccountControl:1.2.840.113556.1.4.803:=2)))',
				array('sAMAccountName', 'displayName', 'userPrincipalName')
			);
			$members = array();
			foreach ($user_records as $user) {
				$sam = trim((string)($user['samaccountname'] ?? ''));
				if ($sam === '' || substr($sam, -1) === '$') {
					continue;
				}
				$identities = kontrol_id_policy_member_identities(
					$sam,
					$user['userprincipalname'] ?? '',
					$domain['workgroup'],
					$domain['realm']
				);
				$available = array();
				foreach ($identities as $identity) {
					$key = strtolower($identity);
					if (!isset($claimed_identities[$key])) {
						$available[] = $identity;
						$claimed_identities[$key] = $mapping['e2guardian_group'];
					}
				}
				if (!empty($available)) {
					$members[] = array(
						'username' => $sam,
						'display_name' => trim((string)($user['displayname'] ?? '')),
						'identities' => $available
					);
				}
			}
			usort($members, function ($left, $right) {
				return strcasecmp($left['username'], $right['username']);
			});
			$cache_mappings[] = array(
				'ad_group' => $mapping['ad_group'],
				'e2guardian_group' => $mapping['e2guardian_group'],
				'account_count' => count($members),
				'members' => $members
			);
		}
		$new_cache = array(
			'version' => 1,
			'source' => 'kerberos-machine-trust',
			'generated_at' => time(),
			'expires_at' => time() + $ttl,
			'mappings' => $cache_mappings
		);
		$encoded = json_encode($new_cache, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
		if ($encoded === false) {
			throw new Exception('Unable to encode the Active Directory policy cache.');
		}
		$changed = json_encode($existing['mappings'] ?? array()) !== json_encode($cache_mappings);
		kontrol_id_atomic_write(KONTROL_ID_POLICY_CACHE, $encoded . "\n", 0640);
		$status = array('last_attempt' => $attempted_at, 'last_success' => time(),
			'ok' => true, 'message' => 'Active Directory policy synchronization completed.');
		kontrol_id_atomic_write(KONTROL_ID_POLICY_STATUS,
			json_encode($status, JSON_PRETTY_PRINT) . "\n", 0640);
		if ($changed || $force) {
			kontrol_id_rebuild_e2guardian_policy();
		}
		return array('ok' => true, 'changed' => $changed, 'skipped' => false,
			'message' => $status['message'], 'cache' => $new_cache);
	} catch (Throwable $exception) {
		$status = array('last_attempt' => $attempted_at,
			'last_success' => (int)($existing['generated_at'] ?? 0), 'ok' => false,
			'message' => $exception->getMessage());
		kontrol_id_atomic_write(KONTROL_ID_POLICY_STATUS,
			json_encode($status, JSON_PRETTY_PRINT) . "\n", 0640);
		log_error('[Kontrol-ID] AD policy synchronization failed: ' . $exception->getMessage());
		return array('ok' => false, 'changed' => false, 'skipped' => false,
			'message' => $exception->getMessage());
	} finally {
		flock($lock, LOCK_UN);
		fclose($lock);
	}
}

function kontrol_id_e2guardian_policy_users($e2guardian_group) {
	$settings = kontrol_id_get_settings();
	if (!kontrol_id_policy_enabled($settings)) {
		return array();
	}
	$wanted = strtolower(trim((string)$e2guardian_group));
	foreach ((array)(kontrol_id_policy_cache()['mappings'] ?? array()) as $mapping) {
		if (strtolower((string)($mapping['e2guardian_group'] ?? '')) !== $wanted) {
			continue;
		}
		$users = array();
		foreach ((array)($mapping['members'] ?? array()) as $member) {
			foreach ((array)($member['identities'] ?? array()) as $identity) {
				$identity = trim((string)$identity);
				if ($identity !== '') {
					$users[] = $identity;
				}
			}
		}
		return array_values(array_unique($users));
	}
	return array();
}

function kontrol_id_policy_schedule() {
	$command = '/usr/local/bin/php -q /usr/local/libexec/kontrol-id-policy-sync.php';
	install_cron_job($command, false, '*', '*', '*', '*', '*', 'root', false);
	install_cron_job($command . ' > /dev/null 2>&1', false, '*', '*', '*', '*', '*', 'root', false);
	if (kontrol_id_policy_enabled()) {
		install_cron_job($command . ' > /dev/null 2>&1', true, '*', '*', '*', '*', '*', 'root', false);
	}
}

function kontrol_id_valid_dns_name($value) {
	return is_string($value) && strlen($value) <= 253 &&
		preg_match('/^(?=.{1,253}$)(?:[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?\.)+[A-Za-z0-9](?:[A-Za-z0-9-]{0,61}[A-Za-z0-9])?$/D', $value);
}

function kontrol_id_domain_values($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	$domain = strtolower(trim($settings['ad_domain'] ?? ''));
	$realm = strtoupper($domain);
	$dc = strtolower(trim($settings['ad_dc'] ?? ''));
	$fqdn = strtolower(trim($settings['proxy_fqdn'] ?? ''));
	$workgroup = strtoupper(trim($settings['ad_workgroup'] ?? ''));
	$netbios = strtoupper(strtok($fqdn, '.') ?: config_get_path('system/hostname', 'KONTROL'));
	$netbios = preg_replace('/[^A-Z0-9-]/', '', $netbios);
	$netbios = substr($netbios, 0, 15);
	return array(
		'domain' => $domain,
		'realm' => $realm,
		'dc' => $dc,
		'fqdn' => $fqdn,
		'workgroup' => $workgroup,
		'netbios' => $netbios,
		'spn' => (!empty($fqdn) && !empty($realm)) ? 'HTTP/' . $fqdn . '@' . $realm : ''
	);
}

function kontrol_id_kerberos_required($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	return ($settings['access_mode'] ?? '') === 'explicit' &&
		in_array($settings['explicit_auth'] ?? '', array('kerberos', 'kerberos_ntlm'), true);
}

function kontrol_id_ntlm_required($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	return ($settings['access_mode'] ?? '') === 'explicit' &&
		in_array($settings['explicit_auth'] ?? '', array('ntlm', 'kerberos_ntlm'), true);
}

function kontrol_id_write_domain_config($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	$domain = kontrol_id_domain_values($settings);
	if (!kontrol_id_valid_dns_name($domain['domain']) ||
		!kontrol_id_valid_dns_name($domain['dc']) ||
		!kontrol_id_valid_dns_name($domain['fqdn']) ||
		!preg_match('/^[A-Z0-9][A-Z0-9-]{0,14}$/D', $domain['workgroup']) ||
		empty($domain['netbios'])) {
		@unlink(KONTROL_ID_KRB5_CONF);
		@unlink(KONTROL_ID_SMB_CONF);
		@unlink(KONTROL_ID_WINBIND_MARKER);
		return;
	}

	$krb5 = "# Automatically generated by Kontrol-ID. Do not edit.\n" .
		"[libdefaults]\n" .
		" default_realm = {$domain['realm']}\n" .
		" dns_lookup_realm = false\n" .
		" dns_lookup_kdc = true\n" .
		" rdns = false\n" .
		" forwardable = true\n" .
		" default_keytab_name = FILE:" . KONTROL_ID_KEYTAB . "\n" .
		" default_tkt_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n" .
		" default_tgs_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n" .
		" permitted_enctypes = aes256-cts-hmac-sha1-96 aes128-cts-hmac-sha1-96\n\n" .
		"[realms]\n" .
		" {$domain['realm']} = {\n" .
		(!empty($domain['dc']) ? "  kdc = {$domain['dc']}\n  admin_server = {$domain['dc']}\n" : '') .
		" }\n\n" .
		"[domain_realm]\n" .
		" .{$domain['domain']} = {$domain['realm']}\n" .
		" {$domain['domain']} = {$domain['realm']}\n";

	$smb = "# Automatically generated by Kontrol-ID. Do not edit.\n" .
		"[global]\n" .
		" workgroup = {$domain['workgroup']}\n" .
		" realm = {$domain['realm']}\n" .
		" netbios name = {$domain['netbios']}\n" .
		" security = ADS\n" .
		" kerberos encryption types = strong\n" .
		" kerberos method = dedicated keytab\n" .
		" dedicated keytab file = " . KONTROL_ID_KEYTAB . "\n" .
		" private dir = " . KONTROL_ID_SAMBA_PRIVATE . "\n" .
		" state directory = " . KONTROL_ID_SAMBA_STATE . "\n" .
		" cache directory = " . KONTROL_ID_SAMBA_CACHE . "\n" .
		" lock directory = " . KONTROL_ID_SAMBA_LOCK . "\n" .
		" pid directory = " . KONTROL_ID_SAMBA_RUN . "\n" .
		" winbind use default domain = yes\n" .
		" winbind enum users = no\n" .
		" winbind enum groups = no\n" .
		" winbind refresh tickets = yes\n" .
		" winbind offline logon = no\n" .
		" idmap config * : backend = tdb\n" .
		" idmap config * : range = 100000-199999\n" .
		" idmap config {$domain['workgroup']} : backend = rid\n" .
		" idmap config {$domain['workgroup']} : range = 200000-999999\n" .
		" client min protocol = SMB2_02\n" .
		" ntlm auth = ntlmv2-only\n" .
		" map to guest = never\n" .
		" load printers = no\n" .
		" disable spoolss = yes\n" .
		" log file = /var/log/kontrol-id-winbind.%m\n" .
		" max log size = 1024\n";

	kontrol_id_atomic_write(KONTROL_ID_KRB5_CONF, $krb5);
	kontrol_id_atomic_write(KONTROL_ID_SMB_CONF, $smb);
	if (kontrol_id_ntlm_required($settings) && ($settings['enable'] ?? '') === 'on') {
		kontrol_id_atomic_write(KONTROL_ID_WINBIND_MARKER, "managed by Kontrol-ID\n", 0640);
	} else {
		@unlink(KONTROL_ID_WINBIND_MARKER);
	}
}

function kontrol_id_run_command(array $command, $stdin = '', array $environment = array(), $timeout = 30) {
	$descriptors = array(
		0 => array('pipe', 'r'),
		1 => array('pipe', 'w'),
		2 => array('pipe', 'w')
	);
	$base_environment = array(
		'PATH' => '/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin',
		'LANG' => 'C',
		'LC_ALL' => 'C'
	);
	$process = @proc_open($command, $descriptors, $pipes, null,
		array_merge($base_environment, $environment), array('bypass_shell' => true));
	if (!is_resource($process)) {
		return array('code' => 127, 'stdout' => '', 'stderr' => 'Unable to start command.');
	}
	if ($stdin !== '') {
		fwrite($pipes[0], $stdin);
	}
	fclose($pipes[0]);
	stream_set_blocking($pipes[1], false);
	stream_set_blocking($pipes[2], false);
	$stdout = '';
	$stderr = '';
	$started = microtime(true);
	$exit_code = -1;
	do {
		$stdout .= stream_get_contents($pipes[1]);
		$stderr .= stream_get_contents($pipes[2]);
		$status = proc_get_status($process);
		if (!$status['running']) {
			$exit_code = $status['exitcode'];
			break;
		}
		if ((microtime(true) - $started) >= $timeout) {
			proc_terminate($process, 15);
			usleep(200000);
			$status = proc_get_status($process);
			if ($status['running']) {
				proc_terminate($process, 9);
			}
			$exit_code = 124;
			$stderr .= "\nCommand timed out.";
			break;
		}
		usleep(20000);
	} while (true);
	$stdout .= stream_get_contents($pipes[1]);
	$stderr .= stream_get_contents($pipes[2]);
	fclose($pipes[1]);
	fclose($pipes[2]);
	$closed_code = proc_close($process);
	if ($exit_code < 0 && $closed_code >= 0) {
		$exit_code = $closed_code;
	}
	return array(
		'code' => $exit_code,
		'stdout' => substr(trim($stdout), 0, 32768),
		'stderr' => substr(trim($stderr), 0, 32768)
	);
}

function kontrol_id_domain_environment($ccache = '') {
	$environment = array(
		'KRB5_CONFIG' => KONTROL_ID_KRB5_CONF,
		'KRB5_KTNAME' => 'FILE:' . KONTROL_ID_KEYTAB
	);
	if ($ccache !== '') {
		$environment['KRB5CCNAME'] = 'FILE:' . $ccache;
	}
	return $environment;
}

function kontrol_id_keytab_check($path = KONTROL_ID_KEYTAB, $require_http_spn = true) {
	if (!is_file($path) || filesize($path) < 1) {
		return array('ok' => false, 'detail' => 'Keytab file is not installed.', 'output' => '');
	}
	$result = kontrol_id_run_command(array('/usr/local/bin/klist', '-k', '-e', $path));
	$output = trim($result['stdout'] . "\n" . $result['stderr']);
	if ($result['code'] !== 0) {
		return array('ok' => false, 'detail' => 'The keytab could not be read by MIT Kerberos.', 'output' => $output);
	}
	$domain = kontrol_id_domain_values();
	$expected = $domain['spn'];
	if ($require_http_spn && ($expected === '' || stripos($output, $expected) === false)) {
		return array('ok' => false, 'detail' => 'The keytab does not contain ' . $expected . '.', 'output' => $output);
	}
	if (preg_match('/\b(?:arcfour|rc4|des(?:3)?)[^\s)]*/i', $output)) {
		return array('ok' => false, 'detail' => 'The keytab contains a legacy Kerberos encryption type. Regenerate it with AES only.', 'output' => $output);
	}
	if ($require_http_spn && stripos($output, 'aes256-') === false && stripos($output, 'aes128-') === false) {
		return array('ok' => false, 'detail' => 'The HTTP service keytab does not contain an AES key.', 'output' => $output);
	}
	return array('ok' => true, 'detail' => 'Kerberos keytab contains the expected HTTP service principal with AES encryption.', 'output' => $output);
}

function kontrol_id_filter_keytab_aes($path) {
	if (!is_file($path) || filesize($path) < 1) {
		throw new Exception('The generated Kerberos keytab is empty.');
	}
	$list = kontrol_id_run_command(array('/usr/local/bin/ktutil'),
		'rkt ' . $path . "\nlist -e\nquit\n", array(), 15);
	$output = trim($list['stdout'] . "\n" . $list['stderr']);
	if ($list['code'] !== 0) {
		throw new Exception('Unable to inspect the generated Kerberos keytab.');
	}
	$weak_slots = array();
	$aes_slots = 0;
	foreach (preg_split('/\R/', $output) as $line) {
		if (!preg_match('/^\s*(\d+)\s+\d+\s+.*\(([^)]+)\)\s*$/', $line, $matches)) {
			continue;
		}
		if (preg_match('/^aes(?:128|256)-/i', $matches[2])) {
			$aes_slots++;
		} else {
			$weak_slots[] = (int)$matches[1];
		}
	}
	if ($aes_slots < 1) {
		throw new Exception('The generated HTTP service keytab does not contain an AES key.');
	}
	if (empty($weak_slots)) {
		return;
	}
	rsort($weak_slots, SORT_NUMERIC);
	$staged = tempnam(KONTROL_ID_DOMAIN_DIR, '.aes-keytab-');
	if ($staged === false) {
		throw new Exception('Unable to stage the AES-only Kerberos keytab.');
	}
	/* MIT ktutil expects a missing destination. tempnam reserves a private,
	 * unpredictable path inside the root-only domain directory first; remove
	 * the zero-byte placeholder immediately before ktutil creates the keytab. */
	@unlink($staged);
	$commands = 'rkt ' . $path . "\n";
	foreach ($weak_slots as $slot) {
		$commands .= 'delent ' . $slot . "\n";
	}
	$commands .= 'wkt ' . $staged . "\nquit\n";
	$filter = kontrol_id_run_command(array('/usr/local/bin/ktutil'), $commands, array(), 15);
	@chown($staged, 'root');
	@chgrp($staged, 'proxy');
	@chmod($staged, 0640);
	$check = kontrol_id_keytab_check($staged, true);
	if ($filter['code'] !== 0 || !$check['ok']) {
		@unlink($staged);
		throw new Exception('Unable to produce a validated AES-only Kerberos keytab.');
	}
	if (!@rename($staged, $path)) {
		@unlink($staged);
		throw new Exception('Unable to install the AES-only Kerberos keytab.');
	}
}

function kontrol_id_import_keytab($uploaded_path) {
	if (!is_uploaded_file($uploaded_path) || !is_file($uploaded_path)) {
		throw new Exception('Select a valid keytab file to upload.');
	}
	$size = filesize($uploaded_path);
	if ($size < 1 || $size > 1048576) {
		throw new Exception('The keytab must be between 1 byte and 1 MiB.');
	}
	$staged = tempnam(KONTROL_ID_DOMAIN_DIR, '.imported-keytab-');
	if ($staged === false || !@copy($uploaded_path, $staged)) {
		if (is_string($staged)) {
			@unlink($staged);
		}
		throw new Exception('Unable to stage the uploaded keytab.');
	}
	@chown($staged, 'root');
	@chgrp($staged, 'proxy');
	@chmod($staged, 0640);
	$check = kontrol_id_keytab_check($staged, true);
	if (!$check['ok']) {
		@unlink($staged);
		throw new Exception($check['detail']);
	}
	if (!@rename($staged, KONTROL_ID_KEYTAB)) {
		@unlink($staged);
		throw new Exception('Unable to install the validated keytab.');
	}
	return $check;
}

function kontrol_id_domain_principal($username, $realm) {
	if (!is_string($username) || !is_string($realm)) {
		return '';
	}
	$username = trim($username);
	$realm = strtoupper(trim($realm));
	if ($username === '' || strlen($username) > 255 ||
		!preg_match('/^[A-Z0-9][A-Z0-9.-]{0,254}$/iD', $realm)) {
		return '';
	}
	/* Accept the formats administrators normally copy from AD tools. A
	 * DOMAIN\\user value is converted to the configured Kerberos realm. */
	if (strpos($username, '\\') !== false) {
		if (substr_count($username, '\\') !== 1) {
			return '';
		}
		list($domain_prefix, $username) = explode('\\', $username, 2);
		if (!preg_match('/^[A-Za-z0-9_.-]{1,63}$/D', $domain_prefix)) {
			return '';
		}
	}
	if (substr_count($username, '@') > 1) {
		return '';
	}
	$account = $username;
	$principal_realm = $realm;
	if (strpos($username, '@') !== false) {
		list($account, $principal_realm) = explode('@', $username, 2);
		/* Kerberos realm names are case-sensitive. Canonicalize the configured
		 * realm so user@example.corp works like user@EXAMPLE.CORP. */
		if (strcasecmp($principal_realm, $realm) === 0) {
			$principal_realm = $realm;
		}
	}
	if (!preg_match('/^[A-Za-z0-9_.-]{1,128}$/D', $account) ||
		!preg_match('/^[A-Z0-9][A-Z0-9.-]{0,254}$/iD', $principal_realm)) {
		return '';
	}
	return $account . '@' . $principal_realm;
}

function kontrol_id_join_domain($username, $password) {
	$domain = kontrol_id_domain_values();
	$principal = kontrol_id_domain_principal($username, $domain['realm'] ?? '');
	if ($principal === '' || !is_string($password) || $password === '' || strlen($password) > 4096) {
		throw new Exception('Enter a valid delegated join account and password.');
	}
	if (!kontrol_id_valid_dns_name($domain['domain']) || !kontrol_id_valid_dns_name($domain['fqdn']) ||
		!kontrol_id_valid_dns_name($domain['dc']) || empty($domain['workgroup'])) {
		throw new Exception('Complete and save the domain settings on the General tab first.');
	}
	kontrol_id_ensure_storage();
	kontrol_id_write_domain_config();
	$ccache = KONTROL_ID_DOMAIN_DIR . '/.join-ccache-' . getmypid();
	$environment = kontrol_id_domain_environment($ccache);
	$keytab_backup = '';
	$keytab_replacement_started = false;
	try {
		$kinit = kontrol_id_run_command(array('/usr/local/bin/kinit', $principal), $password . "\n", $environment, 30);
		$password = str_repeat("\0", strlen($password));
		if ($kinit['code'] !== 0) {
			$detail = trim($kinit['stderr'] ?: $kinit['stdout']);
			throw new Exception('Kerberos authentication failed. Verify the account, password, DNS and clock synchronization.' .
				($detail !== '' ? ' Kerberos reported: ' . $detail : ''));
		}
		$join = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
			'--use-krb5-ccache=FILE:' . $ccache, 'ads', 'join'), '', $environment, 60);
		if ($join['code'] !== 0) {
			throw new Exception('Active Directory join failed: ' . ($join['stderr'] ?: $join['stdout']));
		}
		$http_spn = 'HTTP/' . $domain['fqdn'];
		$spn_pattern = '/(?:^|\R)\s*' . preg_quote($http_spn, '/') . '\s*(?:\R|$)/i';
		$spn_list = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
			'--machine-pass', 'ads', 'setspn', 'list', $domain['netbios']), '', $environment, 30);
		$spn_output = trim($spn_list['stdout'] . "\n" . $spn_list['stderr']);
		if ($spn_list['code'] !== 0 || !preg_match($spn_pattern, $spn_output)) {
			$spn = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
				'--use-krb5-ccache=FILE:' . $ccache, 'ads', 'setspn', 'add', $http_spn),
				'', $environment, 30);
			if ($spn['code'] !== 0) {
				/* A repeated join can race with or rediscover an SPN created by the
				 * same computer. Accept it only after listing this computer account
				 * and proving that the exact expected SPN belongs to it. */
				$spn_list = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
					'--machine-pass', 'ads', 'setspn', 'list', $domain['netbios']), '', $environment, 30);
				$spn_output = trim($spn_list['stdout'] . "\n" . $spn_list['stderr']);
				if ($spn_list['code'] !== 0 || !preg_match($spn_pattern, $spn_output)) {
					throw new Exception('The computer joined, but the HTTP service principal could not be registered: ' .
						($spn['stderr'] ?: $spn['stdout']));
				}
			}
		}
		$enctypes = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
			'--machine-pass', 'ads', 'enctypes', 'set', $domain['netbios'] . '$', '24'), '', $environment, 30);
		if ($enctypes['code'] !== 0) {
			throw new Exception('The computer joined, but its AD encryption policy could not be restricted to AES: ' .
				($enctypes['stderr'] ?: $enctypes['stdout']));
		}
		if (is_file(KONTROL_ID_KEYTAB)) {
			$keytab_backup = KONTROL_ID_KEYTAB . '.prejoin-' . getmypid();
			if (!@rename(KONTROL_ID_KEYTAB, $keytab_backup)) {
				throw new Exception('Unable to stage the previous Kerberos keytab for safe replacement.');
			}
		}
		$keytab_replacement_started = true;
		$keytab = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
			'--machine-pass', 'ads', 'keytab', 'add', 'HTTP/' . $domain['fqdn']), '', $environment, 30);
		if ($keytab['code'] !== 0) {
			throw new Exception('The computer joined, but the HTTP keytab could not be generated: ' .
				($keytab['stderr'] ?: $keytab['stdout']));
		}
		kontrol_id_filter_keytab_aes(KONTROL_ID_KEYTAB);
		@chown(KONTROL_ID_KEYTAB, 'root');
		@chgrp(KONTROL_ID_KEYTAB, 'proxy');
		@chmod(KONTROL_ID_KEYTAB, 0640);
		$check = kontrol_id_keytab_check(KONTROL_ID_KEYTAB, true);
		if (!$check['ok']) {
			throw new Exception($check['detail']);
		}
		if ($keytab_backup !== '') {
			@unlink($keytab_backup);
			$keytab_backup = '';
		}
		return $check;
	} catch (Throwable $exception) {
		if ($keytab_replacement_started) {
			@unlink(KONTROL_ID_KEYTAB);
			if ($keytab_backup !== '' && is_file($keytab_backup)) {
				@rename($keytab_backup, KONTROL_ID_KEYTAB);
			}
		}
		throw $exception;
	} finally {
		kontrol_id_run_command(array('/usr/local/bin/kdestroy', '-c', $ccache), '', $environment, 10);
		@unlink($ccache);
	}
}

function kontrol_id_remove_domain_tree($path) {
	$root = realpath(KONTROL_ID_DOMAIN_DIR);
	$resolved = realpath($path);
	if ($root === false || $resolved === false || $resolved === $root ||
		strpos($resolved, $root . DIRECTORY_SEPARATOR) !== 0) {
		throw new Exception('Refusing to remove domain state outside the Kontrol-ID private directory.');
	}
	$iterator = new RecursiveIteratorIterator(
		new RecursiveDirectoryIterator($resolved, FilesystemIterator::SKIP_DOTS),
		RecursiveIteratorIterator::CHILD_FIRST
	);
	foreach ($iterator as $entry) {
		if ($entry->isLink() || $entry->isFile()) {
			@unlink($entry->getPathname());
		} else {
			@rmdir($entry->getPathname());
		}
	}
	if (!@rmdir($resolved) && is_dir($resolved)) {
		throw new Exception('Unable to remove the local Samba machine state.');
	}
}

function kontrol_id_disable_before_unjoin() {
	$settings = kontrol_id_get_settings();
	if (($settings['enable'] ?? '') === 'on') {
		$settings['enable'] = '';
		kontrol_id_set_settings($settings);
	}
	/* Restore the previously managed Squid/E2Guardian configuration and stop
	 * Winbind before removing credentials needed by explicit authentication. */
	kontrol_id_resync(false);
}

function kontrol_id_clear_local_domain_state() {
	kontrol_id_manage_winbind(false);
	@unlink(KONTROL_ID_WINBIND_MARKER);
	@unlink(KONTROL_ID_KEYTAB);
	foreach ((array)glob(KONTROL_ID_DOMAIN_DIR . '/.*-ccache-*') as $cache) {
		if (is_file($cache)) {
			@unlink($cache);
		}
	}
	if (is_dir(KONTROL_ID_SAMBA_DIR)) {
		kontrol_id_remove_domain_tree(KONTROL_ID_SAMBA_DIR);
	}
	kontrol_id_ensure_storage();
	kontrol_id_write_domain_config();
}

function kontrol_id_unjoin_domain($username, $password) {
	$domain = kontrol_id_domain_values();
	$principal = kontrol_id_domain_principal($username, $domain['realm'] ?? '');
	if ($principal === '' || !is_string($password) || $password === '' || strlen($password) > 4096) {
		throw new Exception('Enter a valid delegated domain account and password.');
	}
	if (!kontrol_id_valid_dns_name($domain['domain']) || !kontrol_id_valid_dns_name($domain['dc']) ||
		empty($domain['realm'])) {
		throw new Exception('Complete and save the domain settings before removing membership.');
	}
	$ccache = KONTROL_ID_DOMAIN_DIR . '/.unjoin-ccache-' . getmypid();
	$environment = kontrol_id_domain_environment($ccache);
	try {
		$kinit = kontrol_id_run_command(array('/usr/local/bin/kinit', $principal), $password . "\n", $environment, 30);
		$password = str_repeat("\0", strlen($password));
		if ($kinit['code'] !== 0) {
			$detail = trim($kinit['stderr'] ?: $kinit['stdout']);
			throw new Exception('Kerberos authentication failed. The domain membership was not removed.' .
				($detail !== '' ? ' Kerberos reported: ' . $detail : ''));
		}
		kontrol_id_disable_before_unjoin();
		$leave = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
			'--use-krb5-ccache=FILE:' . $ccache, 'ads', 'leave'), '', $environment, 60);
		if ($leave['code'] !== 0) {
			throw new Exception('Active Directory unjoin failed; local trust data was preserved: ' .
				($leave['stderr'] ?: $leave['stdout']));
		}
		kontrol_id_clear_local_domain_state();
		return trim($leave['stdout'] . "\n" . $leave['stderr']);
	} finally {
		kontrol_id_run_command(array('/usr/local/bin/kdestroy', '-c', $ccache), '', $environment, 10);
		@unlink($ccache);
	}
}

function kontrol_id_domain_status() {
	$checks = array();
	$settings = kontrol_id_get_settings();
	$domain = kontrol_id_domain_values($settings);
	$checks[] = array('name' => 'Managed configuration',
		'ok' => is_file(KONTROL_ID_KRB5_CONF) && is_file(KONTROL_ID_SMB_CONF),
		'detail' => 'Package-owned krb5.conf and smb4.conf');
	$keytab = kontrol_id_keytab_check(KONTROL_ID_KEYTAB, kontrol_id_kerberos_required($settings));
	$checks[] = array('name' => 'Kerberos keytab', 'ok' => $keytab['ok'], 'detail' => $keytab['detail']);
	if (is_file(KONTROL_ID_SMB_CONF) && is_executable('/usr/local/bin/net')) {
		$join = kontrol_id_run_command(array('/usr/local/bin/net', '--configfile=' . KONTROL_ID_SMB_CONF,
			'ads', 'testjoin'), '', kontrol_id_domain_environment(), 20);
		$checks[] = array('name' => 'Active Directory machine trust', 'ok' => $join['code'] === 0,
			'detail' => $join['code'] === 0 ? 'Machine trust is valid for ' . $domain['realm'] :
				(trim($join['stderr'] ?: $join['stdout']) ?: 'Not joined or the trust cannot be verified.'));
	}
	if (kontrol_id_ntlm_required($settings) && is_executable('/usr/local/bin/wbinfo')) {
		$trust = kontrol_id_run_command(array('/usr/local/bin/wbinfo', '-t'),
			'', kontrol_id_domain_environment(), 20);
		$checks[] = array('name' => 'Winbind secure channel', 'ok' => $trust['code'] === 0,
			'detail' => $trust['code'] === 0 ? 'The NTLM domain trust secret is valid.' :
				(trim($trust['stderr'] ?: $trust['stdout']) ?: 'Winbind trust test failed.'));
	}
	return array('checks' => $checks, 'keytab' => $keytab, 'domain' => $domain);
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
	kontrol_id_save_pki_backup($ca, $client);
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

function kontrol_id_normalize_interfaces($value) {
	if (is_array($value)) {
		$interfaces = $value;
	} else {
		$interfaces = explode(',', (string)$value);
	}
	$interfaces = array_values(array_unique(array_filter(array_map('trim', $interfaces))));
	return implode(',', $interfaces);
}

function kontrol_id_snapshot_config(array $config, array $fields) {
	$snapshot = array();
	foreach ($fields as $field) {
		$snapshot[$field] = array(
			'exists' => array_key_exists($field, $config),
			'value' => $config[$field] ?? null
		);
	}
	return base64_encode(json_encode($snapshot));
}

function kontrol_id_restore_config(array $config, $encoded_snapshot) {
	$decoded = base64_decode((string)$encoded_snapshot, true);
	if ($decoded === false) {
		return $config;
	}
	$snapshot = json_decode($decoded, true);
	if (!is_array($snapshot)) {
		return $config;
	}
	foreach ($snapshot as $field => $saved) {
		if (!is_array($saved) || empty($saved['exists'])) {
			unset($config[$field]);
		} else {
			$config[$field] = $saved['value'];
		}
	}
	return $config;
}

function kontrol_id_squid_managed_fields() {
	return array(
		'enable_squid', 'active_interface', 'proxy_port', 'allow_interface',
		'transparent_proxy', 'transparent_active_interface', 'ssl_proxy',
		'ssl_active_interface', 'ssl_proxy_port', 'sslproxy_mitm_mode',
		'sslproxy_compatibility_mode', 'dhparams_size', 'dca',
		'sslcrtd_children', 'log_sqd'
	);
}

function kontrol_id_e2guardian_managed_fields() {
	return array(
		'enable', 'interface', 'filterports', 'icapport', 'allow_interface', 'applyaction', 'transparent_proxy',
		'transparent_active_interface', 'enablessl', 'ssl_proxy',
		'ssl_proxy_port', 'dca', 'parentmode', 'proxyip', 'proxyport',
		'kontrol_id_tls_bypass_sites'
	);
}

function kontrol_id_squid_block($settings = null) {
	if (!is_array($settings)) {
		$settings = kontrol_id_get_settings();
	}
	$portal = preg_replace('/[\s;]/', '', kontrol_id_portal_url());
	$unknown = preg_replace('/[^A-Za-z0-9_.@-]/', '', $settings['unknown_user']);
	$processes = max(5, min(200, (int)$settings['auth_processes']));
	$lines = array(KONTROL_ID_BEGIN);

	$access_mode = $settings['access_mode'] ?? 'transparent';
	if ($access_mode === 'explicit') {
		$domain = kontrol_id_domain_values($settings);
		$method = $settings['explicit_auth'] ?? 'kerberos';
		if (in_array($method, array('kerberos', 'kerberos_ntlm'), true)) {
			$lines[] = 'auth_param negotiate program /usr/bin/env KRB5_CONFIG=' . KONTROL_ID_KRB5_CONF .
				' KRB5_KTNAME=FILE:' . KONTROL_ID_KEYTAB .
				' /usr/local/libexec/squid/negotiate_kerberos_auth -r -s HTTP/' . $domain['fqdn'];
			$lines[] = 'auth_param negotiate children ' . $processes . ' startup=5 idle=1';
			$lines[] = 'auth_param negotiate keep_alive on';
		}
		if (in_array($method, array('ntlm', 'kerberos_ntlm'), true)) {
			$lines[] = 'auth_param ntlm program /usr/bin/env WINBINDD_SOCKET_DIR=' .
				KONTROL_ID_SAMBA_RUN . '/winbindd /usr/local/bin/ntlm_auth --configfile=' .
				KONTROL_ID_SMB_CONF . ' --helper-protocol=squid-2.5-ntlmssp';
			$lines[] = 'auth_param ntlm children ' . $processes . ' startup=5 idle=1';
			$lines[] = 'auth_param ntlm keep_alive on';
		}
		if ($method === 'basic') {
			$lines[] = 'auth_param basic program /usr/local/libexec/kontrol-basic-auth';
			$lines[] = 'auth_param basic children ' . $processes . ' startup=5 idle=1';
			$lines[] = 'auth_param basic realm Kontrol Internet Access';
			$lines[] = 'auth_param basic credentialsttl 2 hours';
			$lines[] = 'auth_param basic casesensitive off';
		}
		$lines[] = 'acl kontrol_authenticated proxy_auth REQUIRED';
		/* This denial triggers the appropriate proxy authentication challenge.
		 * Authenticated requests continue into the normal Squid network policy. */
		$lines[] = 'http_access deny !kontrol_authenticated';
	} elseif ($access_mode === 'transparent') {
		$lines[] = 'external_acl_type kontrol_identity children-startup=5 children-max=' . $processes .
			' children-idle=5 ttl=5 negative_ttl=5 cache=65536 %SRC /usr/local/libexec/kontrol-identity-helper -c ' . KONTROL_ID_CONF;
		$lines[] = 'acl kontrol_identity external kontrol_identity';
		$lines[] = 'acl kontrol_identity_unknown ext_user -i ' . $unknown;
		/* Evaluate the helper and fail closed if it is unavailable. A successful
		 * lookup deliberately falls through to the existing Squid ACL policy. */
		$lines[] = 'http_access deny !kontrol_identity';
		if ($settings['unknown_action'] === 'portal') {
			$lines[] = 'deny_info 302:' . $portal . ' kontrol_identity_unknown';
			$lines[] = 'http_access deny kontrol_identity kontrol_identity_unknown';
		} elseif ($settings['unknown_action'] === 'deny') {
			$lines[] = 'http_access deny kontrol_identity kontrol_identity_unknown';
		}
	} else {
		$lines[] = '# Transparent interception without user identification.';
	}
	if ($settings['e2guardian_integration'] === 'on') {
		/* Squid performs the TLS bump. E2Guardian receives the resulting HTTP
		 * request and response over ICAP, including the real client IP and the
		 * username returned by the Kontrol-ID external ACL helper. */
		$lines[] = 'icap_enable on';
		$lines[] = 'adaptation_send_client_ip on';
		$lines[] = 'adaptation_send_username on';
		$lines[] = 'icap_client_username_encode off';
		/* E2Guardian 5.4 consumes Squid's ICAP identity from this header. The
		 * Kerberos helper uses -r above so its value matches filtergroupslist. */
		$lines[] = 'icap_client_username_header X-Client-Username';
		$lines[] = 'icap_preview_enable on';
		$lines[] = 'icap_preview_size 1024';
		$lines[] = 'adaptation_masterx_shared_names X-ICAP-E2G';
		/* CONNECT only establishes the tunnel and often contains a destination
		 * IP. Filter the decrypted HTTP request instead so reports contain the
		 * requested hostname and URL without duplicate CONNECT-only rows. */
		$lines[] = 'acl kontrol_e2g_connect method CONNECT';
		$lines[] = 'icap_service kontrol_e2g_req reqmod_precache icap://127.0.0.1:1344/request bypass=off';
		$lines[] = 'adaptation_access kontrol_e2g_req deny kontrol_e2g_connect';
		$lines[] = 'adaptation_access kontrol_e2g_req allow all';
		$lines[] = 'icap_service kontrol_e2g_resp respmod_precache icap://127.0.0.1:1344/response bypass=off';
		$lines[] = 'adaptation_access kontrol_e2g_resp deny kontrol_e2g_connect';
		$lines[] = 'adaptation_access kontrol_e2g_resp allow all';
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
	if ($with_e2guardian && (
		preg_match('/^\s*cache_peer\s+(127\.0\.0\.1|localhost)\s+parent\s+8080\b/im', $directives) ||
		preg_match('/^\s*icap_service\s+\S+\s+\S+\s+icap:\/\/(127\.0\.0\.1|localhost):1344\/(request|response)\b/im', $directives)
	)) {
		$conflicts[] = 'an existing manual E2Guardian proxy or ICAP chain on loopback';
	}
	return $conflicts;
}

function kontrol_id_apply_squid_integration($enabled) {
	$settings = kontrol_id_get_settings();
	$squid = config_get_path('installedpackages/squid/config/0', array());
	$auth = config_get_path('installedpackages/squidauth/config/0', array());
	$cache = config_get_path('installedpackages/squidcache/config/0', array());
	$antivirus = config_get_path('installedpackages/squidantivirus/config/0', array());
	$remote = config_get_path('installedpackages/squidremote/config', array());
	if (!is_array($squid)) {
		$squid = array();
	}
	if (!is_array($auth)) {
		$auth = array('auth_method' => 'none');
	}
	$cache = is_array($cache) ? $cache : array();
	$antivirus = is_array($antivirus) ? $antivirus : array();
	$remote = is_array($remote) ? $remote : array();

	$squid['custom_options'] = kontrol_id_remove_squid_block($squid['custom_options'] ?? '');
	if ($enabled) {
		if (empty($settings['previous_squid_config'])) {
			$settings['previous_squid_config'] = kontrol_id_snapshot_config(
				$squid,
				kontrol_id_squid_managed_fields()
			);
		}
		if (empty($settings['previous_squid_auth_config'])) {
			$settings['previous_squid_auth_config'] = base64_encode(json_encode($auth));
		}
		if (empty($settings['previous_squid_cache_config'])) {
			$settings['previous_squid_cache_config'] = base64_encode(json_encode($cache));
		}
		if (empty($settings['previous_squid_antivirus_config'])) {
			$settings['previous_squid_antivirus_config'] = base64_encode(json_encode($antivirus));
		}
		if (empty($settings['previous_squid_remote_config'])) {
			$settings['previous_squid_remote_config'] = base64_encode(json_encode($remote));
		}
		/* Kontrol-ID supplies the auth_param directives in its reversible custom
		 * block. Keep the Squid package generator from creating a second stack. */
		$auth['auth_method'] = 'none';
		$cache['nocache'] = 'on';
		$cache['harddisk_cache_system'] = 'null';
		$antivirus['enable'] = '';
		$antivirus['enable_advanced'] = 'disabled';
		foreach ($remote as &$peer) {
			if (is_array($peer)) {
				$peer['enable'] = '';
			}
		}
		unset($peer);
		$conflicts = kontrol_id_squid_conflicts(
			$squid['custom_options'],
			$settings['e2guardian_integration'] === 'on'
		);
		if (!empty($conflicts)) {
			throw new Exception('Squid contains ' . implode(' and ', $conflicts) . '. Remove it before enabling managed integration.');
		}
		$squid['custom_options'] = trim($squid['custom_options'], ';') .
			(empty($squid['custom_options']) ? '' : ';') . kontrol_id_squid_block();

		$interfaces = kontrol_id_normalize_interfaces($settings['intercept_interfaces']);
		if (empty($interfaces)) {
			$interfaces = 'lan';
		}
		$squid['enable_squid'] = 'on';
		$squid['active_interface'] = $interfaces;
		$squid['proxy_port'] = !empty($squid['proxy_port']) ? $squid['proxy_port'] : '3128';
		$squid['allow_interface'] = 'on';
		$squid['log_sqd'] = '';
		if (kontrol_id_transparent_mode($settings)) {
			$squid['transparent_proxy'] = 'on';
			$squid['transparent_active_interface'] = $interfaces;
		} else {
			$squid['transparent_proxy'] = '';
			$squid['transparent_active_interface'] = '';
		}
		if ($settings['traffic_mode'] === 'http_https') {
			$squid['ssl_proxy'] = 'on';
			$squid['ssl_active_interface'] = $interfaces;
			$squid['ssl_proxy_port'] = '3129';
			$squid['sslproxy_mitm_mode'] = 'splicewhitelist';
			$squid['sslproxy_compatibility_mode'] = 'modern';
			$squid['dhparams_size'] = '2048';
			$squid['dca'] = $settings['ca_ref'];
			$squid['sslcrtd_children'] = '5';
		} else {
			$squid['ssl_proxy'] = '';
			$squid['ssl_active_interface'] = '';
		}
	}
	if (!$enabled && !empty($settings['previous_squid_config'])) {
		$squid = kontrol_id_restore_config($squid, $settings['previous_squid_config']);
		$settings['previous_squid_config'] = '';
	}
	if (!$enabled && !empty($settings['previous_squid_auth_config'])) {
		$decoded_auth = json_decode(base64_decode($settings['previous_squid_auth_config'], true), true);
		if (is_array($decoded_auth)) {
			$auth = $decoded_auth;
		}
		$settings['previous_squid_auth_config'] = '';
	}
	foreach (array(
		'previous_squid_cache_config' => 'cache',
		'previous_squid_antivirus_config' => 'antivirus',
		'previous_squid_remote_config' => 'remote'
	) as $snapshot_key => $target) {
		if (!$enabled && !empty($settings[$snapshot_key])) {
			$encoded = base64_decode($settings[$snapshot_key], true);
			$decoded = ($encoded === false) ? null : json_decode($encoded, true);
			if (is_array($decoded)) {
				${$target} = $decoded;
			}
			$settings[$snapshot_key] = '';
		}
	}
	$settings['previous_squid_auth_method'] = '';

	config_set_path('installedpackages/squid/config/0', $squid);
	config_set_path('installedpackages/squidauth/config/0', $auth);
	config_set_path('installedpackages/squidcache/config/0', $cache);
	config_set_path('installedpackages/squidantivirus/config/0', $antivirus);
	config_set_path('installedpackages/squidremote/config', $remote);
	kontrol_id_set_settings($settings);
}

function kontrol_id_apply_e2guardian_integration($enabled) {
	$settings = kontrol_id_get_settings();
	$e2g = config_get_path('installedpackages/e2guardianconfig/config/0', array());
	if (!is_array($e2g)) {
		$e2g = array();
	}
	$plugins = array_filter(array_map('trim', explode(',', $e2g['auth_plugin'] ?? '')));
	/* Versions through 0.3 used proxy-basic with E2Guardian as a parent
	 * proxy. ICAP carries identity natively. Preserve the administrator's
	 * complete plug-in selection, but suppress proxy-basic while the managed
	 * ICAP chain is active. */
	if ($enabled) {
		if (($settings['e2guardian_auth_snapshot_taken'] ?? '') !== 'on') {
			$settings['previous_e2guardian_auth_plugin'] = base64_encode($e2g['auth_plugin'] ?? '');
			$settings['e2guardian_auth_snapshot_taken'] = 'on';
		}
		$plugins = array_values(array_diff($plugins, array('proxy-basic.conf')));
		$settings['e2g_proxy_basic_added'] = '';
		$e2g['auth_plugin'] = implode(',', array_unique($plugins));
	} elseif (($settings['e2guardian_auth_snapshot_taken'] ?? '') === 'on') {
		$restored_plugins = base64_decode((string)$settings['previous_e2guardian_auth_plugin'], true);
		$e2g['auth_plugin'] = ($restored_plugins === false) ? '' : $restored_plugins;
		$settings['previous_e2guardian_auth_plugin'] = '';
		$settings['e2guardian_auth_snapshot_taken'] = '';
	}

	$e2g_general = config_get_path('installedpackages/e2guardian/config/0', array());
	$filter_groups = config_get_path('installedpackages/e2guardiangroups/config', array());
	if (!is_array($e2g_general)) {
		$e2g_general = array();
	}
	if (!is_array($filter_groups)) {
		$filter_groups = array();
	}
	if ($enabled) {
		if (empty($settings['previous_e2guardian_config'])) {
			$settings['previous_e2guardian_config'] = kontrol_id_snapshot_config(
				$e2g_general,
				kontrol_id_e2guardian_managed_fields()
			);
		}
		/* Squid is the only TLS interception point.  E2Guardian receives the
		 * already decrypted and identified HTTP stream on loopback. */
		$e2g_general['enable'] = 'on';
		$e2g_general['interface'] = 'lo0';
		$e2g_general['filterports'] = '8080';
		$e2g_general['icapport'] = '1344';
		$e2g_general['allow_interface'] = 'on';
		/* Policy/list changes are applied without dropping the native ICAP
		 * listener and every active Squid connection.  The E2Guardian package
		 * still probes both ICAP methods and refreshes Squid after the reload. */
		$e2g_general['applyaction'] = '-r';
		$e2g_general['transparent_proxy'] = '';
		$e2g_general['transparent_active_interface'] = '';
		$e2g_general['enablessl'] = '';
		$e2g_general['ssl_proxy'] = '';
		$e2g_general['parentmode'] = 'direct';
		$e2g_general['proxyip'] = '';
		$e2g_general['proxyport'] = '';

		/* Squid already owns TLS interception.  Preserve which groups had the
		 * legacy E2Guardian MITM option so disabling the managed profile can
		 * restore the administrator's prior policy exactly. */
		if (!array_key_exists('previous_e2guardian_sslmitm_groups', $settings) ||
			$settings['previous_e2guardian_sslmitm_groups'] === '') {
			$sslmitm_groups = array();
			foreach ($filter_groups as $filter_group) {
				$options = preg_split('/\s*,\s*/', (string)($filter_group['group_options'] ?? ''), -1, PREG_SPLIT_NO_EMPTY);
				if (in_array('sslmitm', $options, true) && !empty($filter_group['name'])) {
					$sslmitm_groups[] = (string)$filter_group['name'];
				}
			}
			$settings['previous_e2guardian_sslmitm_groups'] = base64_encode(json_encode($sslmitm_groups));
		}
		foreach ($filter_groups as $index => $filter_group) {
			$options = preg_split('/\s*,\s*/', (string)($filter_group['group_options'] ?? ''), -1, PREG_SPLIT_NO_EMPTY);
			$options = array_values(array_diff($options, array('sslmitm')));
			$filter_group['group_options'] = implode(',', $options);
			$filter_group['lgroupoptions'] = implode(' ', $options);
			$filter_groups[$index] = $filter_group;
		}
	} elseif (!empty($settings['previous_e2guardian_config'])) {
		$e2g_general = kontrol_id_restore_config(
			$e2g_general,
			$settings['previous_e2guardian_config']
		);
		$settings['previous_e2guardian_config'] = '';

		$sslmitm_groups = json_decode(base64_decode(
			(string)($settings['previous_e2guardian_sslmitm_groups'] ?? ''), true
		), true);
		if (!is_array($sslmitm_groups)) {
			$sslmitm_groups = array();
		}
		foreach ($filter_groups as $index => $filter_group) {
			if (!in_array((string)($filter_group['name'] ?? ''), $sslmitm_groups, true)) {
				continue;
			}
			$options = preg_split('/\s*,\s*/', (string)($filter_group['group_options'] ?? ''), -1, PREG_SPLIT_NO_EMPTY);
			$options[] = 'sslmitm';
			$filter_group['group_options'] = implode(',', array_unique($options));
			$filter_group['lgroupoptions'] = implode(' ', array_unique($options));
			$filter_groups[$index] = $filter_group;
		}
		$settings['previous_e2guardian_sslmitm_groups'] = '';
	}
	config_set_path('installedpackages/e2guardianconfig/config/0', $e2g);
	config_set_path('installedpackages/e2guardian/config/0', $e2g_general);
	foreach ($filter_groups as $index => $filter_group) {
		config_set_path("installedpackages/e2guardiangroups/config/{$index}", $filter_group);
	}
	kontrol_id_set_settings($settings);
}

function kontrol_id_apply_e2guardian_storybook($enabled) {
	$path = '/usr/local/etc/e2guardian/site.story';
	if (!file_exists($path)) {
		return;
	}
	$contents = (string)file_get_contents($path);
	$pattern = '/\R?' . preg_quote(KONTROL_ID_E2G_BEGIN, '/') .
		'.*?' . preg_quote(KONTROL_ID_E2G_END, '/') . '\R?/s';
	$contents = preg_replace($pattern, "\n", $contents);
	$contents = rtrim($contents) . "\n";
	if ($enabled) {
		$contents .= "\n" . KONTROL_ID_E2G_BEGIN . "\n" .
			"# Squid has already decrypted HTTPS before submitting it over ICAP.\n" .
			"function(icapsquidbump)\n" .
			"if(true) return true\n" .
			KONTROL_ID_E2G_END . "\n";
	}
	$tmp = $path . '.kontrol-id.tmp';
	file_put_contents($tmp, $contents, LOCK_EX);
	@chown($tmp, 'root');
	@chgrp($tmp, 'wheel');
	@chmod($tmp, 0644);
	rename($tmp, $path);
}

function kontrol_id_quic_bypass_addresses($list, $include_private = false) {
	if (!function_exists('filter_expand_alias')) {
		require_once('filter.inc');
	}

	$addresses = $include_private ?
		array('192.168.0.0/16', '172.16.0.0/12', '10.0.0.0/8') : array();
	foreach (preg_split('/[;\r\n]+/', (string)$list) as $entry) {
		$entry = trim($entry);
		if ($entry === '') {
			continue;
		}

		if (is_alias($entry)) {
			foreach (preg_split('/\s+/', trim((string)filter_expand_alias($entry))) as $alias_entry) {
				if (is_ipaddrv4($alias_entry) || is_subnetv4($alias_entry)) {
					$addresses[] = $alias_entry;
				}
			}
			continue;
		}

		if (is_ipaddrv4($entry) || is_subnetv4($entry)) {
			$addresses[] = $entry;
			continue;
		}

		if (is_hostname($entry)) {
			$records = @dns_get_record($entry, DNS_A);
			if (is_array($records)) {
				foreach ($records as $record) {
					if (!empty($record['ip']) && is_ipaddrv4($record['ip'])) {
						$addresses[] = $record['ip'];
					}
				}
			}
		}
	}

	return array_values(array_unique($addresses));
}

function kontrol_id_update_quic_bypass_aliases($engine, $enabled) {
	$managed_aliases = array(
		KONTROL_ID_QUIC_SOURCE_ALIAS => array(),
		KONTROL_ID_QUIC_DESTINATION_ALIAS => array()
	);
	if ($enabled) {
		$managed_aliases[KONTROL_ID_QUIC_SOURCE_ALIAS] =
			kontrol_id_quic_bypass_addresses($engine['defined_ip_proxy_off'] ?? '');
		$managed_aliases[KONTROL_ID_QUIC_DESTINATION_ALIAS] =
			kontrol_id_quic_bypass_addresses(
				$engine['defined_ip_proxy_off_dest'] ?? '',
				($engine['private_subnet_proxy_off'] ?? '') === 'on'
			);
	}

	$aliases = config_get_path('aliases/alias', array());
	if (!is_array($aliases)) {
		$aliases = array();
	}
	foreach ($aliases as $index => $alias) {
		if (isset($managed_aliases[$alias['name'] ?? ''])) {
			unset($aliases[$index]);
		}
	}
	$aliases = array_values($aliases);

	foreach ($managed_aliases as $name => $addresses) {
		if (empty($addresses)) {
			continue;
		}
		$aliases[] = array(
			'name' => $name,
			'type' => 'network',
			'address' => implode(' ', $addresses),
			'descr' => 'Managed by Kontrol-ID for HTTP/3 / QUIC bypass',
			'detail' => ''
		);
	}
	config_set_path('aliases/alias', $aliases);
}

function kontrol_id_apply_standalone_engine($enabled) {
	$settings = kontrol_id_get_settings();
	$engine = config_get_path('installedpackages/e2guardian/config/0', array());
	$plugins = config_get_path('installedpackages/e2guardianconfig/config/0', array());
	if (!is_array($engine)) {
		$engine = array();
	}
	if (!is_array($plugins)) {
		$plugins = array();
	}

	$engine['enable'] = $enabled ? 'on' : '';
	$engine['enable_dg'] = $enabled ? 'on' : '';
	$engine['interface'] = kontrol_id_normalize_interfaces($settings['intercept_interfaces']);
	$engine['filterports'] = (string)($settings['http_proxy_port'] ?? '3128');
	$engine['parentmode'] = 'direct';
	$engine['proxyip'] = '';
	$engine['proxyport'] = '';
	$engine['icapport'] = '';
	$engine['watchdog'] = '';
	$engine['allow_interface'] = 'on';
	$apply_action = (string)($settings['applyaction'] ?? '-r');
	$engine['applyaction'] = in_array($apply_action, array('-r', 'rc.d', '-Q'), true) ?
		$apply_action : '-r';
	$engine['daemon_options'] = 'softrestart';
	$engine['inpipe'] = $settings['download_limiter'] ?? 'none';
	$engine['outpipe'] = $settings['upload_limiter'] ?? 'none';
	$engine['httpworkers'] = (string)max(64, min(20000,
		(int)($settings['engine_workers'] ?? 256)));
	$engine['enablessl'] = (($settings['traffic_mode'] ?? 'http_only') === 'http_https') ? 'on' : '';
	$engine['kontrol_id_block_quic'] = (($settings['block_quic'] ?? '') === 'on') ? 'on' : '';
	$previous_ca_ref = $engine['dca'] ?? '';
	$engine['dca'] = $settings['ca_ref'] ?? '';
	$engine['ssl_proxy_port'] = (string)($settings['https_intercept_port'] ?? '3129');
	if ($previous_ca_ref !== '' && $engine['dca'] !== '' &&
		$previous_ca_ref !== $engine['dca']) {
		kontrol_id_clear_mitm_certificate_cache();
		log_error('[Kontrol-ID] Cleared the generated MITM certificate cache after changing the inspection CA.');
	}

	$access_mode = $settings['access_mode'] ?? 'transparent';
	if (kontrol_id_transparent_mode($settings)) {
		$engine['transparent_proxy'] = $enabled ? 'on' : '';
		$engine['transparent_active_interface'] = $engine['interface'];
		$engine['private_subnet_proxy_off'] =
			(($settings['bypass_private_destinations'] ?? '') === 'on') ? 'on' : '';
		$engine['defined_ip_proxy_off'] = trim((string)($settings['bypass_sources'] ?? ''));
		$engine['defined_ip_proxy_off_dest'] = kontrol_id_normalize_destination_bypass(
			$settings['bypass_destinations'] ?? '');
		$plugins['auth_plugin'] = ($enabled && $access_mode === 'transparent') ?
			'kontrol-identity.conf' : '';
	} else {
		$engine['transparent_proxy'] = '';
		$engine['transparent_active_interface'] = '';
		$engine['private_subnet_proxy_off'] = '';
		$engine['defined_ip_proxy_off'] = '';
		$engine['defined_ip_proxy_off_dest'] = '';
		$auth_plugins = array(
			'kerberos' => 'kontrol-negotiate.conf',
			'kerberos_ntlm' => 'kontrol-negotiate.conf,kontrol-ntlm.conf',
			'ntlm' => 'kontrol-ntlm.conf',
			'basic' => 'kontrol-basic.conf'
		);
		$auth_method = $settings['explicit_auth'] ?? 'kerberos';
		$plugins['auth_plugin'] = $enabled ?
			($auth_plugins[$auth_method] ?? $auth_plugins['kerberos']) : '';
	}
	kontrol_id_update_quic_bypass_aliases(
		$engine,
		$enabled && kontrol_id_transparent_mode($settings) &&
		($engine['enablessl'] ?? '') === 'on' &&
		($engine['kontrol_id_block_quic'] ?? '') === 'on'
	);
	$invalid_tls_bypass = array();
	$engine['kontrol_id_tls_bypass_sites'] = implode("\n",
		kontrol_id_normalize_tls_bypass_sites(
			$settings['tls_bypass_sites'] ?? '', $invalid_tls_bypass));

	$domain = kontrol_id_domain_values($settings);
	$auth_config = "# Managed by Kontrol-ID.\n" .
		"plugname = 'kontrol-negotiate'\n" .
		"story_function = 'auth_pf_basic'\n" .
		"keytab = '" . KONTROL_ID_KEYTAB . "'\n" .
		"service_principal = '" . ($domain['spn'] ?? '') . "'\n" .
		"allow_ntlm_fallback = '" .
			(($settings['explicit_auth'] ?? '') === 'kerberos_ntlm' ? 'on' : 'off') . "'\n" .
		"defaultfiltergroup = 1\n";
	kontrol_id_atomic_write(KONTROL_ID_ENGINE_AUTH_CONF, $auth_config, 0640, 'wheel');
	$ntlm_config = "# Managed by Kontrol-ID.\n" .
		"plugname = 'kontrol-ntlm'\n" .
		"story_function = 'auth_pf_basic'\n" .
		"helper_program = '/usr/local/bin/ntlm_auth'\n" .
		"samba_config = '" . KONTROL_ID_SMB_CONF . "'\n" .
		"helper_children = 32\n" .
		"helper_timeout_ms = 10000\n" .
		"defaultfiltergroup = 1\n";
	kontrol_id_atomic_write(KONTROL_ID_ENGINE_NTLM_AUTH_CONF, $ntlm_config, 0640, 'wheel');
	$basic_config = "# Managed by Kontrol-ID.\n" .
		"plugname = 'kontrol-basic'\n" .
		"story_function = 'auth_pf_basic'\n" .
		"helper_program = '/usr/local/libexec/kontrol-basic-auth'\n" .
		"helper_children = 8\n" .
		"helper_timeout_ms = 10000\n" .
		"realm = 'Kontrol Internet Access'\n" .
		"defaultfiltergroup = 1\n";
	kontrol_id_atomic_write(KONTROL_ID_ENGINE_BASIC_AUTH_CONF, $basic_config, 0640, 'wheel');
	$identity_helpers = max(8, min(64, (int)ceil(
		max(64, (int)($settings['engine_workers'] ?? 256)) / 16)));
	$identity_timeout = max(2000, min(60000,
		(int)($settings['connect_timeout_ms'] ?? 1200) +
		(int)($settings['request_timeout_ms'] ?? 2500) + 1000));
	$identity_action = in_array(($settings['unknown_action'] ?? ''),
		array('portal', 'deny', 'allow'), true) ?
		$settings['unknown_action'] : 'deny';
	$identity_portal = preg_replace('/[\r\n\'\\\\]/', '', kontrol_id_portal_url());
	$identity_config = "# Managed by Kontrol-ID.\n" .
		"plugname = 'kontrol-identity'\n" .
		"story_function = 'auth_ip'\n" .
		"helper_program = '/usr/local/libexec/kontrol-identity-helper'\n" .
		"helper_config = '" . KONTROL_ID_CONF . "'\n" .
		"helper_children = " . $identity_helpers . "\n" .
		"helper_timeout_ms = " . $identity_timeout . "\n" .
		"ipgroups = '/usr/local/etc/e2guardian/lists/authplugins/ipgroups'\n" .
		"usergroups = '/usr/local/etc/e2guardian/lists/authplugins/filtergroupslist'\n" .
		"unknown_user = '" . ($settings['unknown_user'] ?? 'unknown') . "'\n" .
		"unknown_action = '" . $identity_action . "'\n" .
		"portal_url = '" . $identity_portal . "'\n" .
		"defaulttransparentfiltergroup = 1\n";
	kontrol_id_atomic_write(KONTROL_ID_ENGINE_IDENTITY_CONF,
		$identity_config, 0640, 'wheel');

	config_set_path('installedpackages/e2guardian/config/0', $engine);
	config_set_path('installedpackages/e2guardianconfig/config/0', $plugins);
}

function kontrol_id_clear_mitm_certificate_cache() {
	$directory = '/usr/local/etc/e2guardian/ssl/generatedcerts';
	if (!is_dir($directory)) {
		return;
	}
	$resolved = realpath($directory);
	if ($resolved !== $directory || strpos($resolved, '/usr/local/etc/e2guardian/ssl/') !== 0) {
		throw new Exception('Refusing to clear a certificate cache outside the Kontrol-ID engine directory.');
	}
	$iterator = new RecursiveIteratorIterator(
		new RecursiveDirectoryIterator($resolved, FilesystemIterator::SKIP_DOTS),
		RecursiveIteratorIterator::CHILD_FIRST
	);
	foreach ($iterator as $entry) {
		if ($entry->isLink() || $entry->isFile()) {
			@unlink($entry->getPathname());
		} elseif ($entry->isDir()) {
			@rmdir($entry->getPathname());
		}
	}
}

function kontrol_id_chain_status() {
	$settings = kontrol_id_get_settings();
	$engine = config_get_path('installedpackages/e2guardian/config/0', array());
	$plugins = config_get_path('installedpackages/e2guardianconfig/config/0', array());
	if (!is_array($plugins)) {
		$plugins = array();
	}
	$interfaces = kontrol_id_normalize_interfaces($settings['intercept_interfaces']);
	$access_mode = $settings['access_mode'] ?? 'transparent';
	$explicit = $access_mode === 'explicit';
	$transparent_identity = $access_mode === 'transparent';
	$auth_method = $settings['explicit_auth'] ?? 'kerberos';
	$http_port = (int)($settings['http_proxy_port'] ?? 3128);
	$https_port = (int)($settings['https_intercept_port'] ?? 3129);
	$https_listener_required =
		($settings['traffic_mode'] ?? 'http_only') === 'http_https';
	$auth_profiles = array(
		'kerberos' => array(
			'plugin' => 'kontrol-negotiate.conf',
			'configs' => array(KONTROL_ID_ENGINE_AUTH_CONF => 'kontrol-negotiate'),
			'label' => 'Kerberos / SPNEGO',
			'detail' => 'Native Negotiate authentication',
			'ready' => is_file(KONTROL_ID_KEYTAB)
		),
		'kerberos_ntlm' => array(
			'plugin' => 'kontrol-negotiate.conf,kontrol-ntlm.conf',
			'configs' => array(
				KONTROL_ID_ENGINE_AUTH_CONF => 'kontrol-negotiate',
				KONTROL_ID_ENGINE_NTLM_AUTH_CONF => 'kontrol-ntlm'
			),
			'label' => 'Kerberos / SPNEGO with NTLMv2 fallback',
			'detail' => 'Native Negotiate authentication with NTLMv2 fallback',
			'ready' => is_file(KONTROL_ID_KEYTAB) &&
				is_file(KONTROL_ID_WINBIND_MARKER) && is_file(KONTROL_ID_WINBIND_PID)
		),
		'ntlm' => array(
			'plugin' => 'kontrol-ntlm.conf',
			'configs' => array(KONTROL_ID_ENGINE_NTLM_AUTH_CONF => 'kontrol-ntlm'),
			'label' => 'NTLMv2 / Winbind',
			'detail' => 'Native NTLMv2 authentication',
			'ready' => is_file(KONTROL_ID_WINBIND_MARKER) && is_file(KONTROL_ID_WINBIND_PID)
		),
		'basic' => array(
			'plugin' => 'kontrol-basic.conf',
			'configs' => array(KONTROL_ID_ENGINE_BASIC_AUTH_CONF => 'kontrol-basic'),
			'label' => 'HTTP Basic',
			'detail' => 'Native Basic authentication through the selected Kontrol server',
			'ready' => is_executable('/usr/local/libexec/kontrol-basic-auth')
		)
	);
	$auth_profile = $auth_profiles[$auth_method] ?? $auth_profiles['kerberos'];
	$expected_plugin = $auth_profile['plugin'];
	$auth_config_ok = true;
	foreach ($auth_profile['configs'] as $auth_config_path => $plugname) {
		$auth_config = (string)@file_get_contents($auth_config_path);
		if (strpos($auth_config, "plugname = '" . $plugname . "'") === false) {
			$auth_config_ok = false;
			break;
		}
	}
	$credential_ready = $auth_profile['ready'];
	$transparent_plugin_ok = $transparent_identity ?
		(($plugins['auth_plugin'] ?? '') === 'kontrol-identity.conf' &&
		is_executable('/usr/local/libexec/kontrol-identity-helper') &&
		strpos((string)@file_get_contents(KONTROL_ID_ENGINE_IDENTITY_CONF),
			"plugname = 'kontrol-identity'") !== false) :
		(($plugins['auth_plugin'] ?? '') === '');
	return array(
		array(
			'name' => 'Kontrol-ID engine',
			'ok' => $settings['enable'] === 'on' && is_file(KONTROL_ID_ENGINE_PID),
			'detail' => 'Standalone multithread gateway service'
		),
		array(
			'name' => $explicit ? $auth_profile['label'] :
				($transparent_identity ? 'Transparent identification' :
				'Transparent proxy without identification'),
			'ok' => ($engine['enable'] ?? '') === 'on' &&
				kontrol_id_normalize_interfaces($engine['interface'] ?? '') === $interfaces &&
				($explicit ? ($credential_ready &&
				(($plugins['auth_plugin'] ?? '') === $expected_plugin) &&
				$auth_config_ok) : $transparent_plugin_ok),
			'detail' => ($explicit ?
				$auth_profile['detail'] :
				($transparent_identity ? 'Native identity helper with user and source-IP policy mapping' :
				'Native interception using the default policy group')) .
				' on ' . ($interfaces ?: 'no interface')
		),
		array(
			'name' => 'Worker pool',
			'ok' => (int)($engine['httpworkers'] ?? 0) >= 64,
			'detail' => (int)($engine['httpworkers'] ?? 0) . ' concurrent worker threads configured'
		),
		array(
			'name' => 'Listener ports',
			'ok' => kontrol_id_listener_owned_by_engine($http_port) &&
				(!$https_listener_required || kontrol_id_listener_owned_by_engine($https_port)),
			'detail' => 'HTTP ' . $http_port . ($https_listener_required ?
				'; transparent HTTPS ' . $https_port : '; transparent HTTPS listener not required')
		)
	);
}

function kontrol_id_validate(&$post, &$input_errors) {
	unset($post['_kontrol_id_activation_deferred']);
	$numeric = array(
		'http_proxy_port' => array(1, 65535),
		'https_intercept_port' => array(1, 65535),
		'winrm_port' => array(1, 65535),
		'connect_timeout_ms' => array(200, 30000),
		'request_timeout_ms' => array(500, 60000),
		'positive_ttl' => array(5, 86400),
		'negative_ttl' => array(1, 3600),
		'stale_ttl' => array(0, 86400),
		'portal_ttl' => array(60, 604800),
		'engine_workers' => array(64, 20000)
	);
	foreach ($numeric as $field => $range) {
		if (!isset($post[$field]) || !is_numeric($post[$field]) ||
			(int)$post[$field] < $range[0] || (int)$post[$field] > $range[1]) {
			$input_errors[] = sprintf('%s must be between %d and %d.', $field, $range[0], $range[1]);
		}
	}
	$http_port = (int)($post['http_proxy_port'] ?? 0);
	$https_port = (int)($post['https_intercept_port'] ?? 0);
	if ($http_port > 0 && $http_port === $https_port) {
		$input_errors[] = 'The HTTP proxy and transparent HTTPS listeners must use different ports.';
	}

	$webgui_protocol = config_get_path('system/webgui/protocol', 'https');
	$webgui_port = (int)config_get_path('system/webgui/port',
		$webgui_protocol === 'http' ? 80 : 443);
	foreach (array(
		'HTTP proxy listener' => $http_port,
		'HTTPS inspection listener' => $https_port
	) as $label => $port) {
		if ($port < 1 || $port > 65535) {
			continue;
		}
		if ($port === $webgui_port) {
			$input_errors[] = $label . ' conflicts with the Kontrol WebGUI port ' . $webgui_port . '.';
		}
		$conflicting_owners = array_diff(
			kontrol_id_listener_owners($port),
			array('kontrol-id', 'e2guardian')
		);
		if (!empty($conflicting_owners)) {
			$input_errors[] = $label . ' port ' . $port . ' is already in use by ' .
				implode(', ', $conflicting_owners) . '.';
		}
	}
	if (!preg_match('/^[A-Za-z0-9_.@-]+$/', $post['unknown_user'] ?? '')) {
		$input_errors[] = 'Unknown user contains invalid characters.';
	}
	if (!in_array($post['access_mode'] ?? '', array(
		'transparent', 'transparent_no_identity', 'explicit'
	), true)) {
		$input_errors[] = 'Select a valid client access mode.';
	}
	if (!in_array($post['explicit_auth'] ?? '', array('kerberos', 'kerberos_ntlm', 'ntlm', 'basic'), true)) {
		$input_errors[] = 'Select a valid explicit authentication method.';
	}
	if (!in_array($post['traffic_mode'] ?? '', array('http_only', 'http_https'), true)) {
		$input_errors[] = 'Select a valid TLS inspection mode.';
	}
	if (!in_array($post['applyaction'] ?? '', array('-r', 'rc.d', '-Q'), true)) {
		$input_errors[] = 'Select a valid policy apply action.';
	}
	$selected_ca = kontrol_id_extract_item(lookup_ca($post['ca_ref'] ?? ''));
	if (!kontrol_id_ca_is_usable($selected_ca)) {
		$input_errors[] = 'Select a valid certificate authority that includes its private key, is currently valid and is permitted to sign certificates.';
	}
	if (!in_array($post['unknown_action'] ?? '', array('portal', 'deny', 'allow'), true)) {
		$input_errors[] = 'Select a valid unknown-user action.';
	}
	if (!isset(kontrol_id_login_template_catalog()[$post['login_template'] ?? ''])) {
		$input_errors[] = 'Select a valid login page template.';
	}
	if (strlen((string)($post['admin_contact'] ?? '')) > 8192) {
		$input_errors[] = 'Administrator Message must not exceed 8192 characters.';
	}
	foreach (array('bypass_sources', 'bypass_destinations') as $field) {
		$value = (string)($post[$field] ?? '');
		if (strlen($value) > 4096 || preg_match('/[\r\n]/', $value)) {
			$input_errors[] = $field . ' must be a single line no longer than 4096 characters.';
		}
	}
	if (strlen((string)($post['tls_bypass_sites'] ?? '')) > 16384) {
		$input_errors[] = 'TLS inspection bypass sites must be no longer than 16384 characters.';
	} else {
		$invalid_tls_bypass = array();
		kontrol_id_normalize_tls_bypass_sites(
			$post['tls_bypass_sites'] ?? '', $invalid_tls_bypass);
		if (!empty($invalid_tls_bypass)) {
			$input_errors[] = 'Invalid TLS inspection bypass entr' .
				(count($invalid_tls_bypass) === 1 ? 'y: ' : 'ies: ') .
				implode(', ', $invalid_tls_bypass) . '.';
		}
	}
	$valid_limiters = array('none', '');
	foreach (config_get_path('dnshaper/queue', array()) as $limiter) {
		if (is_array($limiter) && isset($limiter['number'])) {
			$valid_limiters[] = (string)$limiter['number'];
		}
	}
	foreach (array('download_limiter', 'upload_limiter') as $field) {
		if (!in_array((string)($post[$field] ?? 'none'), $valid_limiters, true)) {
			$input_errors[] = 'Select a valid traffic limiter for ' . $field . '.';
		}
	}
	if (!empty($post['portal_url']) && filter_var($post['portal_url'], FILTER_VALIDATE_URL) === false) {
		$input_errors[] = 'Portal URL is invalid.';
	}
	$explicit_domain_auth = ($post['access_mode'] ?? '') === 'explicit' &&
		in_array($post['explicit_auth'] ?? '', array('kerberos', 'kerberos_ntlm', 'ntlm'), true);
	$explicit_basic_auth = ($post['access_mode'] ?? '') === 'explicit' &&
		($post['explicit_auth'] ?? '') === 'basic';
	if ($explicit_basic_auth) {
		$authcfg = auth_get_authserver($post['auth_server'] ?? '');
		if (!is_array($authcfg)) {
			$input_errors[] = 'Select a valid authentication server for HTTP Basic.';
		} elseif (($authcfg['type'] ?? '') === 'ldap') {
			if (empty(array_filter(array_map('trim', explode(',', $post['ad_groups'] ?? ''))))) {
				$input_errors[] = 'Configure at least one allowed AD/LDAP group for HTTP Basic.';
			}
		} elseif (trim($post['local_group'] ?? '') === '') {
			$input_errors[] = 'Configure the local allowed group for HTTP Basic.';
		}
	}
	if ($explicit_domain_auth) {
		foreach (array(
			'proxy_fqdn' => 'Proxy FQDN',
			'ad_domain' => 'Active Directory domain',
			'ad_dc' => 'Preferred domain controller'
		) as $field => $label) {
			if (!kontrol_id_valid_dns_name(trim($post[$field] ?? ''))) {
				$input_errors[] = $label . ' must be a fully qualified DNS name.';
			}
		}
		if (!preg_match('/^[A-Za-z0-9][A-Za-z0-9-]{0,14}$/D', trim($post['ad_workgroup'] ?? ''))) {
			$input_errors[] = 'The Active Directory NetBIOS name must contain 1 to 15 letters, digits or hyphens.';
		}
	}
	if (($post['enable'] ?? '') === 'on') {
		$activation_prerequisites = array();
		if (empty(kontrol_id_normalize_interfaces($post['intercept_interfaces'] ?? ''))) {
			$input_errors[] = 'Select at least one client interface.';
		}
		if ($explicit_domain_auth &&
			in_array($post['explicit_auth'], array('kerberos', 'kerberos_ntlm'), true) &&
			(!is_file(KONTROL_ID_KEYTAB) || filesize(KONTROL_ID_KEYTAB) < 1)) {
			$activation_prerequisites[] = 'Import or generate the Kerberos keytab on Domain and Keytab.';
		}
		if ($explicit_domain_auth &&
			in_array($post['explicit_auth'] ?? '', array('kerberos_ntlm', 'ntlm'), true) &&
			(!is_file(KONTROL_ID_SAMBA_PRIVATE . '/secrets.tdb') ||
			!is_executable('/usr/local/bin/ntlm_auth') ||
			!is_executable('/usr/local/sbin/winbindd'))) {
			$activation_prerequisites[] = 'Join Active Directory on Domain and Keytab before enabling NTLM.';
		}
		if ($explicit_basic_auth && !is_executable('/usr/local/libexec/kontrol-basic-auth')) {
			$activation_prerequisites[] = 'Install the Kontrol-ID Basic authentication helper before enabling HTTP Basic.';
		}
		if (!empty($activation_prerequisites)) {
			/* Initial domain settings must be saved before the Domain and Keytab
			 * page can generate its package-owned Kerberos/Samba configuration.
			 * Persist the valid settings, but never activate an incomplete stack. */
			$post['enable'] = '';
			$post['_kontrol_id_activation_deferred'] = implode(' ', $activation_prerequisites);
		}
		if (auth_get_authserver($post['auth_server'] ?? 'Local Database') === null) {
			$input_errors[] = 'The selected Kontrol authentication server does not exist.';
		}
	}
}

function kontrol_id_manage_winbind($enabled) {
	$script = '/usr/local/etc/rc.d/kontrol_id_winbind';
	if (!is_executable($script)) {
		return !$enabled;
	}
	if ($enabled && is_file(KONTROL_ID_WINBIND_MARKER) &&
		is_file(KONTROL_ID_SAMBA_PRIVATE . '/secrets.tdb')) {
		mwexec($script . ' forcestop');
		if (mwexec($script . ' start') !== 0) {
			return false;
		}
		for ($attempt = 0; $attempt < 20; $attempt++) {
			$pid = (int)trim((string)@file_get_contents(KONTROL_ID_WINBIND_PID));
			if ($pid > 1 && function_exists('posix_kill') && @posix_kill($pid, 0)) {
				return true;
			}
			usleep(100000);
		}
		return false;
	} else {
		mwexec($script . ' forcestop');
		return true;
	}
}

function kontrol_id_resync($sync = true, $force_engine_restart = false) {
	global $savemsg;

	$changed = kontrol_id_ensure_defaults();
	$changed = kontrol_id_prepare_dependency_defaults() || $changed;
	$changed = kontrol_id_ensure_group() || $changed;
	$changed = kontrol_id_ensure_pki() || $changed;
	$portal_certificate = kontrol_id_ensure_portal_certificate();
	$changed = !empty($portal_certificate['changed']) || $changed;
	$changed = kontrol_id_remove_legacy_e2guardian_menu() || $changed;
	$changed = kontrol_id_remove_legacy_e2guardian_service() || $changed;
	kontrol_id_ensure_storage();
	kontrol_id_write_pki();
	kontrol_id_write_helper_config();
	kontrol_id_write_domain_config();
	kontrol_id_policy_schedule();
	$settings = kontrol_id_get_settings();
	$previous_engine = config_get_path('installedpackages/e2guardian/config/0', array());
	if (!is_array($previous_engine)) {
		$previous_engine = array();
	}
	$previous_plugins = config_get_path('installedpackages/e2guardianconfig/config/0', array());
	if (!is_array($previous_plugins)) {
		$previous_plugins = array();
	}
	$previous_identity_config = is_file(KONTROL_ID_ENGINE_IDENTITY_CONF) ?
		(string)@file_get_contents(KONTROL_ID_ENGINE_IDENTITY_CONF) : '';
	$enabled = ($settings['enable'] === 'on');
	$runtime_ready = true;
	if ($enabled && kontrol_id_kerberos_required($settings) &&
		(!is_file(KONTROL_ID_KEYTAB) || filesize(KONTROL_ID_KEYTAB) < 1)) {
		$runtime_ready = false;
		log_error('[Kontrol-ID] Managed proxy was not activated: the local Kerberos keytab is missing.');
	}
	if ($enabled && ($settings['access_mode'] ?? '') === 'explicit' &&
		($settings['explicit_auth'] ?? '') === 'basic' &&
		!is_executable('/usr/local/libexec/kontrol-basic-auth')) {
		$runtime_ready = false;
		log_error('[Kontrol-ID] Managed proxy was not activated: the Basic authentication helper is missing.');
	}
	$stack_enabled = $enabled && $runtime_ready;
	if (!kontrol_id_manage_winbind($stack_enabled && kontrol_id_ntlm_required($settings))) {
		$stack_enabled = false;
		log_error('[Kontrol-ID] Managed proxy was not activated: Winbind failed to start.');
	}
	kontrol_id_apply_standalone_engine($stack_enabled);
	$current_plugins = config_get_path('installedpackages/e2guardianconfig/config/0', array());
	if (!is_array($current_plugins)) {
		$current_plugins = array();
	}
	$current_identity_config = is_file(KONTROL_ID_ENGINE_IDENTITY_CONF) ?
		(string)@file_get_contents(KONTROL_ID_ENGINE_IDENTITY_CONF) : '';
	$structural_restart =
		(($previous_plugins['auth_plugin'] ?? '') !== ($current_plugins['auth_plugin'] ?? '')) ||
		($previous_identity_config !== $current_identity_config);
	write_config('Kontrol-ID configuration updated.');
	/* Package generators still read the legacy global configuration array.
	 * Reload it so their runtime files and firewall rules see the values just
	 * written through config_set_path(). */
	if (function_exists('config_read_file')) {
		config_read_file(true, false);
	}

	if (file_exists('/usr/local/pkg/e2guardian.inc')) {
		require_once('/usr/local/pkg/e2guardian.inc');
		if (function_exists('sync_package_e2guardian')) {
			$had_csrf = isset($_POST['__csrf_magic']);
			$previous_csrf = $_POST['__csrf_magic'] ?? null;
			$had_managed_resync = isset($GLOBALS['kontrol_id_managed_resync']);
			$previous_managed_resync = $GLOBALS['kontrol_id_managed_resync'] ?? null;
			if (!$had_csrf || empty($previous_csrf)) {
				$_POST['__csrf_magic'] = 'kontrol-id-managed-resync';
			}
			$GLOBALS['kontrol_id_managed_resync'] = true;
			try {
				sync_package_e2guardian('no', false);
			} finally {
				if ($had_managed_resync) {
					$GLOBALS['kontrol_id_managed_resync'] = $previous_managed_resync;
				} else {
					unset($GLOBALS['kontrol_id_managed_resync']);
				}
			}
			if ($had_csrf) {
				$_POST['__csrf_magic'] = $previous_csrf;
			} else {
				unset($_POST['__csrf_magic']);
			}
		}
		$engine_start_ok = true;
		if (function_exists('e2guardian_start')) {
			$engine_start_ok = (e2guardian_start('no', false, true) !== false);
			/* USR1 reloads policy data but cannot replace an instantiated auth
			 * plugin, its structural configuration or upgraded executable text.
			 * Force one controlled restart for package upgrades, access-mode
			 * changes and identity fallback changes. */
			$apply_action_restarts = in_array(($settings['applyaction'] ?? '-r'),
				array('rc.d', '-Q'), true);
			if ($stack_enabled && ($force_engine_restart || $structural_restart) &&
				!$apply_action_restarts) {
				$engine_start_ok =
					(mwexec('/usr/local/etc/rc.d/kontrol_id restart') === 0);
			}
			if (function_exists('e2guardian_wait_for_process_state')) {
				$engine_start_ok = e2guardian_wait_for_process_state($stack_enabled, 10) &&
					$engine_start_ok;
			}
		}
		$listener_ok = true;
		if ($stack_enabled) {
			$listener_ok = kontrol_id_wait_for_listener(
				(int)($settings['http_proxy_port'] ?? 3128), 20);
			if ($listener_ok &&
				($settings['traffic_mode'] ?? 'http_only') === 'http_https') {
				$listener_ok = kontrol_id_wait_for_listener(
					(int)($settings['https_intercept_port'] ?? 3129), 20);
			}
		}

		if ($stack_enabled && (!$engine_start_ok || !$listener_ok)) {
			$failed_http = (int)($settings['http_proxy_port'] ?? 3128);
			$failed_https = (int)($settings['https_intercept_port'] ?? 3129);
			$settings['http_proxy_port'] = (string)($previous_engine['filterports'] ?? '3128');
			$settings['https_intercept_port'] = (string)($previous_engine['ssl_proxy_port'] ?? '3129');
			kontrol_id_set_settings($settings);
			config_set_path('installedpackages/e2guardian/config/0', $previous_engine);
			write_config('Kontrol-ID listener change rolled back after the engine failed to bind.');
			if (function_exists('config_read_file')) {
				config_read_file(true, false);
			}
			$GLOBALS['kontrol_id_managed_resync'] = true;
			try {
				sync_package_e2guardian('no', false);
			} finally {
				unset($GLOBALS['kontrol_id_managed_resync']);
			}
			e2guardian_start('no', false, true);
			if (function_exists('filter_configure')) {
				filter_configure();
			}
			log_error('[Kontrol-ID] Listener update to HTTP ' . $failed_http .
				' / HTTPS ' . $failed_https . ' failed and was rolled back.');
			$savemsg = 'Kontrol-ID could not bind the requested listener ports. The previous ports were restored.';
			return false;
		}
		/* A mode change alters the transparent rdr rules as well as the engine
		 * configuration. Some package execution contexts restart the service
		 * without reloading PF, leaving interception enabled after returning to
		 * explicit mode. Always converge the live rules after the final config. */
		if (function_exists('filter_configure')) {
			filter_configure();
		}
	}
	if ($sync) {
		kontrol_id_sync_on_changes();
	}
	if (!empty($portal_certificate['restart_webgui'])) {
		log_error('[Kontrol-ID] Restarting the WebGUI to activate the portal certificate.');
		send_event('service restart webgui');
	}
}

function kontrol_id_install() {
	try {
		kontrol_id_ensure_defaults();
		kontrol_id_prepare_dependency_defaults();
		kontrol_id_ensure_group();
		kontrol_id_ensure_pki();
		write_config('Kontrol-ID installed with safe defaults.');
		kontrol_id_resync(false, true);
	} catch (Exception $e) {
		log_error('[Kontrol-ID] Installation failed: ' . $e->getMessage());
	}
}

function kontrol_id_deinstall() {
	try {
		$settings = kontrol_id_get_settings();
		$settings['enable'] = '';
		kontrol_id_set_settings($settings);
		kontrol_id_apply_standalone_engine(false);
		$settings['ad_policy_enable'] = '';
		kontrol_id_set_settings($settings);
		$restart_webgui = kontrol_id_restore_webgui_certificate($settings);
		kontrol_id_policy_schedule();
		@unlink(KONTROL_ID_WINBIND_MARKER);
		kontrol_id_manage_winbind(false);
		write_config('Kontrol-ID integrations removed.');
		if ($restart_webgui) {
			send_event('service restart webgui');
		}
		@unlink(KONTROL_ID_CONF);
		@unlink(KONTROL_ID_CA_FILE);
		@unlink(KONTROL_ID_CLIENT_CERT);
		@unlink(KONTROL_ID_CLIENT_KEY);
		@unlink(KONTROL_ID_KRB5_CONF);
		@unlink(KONTROL_ID_SMB_CONF);
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
	$kontrolid = config_get_path('installedpackages/kontrolid', array());
	/* Integration snapshots and enrollment tokens are node-local state.  Do
	 * not overwrite a peer's rollback data or replicate a bearer token. */
	if (is_array($kontrolid['config'][0] ?? null)) {
		foreach (array(
			'previous_squid_auth_method', 'previous_squid_auth_config', 'previous_squid_config',
			'previous_e2guardian_config', 'previous_e2guardian_auth_plugin',
			'e2guardian_auth_snapshot_taken', 'e2g_proxy_basic_added',
			'enrollment_token_hash', 'enrollment_token_expires', 'enrollment_token_uses',
			'portal_cert_ref', 'previous_webgui_cert_ref'
		) as $local_field) {
			unset($kontrolid['config'][0][$local_field]);
		}
	}
	$policy = array();
	if (is_file('/usr/local/pkg/e2guardian.inc')) {
		require_once('/usr/local/pkg/e2guardian.inc');
		if (function_exists('e2guardian_export_sections')) {
			foreach (e2guardian_export_sections(false) as $section) {
				$policy[$section] = config_get_path('installedpackages/' . $section, array());
			}
		}
	}
	$sync_payload = base64_encode(json_encode(array(
		'settings' => $kontrolid['config'][0] ?? array(),
		'sync' => config_get_path('installedpackages/kontrolidsync', array()),
		'policy' => $policy
	)));
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
	$command = "require_once('/usr/local/pkg/kontrol_id.inc');\n" .
		"\$kid_sync = json_decode(base64_decode('{$sync_payload}'), true);\n" .
		"\$kid_local = kontrol_id_get_settings();\n" .
		"\$kid_incoming = \$kid_sync['settings'];\n" .
		"foreach (array('previous_squid_auth_method', 'previous_squid_auth_config', 'previous_squid_config', 'previous_e2guardian_config', 'previous_e2guardian_auth_plugin', 'e2guardian_auth_snapshot_taken', 'e2g_proxy_basic_added', 'enrollment_token_hash', 'enrollment_token_expires', 'enrollment_token_uses', 'portal_cert_ref', 'previous_webgui_cert_ref') as \$kid_local_field) {\n" .
		"  if (array_key_exists(\$kid_local_field, \$kid_local)) { \$kid_incoming[\$kid_local_field] = \$kid_local[\$kid_local_field]; }\n" .
		"}\n" .
		"config_set_path('installedpackages/kontrolid/config/0', array_merge(kontrol_id_defaults(), \$kid_incoming));\n" .
		"config_set_path('installedpackages/kontrolidsync', \$kid_sync['sync']);\n" .
		"foreach ((\$kid_sync['policy'] ?? array()) as \$kid_section => \$kid_policy) {\n" .
		"  config_set_path('installedpackages/' . \$kid_section, \$kid_policy);\n" .
		"}\n" .
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
