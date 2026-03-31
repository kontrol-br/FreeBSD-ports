<?php
/*
 * openvpn_schedule.php
 * Package UI for OpenVPN per-user login schedules.
 */

require_once('guiconfig.inc');
require_once('/usr/local/pkg/openvpn_schedule.inc');

function openvpn_schedule_get_all_schedule_names() {
	$names = [];
	foreach (config_get_path('schedules/schedule', []) as $schedule) {
		$name = trim($schedule['name'] ?? '');
		if ($name !== '') {
			$names[] = $name;
		}
	}
	sort($names, SORT_NATURAL | SORT_FLAG_CASE);
	return $names;
}

function openvpn_schedule_get_admin_members() {
	$admins = [];
	foreach (config_get_path('system/group', []) as $group) {
		if (strcasecmp(($group['name'] ?? ''), 'admins') !== 0) {
			continue;
		}
		foreach (($group['member'] ?? []) as $member) {
			if ($member !== '') {
				$admins[$member] = true;
			}
		}
	}
	return $admins;
}

function openvpn_schedule_get_visible_users() {
	$visible_users = [];
	$admins = openvpn_schedule_get_admin_members();

	foreach (config_get_path('system/user', []) as $user) {
		$username = trim($user['name'] ?? '');
		if ($username === '') {
			continue;
		}
		if (isset($admins[$username])) {
			continue;
		}
		$visible_users[] = [
			'name' => $username,
			'descr' => trim($user['descr'] ?? ''),
		];
	}

	usort($visible_users, static function($a, $b) {
		return strcasecmp($a['name'], $b['name']);
	});

	return $visible_users;
}

function openvpn_schedule_build_user_map() {
	$map = [];
	$pkgcfg = config_get_path('installedpackages/openvpn_schedule/config/0', []);

	foreach (($pkgcfg['user_schedule'] ?? []) as $entry) {
		$username = trim($entry['username'] ?? '');
		$schedule = trim($entry['schedule'] ?? '');
		if ($username === '') {
			continue;
		}
		if ($schedule === '' || strcasecmp($schedule, 'none') === 0) {
			$map[$username] = 'none';
		} else {
			$map[$username] = $schedule;
		}
	}

	return $map;
}

$schedule_names = openvpn_schedule_get_all_schedule_names();
$schedule_options = ['none' => 'None'];
foreach ($schedule_names as $schedule_name) {
	$schedule_options[$schedule_name] = $schedule_name;
}

$users = openvpn_schedule_get_visible_users();
$current_user_map = openvpn_schedule_build_user_map();

if ($_POST) {
	$input_errors = [];
	$new_entries = [];

	foreach ($users as $user) {
		$username = $user['name'];
		$field_name = 'schedule_' . preg_replace('/[^a-zA-Z0-9_]/', '_', $username);
		$selected = trim($_POST[$field_name] ?? 'none');
		if ($selected === '') {
			$selected = 'none';
		}
		if (!array_key_exists($selected, $schedule_options)) {
			$input_errors[] = sprintf("Invalid schedule '%s' selected for user '%s'.", htmlspecialchars($selected), htmlspecialchars($username));
			continue;
		}
		if (strcasecmp($selected, 'none') !== 0) {
			$new_entries[] = [
				'username' => $username,
				'schedule' => $selected,
			];
			$current_user_map[$username] = $selected;
		} else {
			$current_user_map[$username] = 'none';
		}
	}

	if (empty($input_errors)) {
		config_set_path('installedpackages/openvpn_schedule/config/0/user_schedule', $new_entries);
		write_config('Updated OpenVPN per-user schedule settings');
		openvpn_schedule_sync();
		header(url_safe('Location: /openvpn_schedule.php?save=1'));
		exit;
	}
}

$pgtitle = ['VPN', 'OpenVPN Schedule'];
include('head.inc');

if ($_GET['save'] ?? false) {
	print_info_box('Settings saved successfully.', 'success');
}
if (!empty($input_errors)) {
	print_input_errors($input_errors);
}

$form = new Form(false);
$section = new Form_Section('OpenVPN User Schedules');
$section->addInput(new Form_StaticText(
	'About',
	'Assign an existing schedule to each OpenVPN user. Select "None" to allow login at any time.'
));

if (empty($schedule_names)) {
	$section->addInput(new Form_StaticText(
		'Schedules',
		'No schedules were found under Firewall > Schedules. All users are currently set to "None".'
	));
}

foreach ($users as $user) {
	$label = $user['name'];
	if ($user['descr'] !== '') {
		$label .= ' (' . $user['descr'] . ')';
	}
	$field_name = 'schedule_' . preg_replace('/[^a-zA-Z0-9_]/', '_', $user['name']);
	$current_value = $current_user_map[$user['name']] ?? 'none';
	if (!array_key_exists($current_value, $schedule_options)) {
		$current_value = 'none';
	}

	$section->addInput(new Form_Select(
		$field_name,
		$label,
		$current_value,
		$schedule_options
	))->setHelp('Leave as "None" to keep OpenVPN access allowed at all times.');
}

if (empty($users)) {
	$section->addInput(new Form_StaticText(
		'Users',
		'No non-admin local users were found.'
	));
}

$form->add($section);
print($form);
include('foot.inc');
