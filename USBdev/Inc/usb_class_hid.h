/*
 * usb_class_prn.h
 *
 *  Created 02'2024
 *      Author: gbm
 */

#ifndef INC_USB_CLASS_HID_H_
#define INC_USB_CLASS_HID_H_

#define HIDRQ_GET_REPORT	1
#define HIDRQ_GET_IDLE		2
#define HIDRQ_GET_PROTOCOL	3
#define HIDRQ_SET_REPORT	9
#define HIDRQ_SET_IDLE		0x0a
#define HIDRQ_SET_PROTOCOL	0x0b

#define HID_SUBCLASS_BOOTIF	1

#define HID_PROTOCOL_NONE	0
#define HID_PROTOCOL_KB		1
#define HID_PROTOCOL_MOUSE	2

struct hid_services_ {
	void (*SoftReset)(const struct usbdevice_ *usbd);
	void (*UpdateStatus)(const struct usbdevice_ *usbd);
};

struct hid_data_ {
	uint16_t RxLength;
	uint8_t RxData[PRN_DATA_EP_SIZE];
	uint8_t Status;
};

#endif /* INC_USB_CLASS_HID_H_ */
