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
if (!defined('E2GUARDIAN_DIR')) {
        define('E2GUARDIAN_DIR', '/usr/local');
}
require_once(E2GUARDIAN_DIR . "/pkg/e2guardian.inc");

function fetch_blacklist($log_notice = true, $install_process = false) {
        global $config, $g;
        $blacklist_file = E2GUARDIAN_PKGDIR . "/blacklist.tgz";
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
                        unlink_if_exists($blacklist_file);
                        exec("/usr/bin/fetch -o " . escapeshellarg($blacklist_file) . " " . escapeshellarg($url), $output, $return);
                } else {
                        //install process
                        if (file_exists($blacklist_file)) {
                                update_output_window("Found previous blacklist database, skipping download...");
                                $return = 0;
                        } else {
                                update_output_window("Fetching blacklist");
                                if (function_exists('download_file_with_progress_bar')) {
                                        download_file_with_progress_bar($url, $blacklist_file);
                                } else {
                                        exec("/usr/bin/fetch -o " . escapeshellarg($blacklist_file) . " " . escapeshellarg($url), $output, $return);
                                }
                                if (file_exists($blacklist_file)) {
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
function e2g_blacklist_archive_is_safe($blacklist_file) {
        $entries = array();
        exec('/usr/bin/tar -tzf ' . escapeshellarg($blacklist_file) . ' 2>&1', $entries, $return);
        if ($return !== 0 || empty($entries)) {
                return false;
        }

        foreach ($entries as $entry) {
                $entry = trim((string)$entry);
                if ($entry === '') {
                        continue;
                }

                if ($entry[0] === '/' || strpos($entry, "\0") !== false) {
                        return false;
                }

                $parts = explode('/', $entry);
                foreach ($parts as $part) {
                        if ($part === '..') {
                                return false;
                        }
                }
        }

        return true;
}

function e2g_blacklist_tree_has_lists($dir) {
        if (!is_dir($dir)) {
                return false;
        }

        if (is_file($dir . '/global_usage')) {
                return true;
        }

        $entries = array_diff(scandir($dir), array('.', '..'));
        foreach ($entries as $entry) {
                $path = $dir . '/' . $entry;
                if (is_file($path) && in_array($entry, array('domains', 'urls'), true)) {
                        return true;
                }
                if (is_dir($path) && e2g_blacklist_tree_has_lists($path)) {
                        return true;
                }
        }

        return false;
}

function e2g_find_blacklist_source_dir($extract_dir) {
        $candidates = array(
                $extract_dir . '/BL',
                $extract_dir . '/blacklists/BL',
                $extract_dir . '/blacklists'
        );

        foreach ($candidates as $candidate) {
                if (e2g_blacklist_tree_has_lists($candidate)) {
                        return $candidate;
                }
        }

        $dirs = array();
        $entries = array_diff(scandir($extract_dir), array('.', '..'));
        foreach ($entries as $entry) {
                $path = $extract_dir . '/' . $entry;
                if (is_dir($path)) {
                        $dirs[] = $path;
                }
        }

        if (count($dirs) === 1 && e2g_blacklist_tree_has_lists($dirs[0])) {
                return $dirs[0];
        }

        if (e2g_blacklist_tree_has_lists($extract_dir)) {
                return $extract_dir;
        }

        return '';
}

function extract_black_list($log_notice=true) {
        $blacklist_file = E2GUARDIAN_PKGDIR . "/blacklist.tgz";
        if (!file_exists($blacklist_file)) {
                file_notice("E2guardian", $error, "E2guardian" . gettext("Downloaded blacklists not found"), "");
                return;
        }

        if (!e2g_blacklist_archive_is_safe($blacklist_file)) {
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Blacklist archive is invalid or contains unsafe paths."), "");
                return;
        }

        $lists_dir = E2GUARDIAN_ETCDIR . "/lists";
        $blacklists_dir = $lists_dir . "/blacklists";
        if (!is_dir($blacklists_dir) && !@mkdir($blacklists_dir, 0755, true) && !is_dir($blacklists_dir)) {
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not create blacklist target directory."), "");
                return;
        }

        $pid = getmypid();
        $extract_dir = $lists_dir . '/.blacklists.extract.' . $pid;
        $new_parent = $lists_dir . '/.blacklists.new.' . $pid;
        $new_bl = $new_parent . '/BL';
        $target_bl = $blacklists_dir . '/BL';
        $backup_bl = $blacklists_dir . '/BL.old.' . $pid;

        e2g_delTree($extract_dir);
        e2g_delTree($new_parent);
        e2g_delTree($backup_bl);

        if (!@mkdir($extract_dir, 0755, true) || !@mkdir($new_parent, 0755, true)) {
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not create temporary blacklist directory."), "");
                return;
        }

        exec('/usr/bin/tar -xzf ' . escapeshellarg($blacklist_file) . ' -C ' . escapeshellarg($extract_dir) . ' 2>&1', $output, $return);
        if ($return !== 0) {
                e2g_delTree($extract_dir);
                e2g_delTree($new_parent);
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not extract blacklist archive."), "");
                return;
        }

        $source_dir = e2g_find_blacklist_source_dir($extract_dir);
        if ($source_dir === '') {
                e2g_delTree($extract_dir);
                e2g_delTree($new_parent);
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not determine Blacklist extract dir. Categories not updated"), "");
                return;
        }

        if (!@rename($source_dir, $new_bl)) {
                e2g_delTree($extract_dir);
                e2g_delTree($new_parent);
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not stage blacklist content."), "");
                return;
        }

        if (is_dir($target_bl) && !@rename($target_bl, $backup_bl)) {
                e2g_delTree($extract_dir);
                e2g_delTree($new_parent);
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not backup current blacklist content."), "");
                return;
        }

        if (!@rename($new_bl, $target_bl)) {
                if (is_dir($backup_bl)) {
                        @rename($backup_bl, $target_bl);
                }
                e2g_delTree($extract_dir);
                e2g_delTree($new_parent);
                file_notice("E2guardian", $error, "E2guardian - " . gettext("Could not install blacklist content."), "");
                return;
        }

        read_lists($log_notice);

        e2g_delTree($backup_bl);
        e2g_delTree($extract_dir);
        e2g_delTree($new_parent);
}

function read_lists($log_notice=true, $uw="") {
        global $config, $g;

        $dir = E2GUARDIAN_ETCDIR . "/lists";
        $groups = array("phraselists", "blacklists", "whitelists");
        $liston = $config['installedpackages']['e2guardianblacklist']['config'][0]['liston'] ?? 'banned';
        $blacklist_root = (is_dir($dir . '/blacklists/BL') ? $dir . '/blacklists/BL' : $dir . '/blacklists');
        $metadata = e2g_parse_blacklist_metadata($blacklist_root);

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

                $scan_dir = $group_dir;
                if ($group === 'blacklists' && is_dir($group_dir . '/BL')) {
                        $scan_dir = $group_dir . '/BL';
                }

                $lists = array_diff(scandir($scan_dir), array('.', '..'));
                foreach ($lists as $list) {
                        $path = $scan_dir . '/' . $list;
                        if (is_dir($path)) {
                                e2g_collect_category($collection, $scan_dir, $group, array($list), $liston, $metadata);
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
                $edit_file = file_get_contents(E2GUARDIAN_PKGDIR . "/e2guardian_" . $edit_xml . "_acl.xml");
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
                file_put_contents(E2GUARDIAN_PKGDIR . "/e2guardian_" . $edit_xml . "_acl.xml", $edit_file, LOCK_EX);
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
