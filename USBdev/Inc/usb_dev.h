/* 
 * lightweight USB device stack by gbm
 * usb_dev.h - definitions for USB device core
 * Copyright (c) 2022 gbm
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef USB_DEV_H_
#define USB_DEV_H_

#include <stdint.h>
#include <stdbool.h>
#include "usb_hw.h"

// USB standard definitions ===============================================

enum usbd_ep_type_ {USBD_EP_TYPE_CTRL, USBD_EP_TYPE_ISOC, USBD_EP_TYPE_BULK, USBD_EP_TYPE_INTR};

union wb_ {
	struct {
		uint8_t l, h;
	} b;
	uint16_t w;
};

enum usb_rqtype_ {USBRQTYPE_STD, USBRQTYPE_CLASS, USBRQTYPE_VENDOR};

// change!!!
typedef struct USB_Request_
{
    uint8_t Recipient:5;
    uint8_t Type:2;
    uint8_t DirIn:1;
} USB_RequestType;

typedef struct USB_SetupPacket_
{
    USB_RequestType bmRequestType;
    uint8_t bRequest;
    union wb_ wValue, wIndex;
    uint16_t wLength;
} USB_SetupPacket;

// Device State
enum usbd_dev_state_ {USBD_STATE_DEFAULT = 0, USBD_STATE_ADDRESSED, USBD_STATE_CONFIGURED, USBD_STATE_SUSPENDED};

// gbm USB device definitions ============================================
struct usbdevice_;

// endpoint configuration data - constant
struct epcfg_ {
	uint8_t ifidx;
	void (*handler)(const struct usbdevice_ *usbd, uint8_t epn);
};
// mapping of interfaces to classes and instance idx 
struct ifassoc_ {
	uint8_t classid:4, funidx:4;
};

// device configuration data - constant
struct usbdcfg_ {
	uint8_t irqn;
	uint8_t irqpri;
	uint8_t numeppairs:5;
	uint8_t numif:3;
	uint8_t nstringdesc;
	const struct epcfg_ *outepcfg;
	const struct epcfg_ *inepcfg;
	const struct ifassoc_ *ifassoc;		// interface to function association vector
	const struct USBdesc_device_ *devdesc;
	const struct USBdesc_config_ *cfgdesc;
	const uint8_t * const *strdesc;
	const uint8_t hidrepdescsize;
	const uint8_t * const hidrepdesc;
};

// EP0 State
enum usbd_ep0_state_ {USBD_EP0_IDLE, USBD_EP0_SETUP,
	USBD_EP0_DATA_IN, USBD_EP0_DATA_OUT,
	USBD_EP0_STATUS_IN, USBD_EP0_STATUS_OUT, USBD_EP0_STALL};

// device status & data - variable
struct usbdevdata_ {
	uint8_t devstate;
	uint8_t configuration;
	uint8_t setaddress;
	uint8_t ep0state;
	uint16_t status;	// check in USB doc
	USB_SetupPacket req;
//	USB_SetupPacket ep0outpkt;
};

// endpoint status & data - variable
struct epdata_ {
	uint8_t *ptr;	// current address
	uint16_t count;	// no. of bytes read/left to write
	bool sendzlp;
	bool busy;
};

// device config and state structure - constant with pointers to variables
struct usbdevice_ {
	void *usb;	// USB module address, actually USBh_TypeDef *
	const struct USBhw_services_ *hwif;
	const struct usbdcfg_ *cfg;
	struct usbdevdata_ *devdata;
	struct epdata_ *outep;
	struct epdata_ *inep;
	void (*Reset_Handler)(void);
	void (*Suspend_Handler)(void);
	void (*Resume_Handler)(void);
	void (*SOF_Handler)(void);
#if USBD_CDC_CHANNELS
	const struct cdc_services_ *cdc_service;
	struct cdc_data_ *cdc_data;
#endif
#if USBD_PRINTER
	const struct prn_services_ *prn_service;
	struct prn_data_ *prn_data;
#endif
#ifdef USBD_HID
	const struct hid_services_ *hid_service;
	struct hid_data_ *hid_data;
#endif
};

// called by hw module
const struct USBdesc_ep_ *USBdev_GetEPDescriptor(const struct usbdevice_ *usbd, uint8_t epaddr);
void USBdev_SetupEPHandler(const struct usbdevice_ *usbd, uint8_t epn);
void USBdev_OutEPHandler(const struct usbdevice_ *usbd, uint8_t epn, bool setup);
void USBdev_InEPHandler(const struct usbdevice_ *usbd, uint8_t epn);

// called by usb_class - request handling
void USBdev_SendStatus(const struct usbdevice_ *usbd, const uint8_t *data, uint16_t length, bool zlp);
void USBdev_SendStatusOK(const struct usbdevice_ *usbd);
void USBdev_CtrlError(const struct usbdevice_ *usbd);

// called by app
void USBdev_SetRxBuf(const struct usbdevice_ *usbd, uint8_t epn, uint8_t *buf);
void USBdev_EnableRx(const struct usbdevice_ *usbd, uint8_t epn);
bool USBdev_SendData(const struct usbdevice_ *usbd, uint8_t epn, const uint8_t *data, uint16_t length, bool zlp);

#endif
