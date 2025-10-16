<?php
/* $Id$ */
/* ========================================================================== */
/*
	e2guardian.php
	Copyright (C) 2015-2017 Marcello Coutinho
	part of pfSense (http://www.pfSense.com)
	All rights reserved.
*/
/* ========================================================================== */
/*
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
/* ========================================================================== */

require_once("/etc/inc/util.inc");
require_once("/etc/inc/functions.inc");
require_once("/etc/inc/pkg-utils.inc");
require_once("/etc/inc/globals.inc");
require_once("/usr/local/pkg/e2guardian.inc");

function fetch_blacklist($log_notice = true, $install_process = false) {
	global $config, $g;
	if (is_array($config['installedpackages']['e2guardianblacklist']) && is_array($config['installedpackages']['e2guardianblacklist']['config'])) {
		$url = $config['installedpackages']['e2guardianblacklist']['config'][0]['url'];
		$uw = "Found a previous install, checking Blacklist config...";
	} else {
		$uw = "Found a clean install, reading default access lists...";
	}
	if ($install_process == true) {
		update_output_window($uw);
	}
	if (isset($url) && is_url($url)) {
		if ($log_notice == true) {
			print "file download start..";
			unlink_if_exists("/usr/local/pkg/blacklist.tgz");
			exec("/usr/bin/fetch -o /usr/local/pkg/blacklist.tgz " . escapeshellarg($url), $output, $return);
		} else {
			//install process
			if (file_exists("/usr/local/pkg/blacklist.tgz")) {
				update_output_window("Found previous blacklist database, skipping download...");
				$return = 0;
			} else {
				update_output_window("Fetching blacklist");
				download_file_with_progress_bar($url, "/usr/local/pkg/blacklist.tgz");
				if (file_exists("/usr/local/pkg/blacklist.tgz")) {
					$return = 0;
				}
			}
		}
		if ($return == 0) {
			extract_black_list($log_notice);
		} else {
			file_notice("E2guardian",$error,"E2guardian" . gettext("Could not fetch blacklists from url"), "");
		}
	} else {
		if ($install_process == true) {
			read_lists(false, $uw);
		} elseif (!empty($url)) {
			file_notice("E2guardian",$error,"E2guardian" . gettext("Blacklist url is invalid."), "");
		}
	}
}
function extract_black_list($log_notice=true) {
        if (!file_exists("/usr/local/pkg/blacklist.tgz")) {
                file_notice("E2guardian", $error, "E2guardian" . gettext("Downloaded blacklists not found"), "");
                return;
        }

        $lists_dir = "/usr/local/etc/e2guardian/lists";
        if (!is_dir($lists_dir)) {
                @mkdir($lists_dir, 0755, true);
        }

        $cwd = getcwd();
        chdir($lists_dir);

        if (is_dir('blacklists.old')) {
                e2g_delTree($lists_dir . '/blacklists.old');
        }
        if (is_dir('blacklists')) {
                @rename('blacklists', 'blacklists.old');
        }

        exec('/usr/bin/tar -xzf /usr/local/pkg/blacklist.tgz 2>&1', $output, $return);
        if ($return !== 0) {
                if (is_dir('blacklists.old')) {
                        @rename('blacklists.old', 'blacklists');
                }
                if (isset($cwd)) {
                        chdir($cwd);
                }
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not extract blacklist archive."), "");
                return;
        }

        $entries = array_diff(scandir('.'), array('.', '..', 'blacklists', 'blacklists.old'));
        $dirs = array();
        foreach ($entries as $entry) {
                if (is_dir($entry)) {
                        $dirs[] = $entry;
                }
        }

        if (!is_dir('blacklists')) {
                if (count($dirs) === 1) {
                        @rename($dirs[0], 'blacklists');
                } else {
                        @mkdir('blacklists', 0755, true);
                        foreach ($entries as $entry) {
                                @rename($entry, 'blacklists/' . $entry);
                        }
                }
        }

        if (!is_dir('blacklists')) {
                if (is_dir('blacklists.old')) {
                        @rename('blacklists.old', 'blacklists');
                }
                if (isset($cwd)) {
                        chdir($cwd);
                }
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not determine Blacklist extract dir. Categories not updated"), "");
                return;
        }

        read_lists($log_notice);
        e2g_delTree($lists_dir . '/blacklists.old');

        if (isset($cwd)) {
                chdir($cwd);
        }
}

function read_lists($log_notice=true, $uw="") {
        global $config, $g;

        $dir = "/usr/local/etc/e2guardian/lists";
        $groups = array("phraselists", "blacklists", "whitelists");
        $liston = $config['installedpackages']['e2guardianblacklist']['config'][0]['liston'] ?? 'banned';
        $metadata = e2g_parse_blacklist_metadata($dir . '/blacklists');

        foreach ($config['installedpackages'] as $key => $values) {
                if (preg_match("/e2guardian(phrase|black|white)lists/", $key)) {
                        unset($config['installedpackages'][$key]);
                }
        }

        $collection = array(
                'phraselists' => array(),
                'blacklists' => array(),
                'whitelists' => array()
        );

        foreach ($groups as $group) {
                $group_dir = $dir . '/' . $group;
                if (!is_dir($group_dir)) {
                        continue;
                }

                $lists = array_diff(scandir($group_dir), array('.', '..'));
                foreach ($lists as $list) {
                        $path = $group_dir . '/' . $list;
                        if (is_dir($path)) {
                                e2g_collect_category($collection, $group_dir, $group, array($list), $liston, $metadata);
                        } else {
                                e2g_register_list_file($collection, $group, array($list), $path, $list, $liston, $metadata);
                        }
                }
        }

        foreach ($collection as $group => $types) {
                foreach ($types as $xml_type => $entries) {
                        if (empty($entries)) {
                                continue;
                        }
                        $entries = e2g_unique_entries($entries);
                        usort($entries, function ($a, $b) {
                                return strnatcasecmp($a['descr'], $b['descr']);
                        });
                        $config['installedpackages']['e2guardian' . $group . $xml_type]['config'] = $entries;
                }
        }

        if (!empty($metadata)) {
                $config['installedpackages']['e2guardianblacklist']['categories_meta'] = $metadata;
        } else {
                unset($config['installedpackages']['e2guardianblacklist']['categories_meta']);
        }

        $files = array("site", "url");
        $blacklist_domains = array();
        if (isset($config['installedpackages']['e2guardianblacklistsdomains']['config']) &&
            is_array($config['installedpackages']['e2guardianblacklistsdomains']['config'])) {
                $blacklist_domains = $config['installedpackages']['e2guardianblacklistsdomains']['config'];
        }

        foreach ($files as $edit_xml) {
                $edit_file = file_get_contents("/usr/local/pkg/e2guardian_" . $edit_xml . "_acl.xml");
                if (count($blacklist_domains) > 18) {
                        $edit_file = preg_replace('/size.6/', 'size>20', $edit_file);
                        if ($config['installedpackages']['e2guardianblacklist']['config'][0]["liston"] == "both") {
                                $edit_file = preg_replace('/size.5/', 'size>19', $edit_file);
                        }
                } else {
                        $edit_file = preg_replace('/size.20/', 'size>6', $edit_file);
                }
                if ($config['installedpackages']['e2guardianblacklist']['config'][0]["liston"] != "both") {
                        $edit_file = preg_replace('/size.19/', 'size>5', $edit_file);
                }
                file_put_contents("/usr/local/pkg/e2guardian_" . $edit_xml . "_acl.xml", $edit_file, LOCK_EX);
        }

        write_config("Saving...");
        if ($log_notice == true && $uw == "") {
                file_notice("E2guardian", "E2Guardian Blacklist applied, check site and URL access lists for categories", "E2guardian BlackList Updated.");
        } else {
                $uw .= "done\n";
                update_output_window($uw);
        }
}

if ($argv[1] == "update_lists") {
	extract_black_list();
}

if ($argv[1] == "fetch_blacklist") {
	fetch_blacklist();
}

?>
