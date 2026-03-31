<?php
/*
	sarg_reports.php
	part of pfSense (https://www.pfSense.org/)
	Copyright (C) 2012 Marcello Coutinho <marcellocoutinho@gmail.com>
	Copyright (C) 2015 ESF, LLC
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

if ($uname['machine'] == 'amd64') {
        ini_set('memory_limit', '512M');
}

function sarg_reports_base_dir($suffix = "") {
	$dir = "/usr/local/sarg-reports";
	if ($suffix != "") {
		$dir .= "/" . preg_replace("/\W/", "", $suffix);
	}
	return $dir;
}

function sarg_list_report_directories($dir) {
	$reports = array();
	if (!is_dir($dir)) {
		return $reports;
	}
	foreach (glob($dir . "/*", GLOB_ONLYDIR) as $path) {
		$name = basename($path);
		if ($name == "images") {
			continue;
		}
		if (file_exists($path . "/index.html") || file_exists($path . "/index.html.gz")) {
			$reports[$name] = array(
				'name' => $name,
				'path' => $path,
				'mtime' => filemtime($path)
			);
		}
	}
	uasort($reports, function($a, $b) {
		return $b['mtime'] <=> $a['mtime'];
	});
	return $reports;
}

function sarg_has_root_report($dir) {
	return (file_exists($dir . "/index.html") || file_exists($dir . "/index.html.gz"));
}

$input_errors = array();
$savemsg = "";
$dir_suffix = preg_replace("/\W/", "", $_REQUEST['dir']);
$report_base_dir = sarg_reports_base_dir($dir_suffix);

if ($_POST['delete_report'] == "yes") {
	$report_name = preg_replace("/[^a-zA-Z0-9._-]/", "", $_POST['report_name']);
	if ($report_name == "") {
		$input_errors[] = gettext("Report name is invalid.");
	} elseif ($report_name == "images") {
		$input_errors[] = gettext("The shared images directory cannot be deleted.");
	} else {
		$full_path = "{$report_base_dir}/{$report_name}";
		if (!is_dir($full_path)) {
			$input_errors[] = sprintf(gettext("Report '%s' not found."), htmlspecialchars($report_name));
		} elseif (strpos(realpath($full_path), realpath($report_base_dir)) !== 0) {
			$input_errors[] = gettext("Invalid report path.");
		} else {
			conf_mount_rw();
			rmdir_recursive($full_path);
			conf_mount_ro();
			$savemsg = sprintf(gettext("Report '%s' deleted successfully."), htmlspecialchars($report_name));
		}
	}
}

$pgtitle = array(gettext("Package"), gettext("Sarg"), gettext("Reports"));
$shortcut_section = "sarg";
include("head.inc");

if (file_exists("/usr/local/www/sarg_ng.php")) {
    $sarg_frame = "sarg_ng.php";
    $wd = "106%";
    $ht = "7640";
} else {
    $sarg_frame = "sarg_frame.php";
    $wd = "100%";
    $ht = "600";
}

if ($_REQUEST['dir'] != "") {
    $sarg_frame .= "?dir=" . preg_replace("/\W/", "", $_REQUEST['dir']) . "&";
} else {
    $sarg_frame .= "?";
}
    
?>
<body link="#0000CC" vlink="#0000CC" alink="#0000CC">
<form method="post">
<div id="mainlevel">
<table width="100%" border="0" cellpadding="0" cellspacing="0">
	<tr><td>
		<?php
		if (!empty($input_errors)) {
			print_input_errors($input_errors);
		}
		if (!empty($savemsg)) {
			print_info_box($savemsg);
		}
		?>
		<?php
		$tab_array = array();
		$tab_array[] = array(gettext("General"), false, "/pkg_edit.php?xml=sarg.xml&id=0");
		$tab_array[] = array(gettext("Users"), false, "/pkg_edit.php?xml=sarg_users.xml&id=0");
		$tab_array[] = array(gettext("Schedule"), false, "/pkg.php?xml=sarg_schedule.xml");
		$tab_array[] = array(gettext("View Report"), true, "/sarg_reports.php");
		$tab_array[] = array(gettext("XMLRPC Sync"), false, "/pkg_edit.php?xml=sarg_sync.xml&id=0");
		display_top_tabs($tab_array);
		$rdirs = array();
		mwexec('/bin/rm -f /usr/local/www/sarg-images/temp/*');
		if (is_array($config['installedpackages']['sargschedule']['config'])) {
		    $scs = $config['installedpackages']['sargschedule']['config'];
		    foreach ($scs as $sc) {
		        if ($sc['foldersuffix'] == "") {
		            $rdirs['default'] = "";
		        } else {
		            $rdirs[$sc['foldersuffix']] = "?dir={$sc['foldersuffix']}";
		        }
		    }
		}
		foreach ($rdirs as $rdir_i => $rdir_d ) {
		    $m_sel = false;
		    if (preg_match("/dir=$rdir_i/",$_SERVER['REQUEST_URI'])) {
		      $m_sel = true;
		    }
		    if ($rdir_i == "default"  && ! preg_match ("/dir/", $_SERVER['REQUEST_URI'])) {
		        $m_sel = true;
		    }
		    $tab_array2[] = array(gettext("$rdir_i Reports"), $m_sel, "/sarg_reports.php$rdir_d");
		    
		}
		if (count ($rdirs) > 1 || !array_key_exists("default",$rdirs)) {
		  display_top_tabs($tab_array2);
		}
		?>
	</td></tr>
	<tr><td>
		<div id="mainarea">
		</div>
		<br />
		<?php $reports = sarg_list_report_directories($report_base_dir); ?>
		<?php
		$dest_msg = sprintf(gettext("Report destination folder: %s"), htmlspecialchars($report_base_dir));
		if (sarg_has_root_report($report_base_dir)) {
			$dest_msg .= "<br />" . gettext("A main index report exists in this folder (root index.html).");
		}
		print_info_box($dest_msg, 'info', false);
		?>
		<div class="panel panel-default">
			<div class="panel-heading"><h2 class="panel-title"><?=gettext("Manage stored reports")?></h2></div>
			<div class="panel-body">
				<?php if (empty($reports)): ?>
					<?=gettext("No generated report directories were found for this view.")?>
				<?php else: ?>
					<table class="table table-striped table-condensed">
						<thead>
							<tr>
								<th><?=gettext("Report directory")?></th>
								<th><?=gettext("Last update")?></th>
								<th><?=gettext("Actions")?></th>
							</tr>
						</thead>
						<tbody>
							<?php foreach ($reports as $report): ?>
							<tr>
								<td><?=htmlspecialchars($report['name'])?></td>
								<td><?=date("Y-m-d H:i:s", $report['mtime'])?></td>
								<td>
									<button type="submit" class="btn btn-danger btn-xs"
									        name="report_name" value="<?=htmlspecialchars($report['name'])?>"
									        onclick="return confirm('<?=gettext("Delete this report directory? This action cannot be undone.")?>');">
										<?=gettext("Delete")?>
									</button>
								</td>
							</tr>
							<?php endforeach; ?>
						</tbody>
					</table>
				<?php endif; ?>
				<input type="hidden" name="delete_report" value="yes" />
			</div>
		</div>
		<script type="text/javascript">
		//<![CDATA[
		var axel = Math.random() + "";
		var num = axel * 1000000000000000000;
		document.writeln('<iframe src="/<?=$sarg_frame ?>prevent='+ num +'?"  frameborder="0" width="<?=$wd ?>" height="<?=$ht ?>"></iframe>');
		//]]>
		</script>
		<div id="file_div"></div>
	</td></tr>
</table>
</div>
</form>
</body>
</html>
