<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'Status and Enrollment');
$savemsg = '';
$saveerror = false;
$view = $_GET['view'] ?? '';
if (!in_array($view, array('stations', 'sessions'), true)) {
	$view = '';
}
$search = trim(substr($_GET['q'] ?? '', 0, 100));
$page = max(1, (int)($_GET['page'] ?? 1));
$page_size = 50;

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
			kontrol_id_send_provisioner_bundle(1);
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
$now = time();
$endpoint_summary = $db->querySingle(
	'SELECT COUNT(*) AS total, ' .
	'COALESCE(SUM(CASE WHEN revoked=0 THEN 1 ELSE 0 END),0) AS active, ' .
	'COALESCE(SUM(CASE WHEN revoked<>0 THEN 1 ELSE 0 END),0) AS revoked, ' .
	'COALESCE(SUM(CASE WHEN revoked=0 AND last_seen=0 THEN 1 ELSE 0 END),0) AS never_seen ' .
	'FROM endpoints', true
);
$session_summary_stmt = $db->prepare(
	'SELECT COUNT(*) AS total, ' .
	'COALESCE(SUM(CASE WHEN revoked=0 AND expires_at>:now THEN 1 ELSE 0 END),0) AS active ' .
	'FROM portal_sessions'
);
$session_summary_stmt->bindValue(':now', $now, SQLITE3_INTEGER);
$session_summary_result = $session_summary_stmt->execute();
$session_summary = $session_summary_result ? $session_summary_result->fetchArray(SQLITE3_ASSOC) : array('total' => 0, 'active' => 0);

$rows = array();
$row_count = 0;
if ($view === 'stations') {
	$where = '';
	if ($search !== '') {
		$where = ' WHERE ip LIKE :search OR hostname LIKE :search OR fqdn LIKE :search';
	}
	$count_stmt = $db->prepare('SELECT COUNT(*) FROM endpoints' . $where);
	if ($search !== '') {
		$count_stmt->bindValue(':search', '%' . $search . '%', SQLITE3_TEXT);
	}
	$count_result = $count_stmt->execute();
	$count_row = $count_result ? $count_result->fetchArray(SQLITE3_NUM) : array(0);
	$row_count = (int)$count_row[0];
	$max_page = max(1, (int)ceil($row_count / $page_size));
	$page = min($page, $max_page);
	$stmt = $db->prepare(
		'SELECT ip,hostname,fqdn,enrolled_at,last_seen,revoked FROM endpoints' . $where .
		' ORDER BY hostname,ip LIMIT :limit OFFSET :offset'
	);
	if ($search !== '') {
		$stmt->bindValue(':search', '%' . $search . '%', SQLITE3_TEXT);
	}
	$stmt->bindValue(':limit', $page_size, SQLITE3_INTEGER);
	$stmt->bindValue(':offset', ($page - 1) * $page_size, SQLITE3_INTEGER);
	$result = $stmt->execute();
	while ($result && ($row = $result->fetchArray(SQLITE3_ASSOC))) {
		$rows[] = $row;
	}
} elseif ($view === 'sessions') {
	$where = '';
	if ($search !== '') {
		$where = ' WHERE ip LIKE :search OR username LIKE :search OR backend LIKE :search';
	}
	$count_stmt = $db->prepare('SELECT COUNT(*) FROM portal_sessions' . $where);
	if ($search !== '') {
		$count_stmt->bindValue(':search', '%' . $search . '%', SQLITE3_TEXT);
	}
	$count_result = $count_stmt->execute();
	$count_row = $count_result ? $count_result->fetchArray(SQLITE3_NUM) : array(0);
	$row_count = (int)$count_row[0];
	$max_page = max(1, (int)ceil($row_count / $page_size));
	$page = min($page, $max_page);
	$stmt = $db->prepare(
		'SELECT ip,username,backend,created_at,expires_at,revoked FROM portal_sessions' . $where .
		' ORDER BY expires_at DESC LIMIT :limit OFFSET :offset'
	);
	if ($search !== '') {
		$stmt->bindValue(':search', '%' . $search . '%', SQLITE3_TEXT);
	}
	$stmt->bindValue(':limit', $page_size, SQLITE3_INTEGER);
	$stmt->bindValue(':offset', ($page - 1) * $page_size, SQLITE3_INTEGER);
	$result = $stmt->execute();
	while ($result && ($row = $result->fetchArray(SQLITE3_ASSOC))) {
		$rows[] = $row;
	}
}
$db->close();

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
		<p>Download a ready-to-use package for one Windows station. The secure enrollment data is embedded automatically; the operator only needs to run the Provisioner as administrator.</p>
		<?php if (config_get_path('system/webgui/protocol', 'https') !== 'https'): ?>
			<div class="alert alert-danger">The enrollment API requires an HTTPS WebGUI. Enable HTTPS before provisioning stations.</div>
		<?php endif; ?>
		<form method="post" class="form-inline">
			<button class="btn btn-success" type="submit" name="download_bundle" value="1"><i class="fa fa-download"></i> Download configured Provisioner</button>
		</form>
		<p class="help-block">For security, each download enrolls one station and remains valid for 30 minutes. Extract both files to the same folder and run Kontrol-ID-Provisioner.exe. The temporary enrollment manifest is deleted after successful configuration.</p>
		<p class="help-block">Each station creates its own non-exportable private key and receives a unique WinRM HTTPS server certificate. Kontrol-ID uses one appliance client certificate to authenticate its queries; no shared station private key is distributed.</p>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Identity Overview</h2></div>
	<div class="panel-body">
		<div class="row">
			<div class="col-sm-3 text-center"><h3><?=htmlspecialchars((int)($endpoint_summary['active'] ?? 0))?></h3><p>Active stations</p></div>
			<div class="col-sm-3 text-center"><h3><?=htmlspecialchars((int)($endpoint_summary['revoked'] ?? 0))?></h3><p>Revoked stations</p></div>
			<div class="col-sm-3 text-center"><h3><?=htmlspecialchars((int)($endpoint_summary['never_seen'] ?? 0))?></h3><p>Awaiting first query</p></div>
			<div class="col-sm-3 text-center"><h3><?=htmlspecialchars((int)($session_summary['active'] ?? 0))?></h3><p>Active portal sessions</p></div>
		</div>
		<p class="text-center">
			<a class="btn btn-default" href="?view=stations"><i class="fa fa-desktop"></i> Manage stations</a>
			<a class="btn btn-default" href="?view=sessions"><i class="fa fa-user"></i> Manage portal sessions</a>
		</p>
		<p class="help-block">The complete station inventory is retained for identity lookup, audit and individual revocation, but is loaded only when requested. Runtime identity diagnostics remain available on the Real Time tab.</p>
	</div>
</div>

<?php if ($view === 'stations'): ?>
<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Manage Enrolled Stations</h2></div>
	<div class="panel-body">
		<form method="get" class="form-inline">
			<input type="hidden" name="view" value="stations">
			<div class="form-group"><label class="sr-only" for="station-search">Search stations</label><input class="form-control" id="station-search" name="q" value="<?=htmlspecialchars($search)?>" placeholder="IP, hostname or FQDN"></div>
			<button class="btn btn-primary" type="submit"><i class="fa fa-search"></i> Search</button>
			<a class="btn btn-default" href="?view=stations">Clear</a>
			<a class="btn btn-default pull-right" href="kontrol_id_status.php">Back to overview</a>
		</form>
	</div>
	<div class="panel-body table-responsive">
	<table class="table table-striped table-hover">
		<thead><tr><th>IP</th><th>Hostname</th><th>FQDN</th><th>Enrolled</th><th>Last Seen</th><th>Status</th><th></th></tr></thead>
		<tbody>
		<?php foreach ($rows as $row): ?>
		<tr>
			<td><?=htmlspecialchars($row['ip'])?></td><td><?=htmlspecialchars($row['hostname'])?></td><td><?=htmlspecialchars($row['fqdn'])?></td>
			<td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['enrolled_at']))?></td>
			<td><?=$row['last_seen'] ? htmlspecialchars(date('Y-m-d H:i:s', (int)$row['last_seen'])) : 'Never'?></td>
			<td><?=$row['revoked'] ? '<span class="label label-danger">Revoked</span>' : '<span class="label label-success">Active</span>'?></td>
			<td><?php if (!$row['revoked']): ?><form method="post"><button class="btn btn-xs btn-danger" name="revoke_endpoint" value="<?=htmlspecialchars($row['ip'])?>">Revoke</button></form><?php endif; ?></td>
		</tr>
		<?php endforeach; ?>
		<?php if (empty($rows)): ?><tr><td colspan="7">No station matches this search.</td></tr><?php endif; ?>
		</tbody>
	</table>
	<?php if ($row_count > $page_size): ?>
	<nav class="text-center"><ul class="pagination">
		<?php if ($page > 1): ?><li><a href="?<?=htmlspecialchars(http_build_query(array('view' => 'stations', 'q' => $search, 'page' => $page - 1)))?>">&laquo; Previous</a></li><?php endif; ?>
		<li class="active"><span>Page <?=$page?> of <?=max(1, (int)ceil($row_count / $page_size))?></span></li>
		<?php if ($page * $page_size < $row_count): ?><li><a href="?<?=htmlspecialchars(http_build_query(array('view' => 'stations', 'q' => $search, 'page' => $page + 1)))?>">Next &raquo;</a></li><?php endif; ?>
	</ul></nav>
	<?php endif; ?>
	</div>
</div>
<?php elseif ($view === 'sessions'): ?>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Manage Portal Sessions</h2></div>
	<div class="panel-body">
		<form method="get" class="form-inline">
			<input type="hidden" name="view" value="sessions">
			<div class="form-group"><label class="sr-only" for="session-search">Search sessions</label><input class="form-control" id="session-search" name="q" value="<?=htmlspecialchars($search)?>" placeholder="IP, user or backend"></div>
			<button class="btn btn-primary" type="submit"><i class="fa fa-search"></i> Search</button>
			<a class="btn btn-default" href="?view=sessions">Clear</a>
			<a class="btn btn-default pull-right" href="kontrol_id_status.php">Back to overview</a>
		</form>
	</div>
	<div class="panel-body table-responsive">
	<table class="table table-striped table-hover">
		<thead><tr><th>IP</th><th>User</th><th>Backend</th><th>Created</th><th>Expires</th><th>Status</th><th></th></tr></thead>
		<tbody>
		<?php foreach ($rows as $row): $active = !$row['revoked'] && (int)$row['expires_at'] > $now; ?>
		<tr>
			<td><?=htmlspecialchars($row['ip'])?></td><td><?=htmlspecialchars($row['username'])?></td><td><?=htmlspecialchars($row['backend'])?></td>
			<td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['created_at']))?></td><td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['expires_at']))?></td>
			<td><?=$active ? '<span class="label label-success">Active</span>' : '<span class="label label-default">Expired/Revoked</span>'?></td>
			<td><?php if ($active): ?><form method="post"><button class="btn btn-xs btn-danger" name="revoke_session" value="<?=htmlspecialchars($row['ip'])?>">Revoke</button></form><?php endif; ?></td>
		</tr>
		<?php endforeach; ?>
		<?php if (empty($rows)): ?><tr><td colspan="7">No portal session matches this search.</td></tr><?php endif; ?>
		</tbody>
	</table>
	<?php if ($row_count > $page_size): ?>
	<nav class="text-center"><ul class="pagination">
		<?php if ($page > 1): ?><li><a href="?<?=htmlspecialchars(http_build_query(array('view' => 'sessions', 'q' => $search, 'page' => $page - 1)))?>">&laquo; Previous</a></li><?php endif; ?>
		<li class="active"><span>Page <?=$page?> of <?=max(1, (int)ceil($row_count / $page_size))?></span></li>
		<?php if ($page * $page_size < $row_count): ?><li><a href="?<?=htmlspecialchars(http_build_query(array('view' => 'sessions', 'q' => $search, 'page' => $page + 1)))?>">Next &raquo;</a></li><?php endif; ?>
	</ul></nav>
	<?php endif; ?>
	</div>
</div>
<?php endif; ?>

<?php include('foot.inc'); ?>
