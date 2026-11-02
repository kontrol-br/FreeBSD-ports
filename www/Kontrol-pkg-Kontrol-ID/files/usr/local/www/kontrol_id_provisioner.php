<?php

require_once('guiconfig.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

function kontrol_id_provisioner_fail($message) {
	http_response_code(409);
	header('Content-Type: text/plain; charset=utf-8');
	header('Cache-Control: no-store');
	echo $message . "\n";
	exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
	http_response_code(405);
	header('Allow: GET');
	exit;
}
if (config_get_path('system/webgui/protocol', 'https') !== 'https') {
	kontrol_id_provisioner_fail('Enable HTTPS on the WebGUI before downloading the Provisioner.');
}

$settings = kontrol_id_get_settings();
$ca = kontrol_id_extract_item(lookup_ca($settings['ca_ref'] ?? ''));
$ca_pem = !empty($ca['crt']) ? base64_decode($ca['crt'], true) : false;
if ($ca_pem === false || @openssl_x509_read($ca_pem) === false) {
	kontrol_id_provisioner_fail('The selected Kontrol-ID certificate authority is unavailable.');
}

$authority = trim((string)($_SERVER['HTTP_HOST'] ?? ''));
if (!preg_match('/^(?:[A-Za-z0-9.-]+|\[[0-9A-Fa-f:]+\])(?::[0-9]{1,5})?$/D', $authority)) {
	kontrol_id_provisioner_fail('The WebGUI address is invalid.');
}
$base_url = 'https://' . $authority;
$request_host = strtolower((string)parse_url($base_url, PHP_URL_HOST));
$webcert_ref = config_get_path('system/webgui/ssl-certref', '');
$webcert = kontrol_id_extract_item(lookup_cert($webcert_ref));
if (!kontrol_id_certificate_matches_ca($webcert, $ca) ||
	!kontrol_id_certificate_covers_host($webcert, $request_host)) {
	kontrol_id_provisioner_fail('The active WebGUI certificate must be issued by the selected Kontrol-ID CA and include this hostname or IP in its SAN. Save General Settings to regenerate the managed certificate.');
}

$source = '/usr/local/www/kontrol-id/Kontrol-ID-Provisioner.exe';
if (!is_file($source) || filesize($source) < 4096) {
	kontrol_id_provisioner_fail('The Kontrol-ID Provisioner executable is unavailable.');
}

$url_bytes = $base_url;
$payload = 'KID1' . pack('N', strlen($url_bytes)) . $url_bytes .
	pack('N', strlen($ca_pem)) . $ca_pem;
if (strlen($payload) > 65536) {
	kontrol_id_provisioner_fail('The configured CA chain is too large for the Provisioner.');
}
$trailer = $payload . hash('sha256', $payload, true) . pack('N', strlen($payload)) . 'KIDCFG01';

header('Content-Type: application/vnd.microsoft.portable-executable');
header('Content-Disposition: attachment; filename="Kontrol-ID-Provisioner.exe"');
header('Content-Length: ' . (filesize($source) + strlen($trailer)));
header('Cache-Control: no-store, no-cache, must-revalidate');
header('Pragma: no-cache');
header('X-Content-Type-Options: nosniff');
readfile($source);
echo $trailer;
exit;

?>
