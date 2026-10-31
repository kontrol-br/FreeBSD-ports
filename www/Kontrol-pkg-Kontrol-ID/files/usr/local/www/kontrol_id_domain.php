<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'Domain and Keytab');
$savemsg = '';
$input_errors = array();

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	try {
		if (isset($_POST['regenerate_config'])) {
			kontrol_id_ensure_storage();
			kontrol_id_write_domain_config();
			write_config('Kontrol-ID domain configuration regenerated.');
			$savemsg = 'The package-owned Kerberos and Samba configurations were regenerated.';
		} elseif (isset($_POST['import_keytab'])) {
			kontrol_id_ensure_storage();
			kontrol_id_write_domain_config();
			kontrol_id_import_keytab($_FILES['keytab_file']['tmp_name'] ?? '');
			write_config('Kontrol-ID Kerberos keytab imported.');
			$savemsg = 'The keytab was validated and installed with restricted permissions.';
		} elseif (isset($_POST['join_domain'])) {
			kontrol_id_join_domain($_POST['join_username'] ?? '', $_POST['join_password'] ?? '');
			unset($_POST['join_password']);
			write_config('Kontrol-ID joined the configured Active Directory domain.');
			$settings = kontrol_id_get_settings();
			kontrol_id_write_domain_config();
			kontrol_id_manage_winbind(($settings['enable'] ?? '') === 'on' && kontrol_id_ntlm_required($settings));
			$savemsg = 'The machine trust, HTTP service principal and Kerberos keytab were created.';
		} elseif (isset($_POST['unjoin_domain'])) {
			if (($_POST['confirm_unjoin'] ?? '') !== 'yes') {
				throw new Exception('Confirm that Kontrol-ID will be disabled and its local domain trust will be deleted.');
			}
			kontrol_id_unjoin_domain($_POST['unjoin_username'] ?? '', $_POST['unjoin_password'] ?? '');
			unset($_POST['unjoin_password']);
			write_config('Kontrol-ID left Active Directory and removed its local machine trust and keytab.');
			$savemsg = 'The appliance left Active Directory. Kontrol-ID was disabled and the local keytab and machine trust were removed.';
		} elseif (isset($_POST['clear_local_trust'])) {
			if (trim($_POST['local_cleanup_confirmation'] ?? '') !== 'REMOVE LOCAL TRUST') {
				throw new Exception('Type REMOVE LOCAL TRUST exactly to confirm local-only cleanup.');
			}
			kontrol_id_disable_before_unjoin();
			kontrol_id_clear_local_domain_state();
			write_config('Kontrol-ID local Active Directory trust and keytab removed without contacting the domain.');
			$savemsg = 'Local domain trust data was removed. If the controller was unavailable, remove the stale computer account from Active Directory manually.';
		}
	} catch (Throwable $e) {
		unset($_POST['join_password']);
		unset($_POST['unjoin_password']);
		$input_errors[] = $e->getMessage();
	}
}

$settings = kontrol_id_get_settings();
$domain_status = kontrol_id_domain_status();
$domain = $domain_status['domain'];

include('head.inc');

$tab_array = array();
$tab_array[] = array('General', false, '/pkg_edit.php?xml=kontrol_id.xml&amp;id=0');
$tab_array[] = array('Status and Enrollment', false, '/kontrol_id_status.php');
$tab_array[] = array('Domain and Keytab', true, '/kontrol_id_domain.php');
$tab_array[] = array('XMLRPC Sync', false, '/pkg_edit.php?xml=kontrol_id_sync.xml&amp;id=0');
display_top_tabs($tab_array);

if (!empty($input_errors)) {
	print_input_errors($input_errors);
}
if (!empty($savemsg)) {
	print_info_box($savemsg, 'success');
}
?>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Active Directory Status</h2></div>
	<div class="panel-body">
		<dl class="dl-horizontal">
			<dt>Realm</dt><dd><?=htmlspecialchars($domain['realm'] ?: 'Not configured')?></dd>
			<dt>Domain controller</dt><dd><?=htmlspecialchars($domain['dc'] ?: 'Not configured')?></dd>
			<dt>Proxy service</dt><dd><code><?=htmlspecialchars($domain['spn'] ?: 'Not configured')?></code></dd>
			<dt>Authentication</dt><dd><?=htmlspecialchars(($settings['access_mode'] ?? '') === 'explicit' ? $settings['explicit_auth'] : 'Transparent identity')?></dd>
		</dl>
		<div class="table-responsive">
		<table class="table table-striped table-hover">
			<thead><tr><th>Check</th><th>Status</th><th>Detail</th></tr></thead>
			<tbody>
			<?php foreach ($domain_status['checks'] as $check): ?>
			<tr>
				<td><?=htmlspecialchars($check['name'])?></td>
				<td><?=$check['ok'] ? '<span class="label label-success">OK</span>' : '<span class="label label-warning">Not ready</span>'?></td>
				<td><?=htmlspecialchars($check['detail'])?></td>
			</tr>
			<?php endforeach; ?>
			</tbody>
		</table>
		</div>
		<form method="post">
			<button class="btn btn-default" type="submit" name="regenerate_config" value="1"><i class="fa fa-refresh"></i> Regenerate managed configuration</button>
		</form>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Assisted Domain Join</h2></div>
	<div class="panel-body">
		<p>Use a delegated account allowed to create or update this computer account and register the
			<code>HTTP/<?=htmlspecialchars($domain['fqdn'] ?: 'proxy.example.corp')?></code> service principal.
			The password is sent only to <code>kinit</code> over a private pipe, is never placed on a command line,
			and is not stored in the Kontrol configuration.</p>
		<div class="alert alert-info">Configure DNS and NTP first. Save the Proxy FQDN, domain, controller and NetBIOS name on the General tab before joining.</div>
		<form method="post" class="form-horizontal" autocomplete="off">
			<div class="form-group">
				<label class="col-sm-3 control-label" for="join_username">Delegated account</label>
				<div class="col-sm-6"><input class="form-control" id="join_username" name="join_username" type="text" autocomplete="username" placeholder="join-account@<?=htmlspecialchars(strtolower($domain['realm'] ?: 'example.corp'))?>"></div>
			</div>
			<div class="form-group">
				<label class="col-sm-3 control-label" for="join_password">Password</label>
				<div class="col-sm-6"><input class="form-control" id="join_password" name="join_password" type="password" autocomplete="current-password"></div>
			</div>
			<div class="form-group"><div class="col-sm-offset-3 col-sm-6">
				<button class="btn btn-primary" type="submit" name="join_domain" value="1"><i class="fa fa-link"></i> Join domain and create HTTP keytab</button>
			</div></div>
		</form>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Import Existing Keytab</h2></div>
	<div class="panel-body">
		<p>For controlled AD environments, import a keytab generated by the domain team. It must contain
			<code><?=htmlspecialchars($domain['spn'] ?: 'HTTP/proxy.example.corp@EXAMPLE.CORP')?></code> with AES keys.
			The file is checked with MIT Kerberos before it atomically replaces the active keytab.</p>
		<form method="post" enctype="multipart/form-data" class="form-inline">
			<input type="hidden" name="MAX_FILE_SIZE" value="1048576">
			<div class="form-group"><input class="form-control" type="file" name="keytab_file" accept=".keytab,application/octet-stream" required></div>
			<button class="btn btn-primary" type="submit" name="import_keytab" value="1"><i class="fa fa-upload"></i> Validate and import</button>
		</form>
	</div>
</div>

<div class="panel panel-danger">
	<div class="panel-heading"><h2 class="panel-title">Remove Domain Membership</h2></div>
	<div class="panel-body">
		<p>Use a delegated domain account to remove this appliance cleanly from Active Directory. Kontrol-ID is disabled first,
			its reversible Squid/E2Guardian configuration is restored, and only then are the package-owned keytab and Samba machine secrets deleted.</p>
		<form method="post" class="form-horizontal" autocomplete="off">
			<div class="form-group">
				<label class="col-sm-3 control-label" for="unjoin_username">Delegated account</label>
				<div class="col-sm-6"><input class="form-control" id="unjoin_username" name="unjoin_username" type="text" autocomplete="username"></div>
			</div>
			<div class="form-group">
				<label class="col-sm-3 control-label" for="unjoin_password">Password</label>
				<div class="col-sm-6"><input class="form-control" id="unjoin_password" name="unjoin_password" type="password" autocomplete="current-password"></div>
			</div>
			<div class="form-group"><div class="col-sm-offset-3 col-sm-7">
				<label><input type="checkbox" name="confirm_unjoin" value="yes"> Disable Kontrol-ID and permanently delete this node's local domain trust and keytab.</label>
			</div></div>
			<div class="form-group"><div class="col-sm-offset-3 col-sm-6">
				<button class="btn btn-danger" type="submit" name="unjoin_domain" value="1" onclick="return confirm('Leave Active Directory and delete the local keytab and machine trust?');"><i class="fa fa-unlink"></i> Unjoin domain and delete local trust</button>
			</div></div>
		</form>
		<hr>
		<p><strong>Controller unavailable?</strong> Local-only cleanup does not delete the computer account in Active Directory.</p>
		<form method="post" class="form-inline" autocomplete="off">
			<div class="form-group"><input class="form-control" name="local_cleanup_confirmation" type="text" placeholder="REMOVE LOCAL TRUST"></div>
			<button class="btn btn-danger" type="submit" name="clear_local_trust" value="1" onclick="return confirm('Delete only the local keytab and machine trust?');"><i class="fa fa-trash"></i> Remove local trust only</button>
		</form>
	</div>
</div>

<?php if (!empty($domain_status['keytab']['output'])): ?>
<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Installed Keytab Principals</h2></div>
	<div class="panel-body"><pre><?=htmlspecialchars($domain_status['keytab']['output'])?></pre></div>
</div>
<?php endif; ?>

<div class="alert alert-warning">
	<strong>High availability:</strong> XMLRPC synchronizes settings, but it deliberately does not copy machine passwords or keytabs.
	Join each HA member separately, or import a separately controlled keytab on each node. Never embed domain credentials in the package or in config.xml.
</div>

<?php include('foot.inc'); ?>
