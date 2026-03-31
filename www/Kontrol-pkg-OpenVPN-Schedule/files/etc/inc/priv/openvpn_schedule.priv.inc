<?php

/*
 * openvpn_schedule.priv.inc
 * Privileges for OpenVPN Schedule package pages.
 */

global $priv_list;

$priv_list['page-vpn-openvpn-schedule'] = array();
$priv_list['page-vpn-openvpn-schedule']['name'] = 'WebCfg - VPN: OpenVPN Schedule package';
$priv_list['page-vpn-openvpn-schedule']['descr'] = 'Allow access to OpenVPN Schedule package GUI';
$priv_list['page-vpn-openvpn-schedule']['match'] = array();
$priv_list['page-vpn-openvpn-schedule']['match'][] = 'pkg.php?xml=openvpn_schedule*';
$priv_list['page-vpn-openvpn-schedule']['match'][] = 'pkg_edit.php?xml=openvpn_schedule*';
$priv_list['page-vpn-openvpn-schedule']['match'][] = 'openvpn_schedule.php';

?>
