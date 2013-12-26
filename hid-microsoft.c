/*
 *  HID driver for some microsoft "special" devices
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2006-2007 Jiri Kosina
 *  Copyright (c) 2008 Jiri Slaby
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb/input.h>

#include "hid-ids.h"

#define MS_HIDINPUT		0x01
#define MS_ERGONOMY		0x02
#define MS_PRESENTER		0x04
#define MS_RDESC		0x08
#define MS_NOGET		0x10
#define MS_DUPLICATE_USAGES	0x20
#define MS_RDESC_3K		0x40
#define MS_SIDEWINDER	0x80

static __u8 *ms_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	unsigned long quirks = (unsigned long)hid_get_drvdata(hdev);

	/*
	 * Microsoft Wireless Desktop Receiver (Model 1028) has
	 * 'Usage Min/Max' where it ought to have 'Physical Min/Max'
	 */
	if ((quirks & MS_RDESC) && *rsize == 571 && rdesc[557] == 0x19 &&
			rdesc[559] == 0x29) {
		hid_info(hdev, "fixing up Microsoft Wireless Receiver Model 1028 report descriptor\n");
		rdesc[557] = 0x35;
		rdesc[559] = 0x45;
	}
	/* the same as above (s/usage/physical/) */
	if ((quirks & MS_RDESC_3K) && *rsize == 106 && rdesc[94] == 0x19 &&
			rdesc[95] == 0x00 && rdesc[96] == 0x29 &&
			rdesc[97] == 0xff) {
		rdesc[94] = 0x35;
		rdesc[96] = 0x45;
	}
	return rdesc;
}

#define ms_map_key_clear(c)	hid_map_usage_clear(hi, usage, bit, max, \
					EV_KEY, (c))
static int ms_ergonomy_kb_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct input_dev *input = hi->input;

	switch (usage->hid & HID_USAGE) {
	case 0xfd06: ms_map_key_clear(KEY_CHAT);	break;
	case 0xfd07: ms_map_key_clear(KEY_PHONE);	break;
	case 0xff05:
		set_bit(EV_REP, input->evbit);
		ms_map_key_clear(KEY_F13);
		set_bit(KEY_F14, input->keybit);
		set_bit(KEY_F15, input->keybit);
		set_bit(KEY_F16, input->keybit);
		set_bit(KEY_F17, input->keybit);
		set_bit(KEY_F18, input->keybit);
	default:
		return 0;
	}
	return 1;
}

static int ms_presenter_8k_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	set_bit(EV_REP, hi->input->evbit);
	switch (usage->hid & HID_USAGE) {
	case 0xfd08: ms_map_key_clear(KEY_FORWARD);	break;
	case 0xfd09: ms_map_key_clear(KEY_BACK);	break;
	case 0xfd0b: ms_map_key_clear(KEY_PLAYPAUSE);	break;
	case 0xfd0e: ms_map_key_clear(KEY_CLOSE);	break;
	case 0xfd0f: ms_map_key_clear(KEY_PLAY);	break;
	default:
		return 0;
	}
	return 1;
}

static int ms_sidewinder_led(struct hid_device *hdev, char profile_led, char record_led)
{
	struct hid_report *report = hid_register_report(hdev,
		HID_FEATURE_REPORT, 0x07);

	/* LED is set by 2 Bytes. 0x07 is unknown
	 * Profile LEDs cannot be set simultaneously, however
	 * they can be set in any combination with the Record LED
	 */
	switch(profile_led) {
	case 0: break;						/* LEDs OFF */
	case 1: report->field[0]->value[2] = 0x01;	break;	/* LED 1 */
	case 2: report->field[0]->value[3] = 0x01;	break;	/* LED 2 */
	case 3: report->field[0]->value[4] = 0x01;	break;	/* LED 3 */
	case 4: report->field[0]->value[1] = 0x01;	break;	/* LED Auto */
	default:
		return -EINVAL;
	}

	switch(record_led) {
	case 0: break;						/* Record LED OFF */
	case 1: report->field[1]->value[0] = 0x01;	break;	/* Record LED Breath */
	case 2: report->field[1]->value[0] = 0x02;	break;	/* Record LED Blink */
	case 3: report->field[1]->value[0] = 0x03;	break;	/* Record LED Solid */
	default:
		return -EINVAL;
	}

	hid_hw_request(hdev, report, HID_REQ_SET_REPORT);

	return 0;
}

static int ms_sidewinder_profile(struct hid_device *hdev, char get)
{
	static char profile = 1;

	switch (get) {
	case 0:
		if (profile < 1 || profile >= 3)
			profile = 1;
		else 
			profile++;
		break;
	case 1: return profile;
	}

	ms_sidewinder_led(hdev, profile, 0);

	return profile;
}

static int ms_sidewinder_kb_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	set_bit(EV_REP, hi->input->evbit);
	switch (usage->hid & HID_USAGE) {
	case 0xfb01: ms_map_key_clear(KEY_FN_F7);	break;
	case 0xfb02: ms_map_key_clear(KEY_FN_F8);	break;
	case 0xfb03: ms_map_key_clear(KEY_FN_F9);	break;
	case 0xfb04: ms_map_key_clear(KEY_FN_F10);	break;
	case 0xfb05: ms_map_key_clear(KEY_FN_F11);	break;
	case 0xfb06: ms_map_key_clear(KEY_FN_F12);	break;
	case 0xfd12: ms_map_key_clear(KEY_MACRO);	break;
	case 0xfd15: ms_map_key_clear(KEY_UNKNOWN);	break;
	default:
		return 0;
	}
	return 1;
}

static int ms_input_mapping(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	unsigned long quirks = (unsigned long)hid_get_drvdata(hdev);

	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_MSVENDOR)
		return 0;

	if ((quirks & MS_ERGONOMY) &&
			ms_ergonomy_kb_quirk(hi, usage, bit, max))
		return 1;

	if ((quirks & MS_PRESENTER) &&
			ms_presenter_8k_quirk(hi, usage, bit, max))
		return 1;
		
	if ((quirks & MS_SIDEWINDER) &&
			ms_sidewinder_kb_quirk(hi, usage, bit, max))
		return 1;

	return 0;
}

static int ms_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	unsigned long quirks = (unsigned long)hid_get_drvdata(hdev);

	if (quirks & MS_DUPLICATE_USAGES)
		clear_bit(usage->code, *bit);

	return 0;
}

static int ms_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value)
{
	unsigned long quirks = (unsigned long)hid_get_drvdata(hdev);

	if (!(hdev->claimed & HID_CLAIMED_INPUT) || !field->hidinput ||
			!usage->type)
		return 0;

	/* Handling MS keyboards special buttons */
	if (quirks & MS_ERGONOMY && usage->hid == (HID_UP_MSVENDOR | 0xff05)) {
		struct input_dev *input = field->hidinput->input;
		static unsigned int last_key = 0;
		unsigned int key = 0;
		switch (value) {
		case 0x01: key = KEY_F14; break;
		case 0x02: key = KEY_F15; break;
		case 0x04: key = KEY_F16; break;
		case 0x08: key = KEY_F17; break;
		case 0x10: key = KEY_F18; break;
		}
		if (key) {
			input_event(input, usage->type, key, 1);
			last_key = key;
		} else
			input_event(input, usage->type, last_key, 0);

		return 1;
	}

	/* Sidewinder special button handling & profile switching */
	if (quirks & MS_SIDEWINDER &&
			(usage->hid == (HID_UP_MSVENDOR | 0xfb01) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb02) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb03) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb04) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb05) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb06) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfd12) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfd15))) {
		struct input_dev *input = field->hidinput->input;
		switch (usage->hid ^ HID_UP_MSVENDOR) {
		case 0xfb01: /* S1 */
			switch (ms_sidewinder_profile(hdev, 1)) {
			case 1: input_event(input, usage->type, KEY_FN_F7, value);	break; /* Bank 1 */
			case 2: input_event(input, usage->type, KEY_F13, value);	break; /* Bank 2 */
			case 3: input_event(input, usage->type, KEY_F19, value);	break; /* Bank 3 */
			default:
				return 0;
			}
			break;
		case 0xfb02: /* S2 */
			switch (ms_sidewinder_profile(hdev, 1)) {
			case 1: input_event(input, usage->type, KEY_FN_F8, value);	break;
			case 2: input_event(input, usage->type, KEY_F14, value);	break;
			case 3: input_event(input, usage->type, KEY_F20, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb03: /* S3 */
			switch (ms_sidewinder_profile(hdev, 1)) {
			case 1: input_event(input, usage->type, KEY_FN_F9, value);	break;
			case 2: input_event(input, usage->type, KEY_F15, value);	break;
			case 3: input_event(input, usage->type, KEY_F21, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb04: /* S4 */
			switch (ms_sidewinder_profile(hdev, 1)) {
			case 1: input_event(input, usage->type, KEY_FN_F10, value);	break;
			case 2: input_event(input, usage->type, KEY_F16, value);	break;
			case 3: input_event(input, usage->type, KEY_F22, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb05: /* S5 */
			switch (ms_sidewinder_profile(hdev, 1)) {
			case 1: input_event(input, usage->type, KEY_FN_F11, value);	break;
			case 2: input_event(input, usage->type, KEY_F17, value);	break;
			case 3: input_event(input, usage->type, KEY_F23, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb06: /* S6 */
			switch (ms_sidewinder_profile(hdev, 1)) {
			case 1: input_event(input, usage->type, KEY_FN_F12, value);	break;
			case 2: input_event(input, usage->type, KEY_F18, value);	break;
			case 3: input_event(input, usage->type, KEY_F24, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfd12: input_event(input, usage->type, KEY_MACRO, value);	break;
		case 0xfd15:
			if (value)
				ms_sidewinder_profile(hdev, 0);
			break;
		default:
			return 0;
		}

		return 1;
	}

	return 0;
}

static int ms_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	unsigned long quirks = id->driver_data;
	int ret;

	hid_set_drvdata(hdev, (void *)quirks);

	if (quirks & MS_NOGET)
		hdev->quirks |= HID_QUIRK_NOGET;

	if (quirks & MS_SIDEWINDER)
		ms_sidewinder_led(hdev, 1, 0); /* Set initial LED */

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT | ((quirks & MS_HIDINPUT) ?
				HID_CONNECT_HIDINPUT_FORCE : 0));
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	return 0;
err_free:
	return ret;
}

static const struct hid_device_id ms_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_SIDEWINDER_GV),
		.driver_data = MS_HIDINPUT },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_SIDEWINDER_X4),
		.driver_data = MS_SIDEWINDER },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_NE4K),
		.driver_data = MS_ERGONOMY },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_NE4K_JP),
		.driver_data = MS_ERGONOMY },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_LK6K),
		.driver_data = MS_ERGONOMY | MS_RDESC },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_PRESENTER_8K_USB),
		.driver_data = MS_PRESENTER },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_DIGITAL_MEDIA_3K),
		.driver_data = MS_ERGONOMY | MS_RDESC_3K },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_WIRELESS_OPTICAL_DESKTOP_3_0),
		.driver_data = MS_NOGET },
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_COMFORT_MOUSE_4500),
		.driver_data = MS_DUPLICATE_USAGES },

	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_MS_PRESENTER_8K_BT),
		.driver_data = MS_PRESENTER },
	{ }
};
MODULE_DEVICE_TABLE(hid, ms_devices);

static struct hid_driver ms_driver = {
	.name = "microsoft",
	.id_table = ms_devices,
	.report_fixup = ms_report_fixup,
	.input_mapping = ms_input_mapping,
	.input_mapped = ms_input_mapped,
	.event = ms_event,
	.probe = ms_probe,
};
module_hid_driver(ms_driver);

MODULE_LICENSE("GPL");
