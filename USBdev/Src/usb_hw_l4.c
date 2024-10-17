/* 
 * lightweight USB device stack by gbm
 * usb_hw_l4.c - STM32F4/L4/U5 USB OTG peripheral hardware access
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

// verified on F401, L476, L496, L4R5, U575
#if defined(STM32F401xC) \
	|| defined(STM32L476xx) || defined(STM32L496xx) || defined(STM32L4P5xx) || defined(STM32L4R5xx) \
	|| defined(STM32U575xx) || defined(STM32U585xx) || defined(STM32U5A5xx)

#if defined(STM32L476xx) || defined(STM32L496xx) || defined(STM32L4P5xx) || defined(STM32L4R5xx) \
	|| defined(STM32U575xx) || defined(STM32U585xx) || defined(STM32U5A5xx)
#define NEW_OTG
#endif

#include <string.h>
//#include "usb_dev_config.h"
#include "usb_desc_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"

#define STUPCNT0	(3u << USB_OTG_DOEPTSIZ_STUPCNT_Pos)	// to be used for DOEPTSIZ0

#define FIFO_WORDS	320u	// total FIFO memory size in 32-bit words
// 1024 words for U5 OTG HS

#define USB_OTG_CORE_ID_300A          0x4F54300AU
#define USB_OTG_CORE_ID_310A          0x4F54310AU	// L476
// L4R5: 0x4F54330A

#ifndef USB_OTG_DOEPINT_OTEPSPR
#define USB_OTG_DOEPINT_OTEPSPR                (0x1UL << 5)      /*!< Status Phase Received interrupt */
#endif /* defined USB_OTG_DOEPINT_OTEPSPR */
#ifndef USB_OTG_DOEPINT_NAK
#define USB_OTG_DOEPINT_NAK                    (0x1UL << 13)      /*!< NAK interrupt */
#endif /* defined USB_OTG_DOEPINT_NAK */
#ifndef USB_OTG_DOEPINT_STPKTRX
#define USB_OTG_DOEPINT_STPKTRX                (0x1UL << 15)      /*!< Setup Packet Received interrupt */
#endif /* defined USB_OTG_DOEPINT_STPKTRX */
//        USBx_OUTEP(i)->DOEPINT = 0xFB7FU;

// software-friendly USB peripheral reg definition F4/L4
typedef struct USB_OTG_ {
	USB_OTG_GlobalTypeDef Global;
	uint8_t gfill[USB_OTG_DEVICE_BASE - sizeof(USB_OTG_GlobalTypeDef) - USB_OTG_GLOBAL_BASE];
	USB_OTG_DeviceTypeDef Device;
	uint8_t dfill[USB_OTG_IN_ENDPOINT_BASE - sizeof(USB_OTG_DeviceTypeDef) - USB_OTG_DEVICE_BASE];
	USB_OTG_INEndpointTypeDef InEP[1];
	uint8_t ifill[USB_OTG_OUT_ENDPOINT_BASE - sizeof(USB_OTG_INEndpointTypeDef) - USB_OTG_IN_ENDPOINT_BASE];
	USB_OTG_OUTEndpointTypeDef OutEP[1];
	uint8_t ofill[USB_OTG_FIFO_BASE - sizeof(USB_OTG_OUTEndpointTypeDef) - USB_OTG_OUT_ENDPOINT_BASE];
	volatile uint32_t FIFO[31][0x400];
	volatile uint32_t FIFODBG[0x400];	// F4 FIFO RAM debug access at offset 0x20000
} USB_OTG_TypeDef;	// USBh to make it different from possible mfg. additions to header files

//========================================================================
// USB peripheral must be enabled before calling Init
// G0, L4 specific: before enabling USB, set PWR_CR2_USV

// initialize USB peripheral
static void USBhw_Init(const struct usbdevice_ *usbd)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_GlobalTypeDef *usbg = &usb->Global;

	while ((usbg->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);
	usbg->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;
	while (usbg->GRSTCTL & USB_OTG_GRSTCTL_CSRST) ;
	while ((usbg->GRSTCTL & USB_OTG_GRSTCTL_AHBIDL) == 0U);
//	usbg->GCCFG |= USB_OTG_GCCFG_PWRDWN;	// power up phy
	// L4x6 RefMan 47.16.1
//	usbg->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT_Msk | 6 << USB_OTG_GUSBCFG_TRDT_Pos;
#ifdef RCC_AHB2ENR1_OTGHSPHYEN
	// U5A5 HS
	usbg->GUSBCFG = USB_OTG_GUSBCFG_PHYLPCS | USB_OTG_GUSBCFG_FDMOD | 6 << USB_OTG_GUSBCFG_TRDT_Pos;	// force device mode, set TRDT for > 32 MHz
#else
	usbg->GUSBCFG = USB_OTG_GUSBCFG_PHYSEL | USB_OTG_GUSBCFG_FDMOD | 6 << USB_OTG_GUSBCFG_TRDT_Pos;	// force device mode, set TRDT for > 32 MHz
#endif
	// in ST stack, TRDT is 5

	while (usbg->GINTSTS & USB_OTG_GINTSTS_CMOD); 	// while in host mode
#ifdef USB_OTG_GCCFG_PWRDWN
	usbg->GCCFG |= USB_OTG_GCCFG_PWRDWN;	// power up phy was here
#endif

#ifdef NEW_OTG
    usbg->GOTGCTL |= USB_OTG_GOTGCTL_BVALOEN | USB_OTG_GOTGCTL_BVALOVAL;
#else	// F4
	usbg->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;	// disable VBUS sense
    usbg->GOTGCTL |= USB_OTG_GOTGCTL_BSVLD;
#endif
	usbg->GAHBCFG = USB_OTG_GAHBCFG_GINT;
//	usbg->GAHBCFG = USB_OTG_GAHBCFG_GINT | USB_OTG_GAHBCFG_TXFELVL;

	usbg->GINTSTS = 0xBFFFFFFFU;
	// RM 47.16.3
	USB_OTG_DeviceTypeDef *usbdp = &usb->Device;
#ifdef USB_OTG_DCFG_ERRATIM
	usbdp->DCFG |= USB_OTG_DCFG_ERRATIM;	// added 15.10.2024
#endif
#ifdef RCC_AHB2ENR1_OTGHSPHYEN
	// U5A5
	usbdp->DCFG |= 1u << USB_OTG_DCFG_DSPD_Pos;	// Full speed (PERSCHIVL?)
#else
	usbdp->DCFG |= 3u << USB_OTG_DCFG_DSPD_Pos;	// Full speed (PERSCHIVL?)
#endif

	usbg->GINTMSK = USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM
		| USB_OTG_GINTMSK_USBSUSPM | USB_OTG_GINTMSK_WUIM | USB_OTG_GINTMSK_SOFM;

	usbdp->DCTL = 0;	// clear disconnect

    NVIC_EnableIRQ((IRQn_Type)usbd->cfg->irqn);
}

// deinitialize USB peripheral
static void USBhw_DeInit(const struct usbdevice_ *usbd)
{
    NVIC_DisableIRQ((IRQn_Type)usbd->cfg->irqn);

	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_GlobalTypeDef *usbg = &usb->Global;
	USB_OTG_DeviceTypeDef *usbdp = &usb->Device;

	usbdp->DCTL = USB_OTG_DCTL_SDIS;	// disconnect
#ifdef USB_OTG_GCCFG_PWRDWN
	usbg->GCCFG &= ~USB_OTG_GCCFG_PWRDWN;	// power down
#endif
	usbg->GINTSTS = 0xBFFFFFFFU;
	usbg->GINTMSK = 0;
}

// same for G0, F1, ...?
//void USBhw_Deinit(const struct usbdevice_ *usbd)
//{
//    NVIC_DisableIRQ((IRQn_Type)usbd->cfg->irqn);
//}

// get IN endpoint size from USB registers
static uint16_t USBhw_GetInEPSize(const struct usbdevice_ *usbd, uint8_t epn)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	uint32_t inepsize = usb->InEP[epn].DIEPCTL & USB_OTG_DIEPCTL_MPSIZ_Msk;
	return epn ? inepsize : 64 >> (inepsize & 3);	// EP size = 8
}

// not exactly correct rework needed
static void USBhw_SetEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	if (epaddr & EP_IS_IN)	// In
	{
		usb->InEP[epaddr & EPNUMMSK].DIEPCTL |= USB_OTG_DIEPCTL_USBAEP | USB_OTG_DIEPCTL_STALL;
	}
	else
	{
		usb->OutEP[epaddr & EPNUMMSK].DOEPCTL |= USB_OTG_DOEPCTL_USBAEP | USB_OTG_DOEPCTL_STALL;
	}
}

// clear data toggle - required by unstall request
static void USBhw_ClrEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	if (epaddr & EP_IS_IN)	// In
	{
		volatile uint32_t *epctl = &usb->InEP[epaddr & EPNUMMSK].DIEPCTL;
		*epctl = (*epctl | USB_OTG_DIEPCTL_SD0PID_SEVNFRM) & ~USB_OTG_DIEPCTL_STALL;
	}
	else
	{
		volatile uint32_t *epctl = &usb->OutEP[epaddr & EPNUMMSK].DOEPCTL;
		*epctl = (*epctl | USB_OTG_DOEPCTL_SD0PID_SEVNFRM) & ~USB_OTG_DOEPCTL_STALL;
	}
}

static bool USBhw_IsEPStalled(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	return epaddr & EP_IS_IN
		? usb->InEP[epaddr & EPNUMMSK].DIEPCTL & USB_OTG_DIEPCTL_STALL
		: usb->OutEP[epaddr & EPNUMMSK].DOEPCTL & USB_OTG_DOEPCTL_STALL;
}

/*
 * Endpoint deactivates itself when packet count is decremented to 0 or setup phase is done.
 * Reception of setup packet sets NAK.
 */
static void USBhw_EnableRx(const struct usbdevice_ *usbd, uint8_t epn)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_OUTEndpointTypeDef *outep = &usb->OutEP[epn & EPNUMMSK];
	outep->DOEPTSIZ = epn
		? 1u << USB_OTG_DOEPTSIZ_PKTCNT_Pos | (outep->DOEPCTL & USB_OTG_DOEPCTL_MPSIZ_Msk)	// 1 packet, max. size
		: STUPCNT0 | 1u << USB_OTG_DOEPTSIZ_PKTCNT_Pos
			| usbd->cfg->devdesc->bMaxPacketSize0;	// 1 data packet, get ready for setup as well
	outep->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
}

static void USBhw_EnableCtlSetup(const struct usbdevice_ *usbd)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	//USB_OTG_GlobalTypeDef *usbg = &usb->Global;
	USB_OTG_OUTEndpointTypeDef *outep = usb->OutEP;
#if 1

	//if (usbg->GSNPSID <= USB_OTG_CORE_ID_300A || !(outep->DOEPCTL & USB_OTG_DOEPCTL_EPENA))
		outep->DOEPTSIZ = STUPCNT0 | (3 * 8);	// 3 setup packets
		outep->DOEPCTL |= USB_OTG_DOEPCTL_SNAK;
#endif
}

// EP Type hardware setting for L4 is the same as USB standard encoding

static uint8_t ep0siz_enc(uint8_t s)
{
	uint8_t encsiz = 0;
	while (s < 64 && encsiz < 3)
	{
		s <<= 1;
		++encsiz;
	}
	return encsiz;
}

static void reset_in_endpoints(const struct usbdevice_ *usbd)
{
	memset(usbd->inep, 0, sizeof(struct epdata_) * usbd->cfg->numeppairs);
}

// reset request - setup EP0
// RM 47.16.5
static void USBhw_Reset(const struct usbdevice_ *usbd)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_DeviceTypeDef *usbdp = &usb->Device;
	USB_OTG_OUTEndpointTypeDef *OutEP = usb->OutEP;

    usbdp->DCTL &= ~USB_OTG_DCTL_RWUSIG;	// maybe not needed

	for (uint8_t i = 1; i < USB_NEPPAIRS; i++)
		OutEP[i].DOEPCTL = USB_OTG_DOEPCTL_SNAK;

	usbdp->DAINTMSK = 1u << USB_OTG_DAINTMSK_OEPM_Pos | 1u << USB_OTG_DAINTMSK_IEPM_Pos;
	usbdp->DOEPMSK = USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM;
	usbdp->DIEPMSK = USB_OTG_DIEPMSK_EPDM | USB_OTG_DIEPMSK_XFRCM;

	USB_OTG_GlobalTypeDef *usbg = &usb->Global;
	// setup FIFO
	uint16_t rx_fifo_words = 128;
	usbg->GRXFSIZ = rx_fifo_words;	// words -> 512 bytes
	uint8_t ep0size = usbd->cfg->devdesc->bMaxPacketSize0;
	usbg->DIEPTXF0_HNPTXFSIZ = (ep0size / 4) << USB_OTG_DIEPTXF_INEPTXFD_Pos | rx_fifo_words;
	usbg->GRSTCTL = USB_OTG_GRSTCTL_TXFNUM_4 | USB_OTG_GRSTCTL_TXFFLSH | USB_OTG_GRSTCTL_RXFFLSH;

	OutEP[0].DOEPTSIZ = STUPCNT0 | 1u << USB_OTG_DOEPTSIZ_PKTCNT_Pos | ep0size;	// 3 setup packets
	// moved from EnumDone
	USB_OTG_INEndpointTypeDef *InEP = usb->InEP;
	uint8_t ep0encsize = ep0siz_enc(ep0size);
	InEP[0].DIEPCTL = USB_OTG_DIEPCTL_SNAK | ep0encsize;	// EP size = 64
	OutEP[0].DOEPCTL = USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK | ep0encsize;
	// both ep0 are always active
	// enable ints
	usbg->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_IEPINT | USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_SOFM;
    reset_in_endpoints(usbd);
}

static void USBhw_EnumDone(const struct usbdevice_ *usbd)
{
//	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
//	USB_OTG_GlobalTypeDef *usbg = (USB_OTG_GlobalTypeDef *)usbd->usb;
//	USB_OTG_INEndpointTypeDef *InEP = usb->InEP;
}

// clear all int flags - check & correct!
#define USB_OTG_DOEPINT_ALL (USB_OTG_DOEPINT_NAK \
		| USB_OTG_DOEPINT_STPKTRX \
		| USB_OTG_DOEPINT_OTEPDIS | USB_OTG_DOEPINT_STUP \
		| USB_OTG_DOEPINT_EPDISD | USB_OTG_DOEPINT_XFRC)

// setup and enable app endpoints on set configuration request
static void USBhw_SetCfg(const struct usbdevice_ *usbd)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_GlobalTypeDef *usbg = (USB_OTG_GlobalTypeDef *)usbd->usb;
	USB_OTG_DeviceTypeDef *usbdp = &usb->Device;
	USB_OTG_INEndpointTypeDef *InEP = usb->InEP;
	USB_OTG_OUTEndpointTypeDef *OutEP = usb->OutEP;
	const struct usbdcfg_ *cfg = usbd->cfg;
    uint16_t addr = (usbg->DIEPTXF0_HNPTXFSIZ & USB_OTG_DIEPTXF_INEPTXSA_Msk)
    	+ ((usbg->DIEPTXF0_HNPTXFSIZ & USB_OTG_DIEPTXF_INEPTXFD_Msk) >> USB_OTG_DIEPTXF_INEPTXFD_Pos);

	// enable app endpoints
    for (uint8_t i = 1; i < cfg->numeppairs; i++)
	{
		while (usbg->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH);	// wait for previous flush

    	const struct USBdesc_ep_ *inepdesc = USBdev_GetEPDescriptor(usbd, i | EP_IS_IN);
		uint16_t txsize = inepdesc ? getusb16(&inepdesc->wMaxPacketSize) : 0;
		// convert endpoint size to endpoint buffer size in 32-bit words
		uint16_t fifosize = (txsize + 3) / 4;
		if (fifosize < 16)
			fifosize = 16;
		if (txsize)	// in bytes
		{
			usbg->DIEPTXF[i - 1] = fifosize << USB_OTG_DIEPTXF_INEPTXFD_Pos | addr;	// set also for unused EP
			usbg->GRSTCTL = i << USB_OTG_GRSTCTL_TXFNUM_Pos | USB_OTG_GRSTCTL_TXFFLSH;
			InEP[i].DIEPCTL = USB_OTG_DIEPCTL_SD0PID_SEVNFRM | i << USB_OTG_DIEPCTL_TXFNUM_Pos
				| inepdesc->bmAttributes << USB_OTG_DIEPCTL_EPTYP_Pos
				| USB_OTG_DIEPCTL_SNAK | USB_OTG_DIEPCTL_USBAEP | txsize;
			usbdp->DAINTMSK |= 1u << i << USB_OTG_DAINTMSK_IEPM_Pos;
			addr += fifosize;
		}
		else
			InEP[i].DIEPCTL = USB_OTG_DIEPCTL_SD0PID_SEVNFRM | i << USB_OTG_DIEPCTL_TXFNUM_Pos
				| USBD_EP_TYPE_BULK << USB_OTG_DIEPCTL_EPTYP_Pos
				| USB_OTG_DIEPCTL_EPDIS;

    	const struct USBdesc_ep_ *outepdesc = USBdev_GetEPDescriptor(usbd, i);
		uint16_t rxsize = outepdesc ? getusb16(&outepdesc->wMaxPacketSize) : 0;
		if (rxsize)
		{
			OutEP[i].DOEPCTL = USB_OTG_DOEPCTL_SD0PID_SEVNFRM | outepdesc->bmAttributes << USB_OTG_DOEPCTL_EPTYP_Pos
				| USB_OTG_DOEPCTL_USBAEP | rxsize;
			OutEP[i].DOEPINT = USB_OTG_DOEPINT_ALL;	// clear interrupt flags
			usbdp->DAINTMSK |= 1u << i << USB_OTG_DAINTMSK_OEPM_Pos;
			USBhw_EnableRx(usbd, i);
		}
	}
}

// disable app endpoints on set configuration 0 request
static void USBhw_ResetCfg(const struct usbdevice_ *usbd)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_INEndpointTypeDef *InEP = usb->InEP;
	USB_OTG_OUTEndpointTypeDef *OutEP = usb->OutEP;
	const struct usbdcfg_ *cfg = usbd->cfg;
	// enable app endpoints
    for (uint8_t i = 1; i < cfg->numeppairs; i++)
	{
    	// Fix!!! - correct DIEPCTL, DOEPCTL values
    	InEP[i].DIEPCTL |= USB_OTG_DIEPCTL_SNAK;
    	OutEP[i].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_SNAK;
	}
    reset_in_endpoints(usbd);
}

static void USBhw_WriteTxFIFO(const struct usbdevice_ *usbd, uint8_t epn, uint16_t bcount)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;

	if (bcount)
	{
		struct epdata_ *epd = &usbd->inep[epn];
		const uint8_t *src = epd->ptr;
		volatile uint32_t *dest = usb->FIFO[epn];
		epd->count -= bcount;
		while (bcount)
		{
			uint32_t v = *src++;
			if (--bcount)
			{
				v |= *src++ << 8;
				if (--bcount)
				{
					v |= *src++ << 16;
					if (--bcount)
					{
						v |= *src++ << 24;
						--bcount;
					}
				}
			}
			*dest = v;
		}
		epd->ptr = (uint8_t *)src;	// update src pointer
	}
}

static void USBhw_StartTx(const struct usbdevice_ *usbd, uint8_t epn)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	epn &= EPNUMMSK;
	struct epdata_ *epd = &usbd->inep[epn];
	uint16_t epsize = USBhw_GetInEPSize(usbd, epn);
	uint16_t bcount = MIN(epd->count, epsize);
	//uint16_t bcount = epn ? epd->count : MIN(epd->count, epsize);
	//uint16_t npackets = (bcount + epsize - 1) / epsize;
	USB_OTG_INEndpointTypeDef *inep = &usb->InEP[epn];
	if (~inep->DIEPCTL & USB_OTG_DIEPCTL_EPENA && inep->DTXFSTS >= (bcount + 3) / 4)
	{
	    if (epn == 0)
	    {
	    	if (usbd->inep[0].ptr == 0)
	    	{
	    		// status in, prepare for setup Out
	    		if (usbd->devdata->setaddress)
	    		{
	    			// OTG: set address before status In
	    			USB_OTG_DeviceTypeDef *usbdp = &usb->Device;
	    			usbdp->DCFG |= usbd->devdata->setaddress << USB_OTG_DCFG_DAD_Pos;
	    		}
	    		usbd->devdata->ep0state = USBD_EP0_IDLE;
	    		USBhw_EnableCtlSetup(usbd);	// the only call
	    	}
	    	else if (usbd->inep[0].count <= epsize)
	    	{
				// last data packet being sent over control ep - prepare for status out
				usbd->devdata->ep0state = USBD_EP0_STATUS_OUT;
				USBhw_EnableRx(usbd, 0);	// prepare for status out and next setup
	    	}
	    }
		inep->DIEPTSIZ = 1u << USB_OTG_DIEPTSIZ_PKTCNT_Pos | bcount;	// single packet
		inep->DIEPCTL = (inep->DIEPCTL & ~USB_OTG_DIEPCTL_STALL) | USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;
		if (bcount)
		{
#if 1
			USBhw_WriteTxFIFO(usbd, epn, bcount);
#else
			const uint8_t *src = epd->ptr;
			volatile uint32_t *dest = usb->FIFO[epn];
			uint32_t val;
			for (uint16_t i = 0; i < bcount; i++)
			{
				val |= *src++ << ((i & 3) * 8);
				if (((i & 3) == 3) || i == bcount - 1)
				{
					*dest = val;
					val = 0;
				}
			}
			epd->ptr = (uint8_t *)src;
			epd->count -= bcount;
#endif

		}
	//	if (epd->count)
	//		usb->Device.DIEPEMPMSK |= 1u << epn;
	}
}

// read received data packet
static void USBhw_ReadRxData(const struct usbdevice_ *usbd, uint8_t epn, uint16_t bcount)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	volatile uint32_t *fifo = usb->FIFO[0];
	usbd->outep[epn].count = bcount;
	uint8_t *dest = usbd->outep[epn].ptr;

	if (dest)
	{
		// read data
		while (bcount)
		{
			uint32_t data = *fifo;
			*dest++ = data & 0xff;
			if (--bcount)
			{
				*dest++ = (data >> 8) & 0xff;
				if (--bcount)
				{
					*dest++ = (data >> 16) & 0xff;
					if (--bcount)
					{
						*dest++ = data >> 24;
						--bcount;
					}
				}
			}
		}
	}
	else
	{
		// just empty the fifo
		uint32_t wcount = (bcount + 3) / 4;
		while (wcount--)
			*fifo;
	}
}

static void USBhw_IRQHandler(const struct usbdevice_ *usbd)
{
	USB_OTG_TypeDef *usb = (USB_OTG_TypeDef *)usbd->usb;
	USB_OTG_GlobalTypeDef *usbg = &usb->Global;
	
	uint32_t gintsts = usbg->GINTSTS; //ISTR & (usb->CNTR | 0xff);
	
    if (gintsts & USB_OTG_GINTSTS_USBRST) // Reset
	{
        USBhw_Reset(usbd);
        if (usbd->Reset_Handler)
        	usbd->Reset_Handler();
    	usbg->GINTSTS = USB_OTG_GINTSTS_USBRST;
    }
    if (gintsts & USB_OTG_GINTSTS_ENUMDNE) // Reset
	{
        USBhw_EnumDone(usbd);
    	usbg->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;
    }
    if (gintsts & USB_OTG_GINTSTS_SOF)
	{
        if (usbd->SOF_Handler)
        	usbd->SOF_Handler();
    	usbg->GINTSTS = USB_OTG_GINTSTS_SOF;
    }
    if (gintsts & USB_OTG_GINTSTS_USBSUSP)
    {
        reset_in_endpoints(usbd);
        if (usbd->Suspend_Handler)
        	usbd->Suspend_Handler();
    	usbg->GINTSTS = USB_OTG_GINTSTS_USBSUSP;
    }
    if (gintsts & USB_OTG_GINTSTS_WKUINT)
    {
    	usbg->GINTSTS = USB_OTG_GINTSTS_WKUINT;
        if (usbd->Resume_Handler)
        	usbd->Resume_Handler();
    }
    if (gintsts & USB_OTG_GINTSTS_RXFLVL)
    {
    	uint32_t sts = usbg->GRXSTSP;
    	uint8_t epn = (sts & USB_OTG_GRXSTSP_EPNUM_Msk) >> USB_OTG_GRXSTSP_EPNUM_Pos;
    	uint16_t size = (sts & USB_OTG_GRXSTSP_BCNT_Msk) >> USB_OTG_GRXSTSP_BCNT_Pos;
    	switch ((sts & USB_OTG_GRXSTSP_PKTSTS_Msk) >> USB_OTG_GRXSTSP_PKTSTS_Pos)
    	{
    		enum pktsts_ {PKTSTS_GONAK = 1, PKTSTS_OUTREC, PKTSTS_OUTCPLT, PKTSTS_STPCPLT, PKTSTS_STPREC = 6};

    	case PKTSTS_OUTREC:
   			USBhw_ReadRxData(usbd, epn, size);	// sets received size, maybe to 0
    		break;
    	case PKTSTS_STPREC:
   			USBhw_ReadRxData(usbd, epn, size);
    		break;
    	default:
    		;
    	}
    }
    if (gintsts & USB_OTG_GINTSTS_OEPINT)	// Out EP traffic interrupt
	{
    	uint16_t oepint = usb->Device.DAINT >> USB_OTG_DAINTMSK_OEPM_Pos;

    	for (uint8_t epn = 0; epn < usbd->cfg->numeppairs; epn++)
    		if (oepint >> epn & 1)
    		{
    			volatile uint32_t *doepint = &usb->OutEP[epn].DOEPINT;
    			uint32_t doepintv = *doepint;

    			// handle XFRC, STUP, and EP disable
    			if (doepintv & USB_OTG_DOEPINT_XFRC)
    			{
    				*doepint = USB_OTG_DOEPINT_XFRC;
#ifdef NEW_OTG
    				if (usbg->GSNPSID == USB_OTG_CORE_ID_310A)
    				{
    					if (doepintv & USB_OTG_DOEPINT_STPKTRX)
							*doepint = USB_OTG_DOEPINT_STPKTRX;
    					else
    					{
    						if (doepintv & USB_OTG_DOEPINT_OTEPSPR)
    							*doepint = USB_OTG_DOEPINT_OTEPSPR;

        					USBdev_OutEPHandler(usbd, epn, 0);
    					}
    				}
    				else
#endif
    					USBdev_OutEPHandler(usbd, epn, 0);
    			}

    			if (doepintv & USB_OTG_DOEPINT_STUP)
    			{
    				*doepint = USB_OTG_DOEPINT_STUP;
#ifdef NEW_OTG
    				if (usbg->GSNPSID > USB_OTG_CORE_ID_300A && (doepintv & USB_OTG_DOEPINT_STPKTRX))
    					*doepint = USB_OTG_DOEPINT_STPKTRX;
#endif

    				USBdev_OutEPHandler(usbd, epn, 1);
    			}

    			if (doepintv & USB_OTG_DOEPINT_EPDISD)
    			{
    				if (usbg->GINTSTS & USB_OTG_GINTSTS_BOUTNAKEFF)
					{
						usb->Device.DCTL |= USB_OTG_DCTL_CGONAK;
					}
    				*doepint = USB_OTG_DOEPINT_EPDISD;
    			}

    			if (doepintv & USB_OTG_DOEPINT_OTEPSPR)
    			{
    				*doepint = USB_OTG_DOEPINT_OTEPSPR;
    			}
    			if (doepintv & USB_OTG_DOEPINT_NAK)
    			{
    				*doepint = USB_OTG_DOEPINT_NAK;
    			}
    		}
	}
    if (gintsts & USB_OTG_GINTSTS_IEPINT)	// In EP traffic interrupt
	{
    	uint16_t iepint = usb->Device.DAINT;

    	for (uint8_t epn = 0; iepint && epn < usbd->cfg->numeppairs; epn++, iepint >>= 1)
    		if (iepint & 1)
    		{
    			volatile uint32_t *diepint = &usb->InEP[epn].DIEPINT;
    			uint32_t diepintv = *diepint;
    			// handle XFR, TXFE and EP disable ints
    			if (diepintv & USB_OTG_DIEPINT_XFRC)
    			{
    				*diepint = USB_OTG_DIEPINT_XFRC;

					struct epdata_ *epd = &usbd->inep[epn];

					if (epd->count)	// Continue sending
					{
						USBhw_StartTx(usbd, epn);
					}
					else if (epd->sendzlp)	// send a ZLP
					{
						epd->sendzlp = 0;
						USBhw_StartTx(usbd, epn);
					}
					else	// In transfer completed
						USBdev_InEPHandler(usbd, epn);
				}
#if 0
				if ((diepintv & USB_OTG_DIEPINT_TXFE) && (usb->Device.DIEPEMPMSK >> epn & 1))
				{
					USBhw_WriteTxFIFO(usbd, epn);
				}
#endif
    			if (diepintv & USB_OTG_DIEPINT_EPDISD)
    			{
    				while (usbg->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) ;
    				usbg->GRSTCTL = USB_OTG_GRSTCTL_TXFFLSH | epn << USB_OTG_GRSTCTL_TXFNUM_Pos;
    				*diepint = USB_OTG_DIEPINT_EPDISD;
    			}
			}
    }
}
// =======================================================================
const struct USBhw_services_ l4_otgfs_services = {
	.IRQHandler = USBhw_IRQHandler,

	.Init = USBhw_Init,
	.DeInit = USBhw_DeInit,
	.GetInEPSize = USBhw_GetInEPSize,

	.SetCfg = USBhw_SetCfg,
	.ResetCfg = USBhw_ResetCfg,

	.SetEPStall = USBhw_SetEPStall,
	.ClrEPStall = USBhw_ClrEPStall,
	.IsEPStalled = USBhw_IsEPStalled,

	.EnableCtlSetup = USBhw_EnableCtlSetup,
	.EnableRx = USBhw_EnableRx,
	.StartTx = USBhw_StartTx,
};

#endif
