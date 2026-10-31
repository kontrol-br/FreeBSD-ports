#!/usr/local/bin/php -f
<?php

require_once('/usr/local/pkg/kontrol_id.inc');

$result = kontrol_id_sync_ad_policies(false);
if (empty($result['ok'])) {
	fwrite(STDERR, $result['message'] . "\n");
	exit(1);
}
if (!empty($result['message'])) {
	echo $result['message'] . "\n";
}

exit(0);
