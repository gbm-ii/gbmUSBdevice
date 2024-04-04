/*
 * lightweight USB device stack by gbm
 * usb_desc_def.h - USB descriptor layout defs
 * Copyright (c) 2013..2022 gbm
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

#ifndef USB_DESC_DEF_H_
#define USB_DESC_DEF_H_

#include <stdint.h>

// common descriptor header definition for searching within config descriptor
struct USBdesc_header_ {
	uint8_t
		bLength,
		bDescriptorType;
};

// Device descriptor, halfword aligned, 16-bit fields properly aligned by default
// in device descriptor all 16-bit fields are aligned - no need for byte representation
struct USBdesc_device_ {
	uint8_t
		bLength,	// 18
		bDescriptorType;
	uint16_t bcdUSB; // this field is aligned
	uint8_t
		bDeviceClass,
		bDeviceSubClass,
		bDeviceProtocol,
		bMaxPacketSize0;
	uint16_t
		idVendor,
		idProduct,
		bcdDevice;
	uint8_t
		iManufacturer,
		iProduct,
		iSerialNumber,
		bNumConfigurations;
};

struct USBdesc_devicequalifier_ {
	uint8_t
		bLength,	// 10
		bDescriptorType;
	uint16_t	bcdUSB; // aligned! 0x200
	uint8_t
		bDeviceClass,
		bDeviceSubClass,
		bDeviceProtocol,
		bMaxPacketSize,
		bNumConfigurations,
		bReserved;
};

// descriptor components - unaligned -> 16-bit fields represented as structures
// unaligned 16-bit type structure
typedef struct usb16_ {
	uint8_t lo, hi;
} usb16;

static inline uint16_t getusb16(const struct usb16_ *p)
{
	return p->lo | p->hi << 8;
}

#define USB16(a) {((a) & 0xff), ((a) >> 8)}

// Configuration descriptor
struct USBdesc_config_ {
	uint8_t
		bLength,	// 9
		bDescriptorType;
	usb16 wTotalLength;	// - cannot be uint16_t, so that even size is not forced
	uint8_t
		bNumInterfaces,
		bConfigurationValue,
		iConfiguration,
		bmAttributes,
		bMaxPower;
};

// Interface descriptor
struct USBdesc_if_ {
	uint8_t
		bLength,	// 8
		bDescriptorType,
		bInterfaceNumber,
		bAlternateSetting,
		bNumEndpoints,
		bInterfaceClass,
		bInterfaceSubClass,
		bInterfaceProtocol,
		iInterface;
};

// Interface Association Descriptor
struct USBdesc_IAD_ {
	uint8_t
		bLength,	// 8
		bDescriptorType,
		bFirstInterface,
		bInterfaceCount,
		bFunctionClass,
		bFunctionSubClass,
		bFunctionProtocol,
		iFunction;
};

// Endpoint descriptor
struct USBdesc_ep_ {
	uint8_t
		bLength,	// 7
		bDescriptorType,
		bEndpointAddress,
		bmAttributes;
	usb16	wMaxPacketSize;
	uint8_t
		bInterval;
};
// CDC class descriptors =================================================
// Header functional descriptor, CDC120 5.2.3.1
struct USBdesc_funCDChdr_ {
	uint8_t
		bLength,	// 5
		bDescriptorType,
		bDescriptorSubtype;	// Header Func Desc
	usb16	bcdCDC;
};
// Call Management Functional Descriptor, PSTN120 5.3.1
struct USBdesc_CDCcm_ {
	uint8_t
		bLength,	// 5
		bDescriptorType,
		bDescriptorSubtype,	// Header Func Desc */
		bmCapabilities,
		bDataInterface;
};
// Abstract Control Management Functional Descriptor, PSTN120 5.3.2
struct USBdesc_CDCacm_ {
	uint8_t
		bFunctionLength,	// 4
		bDescriptorType,
		bDescriptorSubtype,
		bmCapabilities;
};
// Union Functional Descriptor, 
struct USBdesc_union_ {
	uint8_t
		bFunctionLength,
		bDescriptorType,
		bDescriptorSubtype,
		bMasterInterface,
		bSlaveInterface0;
};
// HID descriptor (follows HID interface desc) ==========================
struct USBdesc_hid_ {
	uint8_t
		bLength,	// 9
		bDescriptorType;
	usb16 bcdHID;	//
	uint8_t bCountryCode,
		bNumDescriptors,
		bHidDescriptorType;
	usb16 wDescriptorLength;
};

//========================================================================
// Function descriptors for configuration descriptor composition

// CDC device (non-composite)
struct cdc_desc_ {
	struct USBdesc_if_ cdccomifdesc;	// CDC comm interface
	struct USBdesc_funCDChdr_ cdchdrfunc;
	struct USBdesc_CDCcm_ cdccmdesc;
	struct USBdesc_CDCacm_ cdcacmdesc;
	struct USBdesc_union_ cdcudesc;
#ifndef	SKIP_CDC_INT_ENDPOINT
	struct USBdesc_ep_ cdcnotif;
#endif
	struct USBdesc_if_ cdcdataclassifdesc;	// CDC data interface
	struct USBdesc_ep_ cdcin;
	struct USBdesc_ep_ cdcout;
};

// CDC device as a member of composite device
struct cdc_single_desc_ {
	struct cdc_desc_ cdcdesc;
};

// CDC device as a member of composite device
struct cdc_comp_desc_ {
	struct USBdesc_IAD_ cdciad;
	struct cdc_desc_ cdcdesc;
};

// MSC device
struct mscdesc_ {
	struct USBdesc_if_ mscifdesc;
	struct USBdesc_ep_ mscin, mscout;
};

// Printer, unidirectional
struct prndesc_ {
	struct USBdesc_if_ prnifdesc;
	struct USBdesc_ep_ prnout;
};

// Printer, bi-directional if
struct prn2desc_ {
	struct USBdesc_if_ prnifdesc;
	struct USBdesc_ep_ prnin;
	struct USBdesc_ep_ prnout;
};

struct hid_ctrlonly_desc_ {
	struct USBdesc_if_ hidifdesc;
	struct USBdesc_hid_ hiddesc;
};

struct hid_inonly_desc_ {
	struct USBdesc_if_ hidifdesc;
	struct USBdesc_hid_ hiddesc;
	struct USBdesc_ep_ hidin;
};

struct hid_inout_desc_ {
	struct USBdesc_if_ hidifdesc;
	struct USBdesc_hid_ hiddesc;
	struct USBdesc_ep_ hidin;
	struct USBdesc_ep_ hidout;
};

//========================================================================
// complete configuration descriptors for simple devices
// dev., single CDC
struct cfgdesc_cdc_ {
	struct USBdesc_config_ cfgdesc;
	struct cdc_single_desc_ cdc[1];
};

// dev., single MSC
struct cfgdesc_msc_ {
	struct USBdesc_config_ cfgdesc;
	struct mscdesc_ msc;
};

// Printer, unidirectional
struct cfgdesc_prn_ {
	struct USBdesc_config_ cfgdesc;
	struct prndesc_ prn;
};
//========================================================================
// Complete configuration descriptors for composite devices

// composite dev., CDC + Printer
struct cfgdesc_cdc_prn_ {
	struct USBdesc_config_ cfgdesc;
	struct cdc_comp_desc_ cdc;
	struct prndesc_ prn;
};

struct cfgdesc_cdc_prn2_ {
	struct USBdesc_config_ cfgdesc;
	struct cdc_comp_desc_ cdc;
	struct prn2desc_ prn;
};

// configurable composite dev., MSC + N*CDC + Printer
struct cfgdesc_msc_ncdc_prn_ {
	struct USBdesc_config_ cfgdesc;
#if USBD_MSC
	struct mscdesc_ msc;
#endif
#if USBD_CDC_CHANNELS
	struct cdc_comp_desc_ cdc[USBD_CDC_CHANNELS];
#endif
#if USBD_PRINTER
	struct prndesc_ prn;
#endif
#if USBD_HID
	struct hid_inonly_desc_ hid;
#endif
};

#endif /* __USB_DESC_H */
