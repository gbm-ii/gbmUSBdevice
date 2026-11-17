/* 
 * lightweight USB device stack by gbm
 * usb_log.h - request log for USB device debugging
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

#include <stddef.h>
#include "usb_std_def.h"
#include "usb_dev.h"

#ifdef USBLOG	// define via compiler option
// Log ===================================================================
enum stp_rsp_ : uint8_t {RSP_NONE, RSP_ERR, RSP_OK, RSP_STATUS};

enum usbevt_ : uint8_t {UE_NONE, UE_RESET, UE_SETCFG, UE_SUSPEND, UE_RESUME};

struct logentry_ {
	uint32_t msec;
	USB_SetupPacket pkt;
	enum stp_rsp_ resp;
	uint8_t resplen;
	uint8_t evt, classid;
};

extern struct logentry_ usblog[USBLOG];
extern uint16_t logidx;
extern uint32_t magic;

void USBlog_storeevt(uint8_t evt);
void USBlog_storerq(USB_SetupPacket *pkt);
void USBlog_storeresp(enum stp_rsp_ resp, uint8_t resplen);
void USBlog_storeclassid(uint8_t classid);

//void USBlog_recordevt(uint8_t ef);

#else
#define USBlog_storeevt(p)
#define USBlog_storerq(p)
#define USBlog_storeresp(r,l)
#define USBlog_storeclassid(p)
#define USBlog_recordevt(e)
#endif
