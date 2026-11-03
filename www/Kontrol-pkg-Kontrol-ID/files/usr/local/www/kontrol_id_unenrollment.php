<?php

$nocsrf = true;
require_once('config.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

function kontrol_id_unenroll_response($status, $payload) {
	http_response_code($status);
	echo json_encode($payload, JSON_UNESCAPED_SLASHES);
	exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
	kontrol_id_unenroll_response(405, array('ok' => false, 'error' => 'POST required'));
}
if (empty($_SERVER['HTTPS']) || $_SERVER['HTTPS'] === 'off') {
	kontrol_id_unenroll_response(400, array('ok' => false, 'error' => 'HTTPS is required'));
}

$body = file_get_contents('php://input');
if (!is_string($body) || strlen($body) > 32768) {
	kontrol_id_unenroll_response(413, array('ok' => false, 'error' => 'Request is too large'));
}
$request = json_decode($body, true);
if (!is_array($request)) {
	kontrol_id_unenroll_response(400, array('ok' => false, 'error' => 'Invalid JSON'));
}

$hostname = strtolower(trim($request['hostname'] ?? ''));
$timestamp = (int)($request['timestamp'] ?? 0);
$nonce = trim($request['nonce'] ?? '');
$certificate_pem = $request['certificate'] ?? '';
$signature_b64 = $request['signature'] ?? '';
if (!preg_match('/^[a-z0-9][a-z0-9-]{0,62}$/', $hostname) ||
	abs(time() - $timestamp) > 300 ||
	!preg_match('/^[A-Za-z0-9+\/=]{40,100}$/', $nonce) ||
	!is_string($certificate_pem) || strlen($certificate_pem) > 16384 ||
	strpos($certificate_pem, 'BEGIN CERTIFICATE') === false ||
	!is_string($signature_b64) || strlen($signature_b64) > 8192) {
	kontrol_id_unenroll_response(400, array('ok' => false, 'error' => 'Invalid unenrollment request'));
}

$certificate = @openssl_x509_read($certificate_pem);
$parsed = $certificate ? @openssl_x509_parse($certificate) : false;
$signature = base64_decode($signature_b64, true);
if ($certificate === false || !is_array($parsed) || $signature === false ||
	strcasecmp($parsed['subject']['CN'] ?? '', $hostname) !== 0) {
	kontrol_id_unenroll_response(400, array('ok' => false, 'error' => 'Invalid station certificate'));
}

$fingerprint = strtoupper(openssl_x509_fingerprint($certificate, 'sha256'));
kontrol_id_ensure_storage();
$db = new SQLite3(KONTROL_ID_DB);
$db->busyTimeout(1000);
$stmt = $db->prepare('SELECT ip FROM endpoints WHERE hostname=:hostname AND cert_sha256=:fingerprint LIMIT 1');
$stmt->bindValue(':hostname', $hostname, SQLITE3_TEXT);
$stmt->bindValue(':fingerprint', $fingerprint, SQLITE3_TEXT);
$result = $stmt->execute();
$endpoint = $result ? $result->fetchArray(SQLITE3_ASSOC) : false;

$payload = "kontrol-id-unenroll-v1\n" . $hostname . "\n" . $timestamp . "\n" . $nonce . "\n" . $fingerprint;
if (@openssl_verify($payload, $signature, $certificate, OPENSSL_ALGO_SHA256) !== 1) {
	$db->close();
	kontrol_id_unenroll_response(403, array('ok' => false, 'error' => 'Station enrollment could not be authenticated'));
}

if (!is_array($endpoint)) {
	$db->close();
	kontrol_id_unenroll_response(200, array('ok' => true, 'status' => 'already_removed'));
}

$ip = $endpoint['ip'];
$db->exec('BEGIN IMMEDIATE');
$stmt = $db->prepare('DELETE FROM endpoints WHERE ip=:ip AND cert_sha256=:fingerprint');
$stmt->bindValue(':ip', $ip, SQLITE3_TEXT);
$stmt->bindValue(':fingerprint', $fingerprint, SQLITE3_TEXT);
$stmt->execute();
$stmt = $db->prepare('DELETE FROM identities WHERE ip=:ip');
$stmt->bindValue(':ip', $ip, SQLITE3_TEXT);
$stmt->execute();
$stmt = $db->prepare('DELETE FROM portal_sessions WHERE ip=:ip');
$stmt->bindValue(':ip', $ip, SQLITE3_TEXT);
$stmt->execute();
$db->exec('COMMIT');
$db->close();

write_config('Kontrol-ID station removed: ' . $hostname . ' (' . $ip . ')');

kontrol_id_unenroll_response(200, array('ok' => true, 'status' => 'removed'));

?>
