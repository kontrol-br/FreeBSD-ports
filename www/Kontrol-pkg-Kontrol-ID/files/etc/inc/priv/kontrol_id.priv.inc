<?php

global $priv_list;

$priv_list['page-services-kontrol-id'] = array();
$priv_list['page-services-kontrol-id']['name'] = 'WebCfg - Services: Kontrol-ID';
$priv_list['page-services-kontrol-id']['descr'] = 'Allow access to the Kontrol-ID package GUI';
$priv_list['page-services-kontrol-id']['match'] = array();
$priv_list['page-services-kontrol-id']['match'][] = 'pkg_edit.php?xml=kontrol_id.xml*';
$priv_list['page-services-kontrol-id']['match'][] = 'pkg_edit.php?xml=kontrol_id_sync.xml*';
$priv_list['page-services-kontrol-id']['match'][] = 'kontrol_id_status.php*';

?>
