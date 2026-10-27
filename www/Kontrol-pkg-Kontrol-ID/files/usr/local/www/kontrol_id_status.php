<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'Stations and Sessions');
$token_once = '';
$savemsg = '';

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	if (isset($_POST['generate_token'])) {
		$token_once = kontrol_id_generate_token((int)($_POST['token_uses'] ?? 1), 900);
		$savemsg = 'Enrollment token generated. It will be shown only on this page load.';
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

include('head.inc');

$tab_array = array();
$tab_array[] = array('General', false, '/pkg_edit.php?xml=kontrol_id.xml&amp;id=0');
$tab_array[] = array('Stations and Sessions', true, '/kontrol_id_status.php');
$tab_array[] = array('XMLRPC Sync', false, '/pkg_edit.php?xml=kontrol_id_sync.xml&amp;id=0');
display_top_tabs($tab_array);

if (!empty($savemsg)) {
	print_info_box($savemsg, 'success');
}
if (!empty($token_once)) {
	print_info_box('<strong>Enrollment token (copy now):</strong><br><code style="font-size:1.2em">' .
		htmlspecialchars($token_once) . '</code>', 'info');
}
?>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Windows Provisioning</h2></div>
	<div class="panel-body">
		<p>Generate a short-lived token, then enter the appliance HTTPS URL, token and WebGUI SHA-256 fingerprint in the Windows provisioner.</p>
		<?php if (config_get_path('system/webgui/protocol', 'https') !== 'https'): ?>
			<div class="alert alert-danger">The enrollment API requires an HTTPS WebGUI. Enable HTTPS before provisioning stations.</div>
		<?php endif; ?>
		<dl class="dl-horizontal">
			<dt>WebGUI SHA-256</dt><dd><code><?=htmlspecialchars($web_fingerprint ?: 'No HTTPS WebGUI certificate found')?></code></dd>
			<dt>Installer</dt><dd><a class="btn btn-sm btn-primary" href="/kontrol-id/Kontrol-ID-Provisioner.exe">Download Kontrol-ID Provisioner</a></dd>
			<dt>Token expires</dt><dd><?=((int)$settings['enrollment_token_expires'] > time()) ? htmlspecialchars(date('Y-m-d H:i:s', (int)$settings['enrollment_token_expires'])) : 'No active token'?></dd>
			<dt>Remaining uses</dt><dd><?=htmlspecialchars($settings['enrollment_token_uses'])?></dd>
		</dl>
		<form method="post" class="form-inline">
			<label for="token_uses">Allowed enrollments</label>
			<input class="form-control" type="number" min="1" max="500" id="token_uses" name="token_uses" value="1">
			<button class="btn btn-success" type="submit" name="generate_token" value="1"><i class="fa fa-key"></i> Generate 15-minute token</button>
		</form>
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
