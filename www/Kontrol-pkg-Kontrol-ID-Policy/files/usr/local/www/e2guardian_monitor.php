<?php
/*
 * e2guardian_monitor.php
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

require_once("/etc/inc/util.inc");
require_once("/etc/inc/functions.inc");
require_once("/etc/inc/pkg-utils.inc");
require_once("/etc/inc/globals.inc");
require_once("guiconfig.inc");
require_once("/usr/local/pkg/kontrol_id.inc");

$pgtitle = array(gettext("Services"), gettext("Kontrol-ID"), gettext("Real Time"));
$shortcut_section = "e2guardian";
include("head.inc");

if ($savemsg) {
    print_info_box($savemsg);
}

display_top_tabs(kontrol_id_tabs('realtime'));

if (is_array($config['installedpackages']['e2guardianlog'])) {
        $e2glog = $config['installedpackages']['e2guardianlog']['config'][0];
} else {
        $e2glog = array();
}

?>

<style type="text/css">
.kid-filter-panel,
.kid-log-panel {
    border: 1px solid rgba(39, 59, 74, .16);
    border-radius: 4px;
    box-shadow: 0 3px 12px rgba(25, 43, 56, .06);
    overflow: hidden;
}

.kid-filter-panel > .panel-heading,
.kid-log-panel > .panel-heading {
    background: #2d3e4b;
    border: 0;
    border-bottom: 3px solid #2787bd;
    color: #fff;
    min-height: 42px;
    padding: 10px 14px;
}

.kid-filter-panel > .panel-heading .panel-title,
.kid-log-panel > .panel-heading .panel-title {
    color: inherit;
    font-size: 13px;
    font-weight: 600;
    letter-spacing: .035em;
    line-height: 19px;
}

.kid-live-state {
    align-items: center;
    background: rgba(255, 255, 255, .1);
    border: 1px solid rgba(255, 255, 255, .18);
    border-radius: 12px;
    display: inline-flex;
    float: right;
    font-size: 10px;
    font-weight: 600;
    gap: 6px;
    letter-spacing: .08em;
    line-height: 18px;
    padding: 0 9px;
    text-transform: uppercase;
}

.kid-live-state .fa-circle {
    color: #63d89a;
    font-size: 7px;
}

.kid-log-panel.kid-is-loading .kid-live-state .fa-circle {
    color: #71c9f3;
}

.kid-log-panel.kid-has-error .kid-live-state .fa-circle {
    color: #ff847c;
}

.kid-log-panel > .panel-body {
    padding: 0;
}

.kid-log-scroll {
    max-height: calc(100vh - 260px);
    min-height: 260px;
    overflow: auto;
    position: relative;
}

.kid-log-table {
    border-collapse: separate;
    border-spacing: 0;
    margin: 0;
    width: 100%;
}

.kid-log-table > tbody > tr:first-child > th {
    background: #f2f6f8;
    border-bottom: 1px solid rgba(34, 62, 81, .24);
    box-shadow: inset 0 2px 0 rgba(39, 135, 189, .8);
    color: #304552;
    font-family: inherit;
    font-size: 10px;
    font-weight: 700;
    letter-spacing: .07em;
    padding: 11px 10px 9px;
    position: sticky;
    text-transform: uppercase;
    top: 0;
    white-space: nowrap;
    z-index: 2;
}

.kid-log-table > tbody > tr > td {
    background: rgba(255, 255, 255, .94);
    border-bottom: 1px solid rgba(43, 66, 82, .11);
    color: #314653;
    font-size: 11px;
    line-height: 1.4;
    padding: 8px 10px;
    vertical-align: top;
}

.kid-log-table > tbody > tr:nth-child(odd):not(:first-child) > td {
    background: rgba(232, 241, 246, .3);
}

.kid-log-table > tbody > tr:not(:first-child):hover > td {
    background: rgba(52, 146, 198, .09);
}

.kid-log-table > tbody > tr > th + th,
.kid-log-table > tbody > tr > td + td {
    border-left: 1px solid rgba(43, 66, 82, .13);
}

.kid-log-table > tbody > tr > td:first-child {
    padding-left: 14px;
    text-align: center;
    width: 38px;
}

.kid-log-table .fa {
    font-size: 13px;
}

.kid-log-table .listr,
.kid-log-table .listlr {
    font-family: Consolas, "Liberation Mono", monospace !important;
}

@media (max-width: 767px) {
    .kid-log-scroll {
        max-height: 62vh;
    }

    .kid-log-table > tbody > tr > td,
    .kid-log-table > tbody > tr:first-child > th {
        padding-left: 8px;
        padding-right: 8px;
    }
}
</style>

<div class="panel panel-default kid-filter-panel">
    <div class="panel-heading"><h2 class="panel-title"><?=gettext("Filtering"); ?></h2></div>
    <div class="panel-body">
        <div class="table">
            <form id="paramsForm" name="paramsForm" method="post" action="">
            <table class="table table-striped table-hover table-condensed">
                <tbody>
                <tr>
                    <td width="22%" valign="top" class="vncellreq">Max lines:</td>
                    <td width="78%" class="vtable">
                        <select name="maxlines" id="maxlines">
                            <option value="5">5 lines</option>
                            <option value="10">10 lines</option>
                            <option value="15">15 lines</option>
                            <option value="20" selected="selected">20 lines</option>
                            <option value="25">25 lines</option>
                            <option value="100">100 lines</option>
                            <option value="200">200 lines</option>
                        </select>
                        <br/>
                        <span class="vexpl">
                            <?=gettext("Max. lines to be displayed.")?>
                        </span>
                    </td>
                </tr>
                <tr>
                    <td width="22%" valign="top" class="vncellreq">Update Interval:</td>
                    <td width="78%" class="vtable">
                        <select name="interval" id="interval">
                            <option value="1000">1 second</option>
                            <option value="3000" selected="selected">3 seconds</option>
                            <option value="5000">5 seconds</option>
                            <option value="10000">10 seconds</option>
                            <option value="15000">15 seconds</option>
                            <option value="20000">20 seconds</option>
                        </select>
                        <br/>
                        <span class="vexpl">
                            <?=gettext("How often log entries are updated.")?>
                        </span>
                    </td>
                </tr>
                <?php if ((int)($e2glog['logfileformat'] ?? 1) === 1) {?>
                <tr>
                    <td width="22%" valign="top" class="vncellreq"><?=gettext("Traffic to show:")?></td>
                    <td width="78%" class="vtable">
                        <select name="traffic" id="traffic">
                            <option value="all" selected="selected"><?=gettext("All connections")?></option>
                            <option value="allowed"><?=gettext("Allowed connections")?></option>
                            <option value="blocked"><?=gettext("Blocked connections")?></option>
                        </select>
                        <br/>
                        <span class="vexpl">
                            <?=gettext("Allowed connections are marked in green and blocked connections in red. Filtered views search the configured log scan window.")?>
                        </span>
                    </td>
                </tr>
                <?php }?>
                <?php if($e2glog['logfileformat'] == 1 || $e2glog['logfileformat'] == 4 || $e2glog['logdeniedcgi'] == "on") {?>
                <tr>
                <td width="22%" valign="top" class="vncellreq">Erro to show:</td>
                                        <td width="78%" class="vtable">
                                                <select name="error" id="error">
                                                        <option value="reason">Reason</option>
                                                        <option value="detailed" selected="selected">Detailed info</option>
                                                </select>
                                                <br/>
                                                <span class="vexpl">
                                                        <?=gettext("Select denied info to show while using logs in E2g format.")?>
                                                </span>
                                        </td>
                </tr>
                <?php }?>
                <tr>
                    <td width="22%" valign="top" class="vncellreq">String filter:</td>
                    <td width="78%" class="vtable">
                        <input name="strfilter" type="text" class="formfld search" id="strfilter" size="50" value="" />
                        <br/>
                        <span class="vexpl">
                            <?=gettext("Enter a grep-like string/pattern to filter the log entries.")?><br/>
                            <?=gettext("E.g.: username, IP address, URL.")?><br/>
                            <?=gettext("Use <strong>!</strong> to invert the sense of matching (to select non-matching lines).")?>
                        </span>
                    </td>
                </tr>
                <tr>
                    <td width="22%" valign="top" class="vncellreq">String filter limit:</td>
                    <td width="78%" class="vtable">
                        <input name="readlines" type="text" class="formfld search" id="readlines" size="50" value="" />
                        <br/>
                        <span class="vexpl">
                            <?=gettext("Enter the number of log entries that should be checked to match the string filter")?><br/>
                        </span>
                    </td>
                </tr>
                </tbody>
            </table>
            </form>
        </div>
    </div>
</div>

<div class="panel panel-default kid-log-panel" id="kid-access-panel">
    <div class="panel-heading">
        <span class="kid-live-state"><i class="fa fa-circle"></i><span class="kid-live-label"><?=gettext("Live")?></span></span>
        <h2 class="panel-title"><?=gettext("Kontrol-ID Access Table"); ?></h2>
    </div>
    <div class="panel-body">
        <div class="kid-log-scroll">
            <table class="table kid-log-table" aria-label="<?=gettext("Kontrol-ID Access Table")?>">
                <tbody id="e2gView"><tr><td></td></tr></tbody>
            </table>
        </div>
    </div>
</div>

    <?php if($e2glog['logfileformat'] == 3 && $e2glog['logdeniedcgi'] == "on") {?>
        <div class="panel panel-default kid-log-panel" id="kid-error-panel">
            <div class="panel-heading">
                <span class="kid-live-state"><i class="fa fa-circle"></i><span class="kid-live-label"><?=gettext("Live")?></span></span>
                <h2 class="panel-title"><?=gettext("Detailed denied log"); ?></h2>
            </div>
            <div class="panel-body">
                <div class="kid-log-scroll">
                    <table class="table kid-log-table" aria-label="<?=gettext("Detailed denied log")?>">
                        <tbody id="e2gerror"><tr><td></td></tr></tbody>
                    </table>
                </div>
            </div>
        </div>
    <?php } ?>

<!-- Function to call programs logs -->
<script type="text/javascript">
//<![CDATA[
document.timeoutVal = 5000;
var kontrolIdLogTimer = null;
var kontrolIdFilterTimer = null;
var kontrolIdLogRequests = [];

function setLogState(panel, state, label) {
    var $panel = $(panel);
    $panel.removeClass('kid-is-loading kid-has-error');
    if (state) {
        $panel.addClass(state);
    }
    $panel.find('.kid-live-label').text(label);
}

function showLog(content, url, program) {
    var panel = (content === 'e2gerror') ? '#kid-error-panel' : '#kid-access-panel';
    setLogState(panel, 'kid-is-loading', '<?=gettext("Updating")?>');

    return jQuery.ajax(url, {
        type: 'post',
        cache: false,
        data: {
            maxlines: $('#maxlines').val(),
            readlines: $('#readlines').val(),
            traffic: $('#traffic').val() || 'all',
            strfilter: $('#strfilter').val(),
            error: $('#error').val(),
            program: program,
            content: content
        },
        success: function(ret) {
            $('#' + content).html(ret);
            setLogState(panel, '', '<?=gettext("Live")?>');
        },
        error: function(request, status) {
            if (status !== 'abort') {
                setLogState(panel, 'kid-has-error', '<?=gettext("Unavailable")?>');
            }
        }
    });
}

function scheduleLogUpdate() {
    window.clearTimeout(kontrolIdLogTimer);
    document.timeoutVal = parseInt($('#interval').val(), 10) || 5000;
    kontrolIdLogTimer = window.setTimeout(updateAllLogs, document.timeoutVal);
}

function updateAllLogs() {
    window.clearTimeout(kontrolIdLogTimer);

    if (document.hidden) {
        scheduleLogUpdate();
        return;
    }

    kontrolIdLogRequests = [showLog('e2gView', 'e2guardian_monitor_data.php', 'access')];
    if ($('#e2gerror').length) {
        kontrolIdLogRequests.push(showLog('e2gerror', 'e2guardian_monitor_data.php', 'e2gerror'));
    }

    jQuery.when.apply(jQuery, kontrolIdLogRequests).always(scheduleLogUpdate);
}

events.push(function() {
    function refreshLogView() {
        window.clearTimeout(kontrolIdLogTimer);
        jQuery.each(kontrolIdLogRequests, function(index, request) {
            if (request && request.readyState !== 4) {
                request.abort();
            }
        });
        updateAllLogs();
    }

    $('#paramsForm').on('change', 'select', refreshLogView);
    $('#paramsForm').on('input', 'input', function() {
        window.clearTimeout(kontrolIdFilterTimer);
        kontrolIdFilterTimer = window.setTimeout(refreshLogView, 300);
    });

    $(document).on('visibilitychange', function() {
        if (!document.hidden) {
            updateAllLogs();
        }
    });

    updateAllLogs();
});
//]]>
</script>
<?php include("foot.inc"); ?>
