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
					$user = getUserEntry($username);
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
?>
<!doctype html>
<html lang="pt-BR">
<head>
	<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
	<title>Kontrol-ID | Identificação necessária</title>
	<style>
	:root{color-scheme:light;--navy:#11263d;--blue:#1769aa;--cyan:#1fa3c8;--ink:#1c2733;--muted:#607080;--line:#dfe7ee;--paper:#fff;--bg:#eef3f7;--danger:#a52a2a;--ok:#176a43}*{box-sizing:border-box}body{margin:0;background:linear-gradient(145deg,#e8f0f6 0,#f7fafc 55%,#e6eef4 100%);color:var(--ink);font:16px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;min-height:100vh;display:grid;place-items:center;padding:24px}.shell{width:min(920px,100%);background:var(--paper);border:1px solid rgba(17,38,61,.12);border-radius:18px;box-shadow:0 24px 70px rgba(17,38,61,.16);overflow:hidden;display:grid;grid-template-columns:1.05fr .95fr}.intro{padding:52px;background:linear-gradient(155deg,var(--navy),#173c5d 65%,#176985);color:#fff}.mark{letter-spacing:.15em;text-transform:uppercase;font-size:.78rem;font-weight:700;color:#8edcf0}.intro h1{font-size:clamp(2rem,4vw,3.4rem);line-height:1.05;margin:.55em 0 .4em}.intro p{color:#d6e6f1;max-width:34rem}.meta{margin-top:2rem;padding-top:1.4rem;border-top:1px solid rgba(255,255,255,.18);font-size:.9rem;color:#bdd1df}.form{padding:52px}.form h2{margin:0 0 .35rem;font-size:1.5rem}.form>p{color:var(--muted);margin:0 0 1.8rem}label{display:block;font-weight:650;margin:.9rem 0 .35rem}input{width:100%;border:1px solid var(--line);border-radius:9px;padding:12px 13px;font:inherit;outline:0}input:focus{border-color:var(--cyan);box-shadow:0 0 0 3px rgba(31,163,200,.14)}button{width:100%;margin-top:1.3rem;border:0;border-radius:9px;padding:13px;background:var(--blue);color:#fff;font:inherit;font-weight:700;cursor:pointer}button:hover{background:#12588f}.notice{border-radius:9px;padding:11px 13px;margin:0 0 1rem;font-size:.93rem}.error{background:#fff0f0;color:var(--danger);border:1px solid #f2caca}.success{background:#edf9f3;color:var(--ok);border:1px solid #bfe3d0}.help{margin-top:1.8rem;padding-top:1.2rem;border-top:1px solid var(--line);color:var(--muted);font-size:.9rem}@media(max-width:760px){.shell{grid-template-columns:1fr}.intro,.form{padding:34px}.intro h1{font-size:2.2rem}}
	</style>
</head>
<body>
<main class="shell">
	<section class="intro"><div class="mark">Kontrol Identity</div><h1>Identificação necessária</h1><p>Esta estação não pôde ser identificada automaticamente. Use uma credencial autorizada para associar temporariamente seu usuário ao endereço da estação.</p><div class="meta">Endereço detectado: <?=kid_h($client_ip)?><br>A senha é validada diretamente pelo Kontrol ou pelo Active Directory configurado.</div></section>
	<section class="form"><h2>Acessar a Internet</h2><p>Informe sua credencial corporativa.</p>
		<?php if ($error): ?><div class="notice error"><?=kid_h($error)?></div><?php endif; ?>
		<?php if ($success): ?><div class="notice success"><?=kid_h($success)?></div><?php endif; ?>
		<?php if (!$success): ?><form method="post" autocomplete="on">
			<input type="hidden" name="csrf" value="<?=kid_h($_SESSION['csrf'])?>"><input type="hidden" name="redirect" value="<?=kid_h($redirect)?>">
			<label for="username">Usuário</label><input id="username" name="username" required autofocus autocomplete="username">
			<label for="password">Senha</label><input id="password" name="password" type="password" required autocomplete="current-password">
			<button type="submit">Entrar</button>
		</form><?php elseif ($redirect): ?><p><a href="<?=kid_h($redirect)?>">Continuar para o endereço solicitado</a></p><?php endif; ?>
		<div class="help"><?=nl2br(kid_h($settings['admin_contact']))?></div>
	</section>
</main>
</body>
</html>
