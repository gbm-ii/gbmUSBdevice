/* 
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

#ifndef USB_STD_DEF_H_
#define USB_STD_DEF_H_

//========================================================================

enum usbdevstate_ {USB_DEVSTATE_ADDRESSED, USB_DEVSTATE_CONFIGURED};

#define USB_LANGID_US 0x0409

/* bmRequestType.Type */
#define USB_RQTYPE_STANDARD 0u
#define USB_RQTYPE_CLASS 1u
#define USB_RQTYPE_VENDOR 2u
#define USB_RQTYPE_RESERVED 3u

// request recipient encoding
enum usbrqrecipient_ {USB_RQREC_DEVICE, USB_RQREC_INTERFACE, USB_RQREC_ENDPOINT, USB_RQREC_OTHER};

/* USB Standard Request Codes */
enum usbstdrq_ {
	USB_STDRQ_GET_STATUS, 
	USB_STDRQ_CLEAR_FEATURE,
	USB_STDRQ_SET_FEATURE = 3,
	USB_STDRQ_SET_ADDRESS = 5,
	USB_STDRQ_GET_DESCRIPTOR,
	USB_STDRQ_SET_DESCRIPTOR,
	USB_STDRQ_GET_CONFIGURATION = 8,
	USB_STDRQ_SET_CONFIGURATION = 9,
	USB_STDRQ_GET_INTERFACE = 10,
	USB_STDRQ_SET_INTERFACE = 11,
	USB_STDRQ_SYNC_FRAME = 12,
};

/* USB Descriptor Types */
enum usb_desc_type {
	USB_DESCTYPE_DEVICE = 1, USB_DESCTYPE_CONFIGURATION,
	USB_DESCTYPE_STRING, USB_DESCTYPE_INTERFACE,
	USB_DESCTYPE_ENDPOINT, USB_DESCTYPE_DEVICE_QUALIFIER,
	USB_DESCTYPE_OTHER_SPEED_CONFIGURATION,
	USB_DESCTYPE_IFPOWER,
	USB_DESCTYPE_OTG,
	USB_DESCTYPE_DEBUG,
	USB_DESCTYPE_IAD,
	USB_DESCTYPE_BOS = 0x0F,
	USB_DESCTYPE_HID = 0x21,
	USB_DESCTYPE_HIDREPORT,
	USB_DESCTYPE_HIDPHYSICAL,
};

// Feature selectors
#define USB_FEATSEL_ENDPOINT_HALT	0	// endpoint only
#define USB_FEATSEL_DEVICE_REMOTE_WAKEUP	1u	// device only
#define USB_FEATSEL_TEST_MODE	2u	// device only

/* bmAttributes in Configuration Descriptor */
#define USB_CONFIGD_POWERED_MASK                0x40
#define USB_CONFIGD_BUS_POWERED                 0x80
#define USB_CONFIGD_SELF_POWERED                0xC0
#define USB_CONFIGD_REMOTE_WAKEUP               0x20

/* bMaxPower in Configuration Descriptor */
#define USB_CONFIGD_POWER_mA(mA)                ((mA)/2)

/* USB Device Classes */
#define USB_CLASS_RESERVED              0x00
#define USB_CLASS_AUDIO                 0x01
#define USB_CLASS_COMMUNICATIONS        0x02
#define USB_CLASS_HID					0x03
#define USB_CLASS_MONITOR               0x04
#define USB_CLASS_PHYSICAL_INTERFACE    0x05
#define USB_CLASS_POWER                 0x06
#define USB_CLASS_PRINTER               0x07
#define USB_CLASS_STORAGE               0x08
#define USB_CLASS_HUB                   0x09
#define USB_CLASS_MISCELLANEOUS         0xEF
#define USB_CLASS_VENDOR_SPECIFIC       0xFF

#define USB_EPTYPE_CTRL	0
#define USB_EPTYPE_ISO	1
#define USB_EPTYPE_BULK	2
#define USB_EPTYPE_INT	3

#endif
