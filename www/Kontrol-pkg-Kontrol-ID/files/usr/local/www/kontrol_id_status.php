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

if (!kontrol_id_transparent_identity_mode()) {
	header('Location: /pkg_edit.php?xml=kontrol_id.xml&id=0&savemsg=' . rawurlencode(
		'Status and Enrollment is available only in Transparent identification mode.'
	));
	exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	if (isset($_POST['revoke_endpoint']) && filter_var($_POST['revoke_endpoint'], FILTER_VALIDATE_IP)) {
		$db = new SQLite3(KONTROL_ID_DB);
		$db->exec('BEGIN IMMEDIATE');
		$stmt = $db->prepare('DELETE FROM endpoints WHERE ip=:ip');
		$stmt->bindValue(':ip', $_POST['revoke_endpoint'], SQLITE3_TEXT);
		$stmt->execute();
		$stmt = $db->prepare('DELETE FROM identities WHERE ip=:ip');
		$stmt->bindValue(':ip', $_POST['revoke_endpoint'], SQLITE3_TEXT);
		$stmt->execute();
		$stmt = $db->prepare('DELETE FROM portal_sessions WHERE ip=:ip');
		$stmt->bindValue(':ip', $_POST['revoke_endpoint'], SQLITE3_TEXT);
		$stmt->execute();
		$db->exec('COMMIT');
		$db->close();
		write_config('Kontrol-ID station removed by administrator: ' . $_POST['revoke_endpoint']);
		$savemsg = 'Station removed.';
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
	'SELECT COUNT(*) AS active, ' .
	'COALESCE(SUM(CASE WHEN last_seen=0 THEN 1 ELSE 0 END),0) AS never_seen ' .
	'FROM endpoints WHERE revoked=0', true
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
	$where = ' WHERE revoked=0';
	if ($search !== '') {
		$where .= ' AND (ip LIKE :search OR hostname LIKE :search OR fqdn LIKE :search)';
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
		'SELECT ip,hostname,fqdn,enrolled_at,last_seen FROM endpoints' . $where .
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
		<p>Download the Windows Provisioner configured with this appliance CA. The same executable can be reused on any number of stations and on other Kontrol appliances that share this CA.</p>
		<?php if (config_get_path('system/webgui/protocol', 'https') !== 'https'): ?>
			<div class="alert alert-danger">The enrollment API requires an HTTPS WebGUI. Enable HTTPS before provisioning stations.</div>
		<?php endif; ?>
		<a class="btn btn-success" href="/kontrol_id_provisioner.php" download><i class="fa fa-download"></i> Download configured Provisioner</a>
		<p class="help-block">This executable contains the public certificate of the selected Kontrol-ID CA and the appliance address. Run it as administrator and authorize enrollment directly in the application with a Kontrol administrator account. Credentials and certificate data remain only in memory; no enrollment JSON file is created.</p>
		<p class="help-block">Each station creates its own non-exportable private key and receives a unique WinRM HTTPS server certificate. Kontrol-ID uses one appliance client certificate to authenticate its queries; no shared station private key is distributed.</p>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Identity Overview</h2></div>
	<div class="panel-body">
		<div class="row">
			<div class="col-sm-4 text-center"><h3><?=htmlspecialchars((int)($endpoint_summary['active'] ?? 0))?></h3><p>Active stations</p></div>
			<div class="col-sm-4 text-center"><h3><?=htmlspecialchars((int)($endpoint_summary['never_seen'] ?? 0))?></h3><p>Awaiting first query</p></div>
			<div class="col-sm-4 text-center"><h3><?=htmlspecialchars((int)($session_summary['active'] ?? 0))?></h3><p>Active portal sessions</p></div>
		</div>
		<p class="text-center">
			<a class="btn btn-default" href="?view=stations"><i class="fa fa-desktop"></i> Manage stations</a>
			<a class="btn btn-default" href="?view=sessions"><i class="fa fa-user"></i> Manage portal sessions</a>
		</p>
		<p class="help-block">Only currently enrolled stations are retained. Removing Kontrol-ID with the Provisioner also removes the station from this list. Runtime identity diagnostics remain available on the Real Time tab.</p>
	</div>
</div>

<?php if ($view === 'stations'): ?>
<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Enrolled Stations</h2></div>
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
		<thead><tr><th>IP</th><th>Hostname</th><th>FQDN</th><th>Enrolled</th><th>Last Seen</th><th></th></tr></thead>
		<tbody>
		<?php foreach ($rows as $row): ?>
		<tr>
			<td><?=htmlspecialchars($row['ip'])?></td><td><?=htmlspecialchars($row['hostname'])?></td><td><?=htmlspecialchars($row['fqdn'])?></td>
			<td><?=htmlspecialchars(date('Y-m-d H:i:s', (int)$row['enrolled_at']))?></td>
			<td><?=$row['last_seen'] ? htmlspecialchars(date('Y-m-d H:i:s', (int)$row['last_seen'])) : 'Never'?></td>
			<td><form method="post"><button class="btn btn-xs btn-danger" name="revoke_endpoint" value="<?=htmlspecialchars($row['ip'])?>">Remove</button></form></td>
		</tr>
		<?php endforeach; ?>
		<?php if (empty($rows)): ?><tr><td colspan="6">No station matches this search.</td></tr><?php endif; ?>
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
