<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'Status and Enrollment');
$savemsg = '';
$saveerror = false;

function kontrol_id_provisioner_base_url() {
	$host = trim($_SERVER['HTTP_HOST'] ?? '');
	if (!preg_match('/^(?:[a-z0-9.-]+|\[[0-9a-f:]+\])(?::[0-9]{1,5})?$/i', $host)) {
		$hostname = config_get_path('system/hostname', 'kontrol');
		$domain = config_get_path('system/domain', 'localdomain');
		$port = (int)config_get_path('system/webgui/port', 443);
		$host = $hostname . (!empty($domain) ? '.' . $domain : '') . (($port > 0 && $port !== 443) ? ':' . $port : '');
	}
	return 'https://' . $host;
}

function kontrol_id_send_provisioner_bundle($uses) {
	if (config_get_path('system/webgui/protocol', 'https') !== 'https') {
		throw new Exception('Enable HTTPS on the WebGUI before creating an enrollment package.');
	}
	$webcert_ref = config_get_path('system/webgui/ssl-certref');
	$webcert = $webcert_ref ? kontrol_id_extract_item(lookup_cert($webcert_ref)) : array();
	if (empty($webcert['crt'])) {
		throw new Exception('The WebGUI certificate is unavailable.');
	}
	$fingerprint = strtoupper(openssl_x509_fingerprint(base64_decode($webcert['crt']), 'sha256'));
	$uses = max(1, min(500, (int)$uses));
	$token = kontrol_id_generate_token($uses, 1800);
	$settings = kontrol_id_get_settings();
	$expires = (int)$settings['enrollment_token_expires'];
	$source = '/usr/local/www/kontrol-id/Kontrol-ID-Provisioner.exe';
	if (!is_file($source) || filesize($source) < 4096) {
		throw new Exception('The Kontrol-ID Provisioner executable is unavailable.');
	}
	$temporary = sys_get_temp_dir() . '/kontrol-id-provisioner-' . bin2hex(random_bytes(8));
	$archive = $temporary . '.zip';
	if (!mkdir($temporary, 0700, true)) {
		throw new Exception('Unable to create the temporary enrollment package.');
	}
	$exe = $temporary . '/Kontrol-ID-Provisioner.exe';
	$config_file = $temporary . '/Kontrol-ID-Enrollment.json';
	try {
		if (!copy($source, $exe)) {
			throw new Exception('Unable to copy the Provisioner executable.');
		}
		$config = array(
			'schema' => 1,
			'appliance' => kontrol_id_provisioner_base_url(),
			'token' => $token,
			'fingerprint' => $fingerprint,
			'expiresAt' => $expires
		);
		if (file_put_contents($config_file, json_encode($config, JSON_UNESCAPED_SLASHES | JSON_PRETTY_PRINT), LOCK_EX) === false) {
			throw new Exception('Unable to create the enrollment manifest.');
		}
		chmod($exe, 0600);
		chmod($config_file, 0600);
		$output = array();
		$status = 0;
		$command = '/usr/bin/tar --format zip -cf ' . escapeshellarg($archive) .
			' -C ' . escapeshellarg($temporary) .
			' Kontrol-ID-Provisioner.exe Kontrol-ID-Enrollment.json 2>&1';
		exec($command, $output, $status);
		if ($status !== 0 || !is_file($archive) || filesize($archive) < 4096) {
			throw new Exception('Unable to assemble the enrollment ZIP: ' . implode(' ', $output));
		}
		header('Content-Type: application/zip');
		header('Content-Disposition: attachment; filename="Kontrol-ID-Provisioner.zip"');
		header('Content-Length: ' . filesize($archive));
		header('Cache-Control: no-store, no-cache, must-revalidate');
		header('Pragma: no-cache');
		header('X-Content-Type-Options: nosniff');
		readfile($archive);
	} finally {
		@unlink($exe);
		@unlink($config_file);
		@unlink($archive);
		@rmdir($temporary);
	}
	exit;
}

if (!kontrol_id_transparent_identity_mode()) {
	header('Location: /pkg_edit.php?xml=kontrol_id.xml&id=0&savemsg=' . rawurlencode(
		'Status and Enrollment is available only in Transparent identification mode.'
	));
	exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	if (isset($_POST['download_bundle'])) {
		try {
			kontrol_id_send_provisioner_bundle((int)($_POST['token_uses'] ?? 1));
		} catch (Exception $e) {
			$savemsg = $e->getMessage();
			$saveerror = true;
		}
	} elseif (isset($_POST['revoke_endpoint']) && filter_var($_POST['revoke_endpoint'], FILTER_VALIDATE_IP)) {
		$db = new SQLite3(KONTROL_ID_DB);
		$stmt = $db->prepare('UPDATE endpoints SET revoked=1 WHERE ip=:ip');
		$stmt->bindValue(':ip', $_POST['revoke_endpoint'], SQLITE3_TEXT);
		$stmt->execute();
		$stmt = $db->prepare('DELETE FROM identities WHERE ip=:ip');
		$stmt->bindValue(':ip', $_POST['revoke_endpoint'], SQLITE3_TEXT);
		$stmt->execute();
		$db->close();
		$savemsg = 'Endpoint revoked.';
	} elseif (isset($_POST['revoke_session']) && filter_var($_POST['revoke_session'], FILTER_VALIDATE_IP)) {
		$db = new SQLite3(KONTROL_ID_DB);
		$stmt = $db->prepare('UPDATE portal_sessions SET revoked=1 WHERE ip=:ip');
		$stmt->bindValue(':ip', $_POST['revoke_session'], SQLITE3_TEXT);
		$stmt->execute();
		$db->close();
		$savemsg = 'Portal session revoked.';
	}
}

kontrol_id_ensure_storage();
$db = new SQLite3(KONTROL_ID_DB, SQLITE3_OPEN_READONLY);
$endpoints = array();
$result = $db->query('SELECT ip,hostname,fqdn,enrolled_at,last_seen,revoked FROM endpoints ORDER BY hostname,ip');
while ($result && ($row = $result->fetchArray(SQLITE3_ASSOC))) {
	$endpoints[] = $row;
}
$sessions = array();
$result = $db->query('SELECT ip,username,backend,created_at,expires_at,revoked FROM portal_sessions ORDER BY expires_at DESC');
while ($result && ($row = $result->fetchArray(SQLITE3_ASSOC))) {
	$sessions[] = $row;
}
$identities = array();
$result = $db->query('SELECT ip,username,source,state,updated_at,expires_at,detail FROM identities ORDER BY updated_at DESC LIMIT 200');
while ($result && ($row = $result->fetchArray(SQLITE3_ASSOC))) {
	$identities[] = $row;
}
$db->close();

$webcert_ref = config_get_path('system/webgui/ssl-certref');
$webcert = $webcert_ref ? kontrol_id_extract_item(lookup_cert($webcert_ref)) : array();
$web_fingerprint = !empty($webcert['crt']) ? strtoupper(openssl_x509_fingerprint(base64_decode($webcert['crt']), 'sha256')) : '';
$settings = kontrol_id_get_settings();
$chain_status = kontrol_id_chain_status();
$chain_ok = !empty($chain_status);
foreach ($chain_status as $check) {
	if (!$check['ok']) {
		$chain_ok = false;
		break;
	}
}
$identity_ca = kontrol_id_extract_item(lookup_ca($settings['ca_ref']));

include('head.inc');

display_top_tabs(kontrol_id_tabs('status'));

if (!empty($savemsg)) {
	print_info_box($savemsg, $saveerror ? 'danger' : 'success');
}
?>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Managed Traffic Chain</h2></div>
	<div class="panel-body">
		<p><strong>Client &rarr; Kontrol-ID secure web gateway &rarr; Internet</strong></p>
		<p>
			Access: <strong><?=($settings['access_mode'] === 'explicit') ? 'Explicit proxy / ' . htmlspecialchars($settings['explicit_auth']) : 'Transparent identification'?></strong> &nbsp;
			Inspection: <strong><?=($settings['traffic_mode'] === 'http_https') ? 'HTTP + HTTPS' : 'HTTP only'?></strong> &nbsp;
			Interfaces: <strong><?=htmlspecialchars(kontrol_id_normalize_interfaces($settings['intercept_interfaces']) ?: 'not selected')?></strong> &nbsp;
			CA: <strong><?=htmlspecialchars($identity_ca['descr'] ?? 'unavailable')?></strong>
		</p>
		<div class="alert alert-<?=$chain_ok ? 'success' : 'warning'?>">
			<?=$chain_ok ? 'The saved configuration matches the complete managed chain.' : 'The managed chain has configuration drift. Review the failed checks below and save General Settings to reapply it.'?>
		</div>
		<div class="table-responsive">
		<table class="table table-striped table-hover">
			<thead><tr><th>Component</th><th>Status</th><th>Expected configuration</th></tr></thead>
			<tbody>
			<?php foreach ($chain_status as $check): ?>
			<tr>
				<td><?=htmlspecialchars($check['name'])?></td>
				<td><?=$check['ok'] ? '<span class="label label-success">OK</span>' : '<span class="label label-danger">Needs attention</span>'?></td>
				<td><?=htmlspecialchars($check['detail'])?></td>
			</tr>
			<?php endforeach; ?>
			</tbody>
		</table>
		</div>
		<p class="help-block">Kontrol-ID owns the proxy, authentication, inspection and policy runtime while enabled. Use <a href="/pkg_edit.php?xml=kontrol_id.xml&amp;id=0">General Settings</a> to change the traffic mode or interfaces.</p>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Windows Provisioning</h2></div>
	<div class="panel-body">
		<p>Generate a secure enrollment package. The appliance URL, one-time token and WebGUI fingerprint are loaded automatically; no values need to be typed on the Windows station.</p>
		<?php if (config_get_path('system/webgui/protocol', 'https') !== 'https'): ?>
			<div class="alert alert-danger">The enrollment API requires an HTTPS WebGUI. Enable HTTPS before provisioning stations.</div>
		<?php endif; ?>
		<dl class="dl-horizontal">
			<dt>WebGUI SHA-256</dt><dd><code><?=htmlspecialchars($web_fingerprint ?: 'No HTTPS WebGUI certificate found')?></code></dd>
			<dt>Token expires</dt><dd><?=((int)$settings['enrollment_token_expires'] > time()) ? htmlspecialchars(date('Y-m-d H:i:s', (int)$settings['enrollment_token_expires'])) : 'No active token'?></dd>
			<dt>Remaining uses</dt><dd><?=htmlspecialchars($settings['enrollment_token_uses'])?></dd>
		</dl>
		<form method="post" class="form-inline">
			<label for="token_uses">Stations allowed by this package</label>
			<input class="form-control" type="number" min="1" max="500" id="token_uses" name="token_uses" value="1">
			<button class="btn btn-success" type="submit" name="download_bundle" value="1"><i class="fa fa-download"></i> Download configured Provisioner</button>
		</form>
		<p class="help-block">The package expires after 30 minutes. Extract both files to the same folder and run Kontrol-ID-Provisioner.exe. The enrollment manifest is deleted after successful configuration.</p>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Enrolled Stations</h2></div>
	<div class="panel-body table-responsive">
	<table class="table table-striped table-hover">
		<thead><tr><th>IP</th><th>Hostname</th><th>FQDN</th><th>Enrolled</th><th>Last Seen</th><th>Status</th><th></th></tr></thead>
		<tbody>
		<?php foreach ($endpoints as $row): ?>
		<tr>
			<td><?=htmlspecialchars($row['ip'])?></td><td><?=htmlspecialchars($row['hostname'])?></td><td><?=htmlspecialchars($row['fqdn'])?></td>
			<td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['enrolled_at']))?></td>
			<td><?=$row['last_seen'] ? htmlspecialchars(date('Y-m-d H:i:s', (int)$row['last_seen'])) : 'Never'?></td>
			<td><?=$row['revoked'] ? '<span class="label label-danger">Revoked</span>' : '<span class="label label-success">Active</span>'?></td>
			<td><?php if (!$row['revoked']): ?><form method="post"><button class="btn btn-xs btn-danger" name="revoke_endpoint" value="<?=htmlspecialchars($row['ip'])?>">Revoke</button></form><?php endif; ?></td>
		</tr>
		<?php endforeach; ?>
		<?php if (empty($endpoints)): ?><tr><td colspan="7">No station has been enrolled.</td></tr><?php endif; ?>
		</tbody>
	</table>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Portal Sessions</h2></div>
	<div class="panel-body table-responsive">
	<table class="table table-striped table-hover">
		<thead><tr><th>IP</th><th>User</th><th>Backend</th><th>Created</th><th>Expires</th><th>Status</th><th></th></tr></thead>
		<tbody>
		<?php foreach ($sessions as $row): $active = !$row['revoked'] && (int)$row['expires_at'] > time(); ?>
		<tr>
			<td><?=htmlspecialchars($row['ip'])?></td><td><?=htmlspecialchars($row['username'])?></td><td><?=htmlspecialchars($row['backend'])?></td>
			<td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['created_at']))?></td><td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['expires_at']))?></td>
			<td><?=$active ? '<span class="label label-success">Active</span>' : '<span class="label label-default">Expired/Revoked</span>'?></td>
			<td><?php if ($active): ?><form method="post"><button class="btn btn-xs btn-danger" name="revoke_session" value="<?=htmlspecialchars($row['ip'])?>">Revoke</button></form><?php endif; ?></td>
		</tr>
		<?php endforeach; ?>
		<?php if (empty($sessions)): ?><tr><td colspan="7">No portal sessions.</td></tr><?php endif; ?>
		</tbody>
	</table>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Recent Identity Cache</h2></div>
	<div class="panel-body table-responsive">
	<table class="table table-striped table-hover">
		<thead><tr><th>IP</th><th>User</th><th>Source</th><th>State</th><th>Updated</th><th>Expires</th><th>Detail</th></tr></thead>
		<tbody>
		<?php foreach ($identities as $row): ?>
		<tr><td><?=htmlspecialchars($row['ip'])?></td><td><?=htmlspecialchars($row['username'])?></td><td><?=htmlspecialchars($row['source'])?></td>
		<td><?=htmlspecialchars($row['state'])?></td><td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['updated_at']))?></td>
		<td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['expires_at']))?></td><td><?=htmlspecialchars($row['detail'])?></td></tr>
		<?php endforeach; ?>
		<?php if (empty($identities)): ?><tr><td colspan="7">The helper has not processed any station yet.</td></tr><?php endif; ?>
		</tbody>
	</table>
	</div>
</div>

<?php include('foot.inc'); ?>
