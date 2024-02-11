/* 
 * lightweight USB device stack by gbm
 * usb_log.c - request log for USB device debugging
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

#include "usb_std_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"
#include "usb_log.h"

#ifdef USBLOG
// Log ===================================================================
#include <stdio.h>
extern uint32_t usbdev_msec;	// defined in usb_app.c

#define USBLOGSIZE	50

struct logentry_ {
	uint16_t msec;
	USB_SetupPacket pkt;
	enum stp_rsp_ resp;
	uint8_t resplen;
};

extern struct logentry_ usblog[USBLOGSIZE];
extern uint16_t logidx;

struct logentry_ usblog[USBLOGSIZE];
uint16_t logidx;

void USBlog_storerq(USB_SetupPacket *pkt)
{
	if (logidx < USBLOGSIZE - 1)
		++logidx;
	usblog[logidx].msec = usbdev_msec;
	usblog[logidx].pkt = *pkt;
}

void USBlog_storeresp(enum stp_rsp_ resp, uint8_t resplen)
{
	usblog[logidx].resp |= resp;
	usblog[logidx].resplen = resplen;
}

uint16_t logrdidx;
size_t USBlog_get(char *s)
{
	size_t len = sprintf(s, "%5d: %02x %02x %04x %04x %04x, %02x %d\r\n",
		usblog[logrdidx].msec,
		*(uint8_t *)&usblog[logrdidx].pkt.bmRequestType,
		usblog[logrdidx].pkt.bRequest,
		usblog[logrdidx].pkt.wValue.w,
		usblog[logrdidx].pkt.wIndex.w,
		usblog[logrdidx].pkt.wLength,
		usblog[logrdidx].resp,
		usblog[logrdidx].resplen);
	++logrdidx;
	logrdidx %= USBLOGSIZE;
	return len;
}

void USBlog_recordevt(uint8_t ef)
{
	usblog[logidx].resp += ef;
}
#else
#define USBlog_storerq(p)
#define USBlog_storeresp(r,l)
#define USBlog_recordevt(e)
#endif
