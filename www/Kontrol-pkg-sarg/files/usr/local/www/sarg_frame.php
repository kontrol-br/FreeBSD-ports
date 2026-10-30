<?php
/*
	sarg_frame.php
	part of the Kontrol Sarg package
	Copyright (C) 2012 Marcello Coutinho <marcellocoutinho@gmail.com>
	Copyright (C) 2015 ESF, LLC
	Copyright (C) 2016-2026 Kontrol Tecnologia
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/
require_once("authgui.inc");

if (session_status() !== PHP_SESSION_ACTIVE) {
	session_start();
}

function sarg_report_path($path) {
	$path = str_replace('\\', '/', (string)$path);
	$parts = array();
	foreach (explode('/', $path) as $part) {
		if ($part === '' || $part === '.') {
			continue;
		}
		if ($part === '..') {
			if (empty($parts)) {
				return false;
			}
			array_pop($parts);
			continue;
		}
		if (!preg_match('/^[A-Za-z0-9._-]+$/D', $part)) {
			return false;
		}
		$parts[] = $part;
	}
	return implode('/', $parts);
}

function sarg_report_join($prefix, $path) {
	return sarg_report_path(($prefix === '' ? '' : $prefix . '/') . $path);
}

function sarg_path_in_roots($path, $roots) {
	$real = realpath($path);
	if ($real === false) {
		return false;
	}
	foreach ($roots as $root) {
		$root = realpath($root);
		if ($root !== false && ($real === $root || strpos($real, $root . DIRECTORY_SEPARATOR) === 0)) {
			return $real;
		}
	}
	return false;
}

$dir_suffix = isset($_GET['dir']) ? (string)$_GET['dir'] : '';
if ($dir_suffix !== '' && !preg_match('/^[A-Za-z0-9_-]+$/D', $dir_suffix)) {
	http_response_code(400);
	exit('Invalid report directory.');
}

$report_root = '/usr/local/sarg-reports' . ($dir_suffix === '' ? '' : '/' . $dir_suffix);
$allowed_roots = array($report_root, '/usr/local/sarg-reports', '/usr/local/etc/sarg/images');

if (isset($_GET['asset'])) {
	$asset = sarg_report_path($_GET['asset']);
	if ($asset === false || !preg_match('/\.(?:png|gif|jpe?g|svg)$/iD', $asset)) {
		http_response_code(400);
		exit('Invalid report asset.');
	}
	$asset_file = sarg_path_in_roots($report_root . '/' . $asset, $allowed_roots);
	if ($asset_file === false || !is_file($asset_file)) {
		http_response_code(404);
		exit('Report asset not found.');
	}
	$extension = strtolower(pathinfo($asset_file, PATHINFO_EXTENSION));
	$types = array('png' => 'image/png', 'gif' => 'image/gif', 'jpg' => 'image/jpeg',
		'jpeg' => 'image/jpeg', 'svg' => 'image/svg+xml');
	header('Content-Type: ' . $types[$extension]);
	header('Cache-Control: private, max-age=300');
	readfile($asset_file);
	exit;
}

$requested = isset($_GET['file']) && $_GET['file'] !== '' ? $_GET['file'] : 'index.html';
$requested = sarg_report_path($requested);
if ($requested === false || !preg_match('/\.html$/iD', $requested)) {
	http_response_code(400);
	exit('Invalid report file.');
}

$report_file = sarg_path_in_roots($report_root . '/' . $requested, array($report_root));
$compressed = false;
if ($report_file === false) {
	$report_file = sarg_path_in_roots($report_root . '/' . $requested . '.gz', array($report_root));
	$compressed = ($report_file !== false);
}

if ($report_file === false || !is_file($report_file)) {
	http_response_code(404);
	exit('The report was not found. Save the Sarg settings and run a schedule to generate it.');
}

if ($compressed) {
	$lines = gzfile($report_file);
	$report = ($lines === false) ? '' : implode('', $lines);
} else {
	$report = file_get_contents($report_file);
}

if ($report === false || $report === '') {
	http_response_code(500);
	exit('The report could not be read.');
}

$prefix = dirname($requested);
$prefix = ($prefix === '.') ? '' : $prefix;
$base_query = array('dir' => $dir_suffix);

function sarg_frame_url($dir_suffix, $file) {
	$query = array('dir' => $dir_suffix);
	if ($file !== 'index.html') {
		$query['file'] = $file;
	}
	return '/sarg_frame.php?' . http_build_query($query);
}

$current_dir = dirname($requested);
$current_dir = ($current_dir === '.') ? '' : $current_dir;
$parent_file = 'index.html';
$has_parent = ($requested !== 'index.html');
if ($has_parent && basename($requested) === 'index.html') {
	$parent_dir = dirname($current_dir);
	$parent_file = ($parent_dir === '.' ? '' : $parent_dir . '/') . 'index.html';
} elseif ($has_parent && $current_dir !== '') {
	$parent_file = $current_dir . '/index.html';
}
$home_url = sarg_frame_url($dir_suffix, 'index.html');
$parent_url = sarg_frame_url($dir_suffix, $parent_file);
$viewer_query = $dir_suffix === '' ? '' : '?dir=' . rawurlencode($dir_suffix);
$viewer_url = '/sarg_reports.php' . $viewer_query;

$report = preg_replace_callback(
	'/href=(["\'])(?![a-z][a-z0-9+.-]*:|\/|#)([^"\']+\.html)(?:[?#][^"\']*)?\1/i',
	function ($match) use ($prefix, $base_query) {
		$target = sarg_report_join($prefix, $match[2]);
		if ($target === false) {
			return 'href="#"';
		}
		$query = $base_query;
		$query['file'] = $target;
		return 'href="/sarg_frame.php?' . htmlspecialchars(http_build_query($query), ENT_QUOTES, 'UTF-8') . '"';
	},
	$report
);

$report = preg_replace_callback(
	'/src=(["\'])(?!data:|[a-z][a-z0-9+.-]*:|\/)([^"\']+\.(?:png|gif|jpe?g|svg))\1/i',
	function ($match) use ($prefix, $base_query) {
		$target = sarg_report_join($prefix, $match[2]);
		if ($target === false) {
			return 'src=""';
		}
		$query = $base_query;
		$query['asset'] = $target;
		return 'src="/sarg_frame.php?' . htmlspecialchars(http_build_query($query), ENT_QUOTES, 'UTF-8') . '"';
	},
	$report
);

$head_assets = '<meta http-equiv="Cache-Control" content="no-cache">' .
	'<meta name="viewport" content="width=device-width, initial-scale=1">' .
	'<link rel="stylesheet" href="/sarg_report.css?v=085">' .
	'<script defer src="/sarg_charts.js?v=085"></script>';
if (stripos($report, '</head>') !== false) {
	$report = preg_replace('/<\/head>/i', $head_assets . '</head>', $report, 1);
} else {
	$report = $head_assets . $report;
}

$_SESSION['pdf'] = $report;
$back_action = 'if (history.length > 1) { history.back(); } else { location.href=' .
	json_encode($parent_url, JSON_HEX_APOS | JSON_HEX_QUOT | JSON_UNESCAPED_SLASHES) . '; }';
$toolbar = '<nav class="sarg-report-toolbar" aria-label="Navegação do relatório">' .
	'<div class="sarg-report-toolbar__path"><span>Relatório Sarg</span><strong>' .
	htmlspecialchars($requested, ENT_QUOTES, 'UTF-8') . '</strong></div>' .
	'<div class="sarg-report-toolbar__actions">' .
	'<button type="button" class="sarg-report-action" onclick="' . htmlspecialchars($back_action, ENT_QUOTES, 'UTF-8') . '">&#8592; Página anterior</button>' .
	($has_parent ? '<a class="sarg-report-action" href="' . htmlspecialchars($parent_url, ENT_QUOTES, 'UTF-8') . '">&#8593; Um nível acima</a>' : '') .
	'<a class="sarg-report-action" href="' . htmlspecialchars($home_url, ENT_QUOTES, 'UTF-8') . '">Início do relatório</a>' .
	'<button type="button" class="sarg-report-action" onclick="window.open(\'/sarg_report_pdf.php\',\'_blank\',\'noopener\')">Versão para impressão</button>' .
	'<a class="sarg-report-action sarg-report-action--primary" href="' . htmlspecialchars($viewer_url, ENT_QUOTES, 'UTF-8') . '" target="_top">Voltar ao Kontrol</a>' .
	'</div></nav>';
if (preg_match('/<body\b[^>]*>/i', $report)) {
	$report = preg_replace_callback('/<body\b[^>]*>/i', function ($match) use ($toolbar) {
		return $match[0] . $toolbar;
	}, $report, 1);
} else {
	$report = $toolbar . $report;
}
if (stripos($report, '</body>') !== false) {
	$report = preg_replace('/<\/body>/i', $toolbar . '</body>', $report, 1);
} else {
	$report .= $toolbar;
}

header('Content-Type: text/html; charset=UTF-8');
echo $report;
?>
