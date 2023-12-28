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

//#define USBLOG
#ifdef USBLOG
// Log ===================================================================
enum stp_rsp_ {RSP_NONE, RSP_ERR, RSP_OK, RSP_STATUS};
void USBlog_storerq(USB_SetupPacket *pkt);
void USBlog_storeresp(enum stp_rsp_ resp, uint8_t resplen);
size_t USBlog_get(char *s);
void USBlog_recordevt(uint8_t ef);
#else
#define USBlog_storerq(p)
#define USBlog_storeresp(r,l)
#define USBlog_recordevt(e)
#endif
