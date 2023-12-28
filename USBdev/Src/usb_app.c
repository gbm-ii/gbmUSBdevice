/* 
 * lightweight USB device stack by gbm
 * usb_app.c - application example
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

#include <string.h>
#include <stdio.h>

#include "usb_hw.h"
#include "usb_dev_config.h"
#include "usb_std_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"
#include "usb_desc_gen.h"
#include "usb_log.h"

#include "usbdev_binding.h"

#define SIGNON_DELAY	50u
#define SIGNON	"\r\nVCOM0 started\r\n"
#define SIGNON1	"\r\nVCOM1 started\r\n"
#define PROMPT	">"

void LED_Toggle(void);	// in main.c
//========================================================================
// VCOM channel data

#if defined(USBD_CDC_CHANNELS) && USBD_CDC_CHANNELS
// define in usb_app.c
static struct cdc_data_ cdc_data[USBD_CDC_CHANNELS] = {
	[0] = {.LineCoding = {.dwDTERate = 115200, .bDataBits = 8}},
#if USBD_CDC_CHANNELS > 1
	[1] = {.LineCoding = {.dwDTERate = 115200, .bDataBits = 8}},
#endif
};
#endif

#if USBD_PRINTER
static uint8_t prnRxData[PRN_DATA_EP_SIZE];	// data buffer
static uint8_t prnRxLen;
#endif

// endpoint data =========================================================
static _Alignas(USB_SetupPacket) uint8_t ep0outpkt[USBD_CTRL_EP_SIZE];	// Control EP Rx buffer

static struct epdata_ out_epdata[USBD_NUM_EPPAIRS] = {
	{.ptr = ep0outpkt, .count = 0},	// control
	{.ptr = 0, .count = 0},	// unused
	{.ptr = cdc_data[0].RxData, .count = 0},
#if USBD_CDC_CHANNELS > 1
	{.ptr = 0, .count = 0},	// unused
	{.ptr = cdc_data[1].RxData, .count = 0},
#endif
#if USBD_PRINTER
	{.ptr = prnRxData, .count = 0},
#endif
};

static struct epdata_ in_epdata[USBD_NUM_EPPAIRS]; // no need to init
//========================================================================

const uint8_t cdc_tx_irqn[USBD_CDC_CHANNELS] = {VCOM0_tx_IRQn,
#if USBD_CDC_CHANNELS > 1
	VCOM1_tx_IRQn
#endif
};

// put character into sendbuf, generate send packet request event
void vcom_putchar(uint8_t ch, char c)
{
	struct cdc_data_ *cdp = &cdc_data[ch];

	while (cdp->TxLength == CDC_DATA_EP_SIZE);	// buffer full -> wait

	__disable_irq();
	cdp->TxData[cdp->TxBuf][cdp->TxLength++] = c;
	__enable_irq();
	NVIC_SetPendingIRQ(cdc_tx_irqn[ch]);
}

void vcom0_putc(uint8_t c)
{
	vcom_putchar(0, c);
}

void vcom0_putstring(const char *s)
{
	while (*s)
		vcom0_putc(*s++);
}

void vcom1_putc(uint8_t c)
{
	vcom_putchar(1, c);
}

void vcom1_putstring(const char *s)
{
	while (*s)
		vcom1_putc(*s++);
}

// return 1 if prompt requested
__attribute__ ((weak)) bool process_input(uint8_t c)
{
	vcom0_putc(c);
	return 0;
}

// forward declaration
const struct usbdevice_ usbdev;

static void allow_rx(uint8_t epn)
{
	__disable_irq();
	usbdev.hwif->EnableRx(&usbdev, epn);
	__enable_irq();
}

#if USBD_PRINTER
void PRN_rx_IRQHandler(void)
{
	if (prnRxLen)
	{
		uint8_t *rxptr = prnRxData; //
		for (uint8_t i = 0; i < prnRxLen; i++)
			process_input(*rxptr++);
		prnRxLen = 0;
		allow_rx(PRN_DATA_OUT_EP);
	}
}
#endif

#if USBD_CDC_CHANNELS
const uint8_t cdc_rx_irqn[USBD_CDC_CHANNELS] = {VCOM0_rx_IRQn
#if USBD_CDC_CHANNELS > 1
		, VCOM1_rx_IRQn
#endif
};

void cdc_LineStateHandler(const struct usbdevice_ *usbd, uint8_t idx)
{
	// overwrites the default handler in usb_class.c
	NVIC_SetPendingIRQ(cdc_rx_irqn[idx]);
}

static uint8_t signon_delay_timer[USBD_CDC_CHANNELS];
static bool signon_rq[USBD_CDC_CHANNELS], prompt_rq;
#endif

// called from SysTick at 1 kHz
void usbdev_timing(void)
{
#if USBD_CDC_CHANNELS
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
		if (signon_delay_timer[ch] && --signon_delay_timer[ch] == 0)
		{
			signon_rq[ch] = 1;
			NVIC_SetPendingIRQ(cdc_rx_irqn[ch]);
		}
#endif
}

// data reception and state change handler, priority lower than USB hw interrupt
void VCOM0_rx_IRQHandler(void)
{
	if (cdc_data[0].LineCodingChanged)
	{
		cdc_data[0].LineCodingChanged = 0;
		// if needed, handle here
	}
	if (cdc_data[0].ControlLineStateChanged)
	{
		if ((cdc_data[0].ControlLineState & (CDC_CTL_DTR | CDC_CTL_RTS)) == (CDC_CTL_DTR | CDC_CTL_RTS))
		{
			signon_delay_timer[0] = SIGNON_DELAY;	// display prompt after 50 ms
			NVIC_EnableIRQ(VCOM0_tx_IRQn);
		}
		else
		{
			//NVIC_DisableIRQ(VCOM0_tx_IRQn);
			// should reset the state
			signon_delay_timer[0] = 0;	// possible hazard - SysTick
			prompt_rq = 0;
		}
		cdc_data[0].ControlLineStateChanged = 0;
	}
	if (cdc_data[0].RxLength)
	{
		uint8_t *rxptr = cdc_data[0].RxData; //
		for (uint8_t i = 0; i < cdc_data[0].RxLength; i++)
			prompt_rq |= process_input(*rxptr++);
		cdc_data[0].RxLength = 0;
		allow_rx(CDC0_DATA_OUT_EP);
	}
	if (signon_rq[0])
	{
		signon_rq[0] = 0;
		vcom0_putstring(SIGNON);
		prompt_rq = 1;
	}
	if (prompt_rq)
	{
		prompt_rq = 0;
		vcom0_putstring(PROMPT);
	}
}

// transmit handler, must have the same priority as USB hw interrupt
void VCOM0_tx_IRQHandler(void)
{
	struct cdc_data_ *cdp = &cdc_data[0];

	if (cdp->TxLength)
	{
		NVIC_DisableIRQ(VCOM0_tx_IRQn);
		USBdev_SendData(&usbdev, CDC0_DATA_IN_EP, cdp->TxData[cdp->TxBuf], cdp->TxLength, 1);
		cdp->TxBuf ^= 1;	// switch buffer
		cdp->TxLength = 0;	// clear counter
	}
}

#if USBD_CDC_CHANNELS > 1
// return 1 if prompt requested
__attribute__ ((weak)) bool process_input1(uint8_t c)
{
	vcom1_putc(c);
	return 0;
}

void VCOM1_rx_IRQHandler(void)
{
	if (cdc_data[1].LineCodingChanged)
	{
		cdc_data[1].LineCodingChanged = 0;
		// if needed, handle here
	}
	if (cdc_data[1].ControlLineStateChanged)
	{
		if ((cdc_data[1].ControlLineState & (CDC_CTL_DTR | CDC_CTL_RTS)) == (CDC_CTL_DTR | CDC_CTL_RTS))
		{
			signon_delay_timer[1] = SIGNON_DELAY;	// display prompt after 50 ms
			NVIC_EnableIRQ(VCOM1_tx_IRQn);
		}
		else
		{
			//NVIC_DisableIRQ(VCOM1_tx_IRQn);
			// should reset the state
			signon_delay_timer[1] = 0;	// possible hazard - SysTick
		}
		cdc_data[1].ControlLineStateChanged = 0;
	}
	if (cdc_data[1].RxLength)
	{
		uint8_t *rxptr = cdc_data[1].RxData; //
		for (uint8_t i = 0; i < cdc_data[1].RxLength; i++)
			prompt_rq |= process_input1(*rxptr++);
		cdc_data[1].RxLength = 0;
		allow_rx(CDC1_DATA_OUT_EP);
	}
	if (signon_rq[1])
	{
		signon_rq[1] = 0;
		vcom1_putstring(SIGNON1);
	}
}

// transmit handler, must have the same priority as USB hw interrupt
void VCOM1_tx_IRQHandler(void)
{
	struct cdc_data_ *cdp = &cdc_data[1];

	if (cdp->TxLength)
	{
		NVIC_DisableIRQ(VCOM1_tx_IRQn);
		USBdev_SendData(&usbdev, CDC1_DATA_IN_EP, cdp->TxData[cdp->TxBuf], cdp->TxLength, 1);
		cdp->TxBuf ^= 1;	// switch buffer
		cdp->TxLength = 0;	// clear counter
	}
}
#endif

// Application routines ==================================================

// called form USB hw interrupt
void DataReceivedHandler(const struct usbdevice_ *usbd, uint8_t epn)
{
	uint16_t length = usbd->outep[epn].count;
	if (length)
	{
#ifdef USBLOG
		if (epn == CDC0_DATA_OUT_EP && *cdc0RxData == 'l')
		{
			char s[80];
			length = USBlog_get(s);
			USBdev_SendData(usbd, CDC0_DATA_IN_EP, (const uint8_t *)s, length, 1);
		}
		else
#endif
		{
			switch (epn)
			{
			case CDC0_DATA_OUT_EP:
				cdc_data[0].RxLength = length;
				NVIC_SetPendingIRQ(VCOM0_rx_IRQn);
				break;
#if USBD_CDC_CHANNELS > 1
			case CDC1_DATA_OUT_EP:
				cdc_data[1].RxLength = length;
				NVIC_SetPendingIRQ(VCOM1_rx_IRQn);
				break;
#endif
#if USBD_PRINTER
			case PRN_DATA_OUT_EP:
				prnRxLen = length;
				NVIC_SetPendingIRQ(PRN_rx_IRQn);
				break;
#endif
			}
		}
	}
	else
		usbd->hwif->EnableRx(usbd, epn);
}

// called form USB hw interrupt
void DataSentHandler(const struct usbdevice_ *usbd, uint8_t epn)
{
	LED_Toggle();
	switch (epn)
	{
	case CDC0_DATA_IN_EP:
		NVIC_EnableIRQ(VCOM0_tx_IRQn);
		break;
#if USBD_CDC_CHANNELS > 1
	case CDC1_DATA_IN_EP:
		NVIC_EnableIRQ(VCOM1_tx_IRQn);
		break;
#endif
	default:

	}
}

// USB descriptors =======================================================
// start with string descriptors since they are referenced in other descriptors
STRLANGID(sdLangID, USB_LANGID_US);
STRINGDESC(sdVendor, u"gbm");
STRINGDESC(sdName, u"Comp");
STRINGDESC(sdSerial, u"0001");
#if USBD_MSC
STRINGDESC(sdMSC, u"MassStorage");
#endif
#if USBD_CDC_CHANNELS
STRINGDESC(sdVcom0, u"VCOM0");
#if USBD_CDC_CHANNELS > 1
STRINGDESC(sdVcom1, u"VCOM1");
#endif
#endif
#if USBD_PRINTER
STRINGDESC(sdPrinter, u"gbmPrinter");
#endif

// string descriptor numbering - must match the order in string desc table
enum usbd_sidx_ {
	USBD_SIDX_LANGID,
	USBD_SIDX_MFG, USBD_SIDX_PRODUCT, USBD_SIDX_SERIALNUM,
#if USBD_MSC
	USBD_SIDX_FUN_MSC,
#endif
#if USBD_CDC_CHANNELS
	USBD_SIDX_FUN_VCOM0,
#if USBD_CDC_CHANNELS > 1
	USBD_SIDX_FUN_VCOM1,
#endif
#endif
#if USBD_PRINTER
	USBD_SIDX_PRINTER,
#endif
	USBD_NSTRINGDESCS	// the last value - must be here
};

// string descriptor table; using reference to .bLength to avoid type cast
static const uint8_t * const strdescv[USBD_NSTRINGDESCS] = {
	&sdLangID.bLength,
	&sdVendor.bLength,
	&sdName.bLength,
	&sdSerial.bLength,
#if USBD_CDC_CHANNELS
	&sdVcom0.bLength,
#if USBD_CDC_CHANNELS > 1
	&sdVcom1.bLength,
#endif
#endif
#if USBD_PRINTER
	&sdPrinter.bLength,
#endif
};
#if 0
// device descriptor for single function CDC ACM
static const struct USBdesc_device_ DevDesc = {
	.bLength = sizeof(struct USBdesc_device_),
	.bDescriptorType = USB_DESCTYPE_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_COMMUNICATIONS,
	.bDeviceSubClass = CDC_ABSTRACT_CONTROL_MODEL,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = USBD_CTRL_EP_SIZE,
	.idVendor = USB_VID,
	.idProduct = USB_PID,
	.bcdDevice = 0x0000,	// device version
	.iManufacturer = USBD_SIDX_MFG,
	.iProduct = USBD_SIDX_PRODUCT,
	.iSerialNumber = USBD_SIDX_SERIALNUM,
	.bNumConfigurations = 1
};

// Configuration descriptor for single function CDC ACM
static const struct cfgdesc_cdc_ ConfigDesc = {
	.cfgdesc = {
		.bLength = sizeof(struct USBdesc_config_),
		.bDescriptorType = USB_DESCTYPE_CONFIGURATION,
		.wTotalLength = USB16(sizeof(ConfigDesc)),
		.bNumInterfaces = USBD_NUM_INTERFACES,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = USB_CONFIGD_BUS_POWERED,
		.bMaxPower = USB_CONFIGD_POWER_mA(100)
	},
	.cdc = CDCVCOMDESC(IFNUM_CDC0_CONTROL, CDC0_INT_IN_EP, CDC0_DATA_IN_EP, CDC0_DATA_OUT_EP)
};
#else
// general composite device
static const struct USBdesc_device_ DevDesc = {
	.bLength = sizeof(struct USBdesc_device_),
	.bDescriptorType = USB_DESCTYPE_DEVICE,
	.bcdUSB = 0x200,
	.bDeviceClass = USB_CLASS_MISCELLANEOUS,
	.bDeviceSubClass = 2,
	.bDeviceProtocol = 1,	// composite device
	.bMaxPacketSize0 = USBD_CTRL_EP_SIZE,
	.idVendor = USB_VID,
	.idProduct = USB_PID,	// was +2 with 8B EP0
	.bcdDevice = 0x0001,	// device version
	.iManufacturer = USBD_SIDX_MFG,
	.iProduct = USBD_SIDX_PRODUCT,
	.iSerialNumber = USBD_SIDX_SERIALNUM,
	.bNumConfigurations = 1
};
static const struct cfgdesc_msc_ncdc_prn_ ConfigDesc = {
	.cfgdesc = {
		.bLength = sizeof(struct USBdesc_config_),
		.bDescriptorType = USB_DESCTYPE_CONFIGURATION,
		.wTotalLength = USB16(sizeof(ConfigDesc)),
		.bNumInterfaces = USBD_NUM_INTERFACES,
		.bConfigurationValue = 1,
		.iConfiguration = 0,
		.bmAttributes = USB_CONFIGD_BUS_POWERED,
		.bMaxPower = USB_CONFIGD_POWER_mA(100)
	},
#if USBD_MSC
	.msc = MSCBOTSCSIDESC(IFNUM_MSC, MSC_BOT_IN_EP, MSC_BOT_OUT_EP, USBD_SIDX_FUN_MSC),
#endif
	.cdc = {
		[0] = {
			.cdciad = CDCVCOMIAD(IFNUM_CDC0_CONTROL, USBD_SIDX_FUN_VCOM0),
			.cdcdesc = CDCVCOMDESC(IFNUM_CDC0_CONTROL, CDC0_INT_IN_EP, CDC0_DATA_IN_EP, CDC0_DATA_OUT_EP)
		},
#if USBD_CDC_CHANNELS > 1
		[1] = {
			.cdciad = CDCVCOMIAD(IFNUM_CDC1_CONTROL, USBD_SIDX_FUN_VCOM1),
			.cdcdesc = CDCVCOMDESC(IFNUM_CDC1_CONTROL, CDC1_INT_IN_EP, CDC1_DATA_IN_EP, CDC1_DATA_OUT_EP)
		},
#endif
	},
#if USBD_PRINTER
	.prn = {
		.prnifdesc = IFDESC(IFNUM_PRN, 1, USB_CLASS_PRINTER, PRN_SUBCLASS_PRINTER, PRN_PROTOCOL_UNIDIR, USBD_SIDX_PRINTER),
//		.prnin = EPDESC(PRN_DATA_IN_EP, USB_EPTYPE_BULK, PRN_DATA_EP_SIZE, 0),
		.prnout = EPDESC(PRN_DATA_OUT_EP, USB_EPTYPE_BULK, PRN_DATA_EP_SIZE, 0)
	},
#endif
};
#endif

// endpoint configuration - constant =====================================
static const struct epcfg_ outcfg[USBD_NUM_EPPAIRS] = {
	{.ifidx = 0, .handler = 0},
#if USBD_MSC
	{.ifidx = IFNUM_MSC, .handler = 0},	// unused
#endif
	{.ifidx = IFNUM_CDC0_CONTROL, .handler = 0},	// unused
	{.ifidx = IFNUM_CDC0_DATA, .handler = DataReceivedHandler},
#if USBD_CDC_CHANNELS > 1
	{.ifidx = IFNUM_CDC1_CONTROL, .handler = 0},	// unused
	{.ifidx = IFNUM_CDC1_DATA, .handler = DataReceivedHandler},
#endif
#if USBD_PRINTER
	{.ifidx = IFNUM_PRN, .handler = DataReceivedHandler},
#endif
};
static const struct epcfg_ incfg[USBD_NUM_EPPAIRS] = {
	{.ifidx = 0, .handler = 0},
#if USBD_MSC
#endif
	{.ifidx = IFNUM_CDC0_CONTROL, .handler = 0},
	{.ifidx = IFNUM_CDC0_DATA, .handler = DataSentHandler},
#if USBD_CDC_CHANNELS > 1
	{.ifidx = IFNUM_CDC1_CONTROL, .handler = 0},
	{.ifidx = IFNUM_CDC1_DATA, .handler = DataSentHandler},
#endif
#if USBD_PRINTER
	{.ifidx = IFNUM_PRN, .handler = DataSentHandler},
#endif
};

// class and instance index for each interface - required for handling class requests
const struct ifassoc_ if2fun[USBD_NUM_INTERFACES] = {
#if USBD_MSC
	[IFNUM_MSC] = {.classid = USB_CLASS_STORAGE, .funidx = 0},
#endif
	[IFNUM_CDC0_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 0},
	[IFNUM_CDC0_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 0},
#if USBD_CDC_CHANNELS > 1
	[IFNUM_CDC1_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 1},
	[IFNUM_CDC1_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 1},
#endif
#if USBD_PRINTER
	[IFNUM_PRN] = {.classid = USB_CLASS_PRINTER, .funidx = 0},
#endif
};

// device config options and descriptors - constant ======================
static const struct usbdcfg_ usbdcfg = {
	.irqn = USB_IRQn,
	.irqpri = 8,
	.numeppairs = USBD_NUM_EPPAIRS,
	.numif = USBD_NUM_INTERFACES,
	.nstringdesc = USBD_NSTRINGDESCS,
	.outepcfg = outcfg,
	.inepcfg = incfg,
	.ifassoc = if2fun,
	.devdesc = &DevDesc,
	.cfgdesc = &ConfigDesc.cfgdesc,
	.strdesc = (const uint8_t **)strdescv,
};

static struct usbdevdata_ uddata;	// device data and status

struct cdc_services_ cdc_service = {
	.SetLineCoding = 0,
	.SetControlLineState = cdc_LineStateHandler,
	// todo: add get status call when implementing notifications
};

// main device data structure - const with pointers to const & variable structures
const struct usbdevice_ usbdev = {
	.usb = (void *)USB_BASE,
	.hwif = &usb_hw_services,
	.cfg = &usbdcfg,
	.devdata = &uddata,
	.outep = out_epdata,
	.inep = in_epdata,
	.cdc_service = &cdc_service,
	.cdc_data = cdc_data
};

// Init routine to start USB device =================================
void USBapp_Init(void)
{
#if USBD_CDC_CHANNELS
	// Tx interrupts are enabled when the device is connected
	NVIC_SetPriority(VCOM0_tx_IRQn, USB_IRQ_PRI);
	NVIC_SetPriority(VCOM0_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_EnableIRQ(VCOM0_rx_IRQn);
#if USBD_CDC_CHANNELS > 1
	NVIC_SetPriority(VCOM1_tx_IRQn, USB_IRQ_PRI);
	NVIC_SetPriority(VCOM1_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_EnableIRQ(VCOM1_rx_IRQn);
#endif
#endif

#if USBD_PRINTER
	NVIC_SetPriority(PRN_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_EnableIRQ(PRN_rx_IRQn);
#endif

	NVIC_SetPriority((IRQn_Type)usbdev.cfg->irqn, USB_IRQ_PRI);
	usbdev.hwif->Init(&usbdev);
}

// USB interrupt routine - invokes general USB interrupt handler passing device structure pointer to it
void USB_IRQHandler(void)
{
	usbdev.hwif->IRQHandler(&usbdev);
}
