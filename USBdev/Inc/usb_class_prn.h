/*
 * usb_class_prn.h
 *
 *  Created 11'2022
 *      Author: gbm
 */

#ifndef INC_USB_CLASS_PRN_H_
#define INC_USB_CLASS_PRN_H_

#define PRNRQ_GET_DEVICE_ID	0
#define PRNRQ_GET_PORT_STATUS	1
#define PRNRQ_SOFT_RESET	2

extern uint8_t prn_Status;

#define PRN_SUBCLASS_PRINTER	1
#define PRN_PROTOCOL_UNIDIR	1
#define PRN_PROTOCOL_BIDIR	2
#define PRN_PROTOCOL_1284_4	3

#define PRN_STATUS_NOTERROR	(1u << 3)
#define PRN_STATUS_SELECT	(1u << 4)
#define PRN_STATUS_PAPEREMPTY	(1u << 5)

struct prn_services_ {
	void (*SoftReset)(const struct usbdevice_ *usbd);
	void (*UpdateStatus)(const struct usbdevice_ *usbd);
};

struct prn_data_ {
	uint16_t RxLength;
	uint8_t RxData[PRN_DATA_EP_SIZE];
	uint8_t Status;
};

#endif /* INC_USB_CLASS_PRN_H_ */
