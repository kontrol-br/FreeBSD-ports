<?php
/*
 * openvpn_schedule.php
 * Package UI for OpenVPN schedule login control.
 */

require_once('guiconfig.inc');
require_once('/usr/local/pkg/openvpn_schedule.inc');

$pconfig = config_get_path('installedpackages/openvpn_schedule/config/0', []);
if (!isset($pconfig['default_policy'])) {
	$pconfig['default_policy'] = 'allow';
}
if (!isset($pconfig['rule'])) {
	$pconfig['rule'] = [];
}

if ($_POST) {
	$input_errors = [];
	$rules = [];
	$line_no = 0;
	foreach (explode("\n", $_POST['rules_csv'] ?? '') as $line) {
		$line_no++;
		$line = trim($line);
		if ($line === '') {
			continue;
		}
		$parts = array_map('trim', explode(',', $line));
		if (count($parts) !== 3) {
			$input_errors[] = "Linha {$line_no}: formato inválido. Use tipo,nome,schedule.";
			continue;
		}
		[$type, $name, $schedule] = $parts;
		if (!in_array($type, ['user', 'group'], true)) {
			$input_errors[] = "Linha {$line_no}: tipo deve ser user ou group.";
		}
		if ($name === '' || $schedule === '') {
			$input_errors[] = "Linha {$line_no}: nome e schedule são obrigatórios.";
		}
		$rules[] = ['type' => $type, 'name' => $name, 'schedule' => $schedule];
	}

	if (empty($input_errors)) {
		$pconfig['default_policy'] = ($_POST['default_policy'] ?? 'allow') === 'deny' ? 'deny' : 'allow';
		$pconfig['rule'] = $rules;
		config_set_path('installedpackages/openvpn_schedule/config/0', $pconfig);
		write_config('Updated OpenVPN schedule package settings');
		openvpn_schedule_sync();
		header(url_safe('Location: /openvpn_schedule.php?save=1'));
		exit;
	}
}

$rules_csv = "";
foreach (($pconfig['rule'] ?? []) as $rule) {
	$rules_csv .= sprintf("%s,%s,%s\n", $rule['type'] ?? '', $rule['name'] ?? '', $rule['schedule'] ?? '');
}

$pgtitle = ['VPN', 'OpenVPN Schedule'];
include('head.inc');

if ($_GET['save'] ?? false) {
	print_info_box('Configuração salva com sucesso.', 'success');
}
if (!empty($input_errors)) {
	print_input_errors($input_errors);
}

$form = new Form(false);
$section = new Form_Section('Política de Login OpenVPN por Horário');

$section->addInput(new Form_Select(
	'default_policy',
	'Política default',
	$pconfig['default_policy'],
	[
		'allow' => 'Permitir se não houver regra (recomendado)',
		'deny' => 'Negar se não houver regra',
	]
));

$section->addInput(new Form_Textarea(
	'rules_csv',
	'Regras (uma por linha)',
	$rules_csv
))->setHelp('Formato: tipo,nome,schedule. Ex.: user,alice,Comercial | group,vpn-users,Noite');

$section->addInput(new Form_StaticText(
	'Prioridade de decisão',
	'1) regra de usuário, 2) regra de grupo, 3) política default.'
));

$section->addInput(new Form_StaticText(
	'Observação',
	'Este package atua apenas no fluxo de autenticação do OpenVPN e reutiliza schedules já existentes do sistema.'
));

$form->add($section);
print($form);
include('foot.inc');
