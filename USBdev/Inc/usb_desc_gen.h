/*
 * lightweight USB device stack by gbm
 * usb_desc_gen.h - USB descriptor generator definitions and macros
 * Copyright (c) 2022..2024 gbm
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

#ifndef USB_DESC_GEN_H_
#define USB_DESC_GEN_H_

#include "usb_desc_def.h"
#include "usb_class_cdc.h"
#include "usb_class_prn.h"
#include "usb_class_hid.h"

// language identifier string descriptor structure
struct langid_ {uint8_t bLength, type; uint16_t v;};

// define language identifier string descriptor
#define STRLANGID(n, id) static const struct langid_ n = {sizeof(struct langid_), USB_DESCTYPE_STRING, id}

// define a string descriptor - name, string as L"text"
#define STRINGDESC(n, s) const struct {uint8_t bLength, type; uint16_t str[sizeof(s) / 2 - 1];} \
	n = {sizeof(s), USB_DESCTYPE_STRING, s};
	
// not parenthesized - watch the arguments! ==============================
	
// interface descriptor
#define IFDESC(ifnum, nep, classid, subclass, protocol, sidx) \
{sizeof(struct USBdesc_if_), USB_DESCTYPE_INTERFACE, \
	ifnum, 0, nep, classid, subclass, protocol, sidx}

// endpoint descriptor
#define EPDESC(addr, type, size, interval) \
{sizeof(struct USBdesc_ep_), USB_DESCTYPE_ENDPOINT, \
	addr, type, USB16(size), interval}

#define MSCBOTSCSIDESC(mscif, datainep, dataoutep, sidx) { \
		.mscifdesc = IFDESC(mscif, 2, USB_CLASS_STORAGE, MSC_SCSI_TRANSPARENT_COMMAND_SET, USB_MSC_BULK_ONLY_TRANSPORT, sidx), \
		.mscin = EPDESC(datainep, USB_EPTYPE_BULK, MSC_BOT_EP_SIZE, 0), \
		.mscout = EPDESC(dataoutep, USB_EPTYPE_BULK, MSC_BOT_EP_SIZE, 0) \
	}

// old order: interface, header, call management, ACM functional, union, notif ep
// new order: interface, header, ACM functional, union, call management, notif ep (CDC120 5.3, PSTN120 5.4)
#define CDCVCOMDESC(ctrlif, notifinep, datainep, dataoutep, capabilities) \
	{ \
		.cdccomifdesc = IFDESC(ctrlif, 1, CDC_COMMUNICATION_INTERFACE_CLASS, CDC_ABSTRACT_CONTROL_MODEL, 0, 0), \
		.cdchdrfunc = {sizeof(struct USBdesc_funCDChdr_), CDC_CS_INTERFACE, CDC_HEADER, USB16(CDC_V1_10)}, \
		.cdcacmdesc = {sizeof(struct USBdesc_CDCacm_), CDC_CS_INTERFACE, CDC_ABSTRACT_CONTROL_MANAGEMENT, capabilities}, \
		.cdcudesc = {sizeof(struct USBdesc_union_),	CDC_CS_INTERFACE, CDC_UNION, (ctrlif), (ctrlif) + 1}, \
		.cdccmdesc = {sizeof(struct USBdesc_CDCcm_), CDC_CS_INTERFACE, CDC_CALL_MANAGEMENT, 0x00, (ctrlif) + 1}, \
		.cdcnotif = EPDESC(notifinep, USB_EPTYPE_INT, CDC_INT_EP_SIZE, CDC_INT_POLLING_INTERVAL), \
		.cdcdataclassifdesc = IFDESC(ctrlif + 1, 2, CDC_DATA_INTERFACE_CLASS, 0, 0, 0), \
		.cdcin = EPDESC(datainep, USB_EPTYPE_BULK, CDC_DATA_EP_SIZE, 0), \
		.cdcout = EPDESC(dataoutep, USB_EPTYPE_BULK, CDC_DATA_EP_SIZE, 0) \
	}
	
#define CDCVCOMIAD(ctrlif, sidx) {sizeof(struct USBdesc_IAD_), USB_DESCTYPE_IAD, \
	ctrlif, 2, USB_CLASS_COMMUNICATIONS, CDC_ABSTRACT_CONTROL_MODEL, 0, sidx}

#endif
