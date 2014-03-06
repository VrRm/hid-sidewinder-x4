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
	__u8 status;
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

static int ms_sidewinder_kb_quirk(struct hid_input *hi, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	struct input_dev *input = hi->input;

	set_bit(EV_REP, input->evbit);
	switch (usage->hid & HID_USAGE) {
	/* S1 - S6 macro keys are shared between Sidewinder X4 and X6 */
	case 0xfb01:	/* S1 */
		ms_map_key_clear(KEY_F13);
		set_bit(KEY_F19, input->keybit);
		set_bit(KEY_WWW, input->keybit);
		break;
	case 0xfb02:	/* S2 */
		ms_map_key_clear(KEY_F14);
		set_bit(KEY_F20, input->keybit);
		set_bit(KEY_MAIL, input->keybit);
		break;
	case 0xfb03:	/* S3 */
		ms_map_key_clear(KEY_F15);
		set_bit(KEY_F21, input->keybit);
		set_bit(KEY_PROG1, input->keybit);
		break;
	case 0xfb04:	/* S4 */
		ms_map_key_clear(KEY_F16);
		set_bit(KEY_F22, input->keybit);
		set_bit(KEY_PROG2, input->keybit);
		break;
	case 0xfb05:	/* S5 */
		ms_map_key_clear(KEY_F17);
		set_bit(KEY_F23, input->keybit);
		set_bit(KEY_PROG3, input->keybit);
		break;
	case 0xfb06:	/* S6 */
		ms_map_key_clear(KEY_F18);
		set_bit(KEY_F24, input->keybit);
		set_bit(KEY_PROG4, input->keybit);
		break;
	/* S7 - S30 macro keys are only present on the Sidewinder X6 */
	case 0xfb07:	/* S7 */
		ms_map_key_clear(BTN_0);
		set_bit(BTN_BASE, input->keybit);
		set_bit(BTN_A, input->keybit);
		break;
	case 0xfb08:	/* S8 */
		ms_map_key_clear(BTN_1);
		set_bit(BTN_BASE2, input->keybit);
		set_bit(BTN_B, input->keybit);
		break;
	case 0xfb09:	/* S9 */
		ms_map_key_clear(BTN_2);
		set_bit(BTN_BASE3, input->keybit);
		set_bit(BTN_C, input->keybit);
		break;
	case 0xfb0a:	/* S10 */
		ms_map_key_clear(BTN_3);
		set_bit(BTN_BASE4, input->keybit);
		set_bit(BTN_X, input->keybit);
		break;
	case 0xfb0b:	/* S11 */
		ms_map_key_clear(BTN_4);
		set_bit(BTN_BASE5, input->keybit);
		set_bit(BTN_Y, input->keybit);
		break;
	case 0xfb0c:	/* S12 */
		ms_map_key_clear(BTN_5);
		set_bit(BTN_BASE6, input->keybit);
		set_bit(BTN_Z, input->keybit);
		break;
	case 0xfb0d:	/* S13 */
		ms_map_key_clear(KEY_F13);
		set_bit(KEY_F19, input->keybit);
		set_bit(KEY_WWW, input->keybit);
		break;
	case 0xfb0e:	/* S14 */
		ms_map_key_clear(KEY_F14);
		set_bit(KEY_F20, input->keybit);
		set_bit(KEY_MAIL, input->keybit);
		break;
	case 0xfb0f:	/* S15 */
		ms_map_key_clear(KEY_F15);
		set_bit(KEY_F21, input->keybit);
		set_bit(KEY_PROG1, input->keybit);
		break;
	case 0xfb10:	/* S16 */
		ms_map_key_clear(KEY_F16);
		set_bit(KEY_F22, input->keybit);
		set_bit(KEY_PROG2, input->keybit);
		break;
	case 0xfb11:	/* S17 */
		ms_map_key_clear(KEY_F17);
		set_bit(KEY_F23, input->keybit);
		set_bit(KEY_PROG3, input->keybit);
		break;
	case 0xfb12:	/* S18 */
		ms_map_key_clear(KEY_F18);
		set_bit(KEY_F24, input->keybit);
		set_bit(KEY_PROG4, input->keybit);
		break;
	case 0xfb13:	/* S19 */
		ms_map_key_clear(KEY_F13);
		set_bit(KEY_F19, input->keybit);
		set_bit(KEY_WWW, input->keybit);
		break;
	case 0xfb14:	/* S20 */
		ms_map_key_clear(KEY_F14);
		set_bit(KEY_F20, input->keybit);
		set_bit(KEY_MAIL, input->keybit);
		break;
	case 0xfb15:	/* S21 */
		ms_map_key_clear(KEY_F15);
		set_bit(KEY_F21, input->keybit);
		set_bit(KEY_PROG1, input->keybit);
		break;
	case 0xfb16:	/* S22 */
		ms_map_key_clear(KEY_F16);
		set_bit(KEY_F22, input->keybit);
		set_bit(KEY_PROG2, input->keybit);
		break;
	case 0xfb17:	/* S23 */
		ms_map_key_clear(KEY_F17);
		set_bit(KEY_F23, input->keybit);
		set_bit(KEY_PROG3, input->keybit);
		break;
	case 0xfb18:	/* S24 */
		ms_map_key_clear(KEY_F18);
		set_bit(KEY_F24, input->keybit);
		set_bit(KEY_PROG4, input->keybit);
		break;
	case 0xfb19:	/* S25 */
		ms_map_key_clear(KEY_F13);
		set_bit(KEY_F19, input->keybit);
		set_bit(KEY_WWW, input->keybit);
		break;
	case 0xfb1a:	/* S26 */
		ms_map_key_clear(KEY_F14);
		set_bit(KEY_F20, input->keybit);
		set_bit(KEY_MAIL, input->keybit);
		break;
	case 0xfb1b:	/* S27 */
		ms_map_key_clear(KEY_F15);
		set_bit(KEY_F21, input->keybit);
		set_bit(KEY_PROG1, input->keybit);
		break;
	case 0xfb1c:	/* S28 */
		ms_map_key_clear(KEY_F16);
		set_bit(KEY_F22, input->keybit);
		set_bit(KEY_PROG2, input->keybit);
		break;
	case 0xfb1d:	/* S29 */
		ms_map_key_clear(KEY_F17);
		set_bit(KEY_F23, input->keybit);
		set_bit(KEY_PROG3, input->keybit);
		break;
	case 0xfb1e:	/* S30 */
		ms_map_key_clear(KEY_F18);
		set_bit(KEY_F24, input->keybit);
		set_bit(KEY_PROG4, input->keybit);
		break;
	case 0xfd11: ms_map_key_clear(KEY_UNKNOWN);	break;	/* X6 only: Macro Pad toggle key*/
	case 0xfd12: ms_map_key_clear(KEY_MACRO);	break;	/* Macro Record key */
	case 0xfd15: ms_map_key_clear(KEY_UNKNOWN);	break;	/* Profile switch key */
	default:
		return 0;
	}
	return 1;
}
#undef ms_map_key_clear

static int ms_sidewinder_control(struct hid_device *hdev, __u8 setup)
{
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	struct hid_report *report =
			hdev->report_enum[HID_FEATURE_REPORT].report_id_hash[7];

	/*
	 * LEDs 1 - 3 should not be set simultaneously, however
	 * they can be set in any combination with Auto or Record LEDs.
	 */
	report->field[0]->value[0] = (setup & 0x01) ? 0x01 : 0x00;	/* X6 only: Macro Pad toggle */
	report->field[0]->value[1] = (setup & 0x02) ? 0x01 : 0x00;	/* LED Auto */
	report->field[0]->value[2] = (setup & 0x04) ? 0x01 : 0x00;	/* LED 1 */
	report->field[0]->value[3] = (setup & 0x08) ? 0x01 : 0x00;	/* LED 2 */
	report->field[0]->value[4] = (setup & 0x10) ? 0x01 : 0x00;	/* LED 3 */

	if (setup & 0x40)
		report->field[1]->value[0] = 0x03;	/* Record LED Solid */
	else if (setup & 0x20)
		report->field[1]->value[0] = 0x02;	/* Record LED Blink */
	else
		report->field[1]->value[0] = 0x00;

	if (sidewinder->status != setup) {
		hid_hw_request(hdev, report, HID_REQ_SET_REPORT);
		sidewinder->status = setup;
	}

	return 0;
}

/*
 * Sidewinder sysfs
 * @profile: show and set profile count and LED status
 * @auto_led: show and set LED Auto
 * @record_led: show and set Record LED
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
	__u8 leds = sidewinder->status & ~(0x1c);	/* Clear Profile LEDs */

	if (sscanf(buf, "%1u", &sidewinder->profile) != 1)
		return -EINVAL;

	if (sidewinder->profile >= 1 && sidewinder->profile <= 3) {
		leds |= 0x02 << sidewinder->profile;
		ms_sidewinder_control(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else
		return -EINVAL;
}

static struct device_attribute dev_attr_ms_sidewinder_profile =
	__ATTR(profile, S_IWUSR | S_IRUGO,
		ms_sidewinder_profile_show,
		ms_sidewinder_profile_store);

static ssize_t ms_sidewinder_record_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	int record_led;

	if (sidewinder->status & 0x40)
		record_led = 2;
	else if (sidewinder->status & 0x20)
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

	if (record_led == 0) {
		leds = sidewinder->status & ~(0xe0);	/* Clear Record LED */
		ms_sidewinder_control(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else if (record_led == 1 || record_led == 2) {
		leds = sidewinder->status & ~(0xe0);	/* Clear Record LED */
		leds |= 0x10 << record_led;
		ms_sidewinder_control(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else
		return -EINVAL;
}

static struct device_attribute dev_attr_ms_sidewinder_record =
	__ATTR(record_led, S_IWUSR | S_IRUGO,
		ms_sidewinder_record_show,
		ms_sidewinder_record_store);

static ssize_t ms_sidewinder_auto_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct hid_device *hdev = container_of(dev, struct hid_device, dev);
	struct ms_data *sc = hid_get_drvdata(hdev);
	struct ms_sidewinder_extra *sidewinder = sc->extra;
	int auto_led;

	if (sidewinder->status & 0x02)	/* Check if Auto LED bit is set */
		auto_led = 1;
	else
		auto_led = 0;

	return snprintf(buf, PAGE_SIZE, "%1d\n", auto_led);
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
		leds = sidewinder->status & ~(0x02);	/* Clear Auto LED */
		if (auto_led == 1)
			leds |= 0x02;
		ms_sidewinder_control(hdev, leds);
		return strnlen(buf, PAGE_SIZE);
	} else
		return -EINVAL;
}

static struct device_attribute dev_attr_ms_sidewinder_auto =
	__ATTR(auto_led, S_IWUSR | S_IRUGO,
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
		ms_sidewinder_control(hdev, 0x02 << sidewinder->profile);
	}
}

#define ms_input_event(c)	input_event(input, usage->type, (c), value)
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

	/*
	 * Sidewinder special button handling & profile switching
	 *
	 * Send out different keycodes for S1 - S6 keys in combination with
	 * any of the 3 profiles. This is shared between Sidewinder X4 and
	 * X6. X6 only: setting up different keycodes for S1 - S30 in
	 * profiles 1 - 3 seems unnecessary, but possible. That would
	 * result in 90 extra keys.
	 */
	if (sc->quirks & MS_SIDEWINDER) {
		struct input_dev *input = field->hidinput->input;
		struct ms_sidewinder_extra *sidewinder = sc->extra;

		switch (usage->hid & HID_USAGE) {
		case 0xfb01: /* S1 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F13);	break;
			case 2: ms_input_event(KEY_F19);	break;
			case 3: ms_input_event(KEY_WWW);	break;
			}
			break;
		case 0xfb02: /* S2 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F14);	break;
			case 2: ms_input_event(KEY_F20);	break;
			case 3: ms_input_event(KEY_MAIL);	break;
			}
			break;
		case 0xfb03: /* S3 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F15);	break;
			case 2: ms_input_event(KEY_F21);	break;
			case 3: ms_input_event(KEY_PROG1);	break;
			}
			break;
		case 0xfb04: /* S4 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F16);	break;
			case 2: ms_input_event(KEY_F22);	break;
			case 3: ms_input_event(KEY_PROG2);	break;
			}
			break;
		case 0xfb05: /* S5 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F17);	break;
			case 2: ms_input_event(KEY_F23);	break;
			case 3: ms_input_event(KEY_PROG3);	break;
			}
			break;
		case 0xfb06: /* S6 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F18);	break;
			case 2: ms_input_event(KEY_F24);	break;
			case 3: ms_input_event(KEY_PROG4);	break;
			}
			break;
		case 0xfb07: /* S7 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(BTN_0);	break;
			case 2: ms_input_event(KEY_F19);	break;
			case 3: ms_input_event(KEY_WWW);	break;
			}
			break;
		case 0xfb08: /* S8 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F14);	break;
			case 2: ms_input_event(KEY_F20);	break;
			case 3: ms_input_event(KEY_MAIL);	break;
			}
			break;
		case 0xfb09: /* S9 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F15);	break;
			case 2: ms_input_event(KEY_F21);	break;
			case 3: ms_input_event(KEY_PROG1);	break;
			}
			break;
		case 0xfb0a: /* S10 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F16);	break;
			case 2: ms_input_event(KEY_F22);	break;
			case 3: ms_input_event(KEY_PROG2);	break;
			}
			break;
		case 0xfb0b: /* S11 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F17);	break;
			case 2: ms_input_event(KEY_F23);	break;
			case 3: ms_input_event(KEY_PROG3);	break;
			}
			break;
		case 0xfb0c: /* S12 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F18);	break;
			case 2: ms_input_event(KEY_F24);	break;
			case 3: ms_input_event(KEY_PROG4);	break;
			}
			break;
		case 0xfb0d: /* S13 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F13);	break;
			case 2: ms_input_event(KEY_F19);	break;
			case 3: ms_input_event(KEY_WWW);	break;
			}
			break;
		case 0xfb0e: /* S14 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F14);	break;
			case 2: ms_input_event(KEY_F20);	break;
			case 3: ms_input_event(KEY_MAIL);	break;
			}
			break;
		case 0xfb0f: /* S15 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F15);	break;
			case 2: ms_input_event(KEY_F21);	break;
			case 3: ms_input_event(KEY_PROG1);	break;
			}
			break;
		case 0xfb10: /* S16 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F16);	break;
			case 2: ms_input_event(KEY_F22);	break;
			case 3: ms_input_event(KEY_PROG2);	break;
			}
			break;
		case 0xfb11: /* S17 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F17);	break;
			case 2: ms_input_event(KEY_F23);	break;
			case 3: ms_input_event(KEY_PROG3);	break;
			}
			break;
		case 0xfb12: /* S18 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F18);	break;
			case 2: ms_input_event(KEY_F24);	break;
			case 3: ms_input_event(KEY_PROG4);	break;
			}
			break;
		case 0xfb13: /* S19 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F13);	break;
			case 2: ms_input_event(KEY_F19);	break;
			case 3: ms_input_event(KEY_WWW);	break;
			}
			break;
		case 0xfb14: /* S20 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F14);	break;
			case 2: ms_input_event(KEY_F20);	break;
			case 3: ms_input_event(KEY_MAIL);	break;
			}
			break;
		case 0xfb15: /* S21 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F15);	break;
			case 2: ms_input_event(KEY_F21);	break;
			case 3: ms_input_event(KEY_PROG1);	break;
			}
			break;
		case 0xfb16: /* S22 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F16);	break;
			case 2: ms_input_event(KEY_F22);	break;
			case 3: ms_input_event(KEY_PROG2);	break;
			}
			break;
		case 0xfb17: /* S23 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F17);	break;
			case 2: ms_input_event(KEY_F23);	break;
			case 3: ms_input_event(KEY_PROG3);	break;
			}
			break;
		case 0xfb18: /* S24 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F18);	break;
			case 2: ms_input_event(KEY_F24);	break;
			case 3: ms_input_event(KEY_PROG4);	break;
			}
			break;
		case 0xfb19: /* S25 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F13);	break;
			case 2: ms_input_event(KEY_F19);	break;
			case 3: ms_input_event(KEY_WWW);	break;
			}
			break;
		case 0xfb1a: /* S26 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F14);	break;
			case 2: ms_input_event(KEY_F20);	break;
			case 3: ms_input_event(KEY_MAIL);	break;
			}
			break;
		case 0xfb1b: /* S27 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F15);	break;
			case 2: ms_input_event(KEY_F21);	break;
			case 3: ms_input_event(KEY_PROG1);	break;
			}
			break;
		case 0xfb1c: /* S28 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F16);	break;
			case 2: ms_input_event(KEY_F22);	break;
			case 3: ms_input_event(KEY_PROG2);	break;
			}
			break;
		case 0xfb1d: /* S29 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F17);	break;
			case 2: ms_input_event(KEY_F23);	break;
			case 3: ms_input_event(KEY_PROG3);	break;
			}
			break;
		case 0xfb1e: /* S30 */
			switch (sidewinder->profile) {
			case 1: ms_input_event(KEY_F18);	break;
			case 2: ms_input_event(KEY_F24);	break;
			case 3: ms_input_event(KEY_PROG4);	break;
			}
			break;
		case 0xfd11:
			if (value) {	/* Run this only once on a keypress */
				__u8 numpad = sidewinder->status ^ (0x01);	/* Toggle Macro Pad */
				ms_sidewinder_control(hdev, numpad);
			}
			break;
		case 0xfd12: ms_input_event(KEY_MACRO);	break;
		case 0xfd15:
			if (value) {	/* Run this only once on a keypress */
				__u8 leds = sidewinder->status & ~(0x1c);	/* Clear Profile LEDs */
				if (sidewinder->profile < 1 || sidewinder->profile >= 3) {
					sidewinder->profile = 1;
				} else
					sidewinder->profile++;

				leds |= 0x02 << sidewinder->profile;	/* Set Profile LEDs */
				ms_sidewinder_control(hdev, leds);
			}
			break;
		}

		return 1;
	}

	return 0;
}
#undef ms_input_event

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

		/* Create sysfs files for the Consumer Control Device only */
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
	{ HID_USB_DEVICE(USB_VENDOR_ID_MICROSOFT, USB_DEVICE_ID_SIDEWINDER_X6),
		.driver_data = MS_SIDEWINDER },
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
