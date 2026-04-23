<?php
/*
 * lcdproc.php
 *
 * part of pfSense (https://www.pfsense.org/)
 * Copyright (c) 2016-2025 Rubicon Communications, LLC (Netgate)
 * Copyright (c) 2011 Michele Di Maria
 * Copyright (c) 2007-2009 Seth Mos <seth.mos@dds.nl>
 * Copyright (c) 2008 Mark J Crane
 * All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
require_once("guiconfig.inc");
require_once("/usr/local/pkg/lcdproc.inc");
global $lcdproc_log_levels, $mtxorb_backlight_color_list, $comport_list;
global $size_list, $driver_list, $connection_type_list, $mtxorb_type_list;
global $port_speed_list, $refresh_frequency_list, $percent_list, $backlight_list;

$lcdproc_config = config_get_path('installedpackages/lcdproc/config/0', []);

// Set default values for anything not in the $config
$pconfig = $lcdproc_config;
if (!isset($pconfig['enable']))                      $pconfig['enable']                      = '';
if (!isset($pconfig['log_level']))                   $pconfig['log_level']                   = '2';
if (!isset($pconfig['comport']))                     $pconfig['enabled']                     = 'ucom1';
if (!isset($pconfig['size']))                        $pconfig['size']                        = '16x2';
if (!isset($pconfig['driver']))                      $pconfig['driver']                      = 'pyramid';
if (!isset($pconfig['connection_type']))             $pconfig['connection_type']             = 'lcd2usb'; // specific to hd44780 driver
if (!isset($pconfig['refresh_frequency']))           $pconfig['refresh_frequency']           = '5';
if (!isset($pconfig['port_speed']))                  $pconfig['port_speed']                  = '0';
if (!isset($pconfig['brightness']))                  $pconfig['brightness']                  = '-1';
if (!isset($pconfig['offbrightness']))               $pconfig['offbrightness']               = '-1';
if (!isset($pconfig['contrast']))                    $pconfig['contrast']                    = '-1';
if (!isset($pconfig['backlight']))                   $pconfig['backlight']                   = 'default';
if (!isset($pconfig['outputleds']))                  $pconfig['outputleds']                  = 'no';
if (!isset($pconfig['controlmenu']))                 $pconfig['controlmenu']                 = 'no';
if (!isset($pconfig['mtxorb_type']))                 $pconfig['mtxorb_type']                 = 'lcd'; // specific to Matrix Orbital driver
if (!isset($pconfig['mtxorb_adjustable_backlight'])) $pconfig['mtxorb_adjustable_backlight'] = true;  // specific to Matrix Orbital driver
if (!isset($pconfig['mtxorb_backlight_color']))      $pconfig['mtxorb_backlight_color']      = '';  // specific to Matrix Orbital driver
if (!isset($pconfig['ch341_port']))                  $pconfig['ch341_port']                  = '0x27';
if (!isset($pconfig['ch341_usbinterface']))          $pconfig['ch341_usbinterface']          = '0';
if (!isset($pconfig['ch341_usbindex']))              $pconfig['ch341_usbindex']              = '0';
if (!isset($pconfig['ch341_usbbulkout']))            $pconfig['ch341_usbbulkout']            = '0x02';
if (!isset($pconfig['ch341_usbbulkin']))             $pconfig['ch341_usbbulkin']             = '0x82';
if (!isset($pconfig['ch341_usbconfig']))             $pconfig['ch341_usbconfig']             = '1';

$detect_message = '';

if ($_POST) {
	$input_errors = [];
	$pconfig = $_POST;

		if (isset($_POST['ch341_autodetect'])) {
		$detected_values = lcdproc_detect_ch341_usb();
		if (!empty($detected_values)) {
			$pconfig = array_merge($pconfig, $detected_values);
			$detect_message = 'CH341 USB dongle detected and USB fields were auto-filled from usbconfig descriptors. I2C Address was set to default 0x27 (descriptor does not expose LCD backpack address). Verify values and save.';
		} else {
			$detect_message = 'No CH341 USB dongle was detected. Connect the device and try again.';
		}
	}

	/* Input validation */
	if (!isset($_POST['ch341_autodetect'])) {
		lcdproc_validate_list($input_errors, 'log_level',              $lcdproc_log_levels,          'Log Level');
		lcdproc_validate_list($input_errors, 'comport',                $comport_list,                'COM Port');
		lcdproc_validate_list($input_errors, 'size',                   $size_list,                   'Display Size');
		lcdproc_validate_list($input_errors, 'driver',                 $driver_list,                 'Driver');
		lcdproc_validate_list($input_errors, 'connection_type',        $connection_type_list,        'Connection Type');
		lcdproc_validate_list($input_errors, 'mtxorb_type',            $mtxorb_type_list,            'Display Type');
		lcdproc_validate_list($input_errors, 'mtxorb_backlight_color', $mtxorb_backlight_color_list, 'Matrix Orbital Background Color');
		lcdproc_validate_list($input_errors, 'port_speed',             $port_speed_list,             'Port Speed');
		lcdproc_validate_list($input_errors, 'refresh_frequency',      $refresh_frequency_list,      'Refresh Frequency');
		lcdproc_validate_list($input_errors, 'brightness',             $percent_list,                'Brightness');
		lcdproc_validate_list($input_errors, 'contrast',               $percent_list,                'Contrast');
		lcdproc_validate_list($input_errors, 'backlight',              $backlight_list,              'Backlight');
		lcdproc_validate_list($input_errors, 'offbrightness',          $percent_list,                'Off Brightness');
		$using_hd44780_ch341 = ($pconfig['driver'] == 'hd44780' && $pconfig['connection_type'] == 'ch341i2c');
		if ($using_hd44780_ch341) {
			if (!preg_match('/^0x[0-9a-fA-F]{2}$/', $pconfig['ch341_port'])) {
				$input_errors[] = 'CH341 I2C address must be in hexadecimal format, for example 0x27.';
			}
			if (!preg_match('/^0x[0-9a-fA-F]{2}$/', $pconfig['ch341_usbbulkout'])) {
				$input_errors[] = 'CH341 UsbBulkOut must be in hexadecimal format, for example 0x02.';
			}
			if (!preg_match('/^0x[0-9a-fA-F]{2}$/', $pconfig['ch341_usbbulkin'])) {
				$input_errors[] = 'CH341 UsbBulkIn must be in hexadecimal format, for example 0x82.';
			}
			foreach (['ch341_usbinterface', 'ch341_usbindex', 'ch341_usbconfig'] as $field) {
				if (!is_numericint($pconfig[$field])) {
					$input_errors[] = sprintf('CH341 %s must be a non-negative integer.', $field);
				}
			}
		}
	}

	if (!isset($_POST['ch341_autodetect']) && empty($input_errors)) {
		$lcdproc_config['enable']                      = $pconfig['enable'];
		$lcdproc_config['log_level']                   = $pconfig['log_level'];
		$lcdproc_config['comport']                     = $pconfig['comport'];
		$lcdproc_config['size']                        = $pconfig['size'];
		$lcdproc_config['driver']                      = $pconfig['driver'];
		$lcdproc_config['connection_type']             = $pconfig['connection_type'];
		$lcdproc_config['refresh_frequency']           = $pconfig['refresh_frequency'];
		$lcdproc_config['port_speed']                  = $pconfig['port_speed'];
		$lcdproc_config['brightness']                  = $pconfig['brightness'];
		$lcdproc_config['offbrightness']               = $pconfig['offbrightness'];
		$lcdproc_config['contrast']                    = $pconfig['contrast'];
		$lcdproc_config['backlight']                   = $pconfig['backlight'];
		$lcdproc_config['outputleds']                  = $pconfig['outputleds'];
		$lcdproc_config['controlmenu']                 = $pconfig['controlmenu'];
		$lcdproc_config['mtxorb_type']                 = $pconfig['mtxorb_type'];
		$lcdproc_config['mtxorb_adjustable_backlight'] = $pconfig['mtxorb_adjustable_backlight'];
		$lcdproc_config['mtxorb_backlight_color']      = $pconfig['mtxorb_backlight_color'];
		$lcdproc_config['ch341_port']                  = $pconfig['ch341_port'];
		$lcdproc_config['ch341_usbinterface']          = $pconfig['ch341_usbinterface'];
		$lcdproc_config['ch341_usbindex']              = $pconfig['ch341_usbindex'];
		$lcdproc_config['ch341_usbbulkout']            = $pconfig['ch341_usbbulkout'];
		$lcdproc_config['ch341_usbbulkin']             = $pconfig['ch341_usbbulkin'];
		$lcdproc_config['ch341_usbconfig']             = $pconfig['ch341_usbconfig'];

		config_set_path('installedpackages/lcdproc/config/0', $lcdproc_config);
		write_config("lcdproc: Settings saved");
		sync_package_lcdproc();
	}
}

$shortcut_section = 'lcdproc';

$pgtitle = array(gettext("Services"), gettext("LCDproc"), gettext("Server"));
include("head.inc");

if (!empty($input_errors)) {
	print_input_errors($input_errors);
}
if (!empty($detect_message)) {
	print_info_box($detect_message, 'info');
}

$tab_array = array();
$tab_array[] = array(gettext("Server"),  true,  "/packages/lcdproc/lcdproc.php");
$tab_array[] = array(gettext("Screens"), false, "/packages/lcdproc/lcdproc_screens.php");
display_top_tabs($tab_array);

$form = new Form();
$section = new Form_Section('LCD connection and hardware');

// Add the Enable checkbox
$section->addInput(
	new Form_Checkbox(
		'enable', // checkbox name (id)
		'Enable', // checkbox label
		'Enable LCDproc service', // checkbox text
		$pconfig['enable'] // checkbox initial value
	)
);

$section->addInput(
	new Form_Select(
		'log_level',
		'Log Level',
		$pconfig['log_level'], // Initial value.
		$lcdproc_log_levels
	)
)->setHelp('Choose the level of detail the LCDProc daemon will write to the system log.');

// Add the com port selector
$section->addInput(
	new Form_Select(
		'comport',
		'COM port',
		$pconfig['comport'], // Initial value.
		$comport_list
	)
)->setHelp('Set the com port LCDproc should use.');

$section->addInput(
	new Form_Select(
		'size',
		'Display Size',
		$pconfig['size'], // Initial value.
		$size_list
	)
)->setHelp('Set the display size lcdproc should use.');

$section->addInput(
	new Form_Select(
		'driver',
		'Driver',
		$pconfig['driver'], // Initial value.
		$driver_list
	)
)->setHelp('Select the LCD driver LCDproc should use. Some drivers will show additional settings.');

// The connection type is HD44780-specific, so is hidden by javascript (below)
// if the HD44780 driver is not being used.
$section->addInput(
	new Form_Select(
		'connection_type',
		'Connection Type',
		$pconfig['connection_type'], // Initial value.
		$connection_type_list
	)
)->setHelp('Select the HD44780 connection type');

/* The mtxorb_type, mtxorb_adjustable_backlight, and mtxorb_backlight_color are
 * Matrix-Orbital-specific, so are hidden by javascript (below) if the MtxOrb
 * driver is not being used.
 */
$subsection = new Form_Group('Display type');
$subsection->add(
	new Form_Select(
		'mtxorb_type',
		'Display Type',
		$pconfig['mtxorb_type'], // Initial value.
		$mtxorb_type_list
	)
);
$subsection->add(
	new Form_Checkbox(
		'mtxorb_adjustable_backlight',          // checkbox name (id)
		'Has adjustable backlight',             // label
		'Has adjustable backlight',             // text
		$pconfig['mtxorb_adjustable_backlight'] // initial value
	)
);
$subsection->add(
	new Form_Select(
		'mtxorb_backlight_color',
		'Background Color',
		$pconfig['mtxorb_backlight_color'], // Initial value.
		array_combine(array_keys($mtxorb_backlight_color_list), array_keys($mtxorb_backlight_color_list))
	)
)->setHelp('LCD Background Color');

$subsection->setHelp(
	'Select the Matrix Orbital display type.%1$s%1$s' .
	'Some firmware versions of Matrix Orbital and compatible modules do not support an adjustable backlight ' .
	'and only can switch the backlight on/off. If the LCD experiences randomly appearing block characters ' .
	'and the backlight cannot be switched on or off, uncheck the adjustable backlight option.%1$s%1$s' .
	'Some Matrix Orbital compatible Adafruit controller boards have extended commands to set ' .
	'the background color. This works independently of the adjustable backlight checkbox. ' .
	'Leave at default if this feature is not supported.',
	'<br/>'
);

$section->add($subsection);

$subsection = new Form_Group('CH341/I2C');
$subsection->add(new Form_Input('ch341_port', 'I2C Address', 'text', $pconfig['ch341_port']));
$subsection->add((new Form_Input('ch341_usbinterface', 'USB Interface', 'number', $pconfig['ch341_usbinterface']))->setAttribute('min', '0'));
$subsection->add((new Form_Input('ch341_usbindex', 'USB Index', 'number', $pconfig['ch341_usbindex']))->setAttribute('min', '0'));
$subsection->add(new Form_Input('ch341_usbbulkout', 'USB Bulk Out', 'text', $pconfig['ch341_usbbulkout']));
$subsection->add(new Form_Input('ch341_usbbulkin', 'USB Bulk In', 'text', $pconfig['ch341_usbbulkin']));
$subsection->add((new Form_Input('ch341_usbconfig', 'USB Config', 'number', $pconfig['ch341_usbconfig']))->setAttribute('min', '0'));
$autodetect_btn = new Form_Button('ch341_autodetect', 'Auto Detect USB Values', null, 'fa-solid fa-wand-magic-sparkles');
$autodetect_btn->setAttribute('type', 'submit')->addClass('btn-primary btn-sm');
$subsection->add($autodetect_btn);
$subsection->setHelp(
	'Used only when Driver is HD44780 and Connection Type is ch341i2c.%1$s' .
	'Recommended defaults: I2C Address 0x27, USB Interface 0, USB Index 0, USB Bulk Out 0x02, USB Bulk In 0x82, USB Config 1.%1$s%1$s' .
	'To find USB values, connect the adapter and run:%1$s' .
	'<code>usbconfig list | grep -i ch34</code>%1$s' .
	'Then inspect the selected device (example <code>ugen0.2</code>):%1$s' .
	'<code>usbconfig -d ugen0.2 dump_all_desc | egrep "bConfigurationValue|bInterfaceNumber|bEndpointAddress"</code>.%1$s' .
	'Use <b>bConfigurationValue</b> for USB Config, <b>bInterfaceNumber</b> for USB Interface, ' .
	'and endpoint addresses for USB Bulk Out / USB Bulk In.%1$s' .
	'The I2C Address is not exposed by USB descriptors; auto-detect keeps default <code>0x27</code>. ' .
	'Use the LCD backpack address from hardware docs (commonly 0x27 or 0x3f).',
	'<br/>'
);
$section->add($subsection);
?>

<script type="text/javascript">
//<![CDATA[
	events.push(
		function() {
			$('#driver').on('change', updateInputVisibility);
			$('#connection_type').on('change', updateInputVisibility);
			updateInputVisibility();
		}
	);

    function updateInputVisibility() {
		var driverName_lowercase = $('#driver').val().toLowerCase();

		// Hide the connection type selection field when not using the HD44780 driver
		var using_HD44780_driver  = driverName_lowercase.indexOf("hd44780") >= 0;
		using_HD44780_driver     |= jQuery("#driver option:selected").text().toLowerCase().indexOf("hd44780") >= 0;
		hideInput('connection_type', !using_HD44780_driver); // Hides the entire section

		// Hide the Matrix Orbital specific fields when not using the MtxOrb driver
		var using_MtxOrb_driver  = driverName_lowercase.indexOf("mtxorb") >= 0;
		hideInput('mtxorb_type', !using_MtxOrb_driver); // Hides the entire section, including the mtxorb_adjustable_backlight checkbox

		var using_CH341 = using_HD44780_driver && $('#connection_type').val() == 'ch341i2c';
		hideInput('ch341_port', !using_CH341);

		// Hide the Output-LEDs checkbox when not using the CFontzPacket driver
		var driverSupportsLEDs  = driverName_lowercase.indexOf("cfontzpacket") >= 0;
		hideCheckbox('outputleds', !driverSupportsLEDs);
	}
//]]>
</script>

<?php

$section->addInput(
	new Form_Select(
		'port_speed',
		'Port Speed',
		$pconfig['port_speed'], // Initial value.
		$port_speed_list
	)
)->setHelp(
	'Set the port speed.%1$s' .
	'Caution: not all the driver or panels support all the speeds, leave "default" if unsure.',
	'<br />'
);

/********* New section *********/
$form->add($section);
$section = new Form_Section('Display preferences');
/********* New section *********/

$section->addInput(
	new Form_Select(
		'refresh_frequency',
		'Refresh Frequency',
		$pconfig['refresh_frequency'], // Initial value.
		$refresh_frequency_list
	)
)->setHelp('Set the duration for which each info screen will be displayed.');

// The connection type is CFontzPacket-specific, so is hidden by javascript (above)
// if a CFontzPacket driver is not being used.
$section->addInput(
	new Form_Checkbox(
		'outputleds', // checkbox name (id)
		'Enable Output LEDs', // checkbox label
		'Enable the output LEDs present on some LCD panels.', // checkbox text
		$pconfig['outputleds'] // checkbox initial value
	)
)->setHelp(
	'This feature is currently supported by the CFontzPacket driver only.%1$s' .
	'Each LED can be off or show two colors: RED (alarm) or GREEN (everything ok) and shows:%1$s' .
	'LED1: NICs status (green: ok, red: at least one nic down)%1$s' .
	'LED2: CARP status (green: master, red: backup, off: CARP not implemented)%1$s' .
	'LED3: CPU status (green %2$s 50%%, red %3$s 50%%)%1$s' .
	'LED4: Gateway status (green: ok, red: at least one gateway not responding, off: no gateway configured).',
	'<br />', '&lt;', '&gt;'
);

$section->addInput(
	new Form_Checkbox(
		'controlmenu', // checkbox name (id)
		'Kontrol control menu', // checkbox label
		'Enable the Kontrol control menu next to LCDproc\'s Options menu.', // checkbox text
		$pconfig['controlmenu'] // checkbox initial value
	)
)->setHelp(
	'Requires a display with buttons (e.g. Crystalfontz 635/735/835).%1$s' .
	'Currently supports several basic functions including reboot and halt.',
	'<br />'
);

$section->addInput(
	new Form_Select(
		'brightness',
		'Brightness',
		$pconfig['brightness'], // Initial value.
		$percent_list
	)
)->setHelp(
	'Set the brightness of the LCD panel.%1$s' . '
	This option is not supported by all the LCD panels, leave "default" if unsure.',
	'<br />'
);

$section->addInput(
	new Form_Select(
		'contrast',
		'Contrast',
		$pconfig['contrast'], // Initial value.
		$percent_list
	)
)->setHelp(
	'Set the contrast of the LCD panel.%1$s' .
	'This option is not supported by all the LCD panels, leave "default" if unsure.',
	'<br />'
);

$section->addInput(
	new Form_Select(
		'backlight',
		'Backlight',
		$pconfig['backlight'], // Initial value.
		$backlight_list
	)
)->setHelp(
	'Set the backlight setting. If set to the default value, then the backlight setting of the display can be influenced by the clients.%1$s' .
	'This option is not supported by all the LCD panels, leave "default" if unsure.',
	'<br />'
);

$section->addInput(
	new Form_Select(
		'offbrightness',
		'Off Brightness',
		$pconfig['offbrightness'], // Initial value.
		$percent_list
	)
)->setHelp(
	'Set the off-brightness of the LCD panel. This value is used when the display is normally switched off in case LCDd is inactive.%1$s' .
	'This option is not supported by all the LCD panels, leave "default" if unsure.',
	'<br />'
);

$form->add($section); // Add the section to our form
print($form); // Finally . . We can display our new form

?>

<div class="infoblock">
	<?=print_info_box('For more information see: <a href="http://lcdproc.org/docs.php3">LCDproc documentation</a>.', 'info')?>
</div>

<?php include("foot.inc"); ?>
