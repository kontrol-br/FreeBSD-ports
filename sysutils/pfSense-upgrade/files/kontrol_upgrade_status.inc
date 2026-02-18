<?php
/*
 * Minimal GUI helper library for staged Kontrol-upgrade status.
 */

function kontrol_upgrade_debug_file() {
    return '/cf/conf/upgrade_log.debug.log';
}

function kontrol_upgrade_debug($message) {
    if (getenv('DEBUG_ENABLED') === '0') {
        return;
    }

    $line = sprintf(
        "%s [DEBUG][php.kontrol_upgrade_status] pid=%d %s\n",
        gmdate('c'),
        getmypid(),
        $message
    );

    @file_put_contents(kontrol_upgrade_debug_file(), $line, FILE_APPEND);
}

function kontrol_upgrade_state_file() {
    return '/var/db/Kontrol/upgrade/state';
}

function kontrol_upgrade_journal_file() {
    return '/var/db/Kontrol/upgrade/journal.log';
}

function kontrol_upgrade_read_state() {
    $state = [];
    $file = kontrol_upgrade_state_file();
    kontrol_upgrade_debug("read_state file={$file}");

    if (!file_exists($file)) {
        kontrol_upgrade_debug('state file does not exist');
        return $state;
    }

    $lines = @file($file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    if (!is_array($lines)) {
        kontrol_upgrade_debug('state file read failed');
        return $state;
    }

    kontrol_upgrade_debug('state lines=' . count($lines));
    foreach ($lines as $line) {
        $parts = explode('=', $line, 2);
        if (count($parts) !== 2) {
            kontrol_upgrade_debug('ignoring malformed line: ' . $line);
            continue;
        }
        $state[trim($parts[0])] = trim($parts[1]);
    }

    kontrol_upgrade_debug('state keys=' . implode(',', array_keys($state)));
    return $state;
}

function kontrol_upgrade_progress_payload() {
    $state = kontrol_upgrade_read_state();
    $payload = [
        'txid' => $state['txid'] ?? null,
        'stage' => $state['stage'] ?? 'idle',
        'current_abi' => $state['current_abi'] ?? null,
        'target_abi' => $state['target_abi'] ?? null,
        'cross_major' => ($state['cross_major'] ?? 'no') === 'yes',
        'finished_at' => $state['finished_at'] ?? null,
    ];

    kontrol_upgrade_debug('progress payload stage=' . $payload['stage'] . ' txid=' . ($payload['txid'] ?? 'none'));
    return $payload;
}

?>
