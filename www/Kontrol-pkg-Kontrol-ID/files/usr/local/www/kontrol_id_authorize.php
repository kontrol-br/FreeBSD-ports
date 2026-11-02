<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

$pgtitle = array('Services', 'Kontrol-ID', 'Authorize Station');
$input_errors = array();

function kontrol_id_authorize_b64url($bytes) {
	return rtrim(strtr(base64_encode($bytes), '+/', '-_'), '=');
}

function kontrol_id_authorize_request(&$input_errors) {
	$values = array(
		'state' => trim($_REQUEST['state'] ?? ''),
		'challenge' => trim($_REQUEST['challenge'] ?? ''),
		'port' => (int)($_REQUEST['port'] ?? 0),
		'hostname' => strtolower(trim($_REQUEST['hostname'] ?? '')),
		'fqdn' => strtolower(trim($_REQUEST['fqdn'] ?? ''))
	);
	if (!preg_match('/^[A-Za-z0-9_-]{43,86}$/', $values['state']) ||
		!preg_match('/^[A-Za-z0-9_-]{43}$/', $values['challenge']) ||
		$values['port'] < 49152 || $values['port'] > 65535 ||
		!preg_match('/^[a-z0-9][a-z0-9-]{0,62}$/', $values['hostname']) ||
		($values['fqdn'] !== '' && !preg_match('/^[a-z0-9][a-z0-9.-]{0,252}$/', $values['fqdn']))) {
		$input_errors[] = 'The station authorization request is invalid or incomplete.';
	}
	return $values;
}

$request = kontrol_id_authorize_request($input_errors);
if ($_SERVER['REQUEST_METHOD'] === 'POST' && empty($input_errors)) {
	$callback = 'http://127.0.0.1:' . $request['port'] . '/callback/';
	if (isset($_POST['cancel'])) {
		header('Location: ' . $callback . '?' . http_build_query(array(
			'error' => 'access_denied',
			'state' => $request['state']
		)));
		exit;
	}
	if (isset($_POST['approve'])) {
		$webcert_ref = config_get_path('system/webgui/ssl-certref');
		$webcert = $webcert_ref ? kontrol_id_extract_item(lookup_cert($webcert_ref)) : array();
		$fingerprint = !empty($webcert['crt']) ?
			strtoupper(openssl_x509_fingerprint(base64_decode($webcert['crt']), 'sha256')) : '';
		if ($fingerprint === '') {
			$input_errors[] = 'The WebGUI certificate is unavailable.';
		} else {
			kontrol_id_ensure_storage();
			$code = kontrol_id_authorize_b64url(random_bytes(32));
			$now = time();
			$db = new SQLite3(KONTROL_ID_DB);
			$db->busyTimeout(1000);
			$db->exec('DELETE FROM enrollment_authorizations WHERE expires_at<' . (int)$now . ' OR (consumed<>0 AND created_at<' . (int)($now - 3600) . ')');
			$stmt = $db->prepare('INSERT INTO enrollment_authorizations ' .
				'(code_hash,pkce_challenge,hostname,fqdn,approved_by,created_at,expires_at,consumed) ' .
				'VALUES(:code_hash,:challenge,:hostname,:fqdn,:approved_by,:created_at,:expires_at,0)');
			$stmt->bindValue(':code_hash', hash('sha256', $code), SQLITE3_TEXT);
			$stmt->bindValue(':challenge', $request['challenge'], SQLITE3_TEXT);
			$stmt->bindValue(':hostname', $request['hostname'], SQLITE3_TEXT);
			$stmt->bindValue(':fqdn', $request['fqdn'], SQLITE3_TEXT);
			$stmt->bindValue(':approved_by', $_SESSION['Username'] ?? 'unknown', SQLITE3_TEXT);
			$stmt->bindValue(':created_at', $now, SQLITE3_INTEGER);
			$stmt->bindValue(':expires_at', $now + 300, SQLITE3_INTEGER);
			$stored = $stmt->execute();
			$db->close();
			if ($stored === false) {
				$input_errors[] = 'Unable to create the station authorization.';
			} else {
				log_error('[Kontrol-ID] Station enrollment authorized by ' . ($_SESSION['Username'] ?? 'unknown') .
					': ' . $request['hostname']);
				header('Location: ' . $callback . '?' . http_build_query(array(
					'code' => $code,
					'state' => $request['state'],
					'fingerprint' => $fingerprint
				)));
				exit;
			}
		}
	}
}

include('head.inc');
if (!empty($input_errors)) {
	print_input_errors($input_errors);
}
?>

<div class="panel panel-default">
	<div class="panel-heading"><h2 class="panel-title">Authorize Kontrol-ID station</h2></div>
	<div class="panel-body">
		<p>A Windows Provisioner is requesting permission to enroll this station:</p>
		<dl class="dl-horizontal">
			<dt>Hostname</dt><dd><?=htmlspecialchars($request['hostname'])?></dd>
			<dt>FQDN</dt><dd><?=htmlspecialchars($request['fqdn'] ?: 'Not reported')?></dd>
			<dt>Authorization</dt><dd>One operation, valid for five minutes</dd>
		</dl>
		<div class="alert alert-info">Approve only if you started the Kontrol-ID Provisioner on this station. No administrator password will be sent to the application.</div>
		<form method="post">
			<?php foreach ($request as $name => $value): ?>
			<input type="hidden" name="<?=htmlspecialchars($name)?>" value="<?=htmlspecialchars($value)?>">
			<?php endforeach; ?>
			<button class="btn btn-primary" type="submit" name="approve" value="1"><i class="fa fa-check"></i> Authorize station</button>
			<button class="btn btn-default" type="submit" name="cancel" value="1">Cancel</button>
		</form>
	</div>
</div>

<?php include('foot.inc'); ?>
