<?php
/*
 * openvpn_schedule_cron.php
 * Disconnects already connected OpenVPN users that are currently outside allowed schedules.
 */

require_once('/usr/local/pkg/openvpn_schedule.inc');

openvpn_schedule_disconnect_out_of_schedule_users();
