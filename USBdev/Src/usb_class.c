/* 
 * lightweight USB device stack by gbm
 * usb_class.c - USB device class requests
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

/*
 * The class module supports single instance of printer and msc class
 * and multiple instances of CDC ACM
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "usb_dev_config.h"
#include "usb_std_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"
#include "usb_class_cdc.h"
#include "usb_class_prn.h"
#include "usb_class_hid.h"

#if USBD_MSC
#include "usb_class_msc_scsi.h"
uint8_t msc_max_lun = 0;
#endif

#if USBD_PRINTER
struct prn_data_ prn_data;

__attribute__ ((weak)) const uint8_t *prn_get_custom_id(void)
{
	return 0;
}
#endif

 // class-specific Clear EP Stall handler called by usb_dev.c
void USBclass_ClearEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
#if USBD_MSC
	uint8_t epn = epaddr & EPNUMMSK;
	uint8_t interface = epaddr & EP_IS_IN ? usbd->cfg->inepcfg[epn].ifidx : usbd->cfg->outepcfg[epn].ifidx;
	uint8_t classid = usbd->cfg->ifassoc[interface].classid;
	if (classid == USB_CLASS_STORAGE)
	{
		msc_bot_reset();
	}
#endif
}

void USBclass_HandleRequest(const struct usbdevice_ *usbd)
{
	USB_SetupPacket *req = &usbd->devdata->req;
	
	switch (req->bmRequestType.Recipient)
	{
	case USB_RQREC_INTERFACE:
		;
		uint8_t interface = req->wIndex.b.l;

		// handle special case of printer request with interface number in wIndex high byte
		if (req->bRequest == PRNRQ_GET_DEVICE_ID && req->wIndex.b.l == 0 && req->wIndex.b.h)
			interface = req->wIndex.b.h;

		if (interface < USBD_NUM_INTERFACES)
		{
			uint8_t classid = usbd->cfg->ifassoc[interface].classid;
			switch (classid)
			{
#if xxUSBD_MSC
			case USB_CLASS_STORAGE:
				switch (req->bRequest)
				{
				case BOTRQ_RESET :
					if (req->wValue.w == 0 && req->wLength == 0 && !req->bmRequestType.DirIn)
					{
						msc_bot_reset();
						USBdev_SendStatusOK(usbd);
					}
					else
						USBdev_CtrlError(usbd);// should stall on unhandled requests
					break;

				case BOTRQ_GET_MAX_LUN :
					if (req->wValue.w == 0 && req->wLength == 1 && req->bmRequestType.DirIn)
					{
//						hmsc->max_lun = USBD_Storage_Interface_fops_FS.GetMaxLun();
						USBdev_SendStatus(usbd, &msc_max_lun, 1, 0);
					}
					else
						USBdev_CtrlError(usbd);
					break;

				default:
					USBdev_CtrlError(usbd);// should stall on unhandled requests
				}
				break;
#endif

#if USBD_CDC_CHANNELS
			case USB_CLASS_COMMUNICATIONS:	// ACM requests are sent via control interface
				;	// get CDC function index (multiple CDC channels support)
				uint8_t funidx = usbd->cfg->ifassoc[interface].funidx;
				switch (req->bRequest)
				{
				case CDCRQ_SET_LINE_CODING:        //0x20
					if (memcmp(&usbd->cdc_data[funidx].LineCoding, usbd->outep[0].ptr, MIN(req->wLength, 7)))
					{
						memcpy(&usbd->cdc_data[funidx].LineCoding, usbd->outep[0].ptr, MIN(req->wLength, 7));
						usbd->cdc_data[funidx].LineCodingChanged = 1;
						if (usbd->cdc_service->SetLineCoding)
							usbd->cdc_service->SetLineCoding(usbd, funidx);
					}
					USBdev_SendStatusOK(usbd);
					break;

				case CDCRQ_GET_LINE_CODING:        //0x21
					USBdev_SendStatus(usbd, (const uint8_t *)&usbd->cdc_data[funidx].LineCoding, MIN(req->wLength, 7), 0);
					break;

				case CDCRQ_SET_CONTROL_LINE_STATE:         //0x22
					if (usbd->cdc_data[funidx].ControlLineState != req->wValue.w)
					{
						usbd->cdc_data[funidx].ControlLineState = req->wValue.w;
						usbd->cdc_data[funidx].ControlLineStateChanged = 1;
						if (usbd->cdc_service->SetControlLineState)
							usbd->cdc_service->SetControlLineState(usbd, funidx);
					}
					USBdev_SendStatusOK(usbd);
					break;
					
				case CDCRQ_SEND_BREAK:	// supported if bmCapabilities bit 2 set
					USBdev_SendStatusOK(usbd);
					break;

				case CDCRQ_SEND_ENCAPSULATED_COMMAND:	// required by CDC120, not used by Windows VCOM
				case CDCRQ_GET_ENCAPSULATED_RESPONSE:	// required by CDC120, not used by Windows VCOM
				default:
					USBdev_CtrlError(usbd);// should stall on unhandled requests
				}
				break;	// USB_CLASS_COMMUNICATIONS
#endif // USBD_CDC_CHANNELS

#if USBD_HID
			case USB_CLASS_HID:	// only single report in+out supported - enough for kb, mouse etc.
				switch (req->bRequest)
				{
				case HIDRQ_GET_REPORT:
					USBdev_SendStatus(usbd,
						req->wValue.b.h == HID_REPORTTYPE_OUT // output report (report id in .l - assume 0)
							? (const uint8_t *)&usbd->hid_data->OutReport
							: (const uint8_t *)&usbd->hid_data->InReport,
						req->wLength, 0);
					break;

				case HIDRQ_GET_IDLE:
					USBdev_SendStatus(usbd, (const uint8_t *)&usbd->hid_data->Idle, 1, 0);
					break;

				case HIDRQ_GET_PROTOCOL:
					USBdev_SendStatus(usbd, (const uint8_t *)&usbd->hid_data->Protocol, 1, 0);
					break;

				case HIDRQ_SET_REPORT:
					if (req->wValue.b.h == HID_REPORTTYPE_OUT)
					{
						memcpy(usbd->hid_data->OutReport, usbd->outep[0].ptr, MIN(req->wLength, HID_OUT_REPORT_SIZE));
						if (usbd->hid_service->UpdateOut)
							usbd->hid_service->UpdateOut(usbd);
					}
					USBdev_SendStatusOK(usbd);
					break;

				case HIDRQ_SET_IDLE:
					usbd->hid_data->Idle = req->wValue.b.h;
					USBdev_SendStatusOK(usbd);
					break;

				case HIDRQ_SET_PROTOCOL:
					usbd->hid_data->Protocol = req->wValue.b.l;
					USBdev_SendStatusOK(usbd);
					break;

				default:
					USBdev_CtrlError(usbd);
				}
				break;
#endif // USBD_HID

#if USBD_PRINTER
			case USB_CLASS_PRINTER:
				// assume only single printer function
				switch (req->bRequest)
				{
				case PRNRQ_GET_DEVICE_ID:
					if (req->wValue.w == 0)
					{
						static const uint8_t prn_DeviceID[91] = {
							"\0\x5b"						// size of string, 2 B, MSB first
							"MFG:Generic;"                  // 12 manufacturer (case sensitive)
							"MDL:Generic_/_Text_Only;"      // 24 model (case sensitive)
							"CMD:1284.4;"                   // 11 PDL command set
							"CLS:PRINTER;"                  // 12 class
							"DES:Generic text only printer;"// 30 description
						};
						const uint8_t *prnId = prn_get_custom_id();
						if (!prnId)
							prnId = prn_DeviceID;
						USBdev_SendStatus(usbd, prnId, MIN(req->wLength, prnId[1]), 0);
					}
					else
						USBdev_CtrlError(usbd);
					break;
				case PRNRQ_GET_PORT_STATUS:
					if (usbd->prn_service->UpdateStatus)
						usbd->prn_service->UpdateStatus(usbd);
					else
						usbd->prn_data->Status = PRN_STATUS_NOTERROR | PRN_STATUS_SELECT;
					USBdev_SendStatus(usbd, &usbd->prn_data->Status, MIN(req->wLength, 1), 0);
					break;
				case PRNRQ_SOFT_RESET:
					// reset
					if (usbd->prn_service->SoftReset)
						usbd->prn_service->SoftReset(usbd);
					USBdev_SendStatusOK(usbd);
					break;
				default:
					USBdev_CtrlError(usbd);
				}
				break;
#endif
#if USBD_MSC
			case USB_CLASS_STORAGE:
				switch (req->bRequest)
				{
#if 1
				case BOTRQ_GET_MAX_LUN :
					if(req->wValue.w == 0 && req->wLength == 1 && req->bmRequestType.DirIn)
						USBdev_SendStatus(usbd, &msc_max_lun, 1, 0);
					else
						USBdev_CtrlError(usbd);
					break;

				case BOTRQ_RESET :
					if(req->wValue.w == 0 && req->wLength == 0 && !req->bmRequestType.DirIn)
					{
						msc_bot_reset();
						USBdev_SendStatusOK(usbd);
					}
					else
						USBdev_CtrlError(usbd);
					break;
#endif
				default:
					USBdev_CtrlError(usbd);
				}
				break;
#endif
			default:
				USBdev_CtrlError(usbd);
			}
		}
		else
			USBdev_CtrlError(usbd);
		break;	//USB_RQREC_INTERFACE
	default:
		USBdev_CtrlError(usbd);
	}
}
