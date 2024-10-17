/* 
 * lightweight USB device stack by gbm
 * usb_hw_l0.c - STM32F0/G4/L0/L4/L5 USB device FS peripheral hardware access
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
#if defined(STM32F042x6) || defined(STM32F072xB) || defined(STM32L073xx) \
	|| defined(STM32L452xx) || defined(STM32L552xx) \
	|| defined(STM32G474xx)

#if defined(STM32F042x6) || defined(STM32F072xB)
#include "stm32f0xx.h"
#endif
#ifdef STM32L073xx
#include "stm32l0xx.h"
#endif
#ifdef STM32L452xx
#include "stm32l4xx.h"
#endif
#ifdef STM32L552xx
#include "stm32l5xx.h"
#endif
#ifdef STM32G474xx
#include "stm32g4xx.h"
#endif

#include <string.h>
#include "usb_dev_config.h"
#include "usb_desc_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"

// verified on STM32F072, L552
// USB registers are 16-bit and may be accessed as 16- or 32-bit
// PMA must be accessed as 16- or 8-bit (not 32!), 2 halfwords per word
// PMA size: L0: 2048 B; F0/G4/L4/L5: 1024 B

typedef volatile uint32_t USBreg;

// endpoint buffer descriptor in packet memory
union rxcount_ {
	struct {
		uint16_t count:10, num_block:6;
	};
	uint16_t v;
};

#define CNT_INVALID	1023

typedef volatile uint16_t PMAreg;

struct USB_BufDesc_ {
	PMAreg TxAddress;
	PMAreg TxCount;
	PMAreg RxAddress;
	volatile union rxcount_ RxCount;
};

// software-friendly USB peripheral reg definition
typedef struct USBh_ {
    USBreg EPR[8];
    USBreg RESERVED[8];
    USBreg CNTR;
    USBreg ISTR;
    USBreg FNR;
    USBreg DADDR;
    USBreg BTABLE;	// buffer table offset in USB memory
    USBreg LPMCSR;
    USBreg BCDR;
	uint8_t fill[1024 - 23 * sizeof(USBreg)];
	// now at base + 1 Ki
	union {
		PMAreg PMA[512];
		struct USB_BufDesc_ BUFDESC[USB_NEPPAIRS];
	} PMA;
} USBh_TypeDef;	// USBh to make it different from possible mfg. additions to header files

// Private functions =====================================================
static inline uint16_t GetRxBufSize(uint8_t block)
{
	return block & 0x20
		? ((block & 0x1f) + 1) * 32
		: (block & 0x1f) * 2;
}

static inline uint8_t SetRxNumBlock(uint16_t rxsize)
{
	return rxsize >= 64
		? 0x20 | (rxsize / 32 - 1)
		: (rxsize / 2);
}

// USB EPR register bit masks
#define USB_EPR_STATTX(a) ((a) << 4)
#define USB_EPR_STATRX(a) ((a) << 12)
// mask for config bits - use when toggling individual flags to avoid changing cfg bits
//#define USB_EPR_CFG	(USB_EPADDR_FIELD | USB_EP_CTR_TX | USB_EP_KIND | USB_EP_T_FIELD | USB_EP_CTR_RX)
#define USB_EPR_CFG	(USB_EPADDR_FIELD | USB_EP_KIND | USB_EP_T_FIELD)
#define USB_EPR_FLAGS	(USB_EP_CTR_TX | USB_EP_CTR_RX)

static void SetEPRState(const struct usbdevice_ *usbd, uint8_t epaddr, uint32_t statemask, uint32_t newstate)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
    volatile uint32_t *epr = &usb->EPR[epaddr & EPNUMMSK];
    *epr = ((*epr & (USB_EPR_CFG | statemask)) ^ newstate) | USB_EPR_FLAGS;
}

// clear data toggle - required by unstall request
static void USBhw_ClrEPToggle(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	SetEPRState(usbd, epaddr, epaddr & EP_IS_IN ? USB_EP_DTOG_TX : USB_EP_DTOG_RX, 0);
}

static void USBhw_SetEPState(const struct usbdevice_ *usbd, uint8_t epaddr, enum usb_epstate_ state)
{
	if (epaddr & EP_IS_IN)	// In
		SetEPRState(usbd, epaddr, USB_EPTX_STAT, USB_EPR_STATTX(state));
	else	// Out
		SetEPRState(usbd, epaddr, USB_EPRX_STAT, USB_EPR_STATRX(state));
}

static void USBhw_SetEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	USBhw_SetEPState(usbd, epaddr, USB_EPSTATE_STALL);
}

static void USBhw_ClrEPStall(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	USBhw_ClrEPToggle(usbd, epaddr);
	USBhw_SetEPState(usbd, epaddr, epaddr & EP_IS_IN ? USB_EPSTATE_NAK : USB_EPSTATE_VALID);	// ...
}

static bool USBhw_IsEPStalled(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
    volatile uint32_t *epr = &usb->EPR[epaddr & EPNUMMSK];
	if (epaddr & EP_IS_IN)	// In
		return (*epr & USB_EPTX_STAT) == USB_EPR_STATTX(USB_EPSTATE_STALL);
	else	// Out
		return (*epr & USB_EPRX_STAT) == USB_EPR_STATRX(USB_EPSTATE_STALL);
}

void USBhw_EnableRx(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBhw_SetEPState(usbd, epn & EPNUMMSK, USB_EPSTATE_VALID);
}

static void USBhw_EnableCtlSetup(const struct usbdevice_ *usbd)
{
	//USBhw_SetEPState(usbd, 0, USB_EPSTATE_STALL);
}

// hardware setting for F1, F0, L0, L4, L5 series ep types - different from USB standard encoding!
#define USBHW_EPTYPE_BULK	0
#define USBHW_EPTYPE_CTRL	1
#define USBHW_EPTYPE_ISO	2
#define USBHW_EPTYPE_INT	3
// ordered by USB std encoding
static const uint8_t eptype[] = {USBHW_EPTYPE_CTRL, USBHW_EPTYPE_ISO, USBHW_EPTYPE_BULK, USBHW_EPTYPE_INT};
// In/Out EP pair shares the same type
#define USB_EPR_EPTYPE(a) ((eptype[a]) << 9)

static void reset_in_endpoints(const struct usbdevice_ *usbd)
{
	memset(usbd->inep, 0, sizeof(struct epdata_) * usbd->cfg->numeppairs);
}

// reset request - setup EP0
static void USBhw_Reset(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct USB_BufDesc_ *bufdesc = usb->PMA.BUFDESC;
	const struct usbdcfg_ *cfg = usbd->cfg;

	// buffer table starts at 0
    uint16_t addr = cfg->numeppairs * 8;
	// setup and enable EP0
	bufdesc[0].TxAddress = addr;
	bufdesc[0].TxCount = 0;
	uint8_t ep0size = cfg->devdesc->bMaxPacketSize0;
	addr += ep0size;
	bufdesc[0].RxAddress = addr;
	bufdesc[0].RxCount.v = (union rxcount_){.num_block = SetRxNumBlock(ep0size), .count = CNT_INVALID}.v;
	addr += ep0size;
    usb->ISTR = 0;
    usb->DADDR = USB_DADDR_EF;
    usb->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM  | USB_CNTR_WKUPM | USB_CNTR_SOFM;
	usb->EPR[0] = USB_EPR_EPTYPE(0);// | USB_EPR_STATRX(USB_EPSTATE_VALID) | USB_EPR_STATTX(USB_EPSTATE_NAK);
    uint32_t epstate = USB_EPR_STATRX(USB_EPSTATE_NAK) | USB_EPR_STATTX(USB_EPSTATE_NAK);
	SetEPRState(usbd, 0, USB_EPRX_STAT | USB_EPTX_STAT | USB_EP_DTOG_TX | USB_EP_DTOG_RX, epstate);
    reset_in_endpoints(usbd);
}

// convert endpoint size to endpoint buffer size
static inline uint16_t epbufsize(uint16_t s)
{
	return (s + 1) & ~1;
}

// setup and enable app endpoints on set configuration request
static void USBhw_SetCfg(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	USBreg *epr = usb->EPR;
	const struct usbdcfg_ *cfg = usbd->cfg;
    uint16_t addr = cfg->numeppairs * 8 + cfg->devdesc->bMaxPacketSize0 * 2;
	// enable app endpoints
	struct USB_BufDesc_ *bufdesc = usb->PMA.BUFDESC;
    for (uint8_t i = 1; i < cfg->numeppairs; i++)
	{
		bufdesc[i].TxAddress = addr;
		bufdesc[i].TxCount = 0;
    	const struct USBdesc_ep_ *ind = USBdev_GetEPDescriptor(usbd, i | EP_IS_IN);
		uint16_t txsize = ind ? epbufsize(getusb16(&ind->wMaxPacketSize)) : 0;
		addr += txsize;
    	const struct USBdesc_ep_ *outd = USBdev_GetEPDescriptor(usbd, i);
		uint16_t rxsize = outd ? epbufsize(getusb16(&outd->wMaxPacketSize)) : 0;
		bufdesc[i].RxAddress = addr;
		bufdesc[i].RxCount.v = (union rxcount_){.num_block = SetRxNumBlock(rxsize), .count = CNT_INVALID}.v;
        addr += rxsize;

//        epr[i] = i | USB_EPR_EPTYPE((ind ? ind->bmAttributes : 0) | (outd ? outd->bmAttributes : 0))
//			| (rxsize && usbd->outep[i].ptr ? USB_EPR_STATRX(USB_EPSTATE_VALID) : USB_EPR_STATRX(USB_EPSTATE_NAK))
//			| USB_EPR_STATTX(USB_EPSTATE_NAK);

        epr[i] = i | USB_EPR_EPTYPE((ind ? ind->bmAttributes : 0) | (outd ? outd->bmAttributes : 0));
        uint32_t epstate = (rxsize && usbd->outep[i].ptr ? USB_EPR_STATRX(USB_EPSTATE_VALID) : USB_EPR_STATRX(USB_EPSTATE_NAK))
			| USB_EPR_STATTX(USB_EPSTATE_NAK);
		SetEPRState(usbd, i, USB_EPRX_STAT | USB_EPTX_STAT | USB_EP_DTOG_TX | USB_EP_DTOG_RX, epstate);
	}
}

// disable app endpoints on set configuration 0 request
void USBhw_ResetCfg(const struct usbdevice_ *usbd)
{
	const struct usbdcfg_ *cfg = usbd->cfg;
	// enable app endpoints
    for (uint8_t i = 1; i < cfg->numeppairs; i++)
	{
		SetEPRState(usbd, i, USB_EPRX_STAT | USB_EPTX_STAT | USB_EP_DTOG_TX | USB_EP_DTOG_RX,
			USB_EPR_STATRX(USB_EPSTATE_NAK) | USB_EPR_STATTX(USB_EPSTATE_NAK));
	}
    reset_in_endpoints(usbd);
}

// Driver functions called from other modules ============================

// USB peripheral must be enabled before in RCC calling Init
// F0/G4: no need to setup USB pins

// initialize USB peripheral
static void USBhw_Init(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	
    usb->CNTR = USB_CNTR_FRES;	// clear PDWN (should wait?)
    usb->CNTR = 0;	// release reset
    usb->DADDR = 0;
    usb->ISTR = 0;
    usb->CNTR = USB_CNTR_RESETM;
    usb->BCDR |= USB_BCDR_DPPU;	// enable DP pull-up
    NVIC_EnableIRQ((IRQn_Type)usbd->cfg->irqn);
}

static void USBhw_DeInit(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;

	NVIC_DisableIRQ((IRQn_Type)usbd->cfg->irqn);
    usb->BCDR &= ~USB_BCDR_DPPU;	// disable DP pull-up
	usb->CNTR = USB_CNTR_FRES | USB_CNTR_PDWN;	// set PDWN
}

// get IN endpoint size from USB registers
static uint16_t USBhw_GetInEPSize(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct USB_BufDesc_ *bufdesc = &usb->PMA.BUFDESC[epn];

	return bufdesc->RxAddress - bufdesc->TxAddress;
}

// write data packet to be sent
static void USBhw_WriteTxData(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct epdata_ *epd = &usbd->inep[epn];
	uint16_t epsize = usbd->hwif->GetInEPSize(usbd, epn);
	uint16_t bcount = MIN(epd->count, epsize);
	usb->PMA.BUFDESC[epn].TxCount = bcount;

	if (bcount)
	{
		epd->count -= bcount;
		volatile uint16_t *dest = &usb->PMA.PMA[(usb->PMA.BUFDESC[epn].TxAddress) / 2];
		const uint8_t *src = epd->ptr;
		while (bcount)
		{
			uint16_t v = *src++;
			if (--bcount)
			{
				v |= *src++ << 8;
				--bcount;
			}
			*dest++ = v;
		}
		epd->ptr = (uint8_t *)src;
	}
}

static void USBhw_StartTx(const struct usbdevice_ *usbd, uint8_t epn)
{
	epn &= EPNUMMSK;
    USBhw_WriteTxData(usbd, epn);
    if (epn == 0 && usbd->inep[0].ptr && usbd->inep[0].count == 0)
    {
    	// last data packet sent over control ep - prepare for status out
    	usbd->devdata->ep0state = USBD_EP0_STATUS_OUT;
        USBhw_SetEPState(usbd, 0, USB_EPSTATE_VALID);
    }
    USBhw_SetEPState(usbd, epn | 0x80, USB_EPSTATE_VALID);
}

// USB ISR and related private functions =================================

// read received data packet
static void USBhw_ReadRxData(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	
	// count field in EP descriptor is updated with some delay (H503 errata), so do something else first
	uint8_t *dst = usbd->outep[epn].ptr;
	const volatile uint8_t *src = (const volatile uint8_t *)usb->PMA.PMA + usb->PMA.BUFDESC[epn].RxAddress;
	uint16_t bcount;
	// "wait for descriptor update"
	while ((bcount = usb->PMA.BUFDESC[epn].RxCount.count) == CNT_INVALID) ;
	usbd->outep[epn].count = bcount;
	//memcpy(usbd->outep[epn].ptr, src, bcount);
	for (uint16_t i = 0; i < bcount; i++)
		*dst++ = *src++;
	usb->PMA.BUFDESC[epn].RxCount.count = CNT_INVALID;
}

static void USBhw_IRQHandler(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	
	uint32_t istr = usb->ISTR & (usb->CNTR | 0xff);
	
    if (istr & USB_ISTR_WKUP)
	{
        if (~usb->FNR & USB_FNR_RXDP)
        {
        	// real resume event
            usb->CNTR &= ~USB_CNTR_LPMODE;
            usb->CNTR &= ~USB_CNTR_FSUSP;
            if (usb->FNR & USB_FNR_RXDM)
            {
            	// resume (not reset)
                // callback...
                if (usbd->Resume_Handler)
                	usbd->Resume_Handler();
            }
        }
        usb->ISTR = ~USB_ISTR_WKUP;
    }
    if (istr & USB_ISTR_RESET) // Reset
	{
        usb->ISTR = ~USB_ISTR_RESET;
        USBhw_Reset(usbd);
        if (usbd->Reset_Handler)
        	usbd->Reset_Handler();
        return;
    }
    if (istr & USB_ISTR_CTR)	// EP traffic interrupt
	{
		uint8_t  epn = usb->ISTR & USB_ISTR_EP_ID;
		volatile uint32_t *epr = &usb->EPR[epn];
		uint32_t eprv = *epr;

		if (eprv & USB_EP_CTR_TX)	// data sent on In endpoint
		{
			*epr = (eprv & USB_EPR_CFG) | (USB_EPR_FLAGS & ~USB_EP_CTR_TX);		// clear CTR_TX
			struct epdata_ *epd = &usbd->inep[epn];

			if (epd->count)	// Continue sending
			{
				//USBlog_recordevt(0x10);
				USBhw_StartTx(usbd, epn);
			}
			else if (epd->sendzlp)	// send a ZLP
			{
				epd->sendzlp = 0;
				USBhw_StartTx(usbd, epn);
			}
			else	// In transfer completed
			{
				if (epn == 0 && usbd->devdata->setaddress)
					usb->DADDR = usbd->devdata->setaddress | USB_DADDR_EF;
				USBdev_InEPHandler(usbd, epn);
			}
		}
		if (eprv & USB_EP_CTR_RX)	// data received on Out endpoint
		{
			USBhw_ReadRxData(usbd, epn);
			*epr = (eprv & USB_EPR_CFG) | (USB_EPR_FLAGS & ~USB_EP_CTR_RX);		// clear CTR_RX
			USBdev_OutEPHandler(usbd, epn, eprv & USB_EP_SETUP);
		}
	}
    if (istr & USB_ISTR_SUSP)	// suspend
	{
        usb->CNTR |= USB_CNTR_FSUSP;
        usb->ISTR = ~USB_ISTR_SUSP;
        usb->CNTR |= USB_CNTR_LPMODE;

        reset_in_endpoints(usbd);

        // callback...
        if (usbd->Suspend_Handler)
        	usbd->Suspend_Handler();
       return;
    }
    if (istr & USB_ISTR_SOF)
	{
        usb->ISTR = ~USB_ISTR_SOF;
        if (usbd->SOF_Handler)
        	usbd->SOF_Handler();
        //usb->CNTR |= USB_CNTR_ESOFM;
    }
}

// =======================================================================
const struct USBhw_services_ l0_fs_services = {
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
