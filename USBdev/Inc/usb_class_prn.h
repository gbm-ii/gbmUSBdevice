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

#endif /* INC_USB_CLASS_PRN_H_ */
