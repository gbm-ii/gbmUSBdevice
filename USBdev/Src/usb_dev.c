/* 
 * lightweight USB device stack by gbm
 * usb_dev.c - hardware-agnostic USB device core
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

#include "usb_dev_config.h"
#include "usb_std_def.h"
#include "usb_desc_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"
#include "usb_log.h"

#ifndef USBLOG
#define USBlog_storerq(p)
#define USBlog_storeresp(r,l)
#define USBlog_recordevt(e)
#endif
//========================================================================
// default callback before clearing Ep stall - override in usb_app.c if needed (for MSC BOT)
 __attribute__ ((weak)) void USBclass_ClearEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
}
// App interface routines ================================================
void USBdev_SetRxBuf(const struct usbdevice_ *usbd, uint8_t epn, uint8_t *buf)
{
	if (epn < usbd->cfg->numeppairs)
		usbd->outep[epn].ptr = buf;
}

bool USBdev_SendData(const struct usbdevice_ *usbd, uint8_t epn, const uint8_t *data, uint16_t length, bool autozlp)
{
	epn &= EPNUMMSK;
	struct epdata_ *epd = &usbd->inep[epn];
	if (epd->busy || (epn && usbd->devdata->devstate != USBD_STATE_CONFIGURED))
		return 1;

	if (!data)
		length = 0;	// send ZLP if nullptr passed
	epd->busy = 1;
	epd->count = length;
	epd->ptr = (uint8_t *)data;
	epd->sendzlp = autozlp && length && length % usbd->hwif->GetInEPSize(usbd, epn) == 0;
	usbd->hwif->StartTx(usbd, epn);
	return 0;
}

void USBdev_SendStatus(const struct usbdevice_ *usbd, const uint8_t *data, uint16_t length, bool zlp)
{
	usbd->devdata->ep0state = USBD_EP0_STATUS_IN;
	USBdev_SendData(usbd, 0, data, length, zlp);
	USBlog_storeresp(RSP_STATUS, length);
}

void USBdev_SendStatusOK(const struct usbdevice_ *usbd)
{
	USBdev_SendStatus(usbd, 0, 0, 0);
}

void USBdev_CtrlError(const struct usbdevice_ *usbd)
{
	// stall both control endpoints
	usbd->devdata->ep0state = USBD_EP0_STALL;
	usbd->hwif->SetEPStall(usbd, EP_IS_IN | 0);
	usbd->hwif->SetEPStall(usbd, 0);
	USBlog_storeresp(RSP_ERR, 0);
}

static void USBdev_GetDescriptor(const struct usbdevice_ *usbd)
{
	const uint8_t *ptr = 0;
	uint16_t size = 0;
	USB_SetupPacket *req = &usbd->devdata->req;
	
	switch (req->wValue.b.h)
	{
	case USB_DESCTYPE_DEVICE:
		ptr = (const uint8_t *)usbd->cfg->devdesc;
		if (req->wLength > *ptr)
		{
			// The 1st read wLength has the size of host control pipe but it should return only one packet.
			size = MIN(*ptr, usbd->cfg->devdesc->bMaxPacketSize0);
			req->wLength = size;	// adjust to suppress automatic ZLP
		}
		break;

	case USB_DESCTYPE_CONFIGURATION:
		ptr = (const uint8_t *)usbd->cfg->cfgdesc;
		size = ptr[2] | ptr[3] << 8;
		break;

	case USB_DESCTYPE_STRING:
		if (req->wValue.b.l < usbd->cfg->nstringdesc)
			ptr = usbd->cfg->strdesc[req->wValue.b.l];
		break;

#if USBD_HID
	case USB_DESCTYPE_HIDREPORT:
		if (req->bmRequestType.Recipient == USB_RQREC_INTERFACE
				&& req->wIndex.w == IFNUM_HID)
		{
			ptr = usbd->cfg->hidrepdesc;
			size = MIN(req->wLength, usbd->cfg->hidrepdescsize);
		}
		break;
#endif

	default:
		break;
	}
	if (ptr)
	{
		if (size == 0)
			size = *ptr;
		USBdev_SendStatus(usbd, ptr, MIN(size, req->wLength), size < req->wLength);
	}
	else
		USBdev_CtrlError(usbd);
}

// Moved to usb_class.c 
void USBclass_HandleRequest(const struct usbdevice_ *usbd);

static void USBdev_HandleRequest(const struct usbdevice_ *usbd)
{
	USB_SetupPacket *req = &usbd->devdata->req;
	
	switch (req->bmRequestType.Type)
	{
	case USB_RQTYPE_STANDARD:
		switch (req->bRequest)
		{
		case USB_STDRQ_SET_ADDRESS:
			usbd->devdata->setaddress = req->wValue.b.l;
			USBdev_SendStatusOK(usbd);
			break;

		case USB_STDRQ_GET_DESCRIPTOR:
			USBdev_GetDescriptor(usbd);
			break;

		case USB_STDRQ_GET_STATUS:
			switch (req->bmRequestType.Recipient)
			{
				static const uint8_t zero[2];
				static uint16_t epstatus;
			case USB_RQREC_DEVICE:	// TODO: should return RWKUP and self-powered status
				USBdev_SendStatus(usbd, (const uint8_t *)&usbd->devdata->status, 2, 0);
				break;
			case USB_RQREC_INTERFACE:
				USBdev_SendStatus(usbd, zero, 2, 0);
				break;
			case USB_RQREC_ENDPOINT:
				epstatus = usbd->hwif->IsEPStalled(usbd, req->wIndex.b.l);
				USBdev_SendStatus(usbd, (const uint8_t *)&epstatus, 2, 0);
				break;
			default:
				USBdev_CtrlError(usbd);	// stall on unhandled requests
			}
			break;

		case USB_STDRQ_GET_CONFIGURATION:
			USBdev_SendStatus(usbd, &usbd->devdata->configuration, 1, 0);
			break;

		case USB_STDRQ_SET_CONFIGURATION:
			switch (req->wValue.w)
			{
			case 0:	// deconfig
				usbd->devdata->configuration = 0;
				usbd->hwif->ResetCfg(usbd);
				USBdev_SendStatusOK(usbd);
				usbd->devdata->devstate = USBD_STATE_ADDRESSED;
				break;
			case 1:	// config
				usbd->devdata->configuration = 1;
				usbd->hwif->SetCfg(usbd);
				USBdev_SendStatusOK(usbd);
				usbd->devdata->devstate = USBD_STATE_CONFIGURED;
				break;
			default:
				USBdev_CtrlError(usbd);	// stall on unhandled requests
			}
			break;

		case USB_STDRQ_CLEAR_FEATURE:
			if (req->bmRequestType.Recipient == USB_RQREC_ENDPOINT && req->wValue.b.l == USB_FEATSEL_ENDPOINT_HALT)
			{
				uint8_t epaddr = req->wIndex.b.l;
				if ((epaddr & EP_IS_IN) && (epaddr & EPNUMMSK) < USBD_NUM_EPPAIRS)
				{
					usbd->inep[epaddr & EPNUMMSK] = (struct epdata_){0, 0, 0, 0};
				}
				USBclass_ClearEPStall(usbd, epaddr);
				usbd->hwif->ClrEPStall(usbd, epaddr);
				USBdev_SendStatusOK(usbd);
			}
			else
				USBdev_CtrlError(usbd);	// stall on unhandled requests
			break;
		case USB_STDRQ_SET_FEATURE:
			if (req->bmRequestType.Recipient == USB_RQREC_ENDPOINT && req->wValue.b.l == USB_FEATSEL_ENDPOINT_HALT)
			{
				usbd->hwif->SetEPStall(usbd, req->wIndex.b.l);
				USBdev_SendStatusOK(usbd);
			}
			else
				USBdev_CtrlError(usbd);	// stall on unhandled requests
			break;
		default:
			USBdev_CtrlError(usbd);	// stall on unhandled requests
		}
		break;
	
	case USB_RQTYPE_CLASS:
		USBclass_HandleRequest(usbd);
		break;
	default:
		USBdev_CtrlError(usbd);// should stall on unhandled requests
	}
}

// data sent on In endpoint
void USBdev_InEPHandler(const struct usbdevice_ *usbd, uint8_t epn)
{
	struct epdata_ *epd = &usbd->inep[epn];
	
	// In transfer completed
	epd->ptr = 0;
	epd->busy = 0;
	if (epn)	// application ep
	{
		if (usbd->cfg->inepcfg[epn].handler)
			usbd->cfg->inepcfg[epn].handler(usbd, epn | EP_IS_IN);
	}
	else	// control ep - end of data/status in phase
	{
		if (usbd->devdata->setaddress)
		{
			USBlog_recordevt(0x40);
			usbd->devdata->setaddress = 0;
			usbd->devdata->devstate = USBD_STATE_ADDRESSED;
		}

		USBlog_recordevt(0x80);
	}
}

// data received on Out endpoint
void USBdev_OutEPHandler(const struct usbdevice_ *usbd, uint8_t epn, bool setup)
{
	if (epn == 0)	// control endpoint
	{
		if (usbd->outep[0].count)
		{
			if (setup)
			{
				// setup packet received - copy to setup packet buffer
				USB_SetupPacket *req = &usbd->devdata->req;
				*req = *(USB_SetupPacket *)usbd->outep[0].ptr;
				USBlog_storerq(req);
				if (req->bmRequestType.DirIn || req->wLength == 0)
				{
					usbd->devdata->ep0state = USBD_EP0_SETUP;
					// ok for F0, L0, wrong for F4
					//usbd->hwif->SetEPStall(usbd, 0);	// disable ep 0 data out
					USBdev_HandleRequest(usbd);	// handle in request or no-data out request
				}
				else
				{
					// non-zero length data out request
					usbd->devdata->ep0state = USBD_EP0_DATA_OUT;
					// F0 fails with this line enabled, which is probably normal - Status In fails
					//usbd->hwif->SetEPStall(usbd, 0x80);	// disable ep 0 data in
					usbd->hwif->EnableRx(usbd, 0);	// prepare for data out
				}
			}
			else // data received on control EP
			{
				if (usbd->devdata->ep0state == USBD_EP0_DATA_OUT)
					USBdev_HandleRequest(usbd);	// data received
				else	// should not happen - maybe should stall ?
				{
					usbd->devdata->ep0state = USBD_EP0_IDLE;
					//usbd->hwif->EnableCtlSetup(usbd);
				}
			}
		}
		else
		{
			// zero-length status received
			if (usbd->devdata->ep0state == USBD_EP0_STATUS_OUT)
			{
				usbd->devdata->ep0state = USBD_EP0_IDLE;
				//usbd->hwif->EnableCtlSetup(usbd);	// status out completed
			}
		}
	}
	else // data received on application endpoint
	{
		if (epn < usbd->cfg->numeppairs && usbd->cfg->outepcfg[epn].handler)
			usbd->cfg->outepcfg[epn].handler(usbd, epn);
	}
}

// find an endpoint descriptor for the specified endpoint - called from usb_hw_xx.c during configuration
const struct USBdesc_ep_ *USBdev_GetEPDescriptor(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	const struct USBdesc_config_ *cd = usbd->cfg->cfgdesc;
	uint16_t cfgdescsize = getusb16(&cd->wTotalLength);
	const struct USBdesc_ep_ *epd;

	for (uint16_t epdoffset = cd->bLength; epdoffset < cfgdescsize; epdoffset += epd->bLength)
	{
		epd = (const struct USBdesc_ep_ *)((uint8_t *)cd + epdoffset);
		if (epd->bDescriptorType == USB_DESCTYPE_ENDPOINT && epd->bEndpointAddress == epaddr)
			return epd;
	}
	return 0;
}
