<?php

$nocsrf = true;
require_once('config.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

function kontrol_id_api_response($status, $payload) {
	http_response_code($status);
	echo json_encode($payload, JSON_UNESCAPED_SLASHES);
	exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
	kontrol_id_api_response(405, array('ok' => false, 'error' => 'POST required'));
}
if (empty($_SERVER['HTTPS']) || $_SERVER['HTTPS'] === 'off') {
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'HTTPS is required'));
}

$body = file_get_contents('php://input');
if (strlen($body) > 65536) {
	kontrol_id_api_response(413, array('ok' => false, 'error' => 'Request is too large'));
}
$request = json_decode($body, true);
if (!is_array($request)) {
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'Invalid JSON'));
}

$remote_ip = $_SERVER['REMOTE_ADDR'] ?? '';
if (filter_var($remote_ip, FILTER_VALIDATE_IP) === false) {
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'Invalid source address'));
}
kontrol_id_ensure_storage();
$db = new SQLite3(KONTROL_ID_DB);
$db->busyTimeout(1000);
$cutoff = time() - 300;
$stmt = $db->prepare('SELECT COUNT(*) FROM auth_attempts WHERE ip=:ip AND attempted_at>:cutoff AND success=0');
$stmt->bindValue(':ip', $remote_ip, SQLITE3_TEXT);
$stmt->bindValue(':cutoff', $cutoff, SQLITE3_INTEGER);
$failures = (int)$stmt->execute()->fetchArray(SQLITE3_NUM)[0];
if ($failures >= 10) {
	$db->close();
	kontrol_id_api_response(429, array('ok' => false, 'error' => 'Too many enrollment attempts'));
}

$valid_token = kontrol_id_token_valid($request['token'] ?? '');
$attempt = $db->prepare('INSERT INTO auth_attempts(ip,attempted_at,success) VALUES(:ip,:now,:success)');
$attempt->bindValue(':ip', $remote_ip, SQLITE3_TEXT);
$attempt->bindValue(':now', time(), SQLITE3_INTEGER);
$attempt->bindValue(':success', $valid_token ? 1 : 0, SQLITE3_INTEGER);
$attempt->execute();
if (!$valid_token) {
	$db->close();
	kontrol_id_api_response(403, array('ok' => false, 'error' => 'Invalid or expired enrollment token'));
}

$hostname = strtolower(trim($request['hostname'] ?? ''));
$fqdn = strtolower(trim($request['fqdn'] ?? ''));
if (!preg_match('/^[a-z0-9][a-z0-9-]{0,62}$/', $hostname)) {
	$db->close();
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'Invalid hostname'));
}
if (!empty($fqdn) && !preg_match('/^[a-z0-9][a-z0-9.-]{0,252}$/', $fqdn)) {
	$db->close();
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'Invalid FQDN'));
}

$csr_pem = $request['csr'] ?? '';
if (!is_string($csr_pem) || strlen($csr_pem) > 32768 || strpos($csr_pem, 'CERTIFICATE REQUEST') === false) {
	$db->close();
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'Invalid certificate request'));
}
$subject = openssl_csr_get_subject($csr_pem, true);
if (!is_array($subject) || strcasecmp($subject['CN'] ?? '', $hostname) !== 0) {
	$db->close();
	kontrol_id_api_response(400, array('ok' => false, 'error' => 'CSR common name does not match hostname'));
}

$settings = kontrol_id_get_settings();
$ca_lookup = lookup_ca($settings['ca_ref']);
$ca = kontrol_id_extract_item($ca_lookup);
if (empty($ca['crt']) || empty($ca['prv'])) {
	$db->close();
	kontrol_id_api_response(500, array('ok' => false, 'error' => 'Kontrol-ID CA is unavailable'));
}
$san = array('DNS:' . $hostname);
if (!empty($fqdn) && $fqdn !== $hostname) {
	$san[] = 'DNS:' . $fqdn;
}
$san[] = 'IP:' . $remote_ip;
$certificate = csr_sign($csr_pem, $ca, 825, 'server', implode(', ', $san), 'sha256');
if (empty($certificate)) {
	$db->close();
	kontrol_id_api_response(500, array('ok' => false, 'error' => 'Unable to sign station certificate'));
}
if (is_array($ca_lookup) && isset($ca_lookup['idx'])) {
	config_set_path('ca/' . $ca_lookup['idx'], $ca);
}

$client = kontrol_id_extract_item(lookup_cert($settings['client_cert_ref']));
if (empty($client['crt'])) {
	$db->close();
	kontrol_id_api_response(500, array('ok' => false, 'error' => 'Kontrol-ID client certificate is unavailable'));
}

$cert_sha256 = strtoupper(openssl_x509_fingerprint($certificate, 'sha256'));
$stmt = $db->prepare('INSERT INTO endpoints(ip,hostname,fqdn,cert_sha256,enrolled_at,last_seen,revoked) ' .
	'VALUES(:ip,:hostname,:fqdn,:fingerprint,:now,0,0) ON CONFLICT(ip) DO UPDATE SET ' .
	'hostname=excluded.hostname,fqdn=excluded.fqdn,cert_sha256=excluded.cert_sha256,' .
	'enrolled_at=excluded.enrolled_at,revoked=0');
$stmt->bindValue(':ip', $remote_ip, SQLITE3_TEXT);
$stmt->bindValue(':hostname', $hostname, SQLITE3_TEXT);
$stmt->bindValue(':fqdn', $fqdn, SQLITE3_TEXT);
$stmt->bindValue(':fingerprint', $cert_sha256, SQLITE3_TEXT);
$stmt->bindValue(':now', time(), SQLITE3_INTEGER);
$stmt->execute();
$stmt = $db->prepare('DELETE FROM identities WHERE ip=:ip');
$stmt->bindValue(':ip', $remote_ip, SQLITE3_TEXT);
$stmt->execute();
$db->close();

kontrol_id_consume_token();
write_config('Kontrol-ID station enrolled: ' . $hostname . ' (' . $remote_ip . ')');

$client_parsed = openssl_x509_parse(base64_decode($client['crt']));
$client_cn = $client_parsed['subject']['CN'] ?? '';
if (empty($client_cn)) {
	kontrol_id_api_response(500, array('ok' => false, 'error' => 'Kontrol-ID client certificate subject is invalid'));
}
$client_subject = '*';
$ca_pem = base64_decode($ca['crt']);
$issuer_thumbprint = strtoupper(openssl_x509_fingerprint($ca_pem, 'sha1'));

kontrol_id_api_response(200, array(
	'ok' => true,
	'caPem' => $ca_pem,
	'serverCertificatePem' => $certificate,
	'serverCertificateSha256' => $cert_sha256,
	'clientCertificateSubject' => $client_subject,
	'clientCertificateIssuerThumbprint' => $issuer_thumbprint,
	'localAccount' => 'KontrolIdentity',
	'winrmPort' => (int)$settings['winrm_port'],
	'resourceUri' => '*'
));

?>
