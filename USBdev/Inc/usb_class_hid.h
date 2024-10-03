/*
 * usb_class_hid.h
 *
 *  Created 02'2024
 *      Author: gbm
 */

#ifndef INC_USB_CLASS_HID_H_
#define INC_USB_CLASS_HID_H_

#if USBD_HID

#define HIDRQ_GET_REPORT	1
#define HIDRQ_GET_IDLE		2
#define HIDRQ_GET_PROTOCOL	3
#define HIDRQ_SET_REPORT	9
#define HIDRQ_SET_IDLE		0x0a
#define HIDRQ_SET_PROTOCOL	0x0b

#define HID_SUBCLASS_NONE	0
#define HID_SUBCLASS_BOOTIF	1

#define HID_PROTOCOL_NONE	0
#define HID_PROTOCOL_KB		1
#define HID_PROTOCOL_MOUSE	2

#define HID_REPORTTYPE_IN	1
#define HID_REPORTTYPE_OUT	2
#define HID_REPORTTYPE_FEAT	3

enum hid_led_ {HID_LED_NUMLOCK = 1, HID_LED_CAPSLOCK, HID_LED_SCROLLLOCK,
	HID_LED_COMPOSE, HID_LED_KANA, HID_LED_POWER, HID_LED_SHIFT,
	HID_LED_DND, HID_LED_MUTE, HID_LED_TONEENABLE,
	HID_LED_MESSAGEWAITING = 19
};

// status lights report of a standard keyboard
#define HIDKB_MSK_NUMLOCK	(1u << 0)
#define HIDKB_MSK_CAPSLOCK	(HIDKB_MSK_NUMLOCK << 1)
#define HIDKB_MSK_SCROLLLOCK	(HIDKB_MSK_CAPSLOCK << 1)
#define HIDKB_MSK_COMPOSE	(HIDKB_MSK_SCROLLLOCK << 1)
#define HIDKB_MSK_KANA	(HIDKB_MSK_COMPOSE << 1)
#define HIDKB_MSK_POWER	(HIDKB_MSK_KANA << 1)
#define HIDKB_MSK_SHIFT	(HIDKB_MSK_POWER << 1)

enum hidkb_key_ {HIDKB_KEY_A = 4, HIDKB_KEY_B, HIDKB_KEY_C, HIDKB_KEY_D,
	HIDKB_KEY_E, HIDKB_KEY_F, HIDKB_KEY_G, HIDKB_KEY_H,
	HIDKB_KEY_X = 0x1b,
	HIDKB_KEY_Y, HIDKB_KEY_Z,
	HIDKB_KEY_1 = 0x1e,
	HIDKB_KEY_0 = 0x27,
	HIDKB_KEY_F1 = 0x3a,
	HIDKB_KPADSTAR = 0x55, HIDKB_KPADMINUS, HIDKB_KPADPLUS, HIDKB_KPADENTER,
	HIDKB_POWER = 0x66,
	HIDKB_MSLEEP = 0xf8
};

// The current implementation supports only one input and output report

struct hid_services_ {
	bool (*UpdateIn)(const struct usbdevice_ *usbd);	// update state before sending In report, return true if state changed
	void (*UpdateOut)(const struct usbdevice_ *usbd);	// update output controls after Out report received
};

struct hid_data_ {
	uint16_t ReportTimer;
	uint8_t SampleTimer;
	uint8_t Idle;	// idle time parameter for set/get idle request
	uint8_t Protocol;
	bool	InRq;
	uint8_t InReport[HID_IN_REPORT_SIZE];
	uint8_t OutReport[HID_OUT_REPORT_SIZE];
};

#endif // USBD_HID
#endif /* INC_USB_CLASS_HID_H_ */
