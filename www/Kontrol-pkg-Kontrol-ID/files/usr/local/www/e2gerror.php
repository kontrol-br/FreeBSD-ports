<?php
/*
 * Kontrol-ID access denied page.
 *
 * E2Guardian sends a non-standard query string whose fields are separated by
 * "::" and whose names and values are separated by "==".  Keep that wire
 * format here, but never render its values without escaping them first.
 */
if (!defined('E2GUARDIAN_DIR')) {
    define('E2GUARDIAN_DIR', '/usr/local');
}
include(E2GUARDIAN_DIR . '/pkg/e2guardian.inc');

// Leave as "none" to disable the optional detailed denied-request log.
// e2g_check_logdeniedcgi() updates this exact definition when logging is enabled.
define('LOG_DENIED', 'none');

function kontrol_id_read_denial_environment()
{
    $values = array();
    $query = isset($_SERVER['QUERY_STRING']) ? (string) $_SERVER['QUERY_STRING'] : '';

    if ($query === '') {
        return $values;
    }

    foreach (explode('::', $query) as $pair) {
        $parts = explode('==', $pair, 2);
        if (count($parts) !== 2) {
            continue;
        }

        $name = strtoupper(preg_replace('/[^A-Za-z0-9_]/', '', $parts[0]));
        if ($name === '') {
            continue;
        }

        $value = urldecode($parts[1]);
        $value = str_replace('|', ' | ', $value);
        $value = strip_tags($value);
        $value = preg_replace('/[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]/', '', $value);
        $values[$name] = substr(trim($value), 0, 4096);
    }

    return $values;
}

function kontrol_id_value($values, $name, $fallback)
{
    return isset($values[$name]) && $values[$name] !== '' ? $values[$name] : $fallback;
}

function kontrol_id_escape($value)
{
    return htmlspecialchars((string) $value, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function kontrol_id_log_value($value)
{
    return str_replace(array("\r", "\n", ';;'), array(' ', ' ', ';'), (string) $value);
}

$in = kontrol_id_read_denial_environment();
$deniedurl = kontrol_id_value($in, 'DENIEDURL', 'Endereço não informado');
$reason = kontrol_id_value($in, 'REASON', 'Acesso não permitido pela política de segurança');
$user = kontrol_id_value($in, 'USER', 'Não identificado');
$ip = kontrol_id_value($in, 'IP', 'Não informado');
$categories = kontrol_id_value($in, 'CATEGORIES', 'Não classificado');
$group = kontrol_id_value($in, 'FILTERGROUP', 'Política padrão');
$host = kontrol_id_value($in, 'HOST', 'Dispositivo não informado');
$now = date('Y-m-d H:i:s');
$display_time = date('d/m/Y H:i:s');
$reference = strtoupper(substr(hash('sha256', $now . '|' . $ip . '|' . $deniedurl . '|' . $reason), 0, 10));

$device = $host;
if ($ip !== 'Não informado' && strpos($host, $ip) === false) {
    $device .= ' · ' . $ip;
}

$bypass_url = '';
$bypass_name = '';
if (isset($in['GBYPASS']) && $in['GBYPASS'] !== '') {
    $bypass_name = 'GBYPASS';
    $bypass_value = $in['GBYPASS'];
} elseif (isset($in['GIBYPASS']) && $in['GIBYPASS'] !== '') {
    $bypass_name = 'GIBYPASS';
    $bypass_value = $in['GIBYPASS'];
}
if ($bypass_name !== '' && $deniedurl !== 'Endereço não informado') {
    $separator = strpos($deniedurl, '?') === false ? '?' : '&';
    $bypass_url = $deniedurl . $separator . $bypass_name . '=' . rawurlencode($bypass_value);
}

if (LOG_DENIED !== 'none') {
    $log_line = implode(';;', array(
        kontrol_id_log_value($now),
        kontrol_id_log_value($user),
        kontrol_id_log_value($ip),
        kontrol_id_log_value($deniedurl),
        kontrol_id_log_value($categories),
        kontrol_id_log_value($reason),
        kontrol_id_log_value($group)
    )) . "\n";
    @file_put_contents(LOG_DENIED, $log_line, FILE_APPEND | LOCK_EX);
}

$nonce = base64_encode(random_bytes(18));
header('Content-Type: text/html; charset=UTF-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('Pragma: no-cache');
header('X-Content-Type-Options: nosniff');
header('X-Frame-Options: DENY');
header("Content-Security-Policy: default-src 'none'; style-src 'nonce-{$nonce}'; script-src 'nonce-{$nonce}'; img-src data:; base-uri 'none'; form-action 'self'; frame-ancestors 'none'");
?>
<!doctype html>
<html lang="pt-BR">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="color-scheme" content="dark">
    <title>Kontrol-ID · Acesso bloqueado</title>
    <style nonce="<?= kontrol_id_escape($nonce) ?>">
        :root {
            --canvas: #101114;
            --surface: #181a1f;
            --surface-soft: #202228;
            --line: rgba(255,255,255,.10);
            --text: #f5f6f7;
            --muted: #aeb3bd;
            --red: #e22b35;
            --red-dark: #b81924;
            --shadow: 0 26px 80px rgba(0,0,0,.42);
        }

        * { box-sizing: border-box; }

        html, body { min-height: 100%; }

        body {
            margin: 0;
            min-height: 100vh;
            color: var(--text);
            background:
                radial-gradient(circle at 12% 15%, rgba(226,43,53,.16), transparent 28rem),
                linear-gradient(rgba(255,255,255,.025) 1px, transparent 1px),
                linear-gradient(90deg, rgba(255,255,255,.025) 1px, transparent 1px),
                var(--canvas);
            background-size: auto, 40px 40px, 40px 40px, auto;
            font: 15px/1.55 -apple-system, BlinkMacSystemFont, "Segoe UI", Inter, Roboto, Arial, sans-serif;
            display: grid;
            place-items: center;
            padding: 30px;
        }

        .shell {
            width: min(1120px, 100%);
            animation: enter .6s cubic-bezier(.2,.8,.2,1) both;
        }

        .brand {
            display: flex;
            align-items: center;
            gap: 12px;
            margin: 0 0 18px 4px;
            color: var(--muted);
            font-size: 12px;
            font-weight: 700;
            letter-spacing: .16em;
            text-transform: uppercase;
        }

        .brand-mark {
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: var(--red);
            box-shadow: 0 0 0 5px rgba(226,43,53,.13);
        }

        .card {
            overflow: hidden;
            display: grid;
            grid-template-columns: minmax(0, .9fr) minmax(420px, 1.1fr);
            background: rgba(24,26,31,.96);
            border: 1px solid var(--line);
            border-radius: 24px;
            box-shadow: var(--shadow);
        }

        .hero, .details { padding: clamp(34px, 5vw, 64px); }

        .hero {
            position: relative;
            min-height: 570px;
            display: flex;
            flex-direction: column;
            justify-content: center;
            border-right: 1px solid var(--line);
        }

        .stop-sign {
            width: 132px;
            height: 132px;
            margin-bottom: 34px;
            display: grid;
            place-items: center;
            color: white;
            background: linear-gradient(145deg, #f33b45, var(--red-dark));
            border: 1px solid rgba(255,255,255,.26);
            border-radius: 50%;
            box-shadow: 0 20px 42px rgba(226,43,53,.26), inset 0 1px rgba(255,255,255,.28);
            animation: stop-in .7s .15s cubic-bezier(.2,.85,.25,1.2) both, breathe 4s 1.1s ease-in-out infinite;
        }

        .stop-sign svg { width: 72px; height: 72px; }

        .eyebrow {
            margin: 0 0 9px;
            color: #ff747c;
            font-size: 12px;
            font-weight: 750;
            letter-spacing: .15em;
            text-transform: uppercase;
        }

        h1 {
            margin: 0;
            font-size: clamp(38px, 5vw, 62px);
            line-height: .98;
            letter-spacing: -.045em;
        }

        .lead {
            max-width: 38rem;
            margin: 22px 0 0;
            color: var(--muted);
            font-size: 17px;
        }

        .actions { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 34px; }

        .button {
            min-height: 44px;
            padding: 10px 17px;
            border: 1px solid var(--line);
            border-radius: 10px;
            color: var(--text);
            background: var(--surface-soft);
            font: inherit;
            font-weight: 650;
            text-decoration: none;
            cursor: pointer;
            transition: transform .16s ease, border-color .16s ease, background .16s ease;
        }

        .button:hover { transform: translateY(-1px); border-color: rgba(255,255,255,.28); background: #292c33; }
        .button:focus-visible { outline: 3px solid rgba(226,43,53,.4); outline-offset: 3px; }
        .button.primary { background: var(--red); border-color: var(--red); }
        .button.primary:hover { background: #f03943; }

        .details { align-self: center; }

        .details-heading {
            display: flex;
            align-items: baseline;
            justify-content: space-between;
            gap: 20px;
            padding-bottom: 18px;
            border-bottom: 1px solid var(--line);
        }

        .details-heading h2 { margin: 0; font-size: 19px; letter-spacing: -.015em; }
        .reference { color: var(--muted); font-size: 12px; white-space: nowrap; }

        dl { margin: 0; }

        .row {
            display: grid;
            grid-template-columns: 116px minmax(0, 1fr);
            gap: 20px;
            padding: 17px 0;
            border-bottom: 1px solid var(--line);
        }

        dt {
            color: var(--muted);
            font-size: 11px;
            font-weight: 700;
            letter-spacing: .09em;
            text-transform: uppercase;
        }

        dd { margin: 0; min-width: 0; overflow-wrap: anywhere; font-weight: 560; }
        .url { color: #ff9096; }

        .help {
            display: flex;
            gap: 12px;
            margin-top: 24px;
            color: var(--muted);
            font-size: 13px;
        }

        .help-dot {
            flex: 0 0 auto;
            width: 7px;
            height: 7px;
            margin-top: 7px;
            border-radius: 50%;
            background: var(--red);
        }

        footer { margin: 15px 4px 0; color: #777d87; font-size: 12px; }

        @keyframes enter {
            from { opacity: 0; transform: translateY(14px); }
            to { opacity: 1; transform: translateY(0); }
        }

        @keyframes stop-in {
            from { opacity: 0; transform: scale(.72) rotate(-8deg); }
            to { opacity: 1; transform: scale(1) rotate(0); }
        }

        @keyframes breathe {
            0%, 100% { box-shadow: 0 20px 42px rgba(226,43,53,.24), inset 0 1px rgba(255,255,255,.28); }
            50% { box-shadow: 0 24px 54px rgba(226,43,53,.38), inset 0 1px rgba(255,255,255,.28); }
        }

        @media (max-width: 840px) {
            body { display: block; padding: 20px; }
            .shell { margin: 0 auto; }
            .card { grid-template-columns: 1fr; }
            .hero { min-height: auto; border-right: 0; border-bottom: 1px solid var(--line); }
            .stop-sign { width: 110px; height: 110px; }
            .stop-sign svg { width: 61px; height: 61px; }
        }

        @media (max-width: 520px) {
            body { padding: 14px; }
            .card { border-radius: 18px; }
            .hero, .details { padding: 28px 22px; }
            .row { grid-template-columns: 1fr; gap: 4px; }
            .details-heading { align-items: flex-start; flex-direction: column; gap: 4px; }
            .button { width: 100%; text-align: center; }
        }

        @media (prefers-reduced-motion: reduce) {
            *, *::before, *::after { animation: none !important; transition: none !important; }
        }
    </style>
</head>
<body>
    <main class="shell" aria-labelledby="page-title">
        <div class="brand"><span class="brand-mark" aria-hidden="true"></span>Kontrol-ID · Secure Web Gateway</div>
        <section class="card">
            <div class="hero">
                <div class="stop-sign" aria-hidden="true">
                    <svg viewBox="0 0 80 80" fill="none" xmlns="http://www.w3.org/2000/svg">
                        <path d="M28 38V23c0-3 2-5 5-5s5 2 5 5v12-18c0-3 2-5 5-5s5 2 5 5v18-15c0-3 2-5 5-5s5 2 5 5v17-10c0-3 2-5 5-5s5 2 5 5v20c0 14-10 24-24 24h-3c-8 0-14-3-19-10L8 47c-2-3-1-7 2-9 3-2 7-1 9 1l9 10V38Z" stroke="currentColor" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                </div>
                <p class="eyebrow">Política de segurança aplicada</p>
                <h1 id="page-title">Acesso<br>bloqueado</h1>
                <p class="lead">Este conteúdo não está disponível para o seu perfil. A tentativa foi registrada de acordo com a política de acesso da organização.</p>
                <div class="actions">
                    <button class="button" id="back-button" type="button">Voltar à página anterior</button>
                    <?php if ($bypass_url !== ''): ?>
                        <a class="button primary" href="<?= kontrol_id_escape($bypass_url) ?>">Continuar com autorização</a>
                    <?php endif; ?>
                </div>
            </div>

            <div class="details">
                <div class="details-heading">
                    <h2>Detalhes da solicitação</h2>
                    <span class="reference">Ref. <?= kontrol_id_escape($reference) ?></span>
                </div>
                <dl>
                    <div class="row">
                        <dt>Endereço</dt>
                        <dd class="url"><?= kontrol_id_escape($deniedurl) ?></dd>
                    </div>
                    <div class="row">
                        <dt>Motivo</dt>
                        <dd><?= kontrol_id_escape($reason) ?></dd>
                    </div>
                    <div class="row">
                        <dt>Categoria</dt>
                        <dd><?= kontrol_id_escape($categories) ?></dd>
                    </div>
                    <div class="row">
                        <dt>Usuário</dt>
                        <dd><?= kontrol_id_escape($user) ?></dd>
                    </div>
                    <div class="row">
                        <dt>Dispositivo</dt>
                        <dd><?= kontrol_id_escape($device) ?></dd>
                    </div>
                    <div class="row">
                        <dt>Política</dt>
                        <dd><?= kontrol_id_escape($group) ?></dd>
                    </div>
                    <div class="row">
                        <dt>Data e hora</dt>
                        <dd><?= kontrol_id_escape($display_time) ?></dd>
                    </div>
                </dl>
                <div class="help"><span class="help-dot" aria-hidden="true"></span><span>Se você acredita que este bloqueio é indevido, entre em contato com o administrador e informe a referência acima.</span></div>
            </div>
        </section>
        <footer>Proteção de acesso fornecida pelo Kontrol-ID</footer>
    </main>
    <script nonce="<?= kontrol_id_escape($nonce) ?>">
        document.getElementById('back-button').addEventListener('click', function () {
            window.history.back();
        });
    </script>
</body>
</html>
