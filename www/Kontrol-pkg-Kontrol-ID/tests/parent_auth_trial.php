#!/usr/local/bin/php
<?php

/* Isolated lab harness for the legacy E2Guardian proxy-after/Squid-parent
 * topology. It never edits the live E2Guardian or Squid configuration. */

if (PHP_SAPI !== 'cli' || $argc !== 2 ||
	!in_array($argv[1], array('transparent', 'basic', 'kerberos', 'ntlm'), true)) {
	fwrite(STDERR, "usage: parent_auth_trial.php transparent|basic|kerberos|ntlm\n");
	exit(64);
}

$mode = $argv[1];
$root = '/var/tmp/kontrol-id-parent-trial';
$e2g = $root . '/e2guardian';
$source = '/usr/local/etc/e2guardian';
$samba_source = '/usr/local/etc/kontrol-id/smb4.conf';
$samba_trial = $root . '/smb4-trial.conf';

function trial_remove_tree($path, $root) {
	$root = rtrim($root, '/');
	if ($path === $root || strpos($path, $root . '/') !== 0 || !is_dir($path)) {
		return;
	}
	$iterator = new RecursiveIteratorIterator(
		new RecursiveDirectoryIterator($path, FilesystemIterator::SKIP_DOTS),
		RecursiveIteratorIterator::CHILD_FIRST
	);
	foreach ($iterator as $entry) {
		$entry->isDir() && !$entry->isLink() ? rmdir($entry->getPathname()) : unlink($entry->getPathname());
	}
	rmdir($path);
}

function trial_copy_tree($source, $destination) {
	mkdir($destination, 0755, true);
	$iterator = new RecursiveIteratorIterator(
		new RecursiveDirectoryIterator($source, FilesystemIterator::SKIP_DOTS),
		RecursiveIteratorIterator::SELF_FIRST
	);
	foreach ($iterator as $entry) {
		$target = $destination . '/' . $iterator->getSubPathName();
		if ($entry->isDir()) {
			if (!is_dir($target)) {
				mkdir($target, 0755, true);
			}
		} else {
			copy($entry->getPathname(), $target);
			chmod($target, 0644);
		}
	}
}

function trial_replace_line($contents, $key, $value) {
	$replacement = $key . ' = ' . $value;
	/* Preserve commented documentation, replace the first active directive and
	 * remove any additional active copies.  Package-generated configurations
	 * may contain both legacy and current spellings of the same setting. */
	$pattern = '/^\s*' . preg_quote($key, '/') . '\s*=.*$(?:\r?\n)?/mi';
	if (preg_match($pattern, $contents)) {
		$seen = false;
		return preg_replace_callback($pattern, function ($match) use (&$seen, $replacement) {
			if ($seen) {
				return '';
			}
			$seen = true;
			return $replacement . "\n";
		}, $contents);
	}
	return $replacement . "\n" . $contents;
}

if (!is_dir($source) || !is_file($source . '/e2guardian.conf')) {
	throw new RuntimeException('Live E2Guardian configuration is unavailable.');
}
if (!is_dir($root)) {
	mkdir($root, 0755, true);
}

/* Samba client tools in this FreeBSD build do not follow the custom
 * winbindd socket path from smb4.conf.  Keep the domain/state settings but
 * let the isolated winbind use Samba's compiled socket path. */
if (is_file($samba_source)) {
	$samba = file_get_contents($samba_source);
	$samba = preg_replace('/^\s*winbindd socket directory\s*=.*$(?:\r?\n)?/mi', '', $samba);
	file_put_contents($samba_trial, $samba);
	chgrp($samba_trial, 'proxy');
	chmod($samba_trial, 0640);
}
trial_remove_tree($e2g, $root);
trial_copy_tree($source, $e2g);

/* Point every copied list/story reference at the isolated tree. */
$iterator = new RecursiveIteratorIterator(
	new RecursiveDirectoryIterator($e2g, FilesystemIterator::SKIP_DOTS)
);
foreach ($iterator as $entry) {
	if (!$entry->isFile() || $entry->getSize() > 8 * 1024 * 1024) {
		continue;
	}
	$contents = file_get_contents($entry->getPathname());
	if ($contents !== false && strpos($contents, $source) !== false) {
		file_put_contents($entry->getPathname(), str_replace($source, $e2g, $contents));
	}
}

$config_path = $e2g . '/e2guardian.conf';
$config = file_get_contents($config_path);
$config = trial_replace_line($config, 'pidfilename', "'{$root}/e2guardian.pid'");
$config = trial_replace_line($config, 'loglocation', "'{$root}/e2guardian-access.log'");
$config = trial_replace_line($config, 'filterip', '127.0.0.1');
$config = trial_replace_line($config, 'filterports', '18080');
$config = trial_replace_line($config, 'icapport', '0');
$config = trial_replace_line($config, 'proxyip', '127.0.0.1');
$config = trial_replace_line($config, 'proxyport', '18888');
$config = trial_replace_line($config, 'enablessl', 'on');
$config = trial_replace_line($config, 'generatedcertpath', "'{$root}/generatedcerts'");
$plugin = $mode === 'ntlm' ? 'proxy-ntlm.conf' :
	($mode === 'transparent' ? 'ip.conf' : 'proxy-basic.conf');
$config = preg_replace('/^\s*authplugin\s*=.*$/mi',
	"authplugin = '{$e2g}/authplugins/{$plugin}'", $config, 1);

/* The Kontrol-generated ICAP profile does not need proxy-after storyboard
 * lists.  Declare the v5.4 lists explicitly in this isolated copy because
 * the proxy-basic/proxy-ntlm plugins validate them during startup. */
$config .= "\n# Isolated proxy-after trial lists\n" .
	"maplist = 'name=defaultusermap,path={$e2g}/lists/authplugins/filtergroupslist'\n" .
	"ipmaplist = 'name=ipmap,path={$e2g}/lists/authplugins/ipgroups'\n" .
	"maplist = 'name=portmap,path={$e2g}/lists/authplugins/portgroups'\n" .
	"iplist = 'name=bannedclient,path={$e2g}/lists/bannediplist'\n" .
	"iplist = 'name=exceptionclient,path={$e2g}/lists/exceptioniplist'\n" .
	"ipsitelist = 'name=authexception,path={$e2g}/lists/authexceptioniplist'\n" .
	"sitelist = 'name=authexception,path={$e2g}/lists/authexceptionsitelist'\n" .
	"urllist = 'name=authexception,path={$e2g}/lists/authexceptionurllist'\n" .
	"sitelist = 'name=nomitm,path={$e2g}/lists/nomitm'\n" .
	"regexpboollist = 'name=browser,path={$e2g}/lists/browser'\n";
file_put_contents($config_path, $config);

foreach (glob($e2g . '/e2guardianf*.conf') as $group_file) {
	$group = file_get_contents($group_file);
	$group = trial_replace_line($group, 'sslmitm', 'on');
	$group = trial_replace_line($group, 'mitmcheckcert', 'off');
	file_put_contents($group_file, $group);
}

/* example.com is the positive control: it must keep the origin certificate.
 * www.wikipedia.org is not excepted and must receive a Kontrol-issued cert. */
foreach (glob($e2g . '/lists/exceptionsitelist.g_*') as $exception) {
	file_put_contents($exception, rtrim((string)@file_get_contents($exception)) . "\nexample.com\n");
}

mkdir($root . '/generatedcerts', 0770, true);
chown($root . '/generatedcerts', 'nobody');
chgrp($root . '/generatedcerts', 'nobody');
foreach (array('e2guardian-access.log', 'e2guardian-error.log') as $log) {
	file_put_contents($root . '/' . $log, '');
	chown($root . '/' . $log, 'nobody');
	chgrp($root . '/' . $log, 'nobody');
}

$auth = '';
if ($mode === 'kerberos') {
	$auth = "auth_param negotiate program /usr/bin/env KRB5_CONFIG=/usr/local/etc/kontrol-id/krb5.conf KRB5_KTNAME=FILE:/var/db/kontrol-id-domain/krb5.keytab /usr/local/libexec/squid/negotiate_kerberos_auth -r -s HTTP/kontrol290.kontrol.corp\n" .
		"auth_param negotiate children 10 startup=1 idle=1\n" .
		"auth_param negotiate keep_alive on\n";
} elseif ($mode === 'ntlm') {
	$auth = "auth_param ntlm program /usr/local/bin/ntlm_auth --configfile={$samba_trial} --helper-protocol=squid-2.5-ntlmssp\n" .
		"auth_param ntlm children 10 startup=1 idle=1\n" .
		"auth_param ntlm keep_alive on\n";
} elseif ($mode === 'basic') {
	$auth = "auth_param basic program /usr/local/bin/ntlm_auth --configfile={$samba_trial} --helper-protocol=squid-2.5-basic\n" .
		"auth_param basic children 10 startup=1 idle=1\n" .
		"auth_param basic realm Kontrol Parent Trial\n" .
		"auth_param basic credentialsttl 5 minutes\n";
}
$auth_acl = $mode === 'transparent' ? '' :
	"acl trial_authenticated proxy_auth REQUIRED\nhttp_access deny !trial_authenticated\n";

$squid = "visible_hostname kontrol-parent-trial\n" .
	"http_port 127.0.0.1:18888\n" .
	"pid_filename {$root}/squid-parent.pid\n" .
	"cache_effective_user squid\ncache_effective_group proxy\n" .
	"coredump_dir {$root}\ncache deny all\ncache_dir null /tmp\n" .
	"access_log stdio:{$root}/squid-access.log squid\n" .
	"cache_log {$root}/squid-cache.log\ncache_store_log none\n" .
	"shutdown_lifetime 1 seconds\n" .
	"acl localhost src 127.0.0.1/32\n" .
	"acl SSL_ports port 443\nacl CONNECT method CONNECT\n" .
	"http_access deny CONNECT !SSL_ports\n" . $auth . $auth_acl .
	"http_access allow localhost\nhttp_access deny all\n";
file_put_contents($root . '/squid-parent.conf', $squid);
file_put_contents($root . '/mode', $mode . "\n");
chmod($root, 0755);
foreach (array('squid-access.log', 'squid-cache.log') as $log) {
	file_put_contents($root . '/' . $log, '');
	chown($root . '/' . $log, 'squid');
	chgrp($root . '/' . $log, 'proxy');
}

echo "Prepared isolated parent-proxy trial mode={$mode}\n";
