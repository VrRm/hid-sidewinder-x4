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
#include <linux/input.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/usb.h>

#include "hid-ids.h"

#define MS_HIDINPUT		0x01
#define MS_ERGONOMY		0x02
#define MS_PRESENTER		0x04
#define MS_RDESC		0x08
#define MS_NOGET		0x10
#define MS_DUPLICATE_USAGES	0x20
#define MS_RDESC_3K		0x40
#define MS_SIDEWINDER	0x80

struct ms_data {
	unsigned long quirks;
	void *extra;
};

struct ms_sidewinder_extra {
	unsigned int profile;
	__u8 led_state;
};

static __u8 *ms_report_fixup(struct hid_device *hdev, __u8 *rdesc,
		unsigned int *rsize)
{
	struct ms_data *sc = hid_get_drvdata(hdev);

	/*
	 * Microsoft Wireless Desktop Receiver (Model 1028) has
	 * 'Usage Min/Max' where it ought to have 'Physical Min/Max'
	 */
	if ((sc->quirks & MS_RDESC) && *rsize == 571 && rdesc[557] == 0x19 &&
			rdesc[559] == 0x29) {
		hid_info(hdev, "fixing up Microsoft Wireless Receiver Model 1028 report descriptor\n");
		rdesc[557] = 0x35;
		rdesc[559] = 0x45;
	}
	/* the same as above (s/usage/physical/) */
	if ((sc->quirks & MS_RDESC_3K) && *rsize == 106 && rdesc[94] == 0x19 &&
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

static int ms_sidewinder_set_leds(struct hid_device *hdev, __u8 leds)
{
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	struct hid_report *report =
			hdev->report_enum[HID_FEATURE_REPORT].report_id_hash[7];

	/* LEDs 1 - 3 should not be set simultaneously, however
	 * they can be set in any combination with Record LEDs
	 */
	report->field[0]->value[0] = 0x00;
	report->field[0]->value[1] = (leds & 0x01) ? 0x01 : 0x00;	/* LED Auto */
	report->field[0]->value[2] = (leds & 0x02) ? 0x01 : 0x00;	/* LED 1 */
	report->field[0]->value[3] = (leds & 0x04) ? 0x01 : 0x00;	/* LED 2 */
	report->field[0]->value[4] = (leds & 0x08) ? 0x01 : 0x00;	/* LED 3 */

	if (leds & 0x80)
		report->field[1]->value[0] = 0x03;	/* Record LED Solid */
	else if (leds & 0x40)
		report->field[1]->value[0] = 0x02;	/* Record LED Blink */
	else if (leds & 0x20)
		report->field[1]->value[0] = 0x01;	/* Record LED Breath */
	else
		report->field[1]->value[0] = 0x00;

	if (sidewinder->led_state != leds) {
		hid_hw_request(hdev, report, HID_REQ_SET_REPORT);
		sidewinder->led_state = leds;
	}

	return 0;
}

/* Sidewinder sysfs drivers
 * @ms_sidewinder_profile: show and set profile count and LED status
 * @ms_sidewinder_auto_led: show and set LED Auto
 * @ms_sidewinder_record_led: show and set Record LED
 */
static ssize_t ms_sidewinder_profile_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;

	return snprintf(buf, PAGE_SIZE, "%1u\n", sidewinder->profile);
}

static ssize_t ms_sidewinder_profile_store(struct device *dev,
		struct device_attribute *attr, char const *buf, size_t count)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	__u8 leds = sidewinder->led_state & ~(0x0e);

	if (sscanf(buf, "%1u", &sidewinder->profile) != 1)
		return -EINVAL;

	if (sidewinder->profile >= 1 && sidewinder->profile <= 3) {
		leds |= 1 << sidewinder->profile;
		ms_sidewinder_set_leds(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else
		return -EINVAL;
}

static struct device_attribute dev_attr_ms_sidewinder_profile =
	__ATTR(ms_sidewinder_profile, S_IWUSR | S_IRUGO,
		ms_sidewinder_profile_show,
		ms_sidewinder_profile_store);

static ssize_t ms_sidewinder_record_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	int record_led;

	if (sidewinder->led_state & 0x80)
		record_led = 3;
	else if (sidewinder->led_state & 0x40)
		record_led = 2;
	else if (sidewinder->led_state & 0x20)
		record_led = 1;
	else
		record_led = 0;

	return snprintf(buf, PAGE_SIZE, "%1d\n", record_led);
}

static ssize_t ms_sidewinder_record_store(struct device *dev,
		struct device_attribute *attr, char const *buf, size_t count)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	unsigned int record_led;
	__u8 leds;

	if (sscanf(buf, "%1d", &record_led) != 1)
		return -EINVAL;

	if (record_led >= 0 && record_led <= 3) {
		leds = sidewinder->led_state & ~(0xf0);	/* Clear Record LED */
		leds |= 0x10 << record_led;
		ms_sidewinder_set_leds(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else
		return -EINVAL;
}

static struct device_attribute dev_attr_ms_sidewinder_record =
	__ATTR(ms_sidewinder_record, S_IWUSR | S_IRUGO,
		ms_sidewinder_record_show,
		ms_sidewinder_record_store);

static ssize_t ms_sidewinder_auto_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;

	return snprintf(buf, PAGE_SIZE, "%1d\n", sidewinder->led_state & 0x01);
}

static ssize_t ms_sidewinder_auto_store(struct device *dev,
		struct device_attribute *attr, char const *buf, size_t count)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	unsigned int auto_led;
	__u8 leds;

	if (sscanf(buf, "%1d", &auto_led) != 1)
		return -EINVAL;

	if (auto_led == 0 || auto_led == 1) {
		leds = sidewinder->led_state & ~(0x01);	/* Clear Auto LED */
		if (auto_led == 1)
			leds |= 0x01;
		ms_sidewinder_set_leds(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else
		return -EINVAL;
}

static struct device_attribute dev_attr_ms_sidewinder_auto =
	__ATTR(ms_sidewinder_auto, S_IWUSR | S_IRUGO,
		ms_sidewinder_auto_show,
		ms_sidewinder_auto_store);

static struct attribute *ms_attributes[] = {
	&dev_attr_ms_sidewinder_profile.attr,
	&dev_attr_ms_sidewinder_record.attr,
	&dev_attr_ms_sidewinder_auto.attr,
	NULL
};

static const struct attribute_group ms_attr_group = {
	.attrs = ms_attributes,
};

static int ms_sidewinder_kb_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	set_bit(EV_REP, hi->input->evbit);
	switch (usage->hid & HID_USAGE) {
	case 0xfb01: ms_map_key_clear(KEY_F13);	break;
	case 0xfb02: ms_map_key_clear(KEY_F14);	break;
	case 0xfb03: ms_map_key_clear(KEY_F15);	break;
	case 0xfb04: ms_map_key_clear(KEY_F16);	break;
	case 0xfb05: ms_map_key_clear(KEY_F17);	break;
	case 0xfb06: ms_map_key_clear(KEY_F18);	break;
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
	struct ms_data *sc = hid_get_drvdata(hdev);

	if ((usage->hid & HID_USAGE_PAGE) != HID_UP_MSVENDOR)
		return 0;

	if ((sc->quirks & MS_ERGONOMY) &&
			ms_ergonomy_kb_quirk(hi, usage, bit, max))
		return 1;

	if ((sc->quirks & MS_PRESENTER) &&
			ms_presenter_8k_quirk(hi, usage, bit, max))
		return 1;
		
	if ((sc->quirks & MS_SIDEWINDER) &&
			ms_sidewinder_kb_quirk(hi, usage, bit, max))
		return 1;

	return 0;
}

static int ms_input_mapped(struct hid_device *hdev, struct hid_input *hi,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct ms_data *sc = hid_get_drvdata(hdev);

	if (sc->quirks & MS_DUPLICATE_USAGES)
		clear_bit(usage->code, *bit);

	return 0;
}

/* Setting initial profile and LED of Sidewinder keyboards */
static void ms_feature_mapping(struct hid_device *hdev,
		struct hid_field *field, struct hid_usage *usage)
{
	struct ms_data *sc = hid_get_drvdata(hdev);

	if (sc->quirks & MS_SIDEWINDER) {
		struct ms_sidewinder_extra *sidewinder = sc->extra;

		sidewinder->profile = 1;
		ms_sidewinder_set_leds(hdev, 1 << sidewinder->profile);
	}
}

static int ms_event(struct hid_device *hdev, struct hid_field *field,
		struct hid_usage *usage, __s32 value)
{
	struct ms_data *sc = hid_get_drvdata(hdev);

	if (!(hdev->claimed & HID_CLAIMED_INPUT) || !field->hidinput ||
			!usage->type)
		return 0;

	/* Handling MS keyboards special buttons */
	if (sc->quirks & MS_ERGONOMY && usage->hid == (HID_UP_MSVENDOR | 0xff05)) {
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
	if (sc->quirks & MS_SIDEWINDER &&
			(usage->hid == (HID_UP_MSVENDOR | 0xfb01) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb02) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb03) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb04) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb05) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfb06) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfd12) ||
			usage->hid == (HID_UP_MSVENDOR | 0xfd15))) {
		struct input_dev *input = field->hidinput->input;
		struct ms_sidewinder_extra *sidewinder = sc->extra;
		__u8 leds = sidewinder->led_state & ~(0x0e);	/* Clear Profile LEDs */

		switch (usage->hid ^ HID_UP_MSVENDOR) {
		case 0xfb01: /* S1 */
			switch (sidewinder->profile) {
			case 1: input_event(input, usage->type, KEY_F13, value);	break;
			case 2: input_event(input, usage->type, KEY_F19, value);	break;
			case 3: input_event(input, usage->type, BTN_0, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb02: /* S2 */
			switch (sidewinder->profile) {
			case 1: input_event(input, usage->type, KEY_F14, value);	break;
			case 2: input_event(input, usage->type, KEY_F20, value);	break;
			case 3: input_event(input, usage->type, BTN_1, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb03: /* S3 */
			switch (sidewinder->profile) {
			case 1: input_event(input, usage->type, KEY_F15, value);	break;
			case 2: input_event(input, usage->type, KEY_F21, value);	break;
			case 3: input_event(input, usage->type, BTN_2, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb04: /* S4 */
			switch (sidewinder->profile) {
			case 1: input_event(input, usage->type, KEY_F16, value);	break;
			case 2: input_event(input, usage->type, KEY_F22, value);	break;
			case 3: input_event(input, usage->type, BTN_3, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb05: /* S5 */
			switch (sidewinder->profile) {
			case 1: input_event(input, usage->type, KEY_F17, value);	break;
			case 2: input_event(input, usage->type, KEY_F23, value);	break;
			case 3: input_event(input, usage->type, BTN_4, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfb06: /* S6 */
			switch (sidewinder->profile) {
			case 1: input_event(input, usage->type, KEY_F18, value);	break;
			case 2: input_event(input, usage->type, KEY_F24, value);	break;
			case 3: input_event(input, usage->type, BTN_5, value);	break;
			default:
				return 0;
			}
			break;
		case 0xfd12: input_event(input, usage->type, KEY_MACRO, value);	break;
		case 0xfd15:
			if (value) {
				if (sidewinder->profile < 1 || sidewinder->profile >= 3) {
					sidewinder->profile = 1;
				} else
					sidewinder->profile++;

				leds |= 1 << sidewinder->profile;	/* Set Profile LEDs */
				ms_sidewinder_set_leds(hdev, leds);
			}
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
	struct ms_data *sc;
	int ret;

	sc = devm_kzalloc(&hdev->dev, sizeof(struct ms_data), GFP_KERNEL);
	if (!sc) {
		hid_err(hdev, "can't alloc microsoft descriptor\n");
		return -ENOMEM;
	}

	sc->quirks = id->driver_data;
	hid_set_drvdata(hdev, sc);

	if (sc->quirks & MS_NOGET)
		hdev->quirks |= HID_QUIRK_NOGET;

	if (sc->quirks & MS_SIDEWINDER) {
		struct ms_sidewinder_extra *sidewinder;

		sidewinder = devm_kzalloc(&hdev->dev, sizeof(struct ms_sidewinder_extra),
					GFP_KERNEL);
		if (!sidewinder) {
			hid_err(hdev, "can't alloc microsoft descriptor\n");
			return -ENOMEM;
		}
		sc->extra = sidewinder;

		/* Create sysfs files for the consumer control device only */
		if (hdev->type == 2) {
			if (sysfs_create_group(&hdev->dev.kobj, &ms_attr_group)) {
				hid_warn(hdev, "Could not create sysfs group\n");
			}
		}
	}

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT | ((sc->quirks & MS_HIDINPUT) ?
				HID_CONNECT_HIDINPUT_FORCE : 0));
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	return 0;
err_free:
	return ret;
}

static void ms_remove(struct hid_device *hdev)
{
	sysfs_remove_group(&hdev->dev.kobj,
		&ms_attr_group);

	hid_hw_stop(hdev);
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
	.feature_mapping = ms_feature_mapping,
	.event = ms_event,
	.probe = ms_probe,
	.remove = ms_remove,
};
module_hid_driver(ms_driver);

MODULE_LICENSE("GPL");
