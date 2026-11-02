<?php
/*
 * e2guardian_monitor_data.php
 *
 * part of pfSense (https://www.pfsense.org)
 * Copyright (c) 2015 Rubicon Communications, LLC (Netgate)
 * Copyright (C) 2012-2017 Marcello Coutinho
 * Copyright (C) 2012-2014 Carlos Cesario <carloscesario@gmail.com>
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

require_once("functions.inc");
if (!defined('E2GUARDIAN_DIR')) {
        define('E2GUARDIAN_DIR', '/usr/local');
}
require_once(E2GUARDIAN_DIR . "/pkg/e2guardian.inc");

global $config;
if (is_array($config['installedpackages']['e2guardianlog'])) {
    $e2glog = $config['installedpackages']['e2guardianlog']['config'][0];
} else {
    $e2glog = array();
}

function e2gm($field){
    global $filter;
    // Apply filter and color
    // Need validate special chars
    if ($filter != "") {
            $field = preg_replace("@($filter)@i","<span><font color='red'>$1</font></span>", $field);
    }
    return $field;
}

/* Requests */
if ($_POST) {
    global $filter, $program, $e2glog, $traffic, $display_limit;
    $filter = preg_replace('/(@|!|>|<)/', "", htmlspecialchars($_POST['strfilter']));
    $program = strtolower($_POST['program']);
    $traffic = strtolower($_POST['traffic'] ?? 'all');
    if (!in_array($traffic, array('all', 'allowed', 'blocked'), true)) {
        $traffic = 'all';
    }
    $display_limit = is_numeric($_POST['maxlines'] ?? null) ? (int)$_POST['maxlines'] : 50;
    $display_limit = max(1, min(500, $display_limit));
    $listr = "class='listr' style='font-family: Consolas, Lucida Console, monospace;'";
    switch ($program) {
        case 'access':
            // Define log file
            $log = '/var/log/e2guardian/access.log';
            // Fetch lines
            $logarr = fetch_log($log);
            switch($e2glog['logfileformat']) {
                case 1:
                    show_tds(array("","Date", "IP", "Url", "Response", "User", "Group", "Blacklist", "Reason"));
                    $shown = 0;
                    foreach ($logarr as $logent) {
                        $live_marker = "\t__KONTROL_ID_TUNNEL_OPEN__";
                        $is_live = (substr($logent, -strlen($live_marker)) === $live_marker);
                        if ($is_live) {
                            $logent = substr($logent, 0, -strlen($live_marker));
                        }
                        //split log
                        if (preg_match("/^(\S+\s+\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*?)\s+([A-Z]+|-)\s+\d+\s+\d+\s+(.*?)\s+\d+\s+(\d+)\s+\S+\s+\S+\s+(\S+)/", $logent, $logline)) {

                            $response_code = (int)$logline[8];
                            $blocked = ($response_code === 403 || strpos($logline[5], '*DENIED*') !== false);
                            if (($traffic === 'allowed' && $blocked) || ($traffic === 'blocked' && !$blocked)) {
                                continue;
                            }

                                                    // Word wrap the URL
                                                    $url = htmlentities($logline[4]);
                                                    $logline[4] = preg_replace("@\<\>@","",$logline[4]);
                                                    $url = html_autowrap($url);

                                                        $logline[5] = html_autowrap($logline[5]);

                            echo "<tr valign='top'>\n";

                            if ($is_live) {
                                echo "<td><i class='fa fa-circle text-info'></i></td>\n";
                            } else if (preg_match("/(404|50\d)/",$logline[8])) {
                                echo "<td><i class='fa fa-times text-warning'></i></td>\n";
                            } else  if (preg_match("/40\d/",$logline[8])) {
                                echo "<td><i class='fa fa-times text-danger'></i></td>\n";
                                                    } else if (preg_match("/30\d/",$logline[8])) {
                                                          echo "<td><i class='fa fa-arrow-circle-o-right text-success'></i></td>\n";
                                                    } else {
                                                            echo "<td><i class='fa fa-check text-success'></i></td>\n";
                                                    }
                                                       echo "<td class='listlr' nowrap='nowrap'>" . e2gm($logline[1]) . "</td>\n";
                                                          echo "<td $listr>" . e2gm($logline[3]) . "</td>\n";
                                                       echo "<td $listr title='{$logline[4]}' width='*'>" . e2gm(preg_replace("/(\?|;).*/","",$url)) . "</td>\n";
                                                    echo "<td $listr>" . ($is_live ? gettext("LIVE") : e2gm($logline[8])) . "</td>\n";
                                                    echo "<td $listr>" . e2gm($logline[2]) . "</td>\n";
                                                    echo "<td $listr>" . e2gm($logline[9]) . "</td>\n";
                            $cat_val = ($logline[7] !== '' && $logline[7] !== '-') ? '<b>' . e2gm($logline[7]) . '</b>' : '-';
                            echo "<td $listr>" . $cat_val . "</td>\n";
                            $clean_reason = preg_replace('/\bhw[0-9]+:\s*/i', '', $logline[5]);
                            $reason = $is_live ?
                                gettext("Active TLS tunnel (SNI); final counters are recorded when it closes.") :
                                e2gm($clean_reason !== '' ? $clean_reason : '-');
                            echo "<td $listr>" . $reason . "</td>\n";
                                                    echo "</tr>\n";
                            $shown++;
                            if ($shown >= $display_limit) {
                                break;
                            }
                        }
                    }

                    break;
                case 3:
                    show_tds(array("","Date", "User", "IP", "Status", "Address"));
                    // Print lines
                    foreach ($logarr as $logent) {
                        // Split line by space delimiter
                        $logline = preg_split("/\s+/", $logent);

                        // Word wrap the URL
                        //$logline[7] = htmlentities($logline[7]);
                        $logline[7] = html_autowrap($logline[7]);

                        echo "<tr valign='top'>\n";
                        if (preg_match("/TCP_DENIED/",$logline[4])) {
                                                        echo "<td><i class='fa fa-times text-danger'></i></td>\n";
                                                } else if (preg_match("/MISS.(4|5)0/",$logline[4])) {
                                                        echo "<td><i class='fa fa-times text-warning'></i></td>\n";
                                                } else if (preg_match("/MISS.30\d/",$logline[4])) {
                                                       echo "<td><i class='fa fa-arrow-circle-o-right text-success'></i></td>\n";
                                                } else {
                                                        echo "<td><i class='fa fa-check text-success'></i></td>\n";
                                                }
                        echo "<td class='listlr' nowrap='nowrap'>" . e2gm("{$logline[0]} {$logline[1]}") . "</td>\n";
                        echo "<td $listr>" . e2gm($logline[8]) . "</td>\n";
                        echo "<td $listr>" . e2gm($logline[3]) . "</td>\n";
                        echo "<td $listr'>" . e2gm($logline[4]) . "</td>\n";
                        echo "<td $listr title='{$logline[7]}'width='*'>" . e2gm(preg_replace("/(\?|;).*/","",$logline[7])) . "</td>\n";
                        echo "</tr>\n";
                    }
                    break;
                case 4:
                        show_tds(array("","IP", "Method", "Url", "Response", "Reason", "List Category", "Group"));
                        foreach ($logarr as $logent) {
                        //split log
                        //if (preg_match("/(\S+\s+\S+) (\S+) (\S+) (\S+) (.*) (GET|OPTIONS|POST|CONNECT) \d+ \d+ (.*) \d (\d\d\d) \S+ \S+ (\S+)/", $logent, $logline)) {
                            //$logline = explode('/\t', $logent);
                            $logline = preg_split("/\t/", $logent);
                            // Word wrap the URL
                            $url = htmlentities($logline[3]);
                            $logline[3] = preg_replace("@\<\>@","",$logline[3]);
                            $url = html_autowrap($url);

                            $logline[4] = html_autowrap($logline[4]);

                            echo "<tr valign='top'>\n";

                            if (preg_match("/(404|50\d)/",$logline[10])) {
                                echo "<td><i class='fa fa-times text-warning'></i></td>\n";
                            } else  if (preg_match("/40\d/",$logline[10])) {
                                echo "<td><i class='fa fa-times text-danger'></i></td>\n";
                            } else if (preg_match("/30\d/",$logline[10])) {
                                echo "<td><i class='fa fa-arrow-circle-o-right text-success'></i></td>\n";
                            } else {
                                echo "<td><i class='fa fa-check text-success'></i></td>\n";
                                //echo "<td>" . print_r($logline) . "</td>\n";
                            }

                            echo "<td class='listlr' nowrap='nowrap'>" . e2gm($logline[1]) . "</td>\n";
                            echo "<td $listr>" . e2gm($logline[5]) . "</td>\n";

                            if ($_REQUEST['error'] == 'detailed') {
                                echo "<td $listr title='{$logline[3]}' width='*'>" . e2gm($url) . "</td>\n";
                            } else {
                                echo "<td $listr title='{$logline[3]}' width='*'>" . e2gm(preg_replace("/(\?|;).*/","",$url)) . "</td>\n";
                            }

                            echo "<td $listr>" . e2gm($logline[10]) . "</td>\n";
                            echo "<td $listr>" . e2gm($logline[4]) . "</td>\n";
                            echo "<td $listr>" . e2gm($logline[8]) . "</td>\n";
                            echo "<td $listr>" . e2gm($logline[13]) . "</td>\n";

//print_r($logline);
                            echo "</tr>\n";
                        }

                    break;
                default:
                    print "e2guardian log format selected is not implemented yet";
                    break;
            }
            break;
        case 'e2gerror':
            show_tds(array("","Date", "User", "IP", "Url", "Reason", "Group"));
            // Define log file
                        $log = '/var/log/e2guardian/denied.log';
                        // Fetch lines
                        $logarr = fetch_log($log);
            foreach ($logarr as $logent) {
                //split log
                $logline = preg_split("/;;/", $logent);
                // Word wrap the URL
                $url = htmlentities($logline[3]);
                                $logline[3] = preg_replace("@\<\>@","",$logline[3]);
                                $url = html_autowrap($url);
                                $logline[5] = html_autowrap($logline[5]);
                                echo "<tr valign='top'>\n";
                                echo "<td><i class='fa fa-times text-danger'></i></td>\n";
                                echo "<td class='listlr' nowrap='nowrap'>" . e2gm($logline[0]) . "</td>\n";
                                echo "<td $listr>" . e2gm($logline[1]) . "</td>\n";
                                echo "<td $listr>" . e2gm($logline[2]) . "</td>\n";
                                echo "<td $listr title='{$logline[3]}' width='*'>" . e2gm(preg_replace("/(\?|;).*/","",$url)) . "</td>\n";
                                if ($_REQUEST['error'] == 'reason') {
                                    echo "<td $listr>" . e2gm($logline[4]) . "</td>\n";
                                } else {
                                   echo "<td $listr>" . e2gm($logline[5]) . "</td>\n";
                                }
                echo "<td $listr>" . e2gm($logline[6]) . "</td>\n";
                                echo "</tr>\n";
                        }
            break;
        }
}

/* Functions */
function html_autowrap($cont) {
    // split strings
    $p = 0;
    $pstep = 25;
    $str = $cont;
    $cont = '';
    for ($p = 0; $p < strlen($str); $p += $pstep) {
        $s = substr($str, $p, $pstep);
        if (!$s) {
            break;
        }
        $cont .= $s . "<wbr />";
    }
    return $cont;
}

// Show Squid Logs
function fetch_log($log) {
    global $filter, $program, $e2glog, $traffic;
    $log = escapeshellarg($log);
    // Get data from form post
    $lines = is_numeric($_POST['maxlines'] ?? null) ? (int)$_POST['maxlines'] : 50;
    $lines = max(1, min(500, $lines));
    $readlines = is_numeric($_POST['readlines'] ?? null) ? (int)$_POST['readlines'] : 2000;
    $readlines = max($lines, min(20000, $readlines));
    $scanlines = ($program === 'access' && ($traffic !== 'all' || $filter !== '')) ? $readlines : $lines;

    if ($program === 'access' && (int)($e2glog['logfileformat'] ?? 1) === 1) {
        return fetch_access_and_live_logs($scanlines, $filter);
    }
    if (preg_match("/!/", htmlspecialchars($_POST['strfilter']))) {
        $grep_arg = "-iv";
    } else {
        $grep_arg = "-i";
    }

    // Check program to execute or no the parser
    if ($program == "access" && $e2glog['logfileformat'] == 3) {
               $parser = "| " . E2GUARDIAN_BINDIR . "/php-cgi -q e2guardian_log_parser.php";
    } else {
        $parser = "";
    }
    //arrumar aqui
    // Get logs based in filter expression
    if ($filter != "" && $program == "access") {
        exec("/usr/bin/tail -n {$readlines} {$log} | /usr/bin/grep {$grep_arg} " . escapeshellarg($filter). " | /usr/bin/tail -r -n {$scanlines} {$parser} ", ${$logarr.$program});
    } else {
        exec("/usr/bin/tail -r -n {$scanlines} {$log} {$parser}", ${$logarr.$program});
    }
    // Return logs
    return ${$logarr.$program};
};

/*
 * Trusted TLS tunnels are accounted in access.log only after they close.  The
 * engine writes a lightweight opening event to request.log so Real Time can
 * show the SNI immediately.  A completed access record supersedes its opening
 * event; reports continue to consume access.log exclusively.
 */
function fetch_access_and_live_logs($scanlines, $filter) {
    $access_log = '/var/log/e2guardian/access.log';
    $live_log = '/var/log/e2guardian/request.log';
    $marker = "\t__KONTROL_ID_TUNNEL_OPEN__";
    $limit = max(1, min(20000, (int)$scanlines));
    $access_lines = array();
    $live_lines = array();

    if (is_file($access_log)) {
        exec('/usr/bin/tail -r -n ' . $limit . ' ' . escapeshellarg($access_log), $access_lines);
    }
    if (is_file($live_log)) {
        exec('/usr/bin/tail -r -n ' . $limit . ' ' . escapeshellarg($live_log), $live_lines);
    }

    $completed = array();
    $entries = array();
    foreach ($access_lines as $line) {
        $identity = kontrol_id_log_identity($line);
        if ($identity !== false) {
            $completed[$identity['key']] = max($completed[$identity['key']] ?? '', $identity['time']);
            $entries[] = array('line' => $line, 'time' => $identity['time'], 'live' => false);
        }
    }

    foreach ($live_lines as $line) {
        if (strpos($line, 'THTTPS_OPEN') === false) {
            continue;
        }
        $identity = kontrol_id_log_identity($line);
        if ($identity === false) {
            continue;
        }
        if (isset($completed[$identity['key']]) && $completed[$identity['key']] >= $identity['time']) {
            continue;
        }
        $entries[] = array('line' => $line . $marker, 'time' => $identity['time'], 'live' => true);
    }

    if ($filter !== '') {
        $entries = array_values(array_filter($entries, function($entry) use ($filter) {
            return stripos($entry['line'], $filter) !== false;
        }));
    }

    usort($entries, function($left, $right) {
        $time_cmp = strcmp($right['time'], $left['time']);
        if ($time_cmp !== 0) {
            return $time_cmp;
        }
        return ((int)$left['live']) <=> ((int)$right['live']);
    });

    $entries = array_slice($entries, 0, $limit);
    return array_map(function($entry) { return $entry['line']; }, $entries);
}

function kontrol_id_log_identity($line) {
    if (!preg_match('/^(\d{4}\.\d{2}\.\d{2}\s+\d{2}:\d{2}:\d{2})\s+(\S+)\s+(\S+)\s+(\S+)/', $line, $match)) {
        return false;
    }
    return array(
        'time' => $match[1],
        'key' => $match[2] . "\n" . $match[3] . "\n" . $match[4],
    );
}

function show_tds($tds) {
    echo "<tr valign='top'>\n";
    foreach ($tds as $td){
        echo "<th class='listhdrr'>" . gettext($td) . "</th>\n";
    }
    echo "</tr>\n";
}

?>
