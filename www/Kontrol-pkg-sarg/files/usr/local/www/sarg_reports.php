<?php
/*
	sarg_reports.php
	part of the Kontrol Sarg package
	Copyright (C) 2012 Marcello Coutinho <marcellocoutinho@gmail.com>
	Copyright (C) 2015 ESF, LLC
	Copyright (C) 2026 Kontrol Tecnologia
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
require_once("guiconfig.inc");
require_once("authgui.inc");

$system = posix_uname();
if ($system['machine'] === 'amd64') {
	ini_set('memory_limit', '512M');
}

function sarg_report_index_file($directory) {
	foreach (array('index.html', 'index.html.gz') as $index_name) {
		$index_file = $directory . '/' . $index_name;
		if (is_file($index_file)) {
			return $index_file;
		}
	}
	return false;
}

function sarg_valid_report_name($name) {
	// Sarg generates period folders such as 2026Oct01-2026Oct31.
	return (bool)preg_match('/^[0-9]{4}[A-Za-z]{3}[0-9]{2}-[0-9]{4}[A-Za-z]{3}[0-9]{2}(?:-[0-9]+)?$/D', $name);
}

function sarg_report_display_name($name) {
	$matches = array();
	if (!preg_match('/^([0-9]{4})([A-Za-z]{3})([0-9]{2})-([0-9]{4})([A-Za-z]{3})([0-9]{2})(?:-[0-9]+)?$/D', $name, $matches)) {
		return $name;
	}
	$start = $matches[1] . '-' . ucfirst(strtolower($matches[2])) . '-' . $matches[3];
	$end = $matches[4] . '-' . ucfirst(strtolower($matches[5])) . '-' . $matches[6];
	return ($start === $end) ? $start : $start . ' — ' . $end;
}

function sarg_report_root_real($report_root) {
	$base = '/usr/local/sarg-reports';
	$base_real = realpath($base);
	$root_real = realpath($report_root);
	if ($base_real === false || $root_real === false || !is_dir($root_real)) {
		return false;
	}
	if ($root_real === $base_real) {
		return $root_real;
	}
	if (is_link($report_root) || dirname($root_real) !== $base_real) {
		return false;
	}
	return $root_real;
}

function sarg_report_entries($report_root, $reserved_names) {
	$entries = array();
	$root_real = sarg_report_root_real($report_root);
	if ($root_real === false) {
		return $entries;
	}

	foreach ((array)glob($root_real . '/*') as $candidate) {
		$name = basename($candidate);
		if (!sarg_valid_report_name($name) ||
		    in_array($name, $reserved_names, true) || is_link($candidate) || !is_dir($candidate)) {
			continue;
		}

		$candidate_real = realpath($candidate);
		$index_file = sarg_report_index_file($candidate);
		if ($candidate_real === false || dirname($candidate_real) !== $root_real || $index_file === false) {
			continue;
		}

		$entries[] = array(
			'name' => $name,
			'label' => sarg_report_display_name($name),
			'path' => $candidate_real,
			'mtime' => (int)filemtime($index_file),
		);
	}

	usort($entries, function ($left, $right) {
		if ($left['mtime'] === $right['mtime']) {
			return strnatcasecmp($right['name'], $left['name']);
		}
		return ($left['mtime'] < $right['mtime']) ? 1 : -1;
	});
	return $entries;
}

function sarg_report_delete_target($report_root, $report_name, $reserved_names) {
	if (!sarg_valid_report_name($report_name) ||
	    in_array($report_name, $reserved_names, true)) {
		return false;
	}

	$root_real = sarg_report_root_real($report_root);
	$candidate = $report_root . '/' . $report_name;
	$candidate_real = realpath($candidate);
	if ($root_real === false || $candidate_real === false || is_link($candidate) ||
	    !is_dir($candidate_real) || dirname($candidate_real) !== $root_real ||
	    sarg_report_index_file($candidate_real) === false) {
		return false;
	}
	return $candidate_real;
}

function sarg_remove_report_tree($report_path) {
	$iterator = new RecursiveIteratorIterator(
		new RecursiveDirectoryIterator($report_path, FilesystemIterator::SKIP_DOTS),
		RecursiveIteratorIterator::CHILD_FIRST
	);
	foreach ($iterator as $entry) {
		$entry_path = $entry->getPathname();
		if ($entry->isLink() || $entry->isFile()) {
			if (!unlink($entry_path)) {
				throw new RuntimeException('Unable to remove report file.');
			}
		} elseif ($entry->isDir() && !rmdir($entry_path)) {
			throw new RuntimeException('Unable to remove report directory.');
		}
	}
	if (!rmdir($report_path)) {
		throw new RuntimeException('Unable to remove report directory.');
	}
}

$selected_dir = isset($_POST['dir']) ? (string)$_POST['dir'] : (isset($_GET['dir']) ? (string)$_GET['dir'] : '');
if ($selected_dir !== '' && !preg_match('/^[A-Za-z0-9_-]+$/D', $selected_dir)) {
	$selected_dir = '';
}

$report_dirs = array('default' => '');
if (isset($config['installedpackages']['sargschedule']['config']) &&
    is_array($config['installedpackages']['sargschedule']['config'])) {
	foreach ($config['installedpackages']['sargschedule']['config'] as $schedule) {
		$folder = isset($schedule['foldersuffix']) ? (string)$schedule['foldersuffix'] : '';
		if ($folder !== '' && preg_match('/^[A-Za-z0-9_-]+$/D', $folder)) {
			$report_dirs[$folder] = $folder;
		}
	}
}
if (!in_array($selected_dir, $report_dirs, true)) {
	$selected_dir = '';
}

$report_root = '/usr/local/sarg-reports' . ($selected_dir === '' ? '' : '/' . $selected_dir);
$reserved_names = ($selected_dir === '') ? array_values(array_filter($report_dirs)) : array();
$input_errors = array();

if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['action']) && $_POST['action'] === 'delete_report') {
	$report_name = isset($_POST['report']) ? (string)$_POST['report'] : '';
	$delete_target = sarg_report_delete_target($report_root, $report_name, $reserved_names);
	if ($delete_target === false) {
		$input_errors[] = gettext('The selected report is invalid or is a protected Sarg directory.');
	} else {
		conf_mount_rw();
		try {
			sarg_remove_report_tree($delete_target);
			log_error(sprintf('[sarg] Generated report "%s" deleted through the WebGUI.', $report_name));
		} catch (Exception $exception) {
			$input_errors[] = gettext('The report could not be completely deleted. Check filesystem permissions and try again.');
		} finally {
			conf_mount_ro();
		}
		if (empty($input_errors)) {
			$query = array('cleanup' => 'deleted');
			if ($selected_dir !== '') {
				$query['dir'] = $selected_dir;
			}
			header('Location: /sarg_reports.php?' . http_build_query($query));
			exit;
		}
	}
}

$report_entries = sarg_report_entries($report_root, $reserved_names);
$latest = empty($report_entries) ? 0 : $report_entries[0]['mtime'];
$latest_label = $latest > 0 ? date('d/m/Y H:i', $latest) : gettext('No report generated yet');
$frame_query = array('dir' => $selected_dir, 'prevent' => mt_rand());
if (!empty($report_entries)) {
	$frame_query['file'] = $report_entries[0]['name'] . '/index.html';
}
$frame_src = '/sarg_frame.php?' . http_build_query($frame_query);
$delete_confirmation = htmlspecialchars(
	json_encode(gettext('Delete this generated report? This action cannot be undone.'), JSON_HEX_APOS | JSON_HEX_QUOT),
	ENT_QUOTES,
	'UTF-8'
);

$pgtitle = array(gettext("Package"), gettext("Sarg"), gettext("Reports"));
$shortcut_section = "sarg";
include("head.inc");

$proxy = $config['installedpackages']['sarg']['config'][0]['proxy_server'];
$proxy = $proxy ? $proxy : 'e2guardian';
$format = '';
if ($proxy === 'e2guardian' && is_array($config['installedpackages']['e2guardianlog']['config'])) {
	$format = (string)$config['installedpackages']['e2guardianlog']['config'][0]['logfileformat'];
}
$format_names = array('1' => 'nativo', '2' => 'CSV', '3' => 'Squid', '4' => 'tabulado');
$format_label = isset($format_names[$format]) ? $format_names[$format] : $format;
$source_label = strtoupper($proxy) . ($format !== '' ? ' · formato ' . $format_label : '');

?>
<style>
.sarg-shell { margin-top: 14px; border: 1px solid #d8e1e8; background: #fff; }
.sarg-summary { display: flex; align-items: center; justify-content: space-between; gap: 24px; padding: 14px 18px; border-left: 4px solid #1769aa; background: #f7f9fb; }
.sarg-summary__title { margin: 0 0 3px; color: #1d2a36; font-size: 16px; font-weight: 600; }
.sarg-summary__meta { color: #667685; font-size: 12px; }
.sarg-summary__aside { display: flex; align-items: center; gap: 18px; }
.sarg-summary__status { text-align: right; }
.sarg-summary__status strong { display: block; color: #1769aa; font-size: 13px; }
.sarg-summary__actions { display: flex; align-items: center; gap: 8px; }
.sarg-action { display: inline-flex; align-items: center; min-height: 34px; padding: 7px 12px; border: 1px solid #b8c7d3; border-radius: 4px; background: #fff; color: #24445e; font-size: 12px; font-weight: 600; text-decoration: none; }
.sarg-action:hover, .sarg-action:focus { border-color: #1769aa; color: #0f4f82; text-decoration: none; }
.sarg-action--primary { border-color: #1769aa; background: #1769aa; color: #fff; }
.sarg-action--primary:hover, .sarg-action--primary:focus { background: #0f4f82; color: #fff; }
.sarg-cleanup { margin-top: 14px; overflow: hidden; border: 1px solid #d8e1e8; border-radius: 4px; background: #fff; }
.sarg-cleanup__header { display: flex; align-items: center; justify-content: space-between; gap: 18px; padding: 13px 16px; border-bottom: 1px solid #d8e1e8; background: #f7f9fb; }
.sarg-cleanup__header h2 { margin: 0 0 2px; color: #1d2a36; font-size: 15px; font-weight: 600; }
.sarg-cleanup__header p { margin: 0; color: #667685; font-size: 12px; }
.sarg-cleanup__count { min-width: 32px; padding: 5px 9px; border-radius: 999px; background: #e6eef5; color: #29485f; font-size: 12px; font-weight: 700; text-align: center; }
.sarg-cleanup__list { margin: 0; padding: 0; list-style: none; }
.sarg-cleanup__item { display: grid; grid-template-columns: minmax(180px, 1fr) auto auto; align-items: center; gap: 12px; padding: 11px 16px; border-bottom: 1px solid #edf1f4; }
.sarg-cleanup__item:last-child { border-bottom: 0; }
.sarg-cleanup__name { min-width: 0; }
.sarg-cleanup__name strong { display: block; overflow: hidden; color: #243746; font-size: 13px; text-overflow: ellipsis; white-space: nowrap; }
.sarg-cleanup__name span { color: #71808d; font-size: 11px; }
.sarg-cleanup__open { color: #1769aa; font-size: 12px; font-weight: 600; }
.sarg-cleanup__delete { margin: 0; }
.sarg-delete-button { min-height: 32px; padding: 6px 10px; border: 1px solid #d9a3a3; border-radius: 4px; background: #fff; color: #a12626; font-size: 12px; font-weight: 600; cursor: pointer; }
.sarg-delete-button:hover, .sarg-delete-button:focus { border-color: #b42318; background: #fff5f4; color: #8f1d16; }
.sarg-cleanup__empty { padding: 18px 16px; color: #667685; font-size: 12px; }
.sarg-empty-report { padding: 42px 20px; color: #667685; text-align: center; }
.sarg-frame { display: block; width: 100%; height: calc(100vh - 270px); min-height: 520px; max-height: 920px; border: 0; border-top: 1px solid #d8e1e8; background: #f4f7f9; }
@media (max-width: 900px) { .sarg-summary { align-items: flex-start; flex-direction: column; } .sarg-summary__aside { width: 100%; justify-content: space-between; } .sarg-summary__status { text-align: left; } }
@media (max-width: 560px) { .sarg-summary__aside { align-items: flex-start; flex-direction: column; } .sarg-summary__actions { flex-wrap: wrap; } .sarg-cleanup__item { grid-template-columns: 1fr auto; } .sarg-cleanup__open { grid-column: 1; } .sarg-cleanup__delete { grid-column: 2; grid-row: 1 / span 2; } .sarg-frame { height: calc(100vh - 330px); min-height: 440px; } }
</style>
<div id="mainlevel">
	<?php
	$tab_array = array();
	$tab_array[] = array(gettext("General"), false, "/pkg_edit.php?xml=sarg.xml&id=0");
	$tab_array[] = array(gettext("Users"), false, "/pkg_edit.php?xml=sarg_users.xml&id=0");
	$tab_array[] = array(gettext("Schedule"), false, "/pkg.php?xml=sarg_schedule.xml");
	$tab_array[] = array(gettext("View Report"), true, "/sarg_reports.php");
	$tab_array[] = array(gettext("XMLRPC Sync"), false, "/pkg_edit.php?xml=sarg_sync.xml&id=0");
	display_top_tabs($tab_array);

	if (count($report_dirs) > 1) {
		$folder_tabs = array();
		foreach ($report_dirs as $label => $folder) {
			$url = '/sarg_reports.php' . ($folder === '' ? '' : '?dir=' . rawurlencode($folder));
			$folder_tabs[] = array(gettext($label . ' Reports'), $folder === $selected_dir, $url);
		}
		display_top_tabs($folder_tabs);
	}
	if (!empty($input_errors)) {
		print_input_errors($input_errors);
	} elseif (isset($_GET['cleanup']) && $_GET['cleanup'] === 'deleted') {
		print_info_box(gettext('The generated report was deleted safely.'), 'success');
	}
	?>
	<section class="sarg-cleanup" aria-labelledby="sarg-cleanup-title">
		<div class="sarg-cleanup__header">
			<div>
				<h2 id="sarg-cleanup-title"><?=gettext('Generated reports')?></h2>
				<p><?=gettext('Delete individual reports here. Sarg base folders and configured schedule folders are protected.')?></p>
			</div>
			<span class="sarg-cleanup__count" title="<?=gettext('Report count')?>"><?=count($report_entries)?></span>
		</div>
		<?php if (!empty($report_entries)): ?>
		<ul class="sarg-cleanup__list">
			<?php foreach ($report_entries as $report):
				$report_query = array('dir' => $selected_dir, 'file' => $report['name'] . '/index.html');
				$report_url = '/sarg_frame.php?' . http_build_query($report_query);
			?>
			<li class="sarg-cleanup__item">
				<div class="sarg-cleanup__name">
					<strong title="<?=htmlspecialchars($report['name'], ENT_QUOTES, 'UTF-8')?>"><?=htmlspecialchars($report['label'], ENT_QUOTES, 'UTF-8')?></strong>
					<span><?=sprintf(gettext('Generated on %s'), date('d/m/Y H:i', $report['mtime']))?></span>
				</div>
				<a class="sarg-cleanup__open" href="<?=htmlspecialchars($report_url, ENT_QUOTES, 'UTF-8')?>" target="sarg-report"><?=gettext('View report')?></a>
				<form method="post" class="sarg-cleanup__delete" onsubmit="return confirm(<?=$delete_confirmation?>);">
					<input type="hidden" name="action" value="delete_report">
					<input type="hidden" name="dir" value="<?=htmlspecialchars($selected_dir, ENT_QUOTES, 'UTF-8')?>">
					<input type="hidden" name="report" value="<?=htmlspecialchars($report['name'], ENT_QUOTES, 'UTF-8')?>">
					<button type="submit" class="sarg-delete-button"><i class="fa fa-trash" aria-hidden="true"></i> <?=gettext('Delete')?></button>
				</form>
			</li>
			<?php endforeach; ?>
		</ul>
		<?php else: ?>
		<div class="sarg-cleanup__empty"><?=gettext('There are no generated reports in this folder.')?></div>
		<?php endif; ?>
	</section>
	<div class="sarg-shell">
		<div class="sarg-summary">
			<div>
				<h2 class="sarg-summary__title"><?=gettext('Internet access analytics')?></h2>
				<div class="sarg-summary__meta"><?=htmlspecialchars($source_label, ENT_QUOTES, 'UTF-8')?> · <?=gettext('Local processing, without external services')?></div>
			</div>
			<div class="sarg-summary__aside">
				<div class="sarg-summary__status">
					<strong><?=gettext('Latest report')?></strong>
					<span class="sarg-summary__meta"><?=htmlspecialchars($latest_label, ENT_QUOTES, 'UTF-8')?></span>
				</div>
				<div class="sarg-summary__actions">
					<a class="sarg-action" href="/pkg_edit.php?xml=sarg.xml&amp;id=0"><?=gettext('Settings')?></a>
					<?php if (!empty($report_entries)): ?>
					<a class="sarg-action sarg-action--primary" href="<?=htmlspecialchars($frame_src, ENT_QUOTES, 'UTF-8')?>" target="_blank" rel="noopener"><?=gettext('Open report in new tab')?></a>
					<?php endif; ?>
				</div>
			</div>
		</div>
		<?php if (!empty($report_entries)): ?>
		<iframe id="sarg-report" name="sarg-report" class="sarg-frame" title="Sarg reports" loading="eager" src="<?=htmlspecialchars($frame_src, ENT_QUOTES, 'UTF-8')?>"></iframe>
		<?php else: ?>
		<div class="sarg-empty-report"><?=gettext('Generate a report from the Sarg schedule to display analytics here.')?></div>
		<?php endif; ?>
	</div>
</div>
<?php include("foot.inc"); ?>
