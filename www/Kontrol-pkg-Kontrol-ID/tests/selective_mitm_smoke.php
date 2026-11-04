#!/usr/local/bin/php
<?php

if (PHP_SAPI !== 'cli' || $argc !== 2 || !is_file($argv[1])) {
    fwrite(STDERR, "usage: selective_mitm_smoke.php /path/to/e2guardian.inc\n");
    exit(64);
}

require_once($argv[1]);

$cases = array(
    '' => 'off',
    'automitm' => 'selective',
    'sslmitm' => 'full',
    'automitm,sslmitm' => 'full'
);

foreach ($cases as $options => $expected) {
    $actual = e2guardian_group_https_inspection_mode(array('group_options' => $options));
    if ($actual !== $expected) {
        fwrite(STDERR, "group_options={$options}: expected {$expected}, got {$actual}\n");
        exit(1);
    }
}

echo "Selective MITM mode mapping OK\n";
