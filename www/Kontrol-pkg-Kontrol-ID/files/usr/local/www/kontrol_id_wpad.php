<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'WPAD');
$input_errors = array();
$savemsg = '';
$settings = kontrol_id_get_settings();

function kontrol_id_wpad_post_settings($current) {
	$updated = $current;
	$updated['wpad_enable'] = isset($_POST['wpad_enable']) ? 'on' : '';
	$updated['wpad_manage_dhcp'] = isset($_POST['wpad_manage_dhcp']) ? 'on' : '';
	$updated['wpad_manage_dns'] = isset($_POST['wpad_manage_dns']) ? 'on' : '';
	$updated['wpad_interfaces'] = implode(';', array_values(array_filter(
		(array)($_POST['wpad_interfaces'] ?? array()),
		function ($value) { return preg_match('/^[A-Za-z0-9_.-]+$/', (string)$value); }
	)));
	$updated['wpad_domain'] = strtolower(rtrim(trim((string)($_POST['wpad_domain'] ?? '')), '.'));
	return $updated;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	try {
		$settings = kontrol_id_wpad_post_settings($settings);
		if (isset($_POST['generate_default'])) {
			$pac_content = kontrol_id_wpad_default_pac($settings);
			$savemsg = 'A recommended PAC was generated in the editor. Review it and click Apply WPAD configuration to save.';
		} elseif (isset($_POST['save'])) {
			$pac_content = str_replace("\r\n", "\n", (string)($_POST['wpad_pac'] ?? ''));
			if ($settings['wpad_domain'] !== '' && !is_domain($settings['wpad_domain'])) {
				throw new Exception('WPAD DNS Domain must be a valid domain name.');
			}
			if (($settings['wpad_enable'] ?? '') === 'on') {
				kontrol_id_wpad_selected_interfaces($settings);
				$pac_content = kontrol_id_wpad_validate_pac($pac_content);
			}
			$settings['wpad_pac'] = base64_encode($pac_content);
			$settings = kontrol_id_wpad_apply_settings($settings, true, true);
			$savemsg = (($settings['wpad_enable'] ?? '') === 'on') ?
				'WPAD was applied atomically. PAC files and selected DHCP/DNS integrations are active.' :
				'WPAD was disabled. Only files and DHCP/DNS entries owned by Kontrol-ID were removed.';
		}
	} catch (Throwable $exception) {
		$input_errors[] = $exception->getMessage();
		$pac_content = (string)($_POST['wpad_pac'] ?? kontrol_id_wpad_pac_content($settings));
	}
}

if (!isset($pac_content)) {
	$pac_content = kontrol_id_wpad_pac_content($settings);
}
$catalog = kontrol_id_wpad_interface_catalog();
$selected = kontrol_id_wpad_interfaces($settings);
$dhcp_owned = kontrol_id_wpad_decode_state($settings['wpad_dhcp_ownership'] ?? '');
$dns_owned = kontrol_id_wpad_decode_state($settings['wpad_dns_ownership'] ?? '');
$domain = kontrol_id_wpad_domain($settings);
$urls = array();
foreach (kontrol_id_wpad_selected_interfaces($settings, false) as $entry) {
	$urls[] = 'http://' . $entry['ip'] . '/wpad.dat';
}
if ($domain !== '') {
	array_unshift($urls, 'http://wpad.' . $domain . '/wpad.dat');
}

include('head.inc');
display_top_tabs(kontrol_id_tabs('wpad'));

if (!empty($input_errors)) {
	print_input_errors($input_errors);
}
if ($savemsg !== '') {
	print_info_box($savemsg, 'success');
}
?>

<style>
.kid-wpad-status { display:flex; flex-wrap:wrap; gap:18px 34px; margin:4px 0; }
.kid-wpad-status > div { min-width:190px; }
.kid-wpad-status small { display:block; color:#667; text-transform:uppercase; letter-spacing:.04em; }
.kid-wpad-editor { font-family:Consolas,Monaco,monospace; min-height:430px; resize:vertical; }
.kid-wpad-url { display:block; margin-top:4px; }
</style>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">WPAD service status</h2></div>
	<div class="panel-body">
		<div class="kid-wpad-status">
			<div><small>Service</small><strong><?=($settings['wpad_enable'] ?? '') === 'on' ? '<span class="text-success">Enabled</span>' : 'Disabled'?></strong></div>
			<div><small>HTTP delivery</small><strong><?=htmlspecialchars(strtoupper(config_get_path('system/webgui/protocol', 'http')))?> WebGUI integration</strong></div>
			<div><small>DHCP Option 252</small><strong><?=count($dhcp_owned)?> managed interface(s)</strong></div>
			<div><small>DNS</small><strong><?=empty($dns_owned) ? 'No managed override' : htmlspecialchars('wpad.' . ($dns_owned['domain'] ?? ''))?></strong></div>
		</div>
		<?php if (($settings['wpad_enable'] ?? '') === 'on'): ?>
			<hr />
			<p><strong>Published addresses</strong></p>
			<?php foreach ($urls as $url): ?><code class="kid-wpad-url"><?=htmlspecialchars($url)?></code><?php endforeach; ?>
			<p class="help-block">The WebGUI keeps its normal HTTPS behavior. On port 80, only the three PAC names are served; other requests keep the standard HTTP-to-HTTPS redirect.</p>
		<?php endif; ?>
	</div>
</div>

<form method="post" id="kid-wpad-form">
	<div class="panel panel-default">
		<div class="panel-heading"><h2 class="panel-title">Automatic proxy discovery</h2></div>
		<div class="panel-body">
			<div class="checkbox">
				<label><input type="checkbox" id="wpad_enable" name="wpad_enable" value="on" <?=($settings['wpad_enable'] ?? '') === 'on' ? 'checked="checked"' : ''?> /> <strong>Enable managed WPAD</strong></label>
			</div>
			<p class="help-block">Publishes one canonical <code>wpad.dat</code> and compatibility links <code>proxy.pac</code> and <code>wpad.da</code>. Kontrol-ID never overwrites unrelated files or existing DHCP/DNS records.</p>

			<div class="form-group kid-wpad-managed">
				<label for="wpad_interfaces">Internal service interfaces</label>
				<select class="form-control" id="wpad_interfaces" name="wpad_interfaces[]" multiple="multiple" size="8">
					<?php foreach ($catalog as $interface => $entry): ?>
					<option value="<?=htmlspecialchars($interface)?>" <?=in_array($interface, $selected, true) ? 'selected="selected"' : ''?>><?=htmlspecialchars($entry['description'] . ' — ' . $entry['ip'])?></option>
					<?php endforeach; ?>
				</select>
				<p class="help-block">Only configured internal IPv4 interfaces are offered; WAN is intentionally excluded. Use Ctrl/Command to select multiple interfaces.</p>
			</div>

			<div class="row kid-wpad-managed">
				<div class="col-sm-6">
					<div class="checkbox"><label><input type="checkbox" name="wpad_manage_dhcp" value="on" <?=($settings['wpad_manage_dhcp'] ?? '') === 'on' ? 'checked="checked"' : ''?> /> Manage DHCP Option 252</label></div>
					<p class="help-block">Each selected DHCP scope receives its own interface URL. A pre-existing unmanaged Option 252 is preserved and blocks activation instead of being overwritten.</p>
				</div>
				<div class="col-sm-6">
					<div class="checkbox"><label><input type="checkbox" name="wpad_manage_dns" value="on" <?=($settings['wpad_manage_dns'] ?? '') === 'on' ? 'checked="checked"' : ''?> /> Manage DNS <code>wpad</code> host override</label></div>
					<p class="help-block">Uses the active DNS Resolver or DNS Forwarder. An existing unmanaged <code>wpad</code> record is never replaced.</p>
				</div>
			</div>

			<div class="form-group kid-wpad-managed">
				<label for="wpad_domain">WPAD DNS domain</label>
				<input class="form-control" type="text" id="wpad_domain" name="wpad_domain" value="<?=htmlspecialchars($settings['wpad_domain'] ?? '')?>" placeholder="<?=htmlspecialchars(config_get_path('system/domain', 'example.com'))?>" />
				<p class="help-block">Leave empty to use the Kontrol system domain. The managed record will be <code>wpad.&lt;domain&gt;</code>.</p>
			</div>
		</div>
	</div>

	<div class="panel panel-default kid-wpad-managed">
		<div class="panel-heading"><h2 class="panel-title">PAC JavaScript editor</h2></div>
		<div class="panel-body">
			<textarea class="form-control kid-wpad-editor" id="wpad_pac" name="wpad_pac" spellcheck="false"><?=htmlspecialchars($pac_content)?></textarea>
			<p class="help-block">The file is compiled by QuickJS in syntax-only mode before it is published. JavaScript is never executed on the firewall. The update is atomic and is rolled back if validation or a managed service reload fails.</p>
			<button class="btn btn-default" type="submit" name="generate_default" value="1"><i class="fa fa-magic"></i> Generate recommended PAC</button>
		</div>
	</div>

	<button class="btn btn-primary" type="submit" name="save" value="1"><i class="fa fa-save"></i> Apply WPAD configuration</button>
</form>

<script type="text/javascript">
events.push(function() {
	function kidWpadVisibility() {
		$('.kid-wpad-managed').toggle($('#wpad_enable').is(':checked'));
	}
	$('#wpad_enable').on('change', kidWpadVisibility);
	kidWpadVisibility();
});
</script>

<?php include('foot.inc'); ?>
