<?php
/*
 * Clone E2Guardian groups and active ACL definitions without changing the
 * generic pfSense package editor.
 */

##|+PRIV
##|*IDENT=page-services-e2guardian-clone
##|*NAME=Services: E2guardian clone ACLs and groups
##|*DESCR=Allow cloning E2guardian ACLs and groups.
##|*MATCH=e2guardian_clone.php*
##|-PRIV

require_once('guiconfig.inc');
require_once('/usr/local/pkg/e2guardian.inc');

function e2guardian_clone_error($message, $xml = '') {
    global $pgtitle, $pglinks;
    $pgtitle = array(gettext('Services'), gettext('E2guardian'), gettext('Clone'));
    $pglinks = array('', '/pkg_edit.php?xml=e2guardian.xml', '@self');
    include('head.inc');
    print_info_box(htmlspecialchars($message), 'danger', false);
    if ($xml !== '') {
        printf('<a class="btn btn-default" href="/pkg.php?xml=%s">%s</a>',
            htmlspecialchars(rawurlencode($xml)), gettext('Back'));
    }
    include('foot.inc');
    exit;
}

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    e2guardian_clone_error(gettext('Cloning requires a valid request from an E2guardian list.'));
}

$xml = basename((string)($_POST['xml'] ?? ''));
$source_id = $_POST['clone_id'] ?? null;
$lists = e2guardian_cloneable_lists();
$package_name = '';
$list_info = null;
foreach ($lists as $candidate_name => $candidate_info) {
    if (hash_equals($candidate_info['xml'], $xml)) {
        $package_name = $candidate_name;
        $list_info = $candidate_info;
        break;
    }
}
if ($package_name === '' || !is_numeric($source_id) || (int)$source_id < 0) {
    e2guardian_clone_error(gettext('The requested E2guardian item is invalid.'), $xml);
}

$items = config_get_path("installedpackages/{$package_name}/config", array());
$source_id = (int)$source_id;
if (!isset($items[$source_id]) || !is_array($items[$source_id])) {
    e2guardian_clone_error(gettext('The E2guardian item to clone no longer exists.'), $xml);
}

$new_id = 0;
while (array_key_exists($new_id, $items)) {
    $new_id++;
}
$draft_name = e2guardian_unique_clone_name((string)($items[$source_id]['name'] ?? ''), $items);
pfSenseHeader('/pkg_edit.php?xml=' . rawurlencode($xml) . '&act=edit&id=' . $new_id .
    '&e2g_clone_source=' . $source_id . '&savemsg=' . rawurlencode(sprintf(
        '%s is an unsaved clone draft. Review its settings and click Save to create it.',
        $draft_name
    )));
exit;
