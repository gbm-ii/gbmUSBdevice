/* 
 * lightweight USB device stack by gbm
 * usb_hw_g0.c - STM32G0, STM32H50x, STM32U53x/54x USB device/host peripheral hardware access
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
#if defined(STM32G0B1xx) \
	|| defined(STM32H503xx) || defined(STM32H533xx) || defined(STM32H563xx) \
	|| defined(STM32U535xx) || defined(STM32U545xx) \
	|| defined(STM32U073xx) || defined(STM32U083xx) || defined(STM32C071xx)

#ifdef STM32G0B1xx
#include "stm32g0xx.h"
#elif defined(STM32H503xx) || defined(STM32H533xx) || defined(STM32H563xx)
#include "stm32h5xx.h"
#elif defined(STM32U535xx) || defined(STM32U545xx)
#include "stm32u5xx.h"
#elif defined(STM32U073xx) || defined(STM32U083xx)
#include "stm32u0xx.h"
#elif defined(STM32C071xx)
#include "stm32c0xx.h"
#else
#error unsupported MCU type
#endif

#include <string.h>	// memset()
#include "usb_dev_config.h"
#include "usb_desc_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"

#ifdef UARTMON
#ifdef STM32H503xx
#define MONUART	USART3
#endif	// H503

void EVTMON(uint8_t a)
{
	while (~MONUART->ISR & USART_ISR_TXE) ;
	MONUART->TDR = a;
}
#else
#define EVTMON(a)
#endif

// In G0/H5/U5 all USB registers incl. PMA must be accessed as 32-bit!
// PMA size: 2048 B
// U0: PMA size 1024 B
typedef volatile uint32_t PMAreg;
typedef volatile uint32_t USBreg;

// endpoint buffer descriptor in packet memory
union USB_BDesc_ {
	uint32_t v;
	struct {
		uint32_t addr:16, count:10, num_block:6;
	};
};

#define CNT_INVALID	1023

struct USB_BufDesc_ {
	volatile union USB_BDesc_ TxAddressCount;
	volatile union USB_BDesc_ RxAddressCount;
};

// software-friendly USB peripheral reg definition
// G0B1: USB peripheral at 0x40005C00, RAM at 0x40009800
// H503: USB peripheral at 0x40016000, RAM at 0x40016400

typedef struct USBh_ {
	union {
		struct {
			USBreg EPR[8];
			USBreg RESERVED[8];
			USBreg CNTR;
			USBreg ISTR;
			USBreg FNR;
			USBreg DADDR;
			USBreg RES_BTABLE;	// not present in G0/H5/U0 - buffer table offset in USB memory
			USBreg LPMCSR;	//
			USBreg BCDR;	//
		};
		uint8_t fill_regblock[USB_PMA_OFFSET];
	};
	union {
		PMAreg PMA[512];
		struct USB_BufDesc_ BUFDESC[USB_NEPPAIRS];
	};
} USBh_TypeDef;	// USBh to make it different from mfg. header file USB definition
//========================================================================
// USB peripheral must be enabled before calling Init
// G0 specific: before enabling USB, set PWR_CR2_USV; no need to setup USB pins

// initialize USB peripheral
static void USBhw_Init(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	
	for (uint8_t i = 0; i < 200; i++)
		usb->CNTR = USB_CNTR_USBRST;	// clear PDWN (should wait 1 us on H5)
    // wait tStartup
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
	usb->CNTR = USB_CNTR_USBRST | USB_CNTR_PDWN;	// set PDWN
}

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

// G0 specific
// get IN endpoint size from USB registers
static uint16_t USBhw_GetInEPSize(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct USB_BufDesc_ *bufdesc = &usb->BUFDESC[epn];

	return bufdesc->RxAddressCount.addr - bufdesc->TxAddressCount.addr;
}

#if 0
static uint16_t USBhw_ReadEPSize(const struct usbdevice_ *usbd, uint8_t epaddr)
{
	uint8_t epn = epaddr & EPNUMMSK;
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct USB_BufDesc_ *bufdesc = &usb->BUFDESC[epn];

	if (epn < usbd->cfg->numeppairs)
	{
		if (epaddr & EP_IS_IN)
			return (bufdesc->RxAddressCount.v - bufdesc->TxAddressCount.v) & 0xffff;
		else
			return GetRxBufSize(bufdesc->RxAddressCount.num_block);
	}
	else
		return 0;
}
#endif
// USB EPR register bit masks
#define USB_EPR_STATTX(a) ((a) << 4)
#define USB_EPR_STATRX(a) ((a) << 12)
// mask for config bits - use when toggling individual flags to avoid changing cfg bits
// change to USB_CHEP_REG_MASK defined in std header
#define USB_EPR_CFG	(USB_CHEP_ADDR | USB_EP_KIND | USB_EP_UTYPE | USB_CHEP_DEVADDR)
#define USB_EPR_FLAGS	(USB_EP_VTTX | USB_EP_VTRX | USB_CHEP_NAK | USB_CHEP_ERRTX | USB_CHEP_ERRRX)

static void SetEPRState(const struct usbdevice_ *usbd, uint8_t epaddr, uint32_t statemask, uint32_t newstate)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
    volatile uint32_t *epr = &usb->EPR[epaddr & EPNUMMSK];
    // keep config bits, write ones to toggle bits in statemask, don't reset w0c flags
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
		SetEPRState(usbd, epaddr, USB_EP_TX_STTX, USB_EPR_STATTX(state));
	else	// Out
		SetEPRState(usbd, epaddr, USB_EP_RX_STRX, USB_EPR_STATRX(state));
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
		return (*epr & USB_CHEP_TX_STTX) == USB_EPR_STATTX(USB_EPSTATE_STALL);
	else	// Out
		return (*epr & USB_CHEP_RX_STRX) == USB_EPR_STATRX(USB_EPSTATE_STALL);
}

static void USBhw_EnableRx(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBhw_SetEPState(usbd, epn & EPNUMMSK, USB_EPSTATE_VALID);
}

static void USBhw_EnableCtlSetup(const struct usbdevice_ *usbd)
{
	//USBhw_SetEPState(usbd, 0, USB_EPSTATE_NAK);
}

// hardware setting for F1/G0/H5 series ep types - different from USB standard encoding!
#define USBHW_EPTYPE_BULK	0
#define USBHW_EPTYPE_CTRL	1
#define USBHW_EPTYPE_ISO	2
#define USBHW_EPTYPE_INT	3
// ordered by USB std
static const uint8_t eptype[] = {USBHW_EPTYPE_CTRL, USBHW_EPTYPE_ISO, USBHW_EPTYPE_BULK, USBHW_EPTYPE_INT};
#define USB_EPR_EPTYPE(a) ((eptype[a]) << 9)

static void reset_in_endpoints(const struct usbdevice_ *usbd)
{
	memset(usbd->inep, 0, sizeof(struct epdata_) * usbd->cfg->numeppairs);
}

// reset request - setup EP0
static void USBhw_Reset(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct USB_BufDesc_ *bufdesc = usb->BUFDESC;
	const struct usbdcfg_ *cfg = usbd->cfg;

	// buffer table starts at 0
    uint16_t addr = 0x40;	// H503 RefMan Fig. 510 cfg->numeppairs * 8;
	// setup and enable EP0
	bufdesc[0].TxAddressCount.v = addr;
	uint8_t ep0size = cfg->devdesc->bMaxPacketSize0;
	addr += ep0size;
	bufdesc[0].RxAddressCount.v = (union USB_BDesc_){.num_block = SetRxNumBlock(ep0size), .addr = addr, .count = CNT_INVALID}.v;
	addr += ep0size;
    usb->ISTR = 0;
    usb->DADDR = USB_DADDR_EF;
    usb->CNTR = USB_CNTR_CTRM | USB_CNTR_RESETM | USB_CNTR_SUSPM | USB_CNTR_SOFM;
	usb->EPR[0] = USB_EPR_EPTYPE(0);// | USB_EPR_STATRX(USB_EPSTATE_VALID) | USB_EPR_STATTX(USB_EPSTATE_NAK);
    uint32_t epstate = USB_EPR_STATRX(USB_EPSTATE_NAK) | USB_EPR_STATTX(USB_EPSTATE_NAK);
	SetEPRState(usbd, 0, USB_EP_RX_STRX | USB_EP_TX_STTX | USB_EP_DTOG_TX | USB_EP_DTOG_RX, epstate);
    reset_in_endpoints(usbd);
}

// convert endpoint size to endpoint buffer size
static inline uint16_t epbufsize(uint16_t s)
{
	return (s + 3) & ~3;
}

// setup and enable app endpoints on set configuration request
static void USBhw_SetCfg(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	USBreg *epr = usb->EPR;
	const struct usbdcfg_ *cfg = usbd->cfg;
    uint16_t addr = 0x40 /*cfg->numeppairs * 8*/ + cfg->devdesc->bMaxPacketSize0 * 2;
	// enable app endpoints
	struct USB_BufDesc_ *bufdesc = usb->BUFDESC;
    for (uint8_t i = 1; i < cfg->numeppairs; i++)
	{
		bufdesc[i].TxAddressCount.v = addr;
    	const struct USBdesc_ep_ *ind = USBdev_GetEPDescriptor(usbd, i | EP_IS_IN);
		uint16_t txsize = ind ? epbufsize(getusb16(&ind->wMaxPacketSize)) : 0;
		addr += txsize;
    	const struct USBdesc_ep_ *outd = USBdev_GetEPDescriptor(usbd, i);
		uint16_t rxsize = outd ? epbufsize(getusb16(&outd->wMaxPacketSize)) : 0;
		// do not remove .v from the line below!
		bufdesc[i].RxAddressCount.v = (union USB_BDesc_){.num_block = SetRxNumBlock(rxsize), .addr = addr, .count = CNT_INVALID}.v;
        addr += rxsize;

//        epr[i] = i | USB_EPR_EPTYPE((ind ? ind->bmAttributes : 0) | (outd ? outd->bmAttributes : 0))
//			| (rxsize && usbd->outep[i].ptr ? USB_EPR_STATRX(USB_EPSTATE_VALID) : USB_EPR_STATRX(USB_EPSTATE_NAK))
//			| USB_EPR_STATTX(USB_EPSTATE_NAK);

        epr[i] = i | USB_EPR_EPTYPE((ind ? ind->bmAttributes : 0) | (outd ? outd->bmAttributes : 0));
        uint32_t epstate = (rxsize && usbd->outep[i].ptr ? USB_EPR_STATRX(USB_EPSTATE_VALID) : USB_EPR_STATRX(USB_EPSTATE_NAK))
			| USB_EPR_STATTX(USB_EPSTATE_NAK);
		SetEPRState(usbd, i, USB_EP_RX_STRX | USB_EP_TX_STTX | USB_EP_DTOG_TX | USB_EP_DTOG_RX, epstate);
	}
    EVTMON('C');
}

// disable app endpoints on set configuration 0 request
static void USBhw_ResetCfg(const struct usbdevice_ *usbd)
{
	const struct usbdcfg_ *cfg = usbd->cfg;
	// disable app endpoints
    for (uint8_t i = 1; i < cfg->numeppairs; i++)
	{
		SetEPRState(usbd, i, USB_EP_RX_STRX | USB_EP_TX_STTX | USB_EP_DTOG_TX | USB_EP_DTOG_RX,
			USB_EPR_STATRX(USB_EPSTATE_NAK) | USB_EPR_STATTX(USB_EPSTATE_NAK));
	}
    reset_in_endpoints(usbd);
}

// write data packet to be sent
static void USBhw_WriteTxData(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	struct epdata_ *epd = &usbd->inep[epn];
	uint16_t epsize = USBhw_GetInEPSize(usbd, epn);
	uint16_t bcount = MIN(epd->count, epsize);
	usb->BUFDESC[epn].TxAddressCount.v = (union USB_BDesc_){.count = bcount, .addr = usb->BUFDESC[epn].TxAddressCount.addr}.v;

	if (bcount)
	{
		epd->count -= bcount;
		volatile uint32_t *dest = &usb->PMA[(usb->BUFDESC[epn].TxAddressCount.v & 0xffff) / 4];
		const uint8_t *src = epd->ptr;
		while (bcount > 3)
		{
			uint32_t v = *src++;
			v |= *src++ << 8;
			v |= *src++ << 16;
			v |= *src++ << 24;
			*dest++ = v;
			bcount -= 4;
		}
		if (bcount)
		{
			uint32_t v = *src++;
			if (--bcount)
			{
				v |= *src++ << 8;
				if (--bcount)
					v |= *src++ << 16;
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
    	// TODO: set EPKIND bit as STATUS_OUT? (need to reset it later, so usb_dev should control it)
        USBhw_SetEPState(usbd, 0, USB_EPSTATE_VALID);
    }
    USBhw_SetEPState(usbd, epn | 0x80, USB_EPSTATE_VALID);
}

// read received data packet
static void USBhw_ReadRxData(const struct usbdevice_ *usbd, uint8_t epn)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	
	// count field in EP descriptor is updated with some delay (H503 errata), so do something else first
	uint8_t *dst = usbd->outep[epn].ptr;
	const uint8_t *src = (const uint8_t *)usb->PMA + usb->BUFDESC[epn].RxAddressCount.addr;
	const volatile uint32_t *srcw = (const volatile uint32_t *)src;
	uint16_t bcount;
	// "wait for descriptor update"
	while ((bcount = usb->BUFDESC[epn].RxAddressCount.count) == CNT_INVALID) ;
	usbd->outep[epn].count = bcount;
	while (bcount)
	{
		uint32_t w = *srcw++;
		*dst++ = w;
		if (--bcount)
		{
			*dst++ = w >> 8;
			if (--bcount)
			{
				*dst++ = w >> 16;
				if (--bcount)
				{
					*dst++ = w >> 24;
					--bcount;
				}
			}
		}
	}
	usb->BUFDESC[epn].RxAddressCount.count = CNT_INVALID;
}

void USBhw_IRQHandler(const struct usbdevice_ *usbd)
{
	USBh_TypeDef *usb = (USBh_TypeDef *)usbd->usb;
	
	uint32_t istr = usb->ISTR & (usb->CNTR | 0xff);
	
    if (istr & USB_ISTR_WKUP)
	{
        //usb->CNTR &= ~USB_CNTR_SUSPRDY;	// cleared by hw
        if (~usb->FNR & USB_FNR_RXDP)
        {
        	// real resume event
            usb->CNTR &= ~USB_CNTR_SUSPEN;
            if (usb->FNR & USB_FNR_RXDM)
            {
            	// resume (not reset)
                // callback...
                if (usbd->Resume_Handler)
                	usbd->Resume_Handler();
                EVTMON('W');
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
        EVTMON('R');
        return;
    }
    if (istr & USB_ISTR_CTR)	// EP traffic interrupt
	{
		uint8_t  epn = usb->ISTR & USB_ISTR_IDN;
		volatile uint32_t *epr = &usb->EPR[epn];
		uint32_t eprv = *epr;
		if (eprv & USB_CHEP_VTTX)	// data sent on In endpoint
		{
			*epr = (eprv & USB_EPR_CFG) | (USB_EPR_FLAGS & ~USB_CHEP_VTTX);	// clear CTR_TX
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
				{
					usb->DADDR = usbd->devdata->setaddress | USB_DADDR_EF;
				    EVTMON('A');
				}
				USBdev_InEPHandler(usbd, epn);
			}
		}
		if (eprv & USB_CHEP_VTRX)	// data received on Out endpoint
		{
			USBhw_ReadRxData(usbd, epn);
			*epr = (eprv & USB_EPR_CFG) | (USB_EPR_FLAGS & ~USB_CHEP_VTRX);		// clear CTR_RX
			USBdev_OutEPHandler(usbd, epn, eprv & USB_EP_SETUP);
		}
	}
    if (istr & USB_ISTR_SUSP)	// suspend
	{
        /* Force low-power mode in the macrocell */
    	usb->CNTR |= USB_CNTR_SUSPEN;

        /* clear ISTR after setting CNTR_FSUSP */
        usb->ISTR = ~USB_ISTR_SUSP;
        usb->CNTR |= USB_CNTR_SUSPRDY | USB_CNTR_WKUPM;

        reset_in_endpoints(usbd);
        // suspend callback should go here
        // callback...
        if (usbd->Suspend_Handler)
        	usbd->Suspend_Handler();
        EVTMON('S');
        return;
    }
    if (istr & USB_ISTR_SOF)
	{
        usb->ISTR = ~USB_ISTR_SOF;
        if (usbd->SOF_Handler)
        	usbd->SOF_Handler();
    }
}
// =======================================================================
const struct USBhw_services_ g0_fs_services = {
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
