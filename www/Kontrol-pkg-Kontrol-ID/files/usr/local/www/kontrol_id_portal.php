<?php

$nocsrf = true;
require_once('config.inc');
require_once('auth.inc');
require_once('/usr/local/pkg/kontrol_id.inc');

header('Cache-Control: no-store');
header('Content-Type: text/html; charset=utf-8');
session_name('KONTROLIDPORTAL');
session_start();
if (empty($_SESSION['csrf'])) {
	$_SESSION['csrf'] = bin2hex(random_bytes(24));
}

$settings = kontrol_id_get_settings();
$client_ip = $_SERVER['REMOTE_ADDR'] ?? '';
$error = '';
$success = '';
$redirect = $_POST['redirect'] ?? ($_GET['url'] ?? '');
if (!empty($redirect) && filter_var($redirect, FILTER_VALIDATE_URL) === false) {
	$redirect = '';
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
	if (!hash_equals($_SESSION['csrf'], $_POST['csrf'] ?? '')) {
		$error = 'A sessão expirou. Atualize a página e tente novamente.';
	} elseif (filter_var($client_ip, FILTER_VALIDATE_IP) === false) {
		$error = 'Não foi possível determinar o endereço desta estação.';
	} else {
		kontrol_id_ensure_storage();
		$db = new SQLite3(KONTROL_ID_DB);
		$db->busyTimeout(1000);
		$cutoff = time() - 300;
		$stmt = $db->prepare('SELECT COUNT(*) FROM auth_attempts WHERE ip=:ip AND attempted_at>:cutoff AND success=0');
		$stmt->bindValue(':ip', $client_ip, SQLITE3_TEXT);
		$stmt->bindValue(':cutoff', $cutoff, SQLITE3_INTEGER);
		$failures = (int)$stmt->execute()->fetchArray(SQLITE3_NUM)[0];
		if ($failures >= 10) {
			$error = 'Muitas tentativas de autenticação. Aguarde alguns minutos.';
		} else {
			$username = trim($_POST['username'] ?? '');
			$password = $_POST['password'] ?? '';
			$authcfg = auth_get_authserver($settings['auth_server']);
			$attributes = array('nas_identifier' => 'kontrol-id-' . gethostname());
			$authenticated = $authcfg ? authenticate_user($username, $password, $authcfg, $attributes) : false;
			$groups = array();
			if ($authenticated) {
				if (($authcfg['type'] ?? '') === 'ldap') {
					$groups = @ldap_get_groups($username, $authcfg);
				} else {
					$user = kontrol_id_extract_item(getUserEntry($username));
					$groups = is_array($user) ? local_user_get_groups($user, true) : array();
				}
			}
			$allowed = false;
			if ($authenticated && ($authcfg['type'] ?? '') === 'ldap') {
				$wanted = array_filter(array_map('trim', explode(',', $settings['ad_groups'])));
				$normalized = array_map('strtolower', is_array($groups) ? $groups : array());
				foreach ($wanted as $group) {
					if (in_array(strtolower($group), $normalized, true)) {
						$allowed = true;
						break;
					}
				}
			} elseif ($authenticated) {
				$allowed = in_array($settings['local_group'], is_array($groups) ? $groups : array(), true);
			}

			$attempt = $db->prepare('INSERT INTO auth_attempts(ip,attempted_at,success) VALUES(:ip,:now,:success)');
			$attempt->bindValue(':ip', $client_ip, SQLITE3_TEXT);
			$attempt->bindValue(':now', time(), SQLITE3_INTEGER);
			$attempt->bindValue(':success', ($authenticated && $allowed) ? 1 : 0, SQLITE3_INTEGER);
			$attempt->execute();

			if ($authenticated && $allowed) {
				$now = time();
				$stmt = $db->prepare('INSERT INTO portal_sessions(ip,username,backend,created_at,expires_at,revoked) ' .
					'VALUES(:ip,:username,:backend,:created,:expires,0) ON CONFLICT(ip) DO UPDATE SET ' .
					'username=excluded.username,backend=excluded.backend,created_at=excluded.created_at,' .
					'expires_at=excluded.expires_at,revoked=0');
				$stmt->bindValue(':ip', $client_ip, SQLITE3_TEXT);
				$stmt->bindValue(':username', $username, SQLITE3_TEXT);
				$stmt->bindValue(':backend', $settings['auth_server'], SQLITE3_TEXT);
				$stmt->bindValue(':created', $now, SQLITE3_INTEGER);
				$stmt->bindValue(':expires', $now + (int)$settings['portal_ttl'], SQLITE3_INTEGER);
				$stmt->execute();
				$success = 'Autenticação concluída. O acesso será liberado em alguns segundos.';
			} elseif ($authenticated) {
				$error = 'A credencial é válida, mas não pertence a um grupo autorizado para acesso à Internet.';
			} else {
				$error = 'Usuário ou senha inválidos.';
			}
		}
		$db->close();
	}
}

function kid_h($value) {
	return htmlspecialchars((string)$value, ENT_QUOTES | ENT_HTML5, 'UTF-8');
}

$theme_id = $settings['login_template'] ?? 'kontrol_dark';
$theme_catalog = kontrol_id_login_template_catalog();
if (!isset($theme_catalog[$theme_id])) {
	$theme_id = 'kontrol_dark';
}
$base_css = @file_get_contents(KONTROL_ID_LOGIN_TEMPLATE_DIR . '/kontrol-id-login-base.css');
$theme_css = @file_get_contents(kontrol_id_login_template_path($theme_id));
?>
<!doctype html>
<html lang="pt-BR" data-kontrol-id-login="<?=kid_h($theme_id)?>">
<head>
	<meta charset="utf-8">
	<meta name="viewport" content="width=device-width,initial-scale=1">
	<meta name="color-scheme" content="light dark">
	<title>Kontrol-ID | Identificação necessária</title>
	<style>
<?=$base_css?>
<?=$theme_css?>
	</style>
</head>
<body>
	<div class="ambient" aria-hidden="true"><span></span><span></span></div>
	<main class="shell" aria-labelledby="login-title">
		<section class="visual">
			<div class="brand"><span class="brand-mark" aria-hidden="true"></span>Kontrol-ID <span>Secure Web Gateway</span></div>
			<div class="identity-icon" aria-hidden="true">
				<svg viewBox="0 0 96 96" fill="none" xmlns="http://www.w3.org/2000/svg">
					<path d="M48 8 79 20v24c0 21-12 36-31 44C29 80 17 65 17 44V20L48 8Z" stroke="currentColor" stroke-width="5" stroke-linejoin="round"/>
					<circle cx="48" cy="39" r="10" stroke="currentColor" stroke-width="5"/>
					<path d="M31 66c3-10 10-15 17-15s14 5 17 15" stroke="currentColor" stroke-width="5" stroke-linecap="round"/>
				</svg>
			</div>
			<p class="kicker">Identidade de rede</p>
			<h1 id="login-title">Identificação<br>necessária</h1>
			<p class="lead">Esta estação ainda não possui uma identidade válida. Autentique-se para liberar temporariamente o acesso à Internet.</p>
			<div class="station"><span>Estação detectada</span><strong><?=kid_h($client_ip)?></strong></div>
		</section>

		<section class="form-panel">
			<header><p class="step">Acesso corporativo</p><h2>Acessar a Internet</h2><p>Use uma credencial autorizada do Kontrol ou do Active Directory.</p></header>
			<?php if ($error): ?><div class="notice error" role="alert"><?=kid_h($error)?></div><?php endif; ?>
			<?php if ($success): ?><div class="notice success" role="status"><?=kid_h($success)?></div><?php endif; ?>
			<?php if (!$success): ?>
				<form method="post" autocomplete="on">
					<input type="hidden" name="csrf" value="<?=kid_h($_SESSION['csrf'])?>">
					<input type="hidden" name="redirect" value="<?=kid_h($redirect)?>">
					<div class="field"><label for="username">Usuário</label><input id="username" name="username" required autofocus autocomplete="username" placeholder="domínio\\usuário ou usuário"></div>
					<div class="field"><label for="password">Senha</label><input id="password" name="password" type="password" required autocomplete="current-password" placeholder="Sua senha"></div>
					<button type="submit"><span>Entrar e liberar acesso</span><span aria-hidden="true">→</span></button>
				</form>
			<?php elseif ($redirect): ?>
				<a class="continue" href="<?=kid_h($redirect)?>">Continuar para o endereço solicitado <span aria-hidden="true">→</span></a>
			<?php else: ?>
				<p class="ready">A identidade já foi associada. Feche esta página e tente acessar novamente.</p>
			<?php endif; ?>
			<?php if (!empty($settings['admin_contact'])): ?><div class="help"><span aria-hidden="true">i</span><p><?=nl2br(kid_h($settings['admin_contact']))?></p></div><?php endif; ?>
			<footer>A autenticação é validada diretamente pelo appliance e não é armazenada nesta página.</footer>
		</section>
	</main>
</body>
</html>
