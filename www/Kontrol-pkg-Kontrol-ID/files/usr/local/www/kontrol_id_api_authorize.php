<?php

$nocsrf = true;
require_once('config.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('X-Content-Type-Options: nosniff');

function kontrol_id_api_authorize_response($status, $payload) {
	http_response_code($status);
	echo json_encode($payload, JSON_UNESCAPED_SLASHES);
	exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
	kontrol_id_api_authorize_response(405, array('ok' => false, 'error' => 'POST required'));
}
if (empty($_SERVER['HTTPS']) || $_SERVER['HTTPS'] === 'off') {
	kontrol_id_api_authorize_response(400, array('ok' => false, 'error' => 'HTTPS is required'));
}

$body = file_get_contents('php://input');
if (!is_string($body) || strlen($body) > 16384) {
	kontrol_id_api_authorize_response(413, array('ok' => false, 'error' => 'Request is too large'));
}
$request = json_decode($body, true);
if (!is_array($request)) {
	kontrol_id_api_authorize_response(400, array('ok' => false, 'error' => 'Invalid request'));
}
$body = str_repeat("\0", strlen($body));

$username = trim((string)($request['username'] ?? ''));
$password = (string)($request['password'] ?? '');
$hostname = strtolower(trim((string)($request['hostname'] ?? '')));
$fqdn = strtolower(trim((string)($request['fqdn'] ?? '')));
$challenge = trim((string)($request['codeChallenge'] ?? ''));
unset($request['password']);
if (!preg_match('/^[A-Za-z0-9_.@\\-\\\\]{1,255}$/D', $username) ||
	$password === '' || strlen($password) > 4096 ||
	!preg_match('/^[a-z0-9][a-z0-9-]{0,62}$/', $hostname) ||
	($fqdn !== '' && !preg_match('/^[a-z0-9][a-z0-9.-]{0,252}$/', $fqdn)) ||
	!preg_match('/^[A-Za-z0-9_-]{43}$/', $challenge)) {
	$password = str_repeat("\0", strlen($password));
	kontrol_id_api_authorize_response(400, array('ok' => false, 'error' => 'Invalid authorization request'));
}

$remote_ip = $_SERVER['REMOTE_ADDR'] ?? '';
if (filter_var($remote_ip, FILTER_VALIDATE_IP) === false) {
	$password = str_repeat("\0", strlen($password));
	kontrol_id_api_authorize_response(400, array('ok' => false, 'error' => 'Invalid source address'));
}

kontrol_id_ensure_storage();
$db = new SQLite3(KONTROL_ID_DB);
$db->busyTimeout(1000);
$cutoff = time() - 300;
$stmt = $db->prepare('SELECT COUNT(*) FROM provisioner_auth_attempts WHERE ip=:ip AND success=0 AND attempted_at>=:cutoff');
$stmt->bindValue(':ip', $remote_ip, SQLITE3_TEXT);
$stmt->bindValue(':cutoff', $cutoff, SQLITE3_INTEGER);
$result = $stmt->execute();
$failures = $result ? (int)$result->fetchArray(SQLITE3_NUM)[0] : 0;
if ($failures >= 10) {
	$db->close();
	$password = str_repeat("\0", strlen($password));
	kontrol_id_api_authorize_response(429, array('ok' => false, 'error' => 'Too many authorization attempts'));
}

$authorized = kontrol_id_authenticate_administrator($username, $password);
$password = str_repeat("\0", strlen($password));
$attempt = $db->prepare('INSERT INTO provisioner_auth_attempts(ip,attempted_at,success) VALUES(:ip,:now,:success)');
$attempt->bindValue(':ip', $remote_ip, SQLITE3_TEXT);
$attempt->bindValue(':now', time(), SQLITE3_INTEGER);
$attempt->bindValue(':success', $authorized ? 1 : 0, SQLITE3_INTEGER);
$attempt->execute();
$db->exec('DELETE FROM provisioner_auth_attempts WHERE attempted_at<' . (int)(time() - 86400));
if (!$authorized) {
	$db->close();
	log_auth_event(LOG_AUTH_EVENT_ERROR, cleanup_invalid_username($username), $remote_ip);
	kontrol_id_api_authorize_response(401, array('ok' => false, 'error' => 'Invalid username or password'));
}

$webcert_ref = config_get_path('system/webgui/ssl-certref');
$webcert = $webcert_ref ? kontrol_id_extract_item(lookup_cert($webcert_ref)) : array();
$fingerprint = !empty($webcert['crt']) ?
	strtoupper(openssl_x509_fingerprint(base64_decode($webcert['crt']), 'sha256')) : '';
if ($fingerprint === '') {
	$db->close();
	kontrol_id_api_authorize_response(503, array('ok' => false, 'error' => 'Provisioning certificate is unavailable'));
}

$code = rtrim(strtr(base64_encode(random_bytes(32)), '+/', '-_'), '=');
$now = time();
$db->exec('DELETE FROM enrollment_authorizations WHERE expires_at<' . (int)$now .
	' OR (consumed<>0 AND created_at<' . (int)($now - 3600) . ')');
$stmt = $db->prepare('INSERT INTO enrollment_authorizations ' .
	'(code_hash,pkce_challenge,hostname,fqdn,approved_by,created_at,expires_at,consumed) ' .
	'VALUES(:code_hash,:challenge,:hostname,:fqdn,:approved_by,:created_at,:expires_at,0)');
$stmt->bindValue(':code_hash', hash('sha256', $code), SQLITE3_TEXT);
$stmt->bindValue(':challenge', $challenge, SQLITE3_TEXT);
$stmt->bindValue(':hostname', $hostname, SQLITE3_TEXT);
$stmt->bindValue(':fqdn', $fqdn, SQLITE3_TEXT);
$stmt->bindValue(':approved_by', $username, SQLITE3_TEXT);
$stmt->bindValue(':created_at', $now, SQLITE3_INTEGER);
$stmt->bindValue(':expires_at', $now + 300, SQLITE3_INTEGER);
$stored = $stmt->execute();
$db->close();
if ($stored === false) {
	kontrol_id_api_authorize_response(503, array('ok' => false, 'error' => 'Unable to authorize station enrollment'));
}

log_auth_event(LOG_AUTH_EVENT_LOGIN, cleanup_invalid_username($username),
	$remote_ip . ' authorized Kontrol-ID enrollment for ' . $hostname);
kontrol_id_api_authorize_response(200, array(
	'ok' => true,
	'authorizationCode' => $code,
	'serverCertificateSha256' => $fingerprint,
	'expiresIn' => 300
));

?>
