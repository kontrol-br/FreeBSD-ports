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
require("guiconfig.inc");

$system = posix_uname();
if ($system['machine'] === 'amd64') {
	ini_set('memory_limit', '512M');
}

$pgtitle = array(gettext("Package"), gettext("Sarg"), gettext("Reports"));
$shortcut_section = "sarg";
include("head.inc");

$selected_dir = isset($_GET['dir']) ? (string)$_GET['dir'] : '';
if ($selected_dir !== '' && !preg_match('/^[A-Za-z0-9_-]+$/D', $selected_dir)) {
	$selected_dir = '';
}

$proxy = $config['installedpackages']['sarg']['config'][0]['proxy_server'];
$proxy = $proxy ? $proxy : 'e2guardian';
$format = '';
if ($proxy === 'e2guardian' && is_array($config['installedpackages']['e2guardianlog']['config'])) {
	$format = (string)$config['installedpackages']['e2guardianlog']['config'][0]['logfileformat'];
}
$format_names = array('1' => 'nativo', '2' => 'CSV', '3' => 'Squid', '4' => 'tabulado');
$format_label = isset($format_names[$format]) ? $format_names[$format] : $format;
$source_label = strtoupper($proxy) . ($format !== '' ? ' · formato ' . $format_label : '');

$report_root = '/usr/local/sarg-reports' . ($selected_dir === '' ? '' : '/' . $selected_dir);
$latest = 0;
foreach ((array)glob($report_root . '/*/index.html') as $index_file) {
	$latest = max($latest, (int)filemtime($index_file));
}
$latest_label = $latest > 0 ? date('d/m/Y H:i', $latest) : gettext('No report generated yet');

$frame_query = array('dir' => $selected_dir, 'prevent' => mt_rand());
$frame_src = '/sarg_frame.php?' . http_build_query($frame_query);
?>
<style>
.sarg-shell { margin-top: 14px; border: 1px solid #d8e1e8; background: #fff; }
.sarg-summary { display: flex; align-items: center; justify-content: space-between; gap: 24px; padding: 14px 18px; border-left: 4px solid #1769aa; background: #f7f9fb; }
.sarg-summary__title { margin: 0 0 3px; color: #1d2a36; font-size: 16px; font-weight: 600; }
.sarg-summary__meta { color: #667685; font-size: 12px; }
.sarg-summary__status { text-align: right; }
.sarg-summary__status strong { display: block; color: #1769aa; font-size: 13px; }
.sarg-frame { display: block; width: 100%; min-height: 420px; border: 0; background: #f4f7f9; }
@media (max-width: 700px) { .sarg-summary { align-items: flex-start; flex-direction: column; } .sarg-summary__status { text-align: left; } }
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

	$report_dirs = array('default' => '');
	if (is_array($config['installedpackages']['sargschedule']['config'])) {
		foreach ($config['installedpackages']['sargschedule']['config'] as $schedule) {
			if (!empty($schedule['foldersuffix'])) {
				$report_dirs[$schedule['foldersuffix']] = $schedule['foldersuffix'];
			}
		}
	}
	if (count($report_dirs) > 1) {
		$folder_tabs = array();
		foreach ($report_dirs as $label => $folder) {
			$url = '/sarg_reports.php' . ($folder === '' ? '' : '?dir=' . rawurlencode($folder));
			$folder_tabs[] = array(gettext($label . ' Reports'), $folder === $selected_dir, $url);
		}
		display_top_tabs($folder_tabs);
	}
	?>
	<div class="sarg-shell">
		<div class="sarg-summary">
			<div>
				<h2 class="sarg-summary__title"><?=gettext('Internet access analytics')?></h2>
				<div class="sarg-summary__meta"><?=htmlspecialchars($source_label, ENT_QUOTES, 'UTF-8')?> · <?=gettext('Local processing, without external services')?></div>
			</div>
			<div class="sarg-summary__status">
				<strong><?=gettext('Latest report')?></strong>
				<span class="sarg-summary__meta"><?=htmlspecialchars($latest_label, ENT_QUOTES, 'UTF-8')?></span>
			</div>
		</div>
		<iframe id="sarg-report" class="sarg-frame" title="Sarg reports" src="<?=htmlspecialchars($frame_src, ENT_QUOTES, 'UTF-8')?>"></iframe>
	</div>
</div>
<script>
(function () {
	var frame = document.getElementById('sarg-report');
	function resizeReport() {
		try {
			var documentHeight = frame.contentWindow.document.documentElement.scrollHeight;
			frame.style.height = Math.max(420, Math.min(8000, documentHeight + 32)) + 'px';
		} catch (ignore) {}
	}
	frame.addEventListener('load', function () {
		resizeReport();
		window.setTimeout(resizeReport, 350);
	});
}());
</script>
</body>
</html>
