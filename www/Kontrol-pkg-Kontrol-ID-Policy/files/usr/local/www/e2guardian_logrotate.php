#!/usr/local/bin/php
<?php
/*
 * e2guardian_scheds.php
 *
 * part of Unofficial packages for pfSense(R) softwate
 * Copyright (c) 2017 Marcello Coutinho
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
require_once("config.inc");
require_once("functions.inc");
require_once("globals.inc");
require_once("interfaces.inc");
require_once("notices.inc");
require_once("pkg-utils.inc");
require_once("services.inc");
require_once("util.inc");
require_once("filter.inc");
if ($pfs_version == "2.3" ) {
        require_once("xmlrpc.inc");
}
require_once("xmlrpc_client.inc");
require_once("e2guardian.inc");
require_once("service-utils.inc");

$e2guardian_cfg = $config['installedpackages']['e2guardian']['config'][0];
$watchdog_cmd = "/usr/local/bin/e2g_watchdog.sh";

e2g_log_info("e2guardian - rotating logs.");
e2g_log_info("e2guardian - stopping watchdog processes before rotation.");
mwexec("/usr/bin/pkill -f {$watchdog_cmd} 2>/dev/null");

//TODO: Make all of this less hardcoded and hacky
mwexec(escapeshellarg(KONTROL_ID_SERVICE_SCRIPT) . ' stop');

e2g_log_info("e2guardian - stopping");
$e2guardian_log = $config['installedpackages']['e2guardianlog']['config'][0];
$logfilecount = ($e2guardian_log['logcount'] ? $e2guardian_log['logcount'] : "30");
$log = '/var/log/e2guardian/access.log';
// This script is the logrotate script file distributed with e2guardian translated to php
if (file_exists("{$log}.{$logfilecount}")) {
    unlink("{$log}.{$logfilecount}");
}
$n = $logfilecount - 1;
while ($n > 0) {
    $m = $n + 1;
    if (file_exists("{$log}.{$n}")) {
        rename("{$log}.{$n}", "{$log}.{$m}");
    }
    $n--;
}
if (file_exists($log)) {
    rename($log, "{$log}.1");
}

// request.log is a bounded, transient Real Time buffer.  access.log remains
// the canonical audit/Sarg source, so retaining request.log archives would
// duplicate data and consume disk without adding reporting value.
$realtime_log = '/var/log/e2guardian/request.log';
if (file_exists($realtime_log)) {
    unlink($realtime_log);
}
foreach (glob("{$realtime_log}.*") ?: array() as $realtime_archive) {
    if (preg_match('/^request\.log\.[0-9]+$/', basename($realtime_archive))) {
        unlink($realtime_archive);
    }
}

//$result = system($script);
//log_error("e2guardian - Rotate command result: " . $result);

e2g_log_info("e2guardian - starting");
mwexec(escapeshellarg(KONTROL_ID_SERVICE_SCRIPT) . ' start');

$e2guardian_pid_file = KONTROL_ID_PIDFILE;
$e2guardian_ready = false;
for ($i = 0; $i < 30; $i++) {
    clearstatcache();
    if (file_exists($e2guardian_pid_file)) {
        $pid = trim(@file_get_contents($e2guardian_pid_file));
        if (!empty($pid) && ctype_digit($pid) && posix_kill((int)$pid, 0)) {
            $e2guardian_ready = true;
            break;
        }
    }
    sleep(1);
}

$e2guardian_cfg = $config['installedpackages']['e2guardian']['config'][0];
if ($e2guardian_cfg['watchdog'] == "on") {
    if ($e2guardian_ready) {
        e2g_log_info("e2guardian - restarting watchdog after rotation.");
        mwexec_bg("/bin/sh {$watchdog_cmd}");
    } else {
        e2g_log_warning("e2guardian - watchdog restart deferred: e2guardian pid not ready.");
    }
} else {
    e2g_log_info("e2guardian - watchdog remains disabled after rotation.");
}

e2g_log_info("e2guardian - log rotation complete.");
?>
