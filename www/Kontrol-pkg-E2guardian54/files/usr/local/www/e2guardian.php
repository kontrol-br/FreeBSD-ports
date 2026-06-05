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

function e2g_blacklist_notice($message) {
        $error = null;
        file_notice("E2guardian", $error, "E2guardian - " . gettext($message), "");
}

function e2g_blacklist_lock() {
        $lock_handle = @fopen('/tmp/e2guardian-blacklist.lock', 'c');
        if ($lock_handle === false || !@flock($lock_handle, LOCK_EX | LOCK_NB)) {
                if (is_resource($lock_handle)) {
                        @fclose($lock_handle);
                }
                e2g_blacklist_notice("A blacklist update is already running.");
                return false;
        }
        return $lock_handle;
}

function e2g_blacklist_unlock($lock_handle) {
        if (is_resource($lock_handle)) {
                @flock($lock_handle, LOCK_UN);
                @fclose($lock_handle);
        }
}

function e2g_blacklist_remove_temp_dir($temp_dir) {
        $temp_prefix = rtrim(sys_get_temp_dir(), '/') . '/e2guardian-blacklist-';
        if (is_string($temp_dir) && strpos($temp_dir, $temp_prefix) === 0 && is_dir($temp_dir)) {
                e2g_delTree($temp_dir);
        }
}

function e2g_blacklist_finish($temp_dir = false, $owns_lock = false, $lock_handle = null) {
        e2g_blacklist_remove_temp_dir($temp_dir);
        if ($owns_lock) {
                e2g_blacklist_unlock($lock_handle);
        }
}

function e2g_compare_list_descriptions($a, $b) {
        return strnatcasecmp($a['descr'], $b['descr']);
}

function e2g_blacklist_archive_file() {
        $tgz_file = E2GUARDIAN_PKGDIR . "/blacklist.tgz";
        $tar_gz_file = E2GUARDIAN_PKGDIR . "/blacklist.tar.gz";

        if (file_exists($tgz_file)) {
                return $tgz_file;
        }
        if (file_exists($tar_gz_file)) {
                return $tar_gz_file;
        }

        return $tgz_file;
}

function e2g_blacklist_safe_archive_path($entry) {
        $entry = str_replace('\\', '/', trim($entry));
        if ($entry === '' || $entry === '.' || substr($entry, 0, 1) === '/') {
                return false;
        }
        foreach (explode('/', $entry) as $component) {
                if ($component === '..') {
                        return false;
                }
        }
        return true;
}

function e2g_blacklist_validate_link_target($target) {
        $target = str_replace('\\', '/', trim($target));
        if ($target === '' || substr($target, 0, 1) === '/') {
                return false;
        }
        foreach (explode('/', $target) as $component) {
                if ($component === '..') {
                        return false;
                }
        }
        return true;
}

function e2g_blacklist_validate_extracted_tree($path) {
        if (is_link($path)) {
                return e2g_blacklist_validate_link_target(readlink($path));
        }
        if (is_file($path)) {
                return true;
        }
        if (!is_dir($path)) {
                return false;
        }

        foreach (array_diff(scandir($path), array('.', '..')) as $entry) {
                if (!e2g_blacklist_validate_extracted_tree($path . '/' . $entry)) {
                        return false;
                }
        }
        return true;
}

function e2g_blacklist_validate_archive($blacklist_file) {
        $entries = array();
        exec('/usr/bin/tar -tzPf ' . escapeshellarg($blacklist_file) . ' 2>&1', $entries, $return);
        if ($return !== 0 || empty($entries)) {
                return false;
        }
        foreach ($entries as $entry) {
                if (!e2g_blacklist_safe_archive_path($entry)) {
                        return false;
                }
        }
        return $prepared_dir;
}

        $verbose_entries = array();
        exec('/usr/bin/tar -tvzPf ' . escapeshellarg($blacklist_file) . ' 2>&1', $verbose_entries, $return);
        if ($return !== 0 || empty($verbose_entries)) {
                return false;
        }
        foreach ($verbose_entries as $entry) {
                $type = substr($entry, 0, 1);
                if ($type === '-' || $type === 'd') {
                        continue;
                }
                if ($type === 'l') {
                        $parts = explode(' -> ', $entry, 2);
                        if (count($parts) !== 2 || !e2g_blacklist_validate_link_target($parts[1])) {
                                return false;
                        }
                        continue;
                }
                if ($type === 'h') {
                        $parts = explode(' link to ', $entry, 2);
                        if (count($parts) !== 2 || !e2g_blacklist_validate_link_target($parts[1])) {
                                return false;
                        }
                        continue;
                }
                return false;
        }
        return true;
}

function e2g_blacklist_create_temp_dir() {
        $temp_dir = @tempnam(sys_get_temp_dir(), 'e2guardian-blacklist-');
        if ($temp_dir === false) {
                return false;
        }
        @unlink($temp_dir);
        if (!@mkdir($temp_dir, 0700)) {
                return false;
        }
        return $temp_dir;
}

function e2g_blacklist_prepare_tree($temp_dir) {
        $entries = array_values(array_diff(scandir($temp_dir), array('.', '..')));
        if (empty($entries)) {
                return false;
        }
        if (count($entries) === 1 && is_dir($temp_dir . '/' . $entries[0])) {
                $single_dir = $temp_dir . '/' . $entries[0];
                if ($entries[0] === 'blacklists') {
                        return $single_dir;
                }
                if (is_dir($single_dir . '/blacklists')) {
                        return $single_dir . '/blacklists';
                }
                if (is_dir($single_dir . '/BL')) {
                        return $single_dir . '/BL';
                }
                return $single_dir;
        }

        $prepared_dir = $temp_dir . '/.prepared-blacklists';
        if (!@mkdir($prepared_dir, 0700)) {
                return false;
        }
        foreach ($entries as $entry) {
                if (!@rename($temp_dir . '/' . $entry, $prepared_dir . '/' . $entry)) {
                        return false;
                }
        }
        return $prepared_dir;
}

function e2g_blacklist_download_archive($url, $blacklist_file, $install_process = false, $options = array()) {
        $download_dir = dirname($blacklist_file);
        if (!is_dir($download_dir) && !@mkdir($download_dir, 0755, true)) {
                e2g_blacklist_notice("Could not create the blacklist download directory.");
                return false;
        }
        $temp_file = @tempnam($download_dir, '.blacklist-download-');
        if ($temp_file === false) {
                e2g_blacklist_notice("Could not create a temporary blacklist download file.");
                return false;
        }

        $return = 1;
        if (!empty($options['source_file'])) {
                $return = @copy($options['source_file'], $temp_file) ? 0 : 1;
        } elseif ($install_process && function_exists('download_file_with_progress_bar')) {
                download_file_with_progress_bar($url, $temp_file);
                $return = file_exists($temp_file) ? 0 : 1;
        } else {
                exec("/usr/bin/fetch -o " . escapeshellarg($temp_file) . " " . escapeshellarg($url), $output, $return);
        }

        if ($return !== 0 || !file_exists($temp_file) || filesize($temp_file) === 0) {
                @unlink($temp_file);
                e2g_blacklist_notice("Could not fetch blacklists from url.");
                return false;
        }
        if (!e2g_blacklist_validate_archive($temp_file)) {
                @unlink($temp_file);
                e2g_blacklist_notice("Downloaded blacklist archive is invalid, empty, or contains unsafe paths or links. Previous archive was kept.");
                return false;
        }
        if (!@rename($temp_file, $blacklist_file)) {
                @unlink($temp_file);
                e2g_blacklist_notice("Could not install the downloaded blacklist archive. Previous archive was kept.");
                return false;
        }
        return true;
}

function fetch_blacklist($log_notice = true, $install_process = false) {
        global $config, $g;
        $lock_handle = e2g_blacklist_lock();
        if ($lock_handle === false) {
                return false;
        }

        $result = false;
        $blacklist_file = e2g_blacklist_archive_file();
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
                } elseif ($install_process == true && file_exists($blacklist_file) && e2g_blacklist_validate_archive($blacklist_file)) {
                        update_output_window("Found previous blacklist database, skipping download...");
                        $result = extract_black_list($log_notice, $lock_handle);
                        e2g_blacklist_unlock($lock_handle);
                        return $result;
                } elseif ($install_process == true) {
                        update_output_window("Fetching blacklist");
                }

                if (e2g_blacklist_download_archive($url, $blacklist_file, $install_process)) {
                        $result = extract_black_list($log_notice, $lock_handle);
                        e2g_blacklist_unlock($lock_handle);
                        return $result;
                }
        } else {
                if ($install_process == true) {
                        read_lists(false, $uw);
                } elseif (!empty($url)) {
                        file_notice("E2guardian", $error, "E2guardian" . gettext("Blacklist url is invalid."), "");
                }
        }
        e2g_blacklist_unlock($lock_handle);
        return false;
}

function extract_black_list($log_notice = true, $lock_handle = null, $options = array()) {
        $owns_lock = false;
        if (!is_resource($lock_handle)) {
                $lock_handle = e2g_blacklist_lock();
                if ($lock_handle === false) {
                        return false;
                }
                $owns_lock = true;
        }

        $temp_dir = false;
        if (!empty($options['hold_lock_seconds'])) {
                sleep((int)$options['hold_lock_seconds']);
        }
        $blacklist_file = isset($options['blacklist_file']) ? $options['blacklist_file'] : e2g_blacklist_archive_file();
        if (!file_exists($blacklist_file)) {
                e2g_blacklist_notice("Downloaded blacklists not found.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }
        if (!e2g_blacklist_validate_archive($blacklist_file)) {
                e2g_blacklist_notice("Blacklist archive is invalid, empty, or contains unsafe paths or links.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }

        $lists_dir = isset($options['lists_dir']) ? $options['lists_dir'] : E2GUARDIAN_ETCDIR . "/lists";
        $blacklists_dir = $lists_dir . '/blacklists';
        $blacklists_old_dir = $lists_dir . '/blacklists.old';
        if (!is_dir($lists_dir) && !@mkdir($lists_dir, 0755, true)) {
                e2g_blacklist_notice("Could not create the E2guardian lists directory.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }

        $temp_dir = e2g_blacklist_create_temp_dir();
        if ($temp_dir === false) {
                e2g_blacklist_notice("Could not create a temporary blacklist extraction directory.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }
        exec('/usr/bin/tar -xzf ' . escapeshellarg($blacklist_file) . ' -C ' . escapeshellarg($temp_dir) . ' 2>&1', $output, $return);
        if ($return !== 0) {
                e2g_blacklist_notice("Could not extract blacklist archive.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }

        if (!e2g_blacklist_validate_extracted_tree($temp_dir)) {
                e2g_blacklist_notice("Blacklist archive contains unsafe extracted entries.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }

        $prepared_dir = e2g_blacklist_prepare_tree($temp_dir);
        if ($prepared_dir === false || !is_dir($prepared_dir)) {
                e2g_blacklist_notice("Could not determine Blacklist extract dir. Categories not updated.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }

        if (is_dir($blacklists_old_dir) && is_dir($blacklists_dir)) {
                e2g_delTree($blacklists_old_dir);
        }
        if (is_dir($blacklists_old_dir) && !is_dir($blacklists_dir) && !@rename($blacklists_old_dir, $blacklists_dir)) {
                e2g_blacklist_notice("Could not restore the previous blacklist tree.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }
        if (is_dir($blacklists_dir) && !@rename($blacklists_dir, $blacklists_old_dir)) {
                e2g_blacklist_notice("Could not preserve the previous blacklist tree.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }
        $simulate_install_failure = !empty($options['simulate_install_failure']);
        if ($simulate_install_failure || !@rename($prepared_dir, $blacklists_dir)) {
                if (!is_dir($blacklists_dir) && is_dir($blacklists_old_dir)) {
                        @rename($blacklists_old_dir, $blacklists_dir);
                }
                e2g_blacklist_notice("Could not install the new blacklist tree; the previous tree was restored.");
                e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
                return false;
        }

        if (empty($options['skip_read_lists'])) {
                read_lists($log_notice);
        }
        if (is_dir($blacklists_old_dir)) {
                e2g_delTree($blacklists_old_dir);
        }
        e2g_blacklist_finish($temp_dir, $owns_lock, $lock_handle);
        return true;
}

function read_lists($log_notice=true, $uw="") {
        global $config, $g;

        $dir = E2GUARDIAN_ETCDIR . "/lists";
        $groups = array("phraselists", "blacklists", "whitelists");
        $liston = isset($config['installedpackages']['e2guardianblacklist']['config'][0]['liston']) ? $config['installedpackages']['e2guardianblacklist']['config'][0]['liston'] : 'banned';
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
                        usort($entries, 'e2g_compare_list_descriptions');
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
