<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'AD Policy Mapping');
$savemsg = '';
$input_errors = array();
$discovered_groups = array();
$posted_mappings = null;

function kontrol_id_policy_post_rows() {
	$ad_groups = is_array($_POST['ad_group'] ?? null) ? $_POST['ad_group'] : array();
	$e2guardian_groups = is_array($_POST['e2guardian_group'] ?? null) ? $_POST['e2guardian_group'] : array();
	$rows = array();
	$maximum = max(count($ad_groups), count($e2guardian_groups));
	for ($index = 0; $index < $maximum; $index++) {
		$ad_group = trim((string)($ad_groups[$index] ?? ''));
		$e2guardian_group = trim((string)($e2guardian_groups[$index] ?? ''));
		if ($ad_group === '' && $e2guardian_group === '') {
			continue;
		}
		$rows[] = array('ad_group' => $ad_group, 'e2guardian_group' => $e2guardian_group);
	}
	return $rows;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	try {
		if (isset($_POST['discover_groups'])) {
			$posted_mappings = kontrol_id_policy_post_rows();
			$discovered_groups = kontrol_id_ad_group_names();
			$savemsg = sprintf('Kerberos query completed. %d Active Directory groups are available.', count($discovered_groups));
		} elseif (isset($_POST['sync_now'])) {
			$result = kontrol_id_sync_ad_policies(true);
			if (empty($result['ok'])) {
				throw new Exception($result['message']);
			}
			$savemsg = $result['message'];
		} elseif (isset($_POST['save_policy'])) {
			$settings = kontrol_id_get_settings();
			$enabled = ($_POST['ad_policy_enable'] ?? '') === 'on';
			$ttl = filter_var($_POST['ad_policy_cache_ttl'] ?? '', FILTER_VALIDATE_INT,
				array('options' => array('min_range' => 60, 'max_range' => 86400)));
			if ($ttl === false) {
				$input_errors[] = 'Cache TTL must be between 60 and 86400 seconds.';
			}
			$rows = kontrol_id_policy_post_rows();
			$available = kontrol_id_e2guardian_group_options();
			$seen_ad = array();
			$seen_e2g = array();
			foreach ($rows as $index => $row) {
				$number = $index + 1;
				if ($row['ad_group'] === '' || $row['e2guardian_group'] === '') {
					$input_errors[] = "Mapping {$number} must select both an AD group and an E2Guardian group.";
					continue;
				}
				if (strlen($row['ad_group']) > 128 || preg_match('/[\x00-\x1f\x7f]/', $row['ad_group'])) {
					$input_errors[] = "Mapping {$number} has an invalid AD group name.";
				}
				if (!array_key_exists($row['e2guardian_group'], $available)) {
					$input_errors[] = "Mapping {$number} references an E2Guardian group that no longer exists.";
				}
				$ad_key = strtolower($row['ad_group']);
				$e2g_key = strtolower($row['e2guardian_group']);
				if (isset($seen_ad[$ad_key])) {
					$input_errors[] = "AD group '{$row['ad_group']}' is mapped more than once.";
				}
				if (isset($seen_e2g[$e2g_key])) {
					$input_errors[] = "E2Guardian group '{$row['e2guardian_group']}' is mapped more than once.";
				}
				$seen_ad[$ad_key] = true;
				$seen_e2g[$e2g_key] = true;
			}
			if ($enabled && empty($rows)) {
				$input_errors[] = 'Add at least one mapping before enabling Active Directory policies.';
			}
			if ($enabled && !is_file(KONTROL_ID_SAMBA_PRIVATE . '/secrets.tdb')) {
				$input_errors[] = 'Join this appliance to Active Directory before enabling policy mapping.';
			}
			if (empty($input_errors)) {
				$settings['ad_policy_enable'] = $enabled ? 'on' : '';
				$settings['ad_policy_cache_ttl'] = (string)$ttl;
				$settings['ad_policy_mappings'] = base64_encode(json_encode($rows));
				kontrol_id_set_settings($settings);
				write_config('Kontrol-ID Active Directory policy mappings updated.');
				kontrol_id_policy_schedule();
				$savemsg = 'Active Directory policy mappings were saved.';
				if ($enabled) {
					$result = kontrol_id_sync_ad_policies(true);
					if (empty($result['ok'])) {
						$input_errors[] = 'The settings were saved, but the first synchronization failed: ' . $result['message'];
					} else {
						$savemsg .= ' ' . $result['message'];
					}
				}
			}
		}
	} catch (Throwable $exception) {
		$input_errors[] = $exception->getMessage();
	}
}

$settings = kontrol_id_get_settings();
$mappings = is_array($posted_mappings) ? $posted_mappings : kontrol_id_policy_mappings($settings);
if (empty($mappings)) {
	$mappings[] = array('ad_group' => '', 'e2guardian_group' => '');
}
$e2guardian_groups = kontrol_id_e2guardian_group_options();
$cache = kontrol_id_policy_cache();
$status = kontrol_id_policy_status();
$domain_joined = is_file(KONTROL_ID_SAMBA_PRIVATE . '/secrets.tdb');

include('head.inc');
display_top_tabs(kontrol_id_tabs('policy'));

if (!empty($input_errors)) {
	print_input_errors($input_errors);
}
if (!empty($savemsg)) {
	print_info_box($savemsg, 'success');
}

?>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Kerberos Active Directory Policy Mapping</h2></div>
	<div class="panel-body">
		<p>Kontrol-ID uses the appliance machine trust and requires Kerberos for every directory query. No additional LDAP bind account or password is stored. Mappings are evaluated from top to bottom; the first mapping wins when a user belongs to more than one AD group.</p>
		<div class="alert <?=$domain_joined ? 'alert-success' : 'alert-warning'?>">
			<strong>Domain trust:</strong> <?=$domain_joined ? 'available' : 'not configured'?>.
			<?php if (!$domain_joined): ?>Complete <a href="/kontrol_id_domain.php">Domain and Keytab</a> before enabling this feature.<?php endif; ?>
		</div>
		<?php if (empty($e2guardian_groups)): ?>
		<div class="alert alert-warning">Create at least one policy under <a href="/pkg.php?xml=e2guardian_groups.xml">Services &gt; E2Guardian &gt; Groups</a> before configuring an AD mapping.</div>
		<?php endif; ?>
		<div class="alert alert-info">The first E2Guardian group is the default/fallback policy for every identity that does not match another mapping. It can also be selected explicitly for an AD group; place a broad mapping such as Domain Users after more specific policies because the first matching row wins.</div>
		<form method="post" class="form-horizontal">
			<div class="form-group">
				<label class="col-sm-3 control-label">Enable AD policy mapping</label>
				<div class="col-sm-8"><label class="checkbox-inline"><input type="checkbox" name="ad_policy_enable" value="on" <?=($settings['ad_policy_enable'] ?? '') === 'on' ? 'checked' : ''?>> Use Kerberos machine trust to maintain E2Guardian policy membership</label></div>
			</div>
			<div class="form-group">
				<label class="col-sm-3 control-label" for="ad_policy_cache_ttl">Membership cache TTL</label>
				<div class="col-sm-3"><div class="input-group"><input class="form-control" id="ad_policy_cache_ttl" name="ad_policy_cache_ttl" type="number" min="60" max="86400" value="<?=htmlspecialchars($settings['ad_policy_cache_ttl'] ?? '300')?>"><span class="input-group-addon">seconds</span></div><span class="help-block">The cron checks every minute but contacts AD only when this cache expires.</span></div>
			</div>
			<table class="table table-striped table-hover" id="policy-mappings">
				<thead><tr><th style="width:45%">Active Directory group</th><th style="width:45%">E2Guardian policy group</th><th style="width:10%"></th></tr></thead>
				<tbody>
				<?php foreach ($mappings as $mapping): ?>
				<tr>
					<td><div class="kontrol-id-ad-picker"><input class="form-control kontrol-id-ad-group" name="ad_group[]" maxlength="128" value="<?=htmlspecialchars($mapping['ad_group'])?>" placeholder="Domain Users" autocomplete="off" aria-autocomplete="list" aria-expanded="false"><div class="kontrol-id-ad-options" role="listbox" hidden></div></div></td>
					<td><div class="kontrol-id-policy-picker"><select class="form-control kontrol-id-policy-group" name="e2guardian_group[]"><option value="">Select policy group</option><?php foreach ($e2guardian_groups as $name => $description): ?><option value="<?=htmlspecialchars($name)?>" <?=$mapping['e2guardian_group'] === $name ? 'selected' : ''?>><?=htmlspecialchars($name)?></option><?php endforeach; ?></select></div></td>
					<td><button class="btn btn-danger btn-sm remove-policy-row" type="button"><i class="fa fa-trash"></i></button></td>
				</tr>
				<?php endforeach; ?>
				</tbody>
			</table>
			<div class="form-group"><div class="col-sm-offset-3 col-sm-8">
				<button class="btn btn-default" id="add-policy-row" type="button"><i class="fa fa-plus"></i> Add mapping</button>
				<button class="btn btn-default" name="discover_groups" value="1" type="submit"><i class="fa fa-search"></i> Discover AD groups</button>
				<button class="btn btn-primary" name="save_policy" value="1" type="submit"><i class="fa fa-save"></i> Save and apply</button>
			</div></div>
		</form>
	</div>
</div>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Synchronization Status</h2></div>
	<div class="panel-body">
		<dl class="dl-horizontal">
			<dt>Source</dt><dd><?=htmlspecialchars($cache['source'] ?? 'No successful synchronization')?></dd>
			<dt>Last attempt</dt><dd><?=!empty($status['last_attempt']) ? htmlspecialchars(date('Y-m-d H:i:s', $status['last_attempt'])) : 'Never'?></dd>
			<dt>Last success</dt><dd><?=!empty($status['last_success']) ? htmlspecialchars(date('Y-m-d H:i:s', $status['last_success'])) : 'Never'?></dd>
			<dt>Result</dt><dd class="<?=isset($status['ok']) && !$status['ok'] ? 'text-danger' : 'text-success'?>"><?=htmlspecialchars($status['message'] ?? 'Not synchronized yet')?></dd>
		</dl>
		<form method="post"><button class="btn btn-info" name="sync_now" value="1" type="submit" <?=!kontrol_id_policy_enabled($settings) ? 'disabled' : ''?>><i class="fa fa-refresh"></i> Synchronize now</button></form>
		<br>
		<table class="table table-striped table-condensed">
			<thead><tr><th>AD group</th><th>E2Guardian policy</th><th>Accounts</th><th>Cached identities</th></tr></thead>
			<tbody>
			<?php foreach ((array)($cache['mappings'] ?? array()) as $mapping): $identity_count = 0; foreach ((array)($mapping['members'] ?? array()) as $member) { $identity_count += count((array)($member['identities'] ?? array())); } ?>
			<tr><td><?=htmlspecialchars($mapping['ad_group'] ?? '')?></td><td><?=htmlspecialchars($mapping['e2guardian_group'] ?? '')?></td><td><?=intval($mapping['account_count'] ?? 0)?></td><td><?=$identity_count?></td></tr>
			<?php endforeach; ?>
			<?php if (empty($cache['mappings'])): ?><tr><td colspan="4">No membership cache is available.</td></tr><?php endif; ?>
			</tbody>
		</table>
	</div>
</div>

<style>
.kontrol-id-ad-picker,
.kontrol-id-policy-picker {
	position: relative;
}
.kontrol-id-ad-picker::after,
.kontrol-id-policy-picker::after {
	content: '';
	position: absolute;
	top: 15px;
	right: 12px;
	width: 0;
	height: 0;
	border-left: 4px solid transparent;
	border-right: 4px solid transparent;
	border-top: 5px solid #4c5964;
	pointer-events: none;
}
.kontrol-id-ad-group,
.kontrol-id-policy-group {
	height: 34px;
	font-size: 13px;
	line-height: 1.42857143;
	padding-right: 30px;
}
.kontrol-id-policy-group {
	-webkit-appearance: none;
	-moz-appearance: none;
	appearance: none;
	background-image: none;
}
.kontrol-id-ad-options {
	margin-top: 2px;
	max-height: 275px;
	overflow-y: auto;
	background: #fff;
	border: 1px solid #b8c2cc;
	border-radius: 3px;
	box-shadow: 0 4px 12px rgba(0, 0, 0, .16);
}
.kontrol-id-ad-option {
	display: block;
	width: 100%;
	padding: 5px 9px;
	border: 0;
	background: transparent;
	color: #263238;
	font-size: 12px;
	font-weight: 400;
	line-height: 1.25;
	text-align: left;
	white-space: nowrap;
	overflow: hidden;
	text-overflow: ellipsis;
}
.kontrol-id-ad-option:hover,
.kontrol-id-ad-option.is-active {
	background: #e8f3fb;
	color: #0b5f94;
}
.kontrol-id-ad-empty {
	padding: 6px 9px;
	color: #6c757d;
	font-size: 12px;
	line-height: 1.25;
}
</style>

<script>
events.push(function () {
	var adGroups = <?=json_encode(array_values($discovered_groups), JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT)?>;

	function closeAdPickers(except) {
		$('.kontrol-id-ad-picker').each(function () {
			if (except && this === except) {
				return;
			}
			$(this).find('.kontrol-id-ad-options').prop('hidden', true).empty();
			$(this).find('.kontrol-id-ad-group').attr('aria-expanded', 'false');
		});
	}

	function renderAdOptions(input) {
		var picker = input.closest('.kontrol-id-ad-picker');
		var options = picker.find('.kontrol-id-ad-options');
		var query = $.trim(input.val()).toLocaleLowerCase();
		var matches = adGroups.filter(function (group) {
			return query === '' || group.toLocaleLowerCase().indexOf(query) !== -1;
		});
		closeAdPickers(picker[0]);
		options.empty().data('active', -1);
		if (!matches.length) {
			$('<div class="kontrol-id-ad-empty">').text(adGroups.length ? 'No matching AD groups' : 'Use Discover AD groups to load the directory').appendTo(options);
		} else {
			matches.forEach(function (group) {
				$('<button type="button" class="kontrol-id-ad-option" role="option">').text(group).attr('title', group).appendTo(options);
			});
		}
		options.prop('hidden', false);
		input.attr('aria-expanded', 'true');
	}

	function activateAdOption(input, direction) {
		var options = input.closest('.kontrol-id-ad-picker').find('.kontrol-id-ad-options');
		var items = options.find('.kontrol-id-ad-option');
		if (!items.length) {
			return;
		}
		var active = Number(options.data('active'));
		active = (active + direction + items.length) % items.length;
		items.removeClass('is-active').attr('aria-selected', 'false');
		var selected = items.eq(active).addClass('is-active').attr('aria-selected', 'true');
		options.data('active', active);
		selected[0].scrollIntoView({block: 'nearest'});
	}

	$('#add-policy-row').on('click', function () {
		var row = $('#policy-mappings tbody tr:last').clone();
		row.find('input').val('');
		row.find('select').val('');
		row.find('.kontrol-id-ad-options').prop('hidden', true).empty();
		row.find('.kontrol-id-ad-group').attr('aria-expanded', 'false');
		$('#policy-mappings tbody').append(row);
	});
	$(document).on('focus input click', '.kontrol-id-ad-group', function () {
		renderAdOptions($(this));
	});
	$(document).on('keydown', '.kontrol-id-ad-group', function (event) {
		var input = $(this);
		var options = input.closest('.kontrol-id-ad-picker').find('.kontrol-id-ad-options');
		if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
			event.preventDefault();
			if (options.prop('hidden')) {
				renderAdOptions(input);
			}
			activateAdOption(input, event.key === 'ArrowDown' ? 1 : -1);
		} else if (event.key === 'Enter' && !options.prop('hidden')) {
			var active = options.find('.kontrol-id-ad-option.is-active');
			if (active.length) {
				event.preventDefault();
				input.val(active.text());
				closeAdPickers();
			}
		} else if (event.key === 'Escape') {
			closeAdPickers();
		}
	});
	$(document).on('mouseenter', '.kontrol-id-ad-option', function () {
		var item = $(this);
		var options = item.closest('.kontrol-id-ad-options');
		options.find('.kontrol-id-ad-option').removeClass('is-active').attr('aria-selected', 'false');
		item.addClass('is-active').attr('aria-selected', 'true');
		options.data('active', item.index());
	});
	$(document).on('mousedown', '.kontrol-id-ad-option', function (event) {
		event.preventDefault();
		var item = $(this);
		item.closest('.kontrol-id-ad-picker').find('.kontrol-id-ad-group').val(item.text()).focus();
		closeAdPickers();
	});
	$(document).on('mousedown', function (event) {
		if (!$(event.target).closest('.kontrol-id-ad-picker').length) {
			closeAdPickers();
		}
	});
	$(document).on('click', '.remove-policy-row', function () {
		if ($('#policy-mappings tbody tr').length > 1) {
			$(this).closest('tr').remove();
		} else {
			$(this).closest('tr').find('input,select').val('');
		}
	});
});
</script>

<?php include('foot.inc'); ?>
