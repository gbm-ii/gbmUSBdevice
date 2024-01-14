/* 
 * lightweight USB device stack by gbm
 * usb_app.c - application example
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

#include <string.h>
#include <stdio.h>

#include "usb_hw.h"
#include "usb_dev_config.h"
#include "usb_std_def.h"
#include "usb_dev.h"
#include "usb_hw_if.h"
#include "usb_desc_gen.h"
#include "usb_log.h"
#include "usb_app.h"

#include "usbdev_binding.h"

// forward def
static const struct cfgdesc_msc_ncdc_prn_ ConfigDesc;

#define SIGNON_DELAY	50u
#define SIGNON0	"\r\nVCOM0 ready\r\n"
#define SIGNON1	"\r\nVCOM1 ready\r\n"
#define SIGNON2	"\r\nVCOM2 ready\r\n"
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
#if USBD_CDC_CHANNELS > 2
	[2] = {.LineCoding = {.dwDTERate = 115200, .bDataBits = 8}},
#endif
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
#ifndef USE_COMMON_CDC_INT_IN_EP
	{.ptr = 0, .count = 0},	// unused
#endif
	{.ptr = cdc_data[1].RxData, .count = 0},
#if USBD_CDC_CHANNELS > 2
#ifndef USE_COMMON_CDC_INT_IN_EP
	{.ptr = 0, .count = 0},	// unused
#endif
	{.ptr = cdc_data[2].RxData, .count = 0},
#endif
#endif
#if USBD_PRINTER
	{.ptr = prnRxData, .count = 0},
#endif
};

static struct epdata_ in_epdata[USBD_NUM_EPPAIRS]; // no need to init
//========================================================================

struct vcomcfg_ {
	uint8_t rx_irqn, tx_irqn;
	const char *signon, *prompt;
};

const struct vcomcfg_ vcomcfg[USBD_CDC_CHANNELS] = {
	{VCOM0_rx_IRQn, VCOM0_tx_IRQn, SIGNON0, ">"},
#if USBD_CDC_CHANNELS > 1
	{VCOM1_rx_IRQn, VCOM1_tx_IRQn, SIGNON1},
#if USBD_CDC_CHANNELS > 2
	{VCOM2_rx_IRQn, VCOM2_tx_IRQn, SIGNON2},
#endif
#endif
};

// put character into sendbuf, generate send packet request event
void vcom_putchar(uint8_t ch, char c)
{
	if (ch < USBD_CDC_CHANNELS)
	{
		struct cdc_data_ *cdp = &cdc_data[ch];

		while (cdp->connected && cdp->TxLength == CDC_DATA_EP_SIZE);	// buffer full -> wait

		if (cdp->connected)
		{
			__disable_irq();
			cdp->TxData[cdp->TxBuf][cdp->TxLength++] = c;
			__enable_irq();
			NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);
		}
	}
}

void vcom_putstring(uint8_t ch, const char *s)
{
	if (ch < USBD_CDC_CHANNELS && s)
		while (*s)
			vcom_putchar(ch, *s++);
}

void vcom0_putc(uint8_t c)
{
	vcom_putchar(0, c);
}

void vcom0_putstring(const char *s)
{
	vcom_putstring(0, s);
}

void vcom1_putc(uint8_t c)
{
	vcom_putchar(1, c);
}

void vcom1_putstring(const char *s)
{
	vcom_putstring(1, s);
}

// return 1 if prompt requested
__attribute__ ((weak)) bool process_input(uint8_t ch, uint8_t c)
{
	vcom_putchar(ch, c);	// echo to the same channel
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
			process_input(0, *rxptr++);	// same handling as vcom0
		prnRxLen = 0;
		allow_rx(PRN_DATA_OUT_EP);
	}
}
#endif

#if USBD_CDC_CHANNELS
void cdc_LineStateHandler(const struct usbdevice_ *usbd, uint8_t ch)
{
	// called from USB interrupt, overwrites the default handler in usb_class.c
	if ((cdc_data[ch].ControlLineState & (CDC_CTL_DTR | CDC_CTL_RTS)) == (CDC_CTL_DTR | CDC_CTL_RTS))
	{
		cdc_data[ch].connstart_timer = SIGNON_DELAY;	// display prompt after 50 ms
	}
	else
	{
		//NVIC_DisableIRQ(VCOM0_tx_IRQn);
		// should reset the state
		cdc_data[ch].connstart_timer = 0;	// possible hazard w USB interrupt
		cdc_data[ch].connected = 0;
	}
	cdc_data[ch].ControlLineStateChanged = 0;
}

// Serial state notification =============================================
struct cdc_seriastatenotif_  ssnotif = {
	.bmRequestType = {.Recipient = USB_RQREC_INTERFACE, .Type = USB_RQTYPE_CLASS, .DirIn = 1},
	.bNotification = CDC_NOTIFICATION_SERIAL_STATE,
	.wIndex = 0,	// interface
	.wLength = 2,	// size of wSerialState
	.wSerialState = 0
};

static void send_serialstate_notif(uint8_t ch)
{
	ssnotif.wIndex = ConfigDesc.cdc[ch].cdcdesc.cdccomifdesc.bInterfaceNumber;	// interface
	ssnotif.wSerialState = cdc_data[ch].SerialState;
	USBdev_SendData(&usbdev, ConfigDesc.cdc[ch].cdcdesc.cdcnotif.bEndpointAddress,
		(const uint8_t *)&ssnotif, sizeof(ssnotif), 0);
	cdc_data[ch].SerialState &= CDC_SERIAL_STATE_TX_CARRIER | CDC_SERIAL_STATE_RX_CARRIER;	// reset other flags
}
#endif
//========================================================================
// called from USB interrupt at 1 kHz (SOF)
void usbdev_tick(void)
{
#if USBD_CDC_CHANNELS
	static uint16_t dt;
	if ((++dt & 0x3ff) == 0)
	{
		cdc_data[0].SerialState = dt >> 10 & 3;
		//send_serialstate_notif(0);
	}
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
		if (cdc_data[ch].connstart_timer && --cdc_data[ch].connstart_timer == 0)
		{
			cdc_data[ch].connected = 1;
			cdc_data[ch].signon_rq = 1;
			NVIC_SetPendingIRQ(vcomcfg[ch].rx_irqn);
			NVIC_EnableIRQ(vcomcfg[ch].tx_irqn);
		}
#endif
}

#if USBD_CDC_CHANNELS
// data reception and state change handler, priority lower than USB hw interrupt
void VCOM_rx_IRQHandler(uint8_t ch)
{
	bool prompt_rq = 0;

	if (cdc_data[ch].LineCodingChanged)
	{
		cdc_data[ch].LineCodingChanged = 0;
		// if needed, handle here
	}
	if (cdc_data[ch].ControlLineStateChanged)
	{
	}
	if (cdc_data[ch].RxLength)
	{
		uint8_t *rxptr = cdc_data[ch].RxData; //
		for (uint8_t i = 0; i < cdc_data[ch].RxLength; i++)
			prompt_rq |= process_input(ch, *rxptr++);
		cdc_data[ch].RxLength = 0;
		allow_rx(ConfigDesc.cdc[ch].cdcdesc.cdcout.bEndpointAddress);
	}
	if (cdc_data[ch].signon_rq)
	{
		cdc_data[ch].signon_rq = 0;
		vcom_putstring(ch, vcomcfg[ch].signon);
		prompt_rq = 1;
	}
	if (prompt_rq)
	{
		prompt_rq = 0;
		vcom_putstring(ch, vcomcfg[ch].prompt);
	}
}

void VCOM0_rx_IRQHandler(void)
{
	VCOM_rx_IRQHandler(0);
}

// transmit handler, must have the same priority as USB hw interrupt
void VCOM_tx_IRQHandler(uint8_t ch)
{
	struct cdc_data_ *cdp = &cdc_data[ch];

	if (cdp->TxLength)
	{
		NVIC_DisableIRQ(vcomcfg[ch].tx_irqn);
		USBdev_SendData(&usbdev, ConfigDesc.cdc[ch].cdcdesc.cdcin.bEndpointAddress,
			cdp->TxData[cdp->TxBuf], cdp->TxLength, 1);
		cdp->TxBuf ^= 1;	// switch buffer
		cdp->TxLength = 0;	// clear counter
	}
}

void VCOM0_tx_IRQHandler(void)
{
	VCOM_tx_IRQHandler(0);
}

#if USBD_CDC_CHANNELS > 1
void VCOM1_rx_IRQHandler(void)
{
	VCOM_rx_IRQHandler(1);
}

// transmit handler, must have the same priority as USB hw interrupt
void VCOM1_tx_IRQHandler(void)
{
	VCOM_tx_IRQHandler(1);
}

#if USBD_CDC_CHANNELS > 2
void VCOM2_rx_IRQHandler(void)
{
	VCOM_rx_IRQHandler(2);
}

// transmit handler, must have the same priority as USB hw interrupt
void VCOM2_tx_IRQHandler(void)
{
	VCOM_tx_IRQHandler(2);
}
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS

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
#if USBD_CDC_CHANNELS > 2
			case CDC2_DATA_OUT_EP:
				cdc_data[2].RxLength = length;
				NVIC_SetPendingIRQ(VCOM2_rx_IRQn);
				break;
#endif
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
//	LED_Toggle();
	switch (epn)
	{
	case CDC0_DATA_IN_EP:
		NVIC_EnableIRQ(VCOM0_tx_IRQn);
		break;
#if USBD_CDC_CHANNELS > 1
	case CDC1_DATA_IN_EP:
		NVIC_EnableIRQ(VCOM1_tx_IRQn);
		break;
#if USBD_CDC_CHANNELS > 2
	case CDC2_DATA_IN_EP:
		NVIC_EnableIRQ(VCOM2_tx_IRQn);
		break;
#endif
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
#ifndef USE_COMMON_CDC_INT_IN_EP
	{.ifidx = IFNUM_CDC1_CONTROL, .handler = 0},	// unused
#endif
	{.ifidx = IFNUM_CDC1_DATA, .handler = DataReceivedHandler},
#if USBD_CDC_CHANNELS > 2
#ifndef USE_COMMON_CDC_INT_IN_EP
	{.ifidx = IFNUM_CDC2_CONTROL, .handler = 0},	// unused
#endif
	{.ifidx = IFNUM_CDC2_DATA, .handler = DataReceivedHandler},
#endif
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
#ifndef USE_COMMON_CDC_INT_IN_EP
	{.ifidx = IFNUM_CDC1_CONTROL, .handler = 0},
#endif
	{.ifidx = IFNUM_CDC1_DATA, .handler = DataSentHandler},
#if USBD_CDC_CHANNELS > 2
#ifndef USE_COMMON_CDC_INT_IN_EP
	{.ifidx = IFNUM_CDC2_CONTROL, .handler = 0},
#endif
	{.ifidx = IFNUM_CDC2_DATA, .handler = DataSentHandler},
#endif
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
#if USBD_CDC_CHANNELS > 2
	[IFNUM_CDC2_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 2},
	[IFNUM_CDC2_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 2},
#endif
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
	.SOF_Handler = usbdev_tick,
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
