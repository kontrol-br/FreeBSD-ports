#!/usr/bin/env php
<?php
if (!function_exists('gettext')) {
        function gettext($message) {
                return $message;
        }
}
function fail($message) {
        fwrite(STDERR, "FAIL: {$message}\n");
        exit(1);
}
function assert_true($condition, $message) {
        if (!$condition) {
                fail($message);
        }
}
function e2g_delTree($dir) {
        if (!is_dir($dir)) {
                return false;
        }
        foreach (array_diff(scandir($dir), array('.', '..')) as $entry) {
                $path = $dir . '/' . $entry;
                is_dir($path) && !is_link($path) ? e2g_delTree($path) : unlink($path);
        }
        return rmdir($dir);
}
function file_notice($package, $error, $message, $url) {
        $GLOBALS['notices'][] = $message;
}

$source = file_get_contents(__DIR__ . '/../files/usr/local/www/e2guardian.php');
$start = strpos($source, 'function e2g_blacklist_notice(');
$end = strpos($source, 'function read_lists(');
assert_true($start !== false && $end !== false, 'could not load blacklist implementation');
eval(substr($source, $start, $end - $start));

function make_tree($base) {
        mkdir($base . '/lists/authplugins', 0777, true);
        mkdir($base . '/lists/blacklists/old-category', 0777, true);
        foreach (array(
                'authplugins/ipgroups',
                'authplugins/ipgroups.sample',
                'authexceptionurllist',
                'authexceptionurllist.sample',
                'bannedurllist.g_Default',
                'another-package-managed-list.sample',
                'blacklists/old-category/domains'
        ) as $file) {
                file_put_contents($base . '/lists/' . $file, "fixture\n");
        }
}
function create_archive($base, $name, $entries) {
        $archive_root = $base . '/archive-' . $name;
        mkdir($archive_root, 0777, true);
        foreach ($entries as $file) {
                $path = $archive_root . '/' . $file;
                if (!is_dir(dirname($path))) {
                        mkdir(dirname($path), 0777, true);
                }
                file_put_contents($path, "fixture\n");
        }
        $archive = $base . '/' . $name . '.tgz';
        exec('/usr/bin/tar -czf ' . escapeshellarg($archive) . ' -C ' . escapeshellarg($archive_root) . ' .', $output, $return);
        assert_true($return === 0, "could not create {$name} archive");
        return $archive;
}
function assert_preserved($lists) {
        foreach (array(
                'authplugins/ipgroups',
                'authplugins/ipgroups.sample',
                'authexceptionurllist',
                'authexceptionurllist.sample',
                'bannedurllist.g_Default',
                'another-package-managed-list.sample'
        ) as $file) {
                assert_true(is_file($lists . '/' . $file), "managed list moved or removed: {$file}");
                assert_true(!file_exists($lists . '/blacklists/' . $file), "managed list incorrectly nested: {$file}");
        }
}
function update_with($base, $archive, $extra = array()) {
        return extract_black_list(false, null, $extra + array(
                'blacklist_file' => $archive,
                'lists_dir' => $base . '/lists',
                'skip_read_lists' => true
        ));
}

$root = sys_get_temp_dir() . '/e2guardian-smoke-' . getmypid();
mkdir($root, 0777, true);
try {
        foreach (array(
                'blacklists-root' => array('blacklists/new-category/domains', 'blacklists/new-category/urls'),
                'BL-root' => array('BL/new-category/domains', 'BL/new-category/urls'),
                'direct-categories' => array('new-category/domains', 'new-category/urls', 'second-category/domains')
        ) as $scenario => $entries) {
                $base = $root . '/' . $scenario;
                make_tree($base);
                assert_true(update_with($base, create_archive($base, $scenario, $entries)), "{$scenario} update failed");
                assert_preserved($base . '/lists');
                assert_true(is_file($base . '/lists/blacklists/BL/new-category/domains'), "{$scenario} domains missing");
                assert_true(is_file($base . '/lists/blacklists/BL/new-category/urls'), "{$scenario} urls missing");
        }

        $base = $root . '/corrupt';
        make_tree($base);
        $archive = $base . '/corrupt.tgz';
        file_put_contents($archive, "not a tarball\n");
        assert_true(!update_with($base, $archive), 'corrupt archive unexpectedly accepted');
        assert_true(is_file($base . '/lists/blacklists/old-category/domains'), 'corrupt archive replaced old blacklist');
        assert_preserved($base . '/lists');

        $base = $root . '/empty';
        make_tree($base);
        $archive = $base . '/empty.tgz';
        exec('/usr/bin/tar -czf ' . escapeshellarg($archive) . ' --files-from /dev/null', $output, $return);
        assert_true($return === 0, 'could not create empty archive');
        assert_true(!update_with($base, $archive), 'empty archive unexpectedly accepted');
        assert_true(is_file($base . '/lists/blacklists/old-category/domains'), 'empty archive replaced old blacklist');
        assert_preserved($base . '/lists');

        $base = $root . '/traversal';
        make_tree($base);
        mkdir($base . '/payload', 0777, true);
        file_put_contents($base . '/payload/file', "fixture\n");
        $archive = $base . '/traversal.tgz';
        exec('/usr/bin/tar -czf ' . escapeshellarg($archive) . ' -C ' . escapeshellarg($base . '/payload') . ' --transform=' . escapeshellarg('s|file|../outside|') . ' file', $output, $return);
        assert_true($return === 0, 'could not create traversal archive');
        assert_true(!update_with($base, $archive), 'traversal archive unexpectedly accepted');
        assert_true(!file_exists($base . '/outside'), 'traversal wrote outside extraction directory');
        assert_preserved($base . '/lists');

        $base = $root . '/absolute';
        make_tree($base);
        mkdir($base . '/payload', 0777, true);
        file_put_contents($base . '/payload/file', "fixture\n");
        $archive = $base . '/absolute.tgz';
        exec('/usr/bin/tar -czPf ' . escapeshellarg($archive) . ' -C ' . escapeshellarg($base . '/payload') . ' --transform=' . escapeshellarg('s|file|/tmp/e2guardian-absolute-smoke|') . ' file', $output, $return);
        assert_true($return === 0, 'could not create absolute-path archive');
        assert_true(!update_with($base, $archive), 'absolute-path archive unexpectedly accepted');
        assert_true(!file_exists('/tmp/e2guardian-absolute-smoke'), 'absolute path wrote outside extraction directory');
        assert_preserved($base . '/lists');

        $base = $root . '/symlink';
        make_tree($base);
        mkdir($base . '/payload/BL/new-category', 0777, true);
        file_put_contents($base . '/payload/BL/new-category/domains', "fixture\n");
        symlink('/tmp', $base . '/payload/BL/link');
        $archive = $base . '/symlink.tgz';
        exec('/usr/bin/tar -czf ' . escapeshellarg($archive) . ' -C ' . escapeshellarg($base . '/payload') . ' BL', $output, $return);
        assert_true($return === 0, 'could not create symlink archive');
        assert_true(update_with($base, $archive), 'symlink archive unexpectedly rejected');
        assert_true(is_file($base . '/lists/blacklists/BL/new-category/domains'), 'symlink archive did not install blacklist');
        assert_preserved($base . '/lists');

        $base = $root . '/interrupted-rollback';
        make_tree($base);
        rename($base . '/lists/blacklists', $base . '/lists/blacklists.old');
        $archive = create_archive($base, 'interrupted-rollback', array('BL/new-category/domains'));
        assert_true(update_with($base, $archive), 'update did not recover interrupted rollback');
        assert_true(is_file($base . '/lists/blacklists/BL/new-category/domains'), 'recovered update did not install new blacklist');
        assert_preserved($base . '/lists');

        $base = $root . '/rollback';
        make_tree($base);
        $archive = create_archive($base, 'rollback', array('BL/new-category/domains'));
        assert_true(!update_with($base, $archive, array('simulate_install_failure' => true)), 'simulated install failure unexpectedly succeeded');
        assert_true(is_file($base . '/lists/blacklists/old-category/domains'), 'rollback did not preserve previous legacy blacklist');
        assert_true(!is_dir($base . '/lists/blacklists.old'), 'rollback left blacklists.old behind');
        assert_preserved($base . '/lists');

        $base = $root . '/concurrency';
        make_tree($base);
        $archive = create_archive($base, 'concurrency', array('BL/new-category/domains'));
        $pid = pcntl_fork();
        assert_true($pid !== -1, 'could not fork concurrency worker');
        if ($pid === 0) {
                exit(update_with($base, $archive, array('hold_lock_seconds' => 2)) ? 0 : 1);
        }
        usleep(300000);
        assert_true(!update_with($base, $archive), 'concurrent update unexpectedly acquired the lock');
        pcntl_waitpid($pid, $status);
        assert_true(pcntl_wexitstatus($status) === 0, 'lock-holding update failed');

        echo "PASS: blacklist extraction smoke tests\n";
} finally {
        e2g_delTree($root);
}
