<?php
/*
 * kontrol_id.widget.php
 *
 * Dashboard health, queue monitoring and DSTAT history for the
 * Kontrol-ID secure web gateway.
 */

require_once('guiconfig.inc');
require_once('functions.inc');
require_once('service-utils.inc');

if (!function_exists('kontrol_id_widget_history_stats')) {
	function kontrol_id_widget_history_stats($path, $limit = 10) {
		$history = array();
		if (!is_readable($path)) {
			return $history;
		}

		$lines = array();
		exec('/usr/bin/tail -n 150 ' . escapeshellarg($path), $lines);
		for ($index = count($lines) - 1; $index >= 0; $index--) {
			$line = trim((string)$lines[$index]);
			if ($line === '' || stripos($line, 'httpw') !== false) {
				continue;
			}
			$fields = preg_split('/\s+/', $line);
			if (count($fields) < 11 || !is_numeric($fields[0]) || !is_numeric($fields[3])) {
				continue;
			}
			$history[] = array(
				'timestamp' => (int)$fields[0],
				'busy' => (float)$fields[2],
				'http_queue' => (float)$fields[3],
				'log_queue' => (float)$fields[4],
				'connections' => (float)$fields[5],
				'connections_rate' => (float)$fields[6],
				'requests' => (float)$fields[7],
				'requests_rate' => (float)$fields[8],
				'max_fds' => (float)$fields[9],
				'list_cache' => (float)$fields[10]
			);
			if (count($history) >= $limit) {
				break;
			}
		}
		return $history;
	}
}

if (!function_exists('kontrol_id_widget_number')) {
	function kontrol_id_widget_number($value, $decimals = 0) {
		return number_format((float)$value, $decimals, ',', '.');
	}
}

$requested_widgetkey = $_POST['widgetkey'] ?? $_GET['widgetkey'] ?? null;
if ($requested_widgetkey !== null) {
	if (!is_valid_widgetkey($requested_widgetkey, $user_settings, __FILE__)) {
		print gettext('Invalid Widget Key');
		exit;
	}
	$widgetkey = $requested_widgetkey;
}

$service = array('name' => 'kontrol_id');
$service_running = get_service_status($service);
$service_enabled = config_get_path('installedpackages/kontrolid/config/0/enable') === 'on';
$history = kontrol_id_widget_history_stats('/var/log/kontrol-id/kontrol-id-stats.log', 10);
$stats = !empty($history) ? $history[0] : array();

$stats_fresh = !empty($stats['timestamp']) && (time() - $stats['timestamp']) <= 420;
$status_class = $service_running ? 'success' : ($service_enabled ? 'danger' : 'warning');
$status_icon = $service_running ? 'fa-check-circle' : ($service_enabled ? 'fa-times-circle' : 'fa-pause-circle');
$status_text = $service_running ? gettext('Running') : ($service_enabled ? gettext('Stopped') : gettext('Disabled'));
$sample_text = !empty($stats['timestamp']) ? sprintf(gettext('DSTAT: %s (%s)'), date('H:i:s', $stats['timestamp']), $stats_fresh ? gettext('Online') : gettext('Stale')) : gettext('Waiting for DSTAT statistics');

$http_queue = (int)($stats['http_queue'] ?? 0);
$log_queue = (int)($stats['log_queue'] ?? 0);
$http_queue_alert = $http_queue > 10 ? 'kontrol-id-widget__queue--danger' : ($http_queue > 0 ? 'kontrol-id-widget__queue--warning' : '');
$log_queue_alert = $log_queue > 20 ? 'kontrol-id-widget__queue--danger' : ($log_queue > 0 ? 'kontrol-id-widget__queue--warning' : '');
?>
<style>
.kontrol-id-widget { color: inherit; }
.kontrol-id-widget__status { display:flex; align-items:center; justify-content:space-between; gap:8px; padding:1px 1px 8px; border-bottom:1px solid rgba(127,127,127,.22); }
.kontrol-id-widget__service { display:flex; align-items:center; gap:7px; min-width:0; }
.kontrol-id-widget__service i { font-size:1.15rem; }
.kontrol-id-widget__service strong { display:block; font-size:13px; line-height:1.15; }
.kontrol-id-widget__meta { color:#6b7785; font-size:10px; line-height:1.25; margin-top:2px; }
.kontrol-id-widget__mode { white-space:nowrap; font-size:10px; font-weight:600; letter-spacing:.02em; text-transform:uppercase; }
.kontrol-id-widget__metrics { display:grid; grid-template-columns:repeat(4,minmax(0,1fr)); gap:0; margin:0; }
.kontrol-id-widget__metric { padding:8px 5px 6px; border-right:1px solid rgba(127,127,127,.18); min-width:0; }
.kontrol-id-widget__metric:last-child { border-right:0; }
.kontrol-id-widget__value { display:block; font-size:17px; font-weight:700; line-height:1.05; overflow:hidden; text-overflow:ellipsis; }
.kontrol-id-widget__label { display:block; margin-top:3px; color:#6b7785; font-size:9px; font-weight:600; letter-spacing:.035em; text-transform:uppercase; }
.kontrol-id-widget__queues { display:flex; flex-wrap:wrap; justify-content:space-between; align-items:center; gap:8px; margin-top:3px; padding:8px 2px 7px; border-top:1px solid rgba(127,127,127,.16); color:#5d6875; font-size:11.5px; }
.kontrol-id-widget__queue-item { display:inline-flex; align-items:center; gap:4px; white-space:nowrap; padding:1px 4px; border-radius:3px; }
.kontrol-id-widget__queue-item strong { font-size:12.5px; font-weight:700; color:inherit; }
.kontrol-id-widget__queue-item i { font-size:11px; opacity:.85; }
.kontrol-id-widget__queue--warning { color:#d97706 !important; font-weight:700; }
.kontrol-id-widget__queue--danger { color:#dc2626 !important; font-weight:700; }

.kontrol-id-widget__history-wrap { margin-top:4px; border-top:1px solid rgba(127,127,127,.18); padding-top:6px; }
.kontrol-id-widget__history-head { display:flex; justify-content:space-between; align-items:center; margin-bottom:5px; padding:0 2px; font-size:10px; font-weight:600; color:#6b7785; }
.kontrol-id-widget__history-head span:first-child { display:inline-flex; align-items:center; gap:5px; text-transform:uppercase; letter-spacing:.03em; }
.kontrol-id-widget__history-head .badge-samples { background:rgba(127,127,127,.15); color:inherit; padding:1px 6px; border-radius:10px; font-size:9px; font-weight:500; }

.kontrol-id-widget__table-responsive { width:100%; overflow-x:auto; }
.kontrol-id-widget__table { width:100%; border-collapse:collapse; font-size:10.5px; table-layout:fixed; }
.kontrol-id-widget__table th { padding:5px 6px; font-size:9.5px; font-weight:600; text-transform:uppercase; letter-spacing:.03em; color:#6b7785; border-bottom:1px solid rgba(127,127,127,.25); background:rgba(127,127,127,.04); }
.kontrol-id-widget__table td { padding:4.5px 6px; border-bottom:1px solid rgba(127,127,127,.12); vertical-align:middle; white-space:nowrap; }
.kontrol-id-widget__table tbody tr:last-child td { border-bottom:0; }
.kontrol-id-widget__table tbody tr:hover { background:rgba(127,127,127,.05); }

.kontrol-id-widget__table .col-time { width:22%; font-family:monospace; font-size:10px; }
.kontrol-id-widget__table .col-num { width:15.6%; text-align:right; font-variant-numeric:tabular-nums; }
.kontrol-id-widget__table .num-warn { color:#d97706; font-weight:700; }
.kontrol-id-widget__table .num-alert { color:#dc2626; font-weight:700; }

.kontrol-id-widget__empty { padding:14px 6px 10px; text-align:center; color:#6b7785; font-size:12px; }
.kontrol-id-widget__empty i { display:block; margin-bottom:6px; font-size:18px; opacity:.72; }
@media (max-width:520px) {
	.kontrol-id-widget__metrics { grid-template-columns:repeat(2,minmax(0,1fr)); }
	.kontrol-id-widget__metric:nth-child(2) { border-right:0; }
	.kontrol-id-widget__metric:nth-child(-n+2) { border-bottom:1px solid rgba(127,127,127,.18); }
	.kontrol-id-widget__queues { flex-wrap:wrap; gap:6px; }
}
</style>
<div id="kontrol-id-widget-<?=htmlspecialchars($widgetkey ?? '0')?>" class="kontrol-id-widget">
	<div class="kontrol-id-widget__status">
		<div class="kontrol-id-widget__service">
			<i class="fa-solid <?=$status_icon?> text-<?=$status_class?>" aria-hidden="true"></i>
			<div>
				<strong><?=gettext('Secure Web Gateway')?></strong>
				<div class="kontrol-id-widget__meta"><?=htmlspecialchars($sample_text)?></div>
			</div>
		</div>
		<span class="kontrol-id-widget__mode text-<?=$status_class?>"><?=htmlspecialchars($status_text)?></span>
	</div>

	<?php if (!empty($stats)): ?>
	<div class="kontrol-id-widget__metrics">
		<div class="kontrol-id-widget__metric">
			<span class="kontrol-id-widget__value"><?=kontrol_id_widget_number($stats['busy'])?></span>
			<span class="kontrol-id-widget__label"><?=gettext('Busy workers')?></span>
		</div>
		<div class="kontrol-id-widget__metric">
			<span class="kontrol-id-widget__value"><?=kontrol_id_widget_number($stats['connections'])?></span>
			<span class="kontrol-id-widget__label"><?=gettext('Connections')?></span>
		</div>
		<div class="kontrol-id-widget__metric">
			<span class="kontrol-id-widget__value"><?=kontrol_id_widget_number($stats['requests'])?></span>
			<span class="kontrol-id-widget__label"><?=gettext('Requests')?></span>
		</div>
		<div class="kontrol-id-widget__metric">
			<span class="kontrol-id-widget__value"><?=kontrol_id_widget_number($stats['requests_rate'], 1)?></span>
			<span class="kontrol-id-widget__label"><?=gettext('Requests / s')?></span>
		</div>
	</div>
	<div class="kontrol-id-widget__queues">
		<span class="kontrol-id-widget__queue-item <?=$http_queue_alert?>"><i class="fa-solid fa-globe" aria-hidden="true"></i> <?=gettext('HTTP queue')?>: <strong><?=kontrol_id_widget_number($stats['http_queue'])?></strong></span>
		<span class="kontrol-id-widget__queue-item <?=$log_queue_alert?>"><i class="fa-solid fa-list" aria-hidden="true"></i> <?=gettext('Log queue')?>: <strong><?=kontrol_id_widget_number($stats['log_queue'])?></strong></span>
		<span class="kontrol-id-widget__queue-item"><i class="fa-solid fa-bolt" aria-hidden="true"></i> <?=gettext('Connections / s')?>: <strong><?=kontrol_id_widget_number($stats['connections_rate'], 1)?></strong></span>
	</div>

	<?php if (count($history) > 1): ?>
	<div class="kontrol-id-widget__history-wrap">
		<div class="kontrol-id-widget__history-head">
			<span><i class="fa-solid fa-clock-rotate-left"></i> <?=gettext('Histórico DSTAT (últimos 50 min)')?></span>
			<span class="badge-samples"><?=count($history)?> amostras</span>
		</div>
		<div class="kontrol-id-widget__table-responsive">
			<table class="kontrol-id-widget__table">
				<thead>
					<tr>
						<th class="col-time"><?=gettext('Hora')?></th>
						<th class="col-num"><?=gettext('Busy')?></th>
						<th class="col-num"><?=gettext('HTTP Q')?></th>
						<th class="col-num"><?=gettext('Log Q')?></th>
						<th class="col-num"><?=gettext('Conx/s')?></th>
						<th class="col-num"><?=gettext('Reqs/s')?></th>
					</tr>
				</thead>
				<tbody>
					<?php foreach ($history as $h): ?>
					<?php
						$hq = (int)$h['http_queue'];
						$lq = (int)$h['log_queue'];
						$hq_style = $hq > 10 ? 'num-alert' : ($hq > 0 ? 'num-warn' : '');
						$lq_style = $lq > 20 ? 'num-alert' : ($lq > 0 ? 'num-warn' : '');
					?>
					<tr>
						<td class="col-time"><?=date('H:i:s', $h['timestamp'])?></td>
						<td class="col-num"><?=kontrol_id_widget_number($h['busy'])?></td>
						<td class="col-num <?=$hq_style?>"><?=kontrol_id_widget_number($h['http_queue'])?></td>
						<td class="col-num <?=$lq_style?>"><?=kontrol_id_widget_number($h['log_queue'])?></td>
						<td class="col-num"><?=kontrol_id_widget_number($h['connections_rate'], 1)?></td>
						<td class="col-num"><?=kontrol_id_widget_number($h['requests_rate'], 1)?></td>
					</tr>
					<?php endforeach; ?>
				</tbody>
			</table>
		</div>
	</div>
	<?php endif; ?>

	<?php else: ?>
	<div class="kontrol-id-widget__empty">
		<i class="fa-solid fa-chart-line" aria-hidden="true"></i>
		<?=gettext('Statistics will appear after the Kontrol-ID engine starts processing traffic.')?>
	</div>
	<?php endif; ?>
</div>
<script type="text/javascript">
//<![CDATA[
(function() {
	var widgetId = <?=json_encode($widgetkey ?? '0')?>;
	var container = '#kontrol-id-widget-' + widgetId.replace(/[^A-Za-z0-9_-]/g, '');
	var refresh = function() {
		if (document.hidden || !jQuery(container).length) {
			return;
		}
		jQuery.ajax({
			url: '/widgets/widgets/kontrol_id.widget.php',
			type: 'post',
			data: { widgetkey: widgetId },
			timeout: 5000,
			success: function(markup) {
				var replacement = jQuery(markup).filter(container);
				if (replacement.length) {
					jQuery(container).replaceWith(replacement);
				}
			}
		});
	};
	window.setInterval(refresh, 15000);
})();
//]]>
</script>
