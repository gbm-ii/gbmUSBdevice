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

#include "usb_log.h"

#ifdef USBLOG
// Log ===================================================================
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

extern uint32_t usbdev_msec;	// defined in usb_app.c
extern volatile uint32_t mstimer;		// incremented by SysTick

__attribute__((section(".ram_noinit1"))) struct logentry_ usblog[USBLOG];
__attribute__((section(".ram_noinit1"))) uint16_t logidx;
__attribute__((section(".ram_noinit1"))) uint32_t magic;

#define MAGICVAL	0x34567890

static void adv_log_ptr(void)
{
	if (magic != MAGICVAL)
	{
		logidx = 0;
		memset(usblog, 0, sizeof usblog);
		magic = MAGICVAL;
	}
	else if (logidx < USBLOG - 1)
	{
		++logidx;
	}
	usblog[logidx].msec = mstimer;
}

void USBlog_storeevt(uint8_t evt)
{
	adv_log_ptr();
	usblog[logidx].evt = evt;
}

void USBlog_storerq(USB_SetupPacket *pkt)
{
	adv_log_ptr();
	usblog[logidx].evt = UE_NONE;
	usblog[logidx].pkt = *pkt;
}

void USBlog_storeresp(enum stp_rsp_ resp, uint8_t resplen)
{
	usblog[logidx].resp |= resp;
	usblog[logidx].resplen = resplen;
}

void USBlog_recordevt(uint8_t ef)
{
	usblog[logidx].resp += ef;
}
#endif
