<?php
/*
 * Minimal GUI helper library for staged Kontrol-upgrade status.
 */

function kontrol_upgrade_state_file() {
    return '/var/db/Kontrol/upgrade/state';
}

function kontrol_upgrade_journal_file() {
    return '/var/db/Kontrol/upgrade/journal.log';
}

function kontrol_upgrade_read_state() {
    $state = [];
    $file = kontrol_upgrade_state_file();
    if (!file_exists($file)) {
        return $state;
    }

    $lines = @file($file, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
    if (!is_array($lines)) {
        return $state;
    }

    foreach ($lines as $line) {
        $parts = explode('=', $line, 2);
        if (count($parts) !== 2) {
            continue;
        }
        $state[trim($parts[0])] = trim($parts[1]);
    }

    return $state;
}

function kontrol_upgrade_progress_payload() {
    $state = kontrol_upgrade_read_state();
    return [
        'txid' => $state['txid'] ?? null,
        'stage' => $state['stage'] ?? 'idle',
        'current_abi' => $state['current_abi'] ?? null,
        'target_abi' => $state['target_abi'] ?? null,
        'cross_major' => ($state['cross_major'] ?? 'no') === 'yes',
        'finished_at' => $state['finished_at'] ?? null,
    ];
}

?>
