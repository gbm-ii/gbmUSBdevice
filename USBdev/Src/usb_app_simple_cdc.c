/* 
 * lightweight USB device stack by gbm
 * usb_app_simple.c - single CDC ACM VCP application example
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

#ifdef SIMPLE_CDC

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

// forward decl
static const struct cfgdesc_cdc_ ConfigDesc;

#define SIGNON_DELAY	50u
#define SIGNON0	"\r\nVCOM ready\r\n"
#define PROMPT0	">"

#define TX_TOUT	2u	// Tx timeout when buffer not empty in ms

// VCOM channel data =====================================================

#if defined(USBD_CDC_CHANNELS) && USBD_CDC_CHANNELS
// define in usb_app.c
static struct cdc_data_ cdc_data[USBD_CDC_CHANNELS] = {
	[0] = {.LineCoding = {.dwDTERate = 115200, .bDataBits = 8}},
};
#endif	// USBD_CDC_CHANNELS


// endpoint data =========================================================
static _Alignas(USB_SetupPacket) uint8_t ep0outpkt[USBD_CTRL_EP_SIZE];	// Control EP Rx buffer

static struct epdata_ out_epdata[USBD_NUM_EPPAIRS] = {
	{.ptr = ep0outpkt, .count = 0},	// control
#if USBD_CDC_CHANNELS
	{.ptr = 0, .count = 0},	// unused
	{.ptr = cdc_data[0].RxData, .count = 0},
#endif	// USBD_CDC_CHANNELS
};

static struct epdata_ in_epdata[USBD_NUM_EPPAIRS]; // no need to init
//========================================================================

#if USBD_CDC_CHANNELS
struct vcomcfg_ {
	uint8_t rx_irqn, tx_irqn;
	const char *signon, *prompt;
};

static const struct vcomcfg_ vcomcfg[USBD_CDC_CHANNELS] = {
	{VCOM0_rx_IRQn, VCOM0_tx_IRQn, SIGNON0, PROMPT0},
};

// put character into sendbuf, generate send packet request event
static void vcom_putchar(uint8_t ch, char c)
{
	if (ch < USBD_CDC_CHANNELS)
	{
		struct cdc_data_ *cdp = &cdc_data[ch];

		while (cdp->connected && cdp->TxLength == CDC_DATA_EP_SIZE) ;	// buffer full -> wait

		if (cdp->connected)
		{
			__disable_irq();
			cdp->TxData[cdp->TxLength++] = c;
			if (cdp->TxLength == CDC_DATA_EP_SIZE)
			{
				cdp->TxTout = 0;
				NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);
			}
			else
				cdp->TxTout = TX_TOUT;
			__enable_irq();
		}
	}
}

static void vcom_putstring(uint8_t ch, const char *s)
{
	if (ch < USBD_CDC_CHANNELS && s)
		while (*s)
			vcom_putchar(ch, *s++);
}

// vcom0 tx/in app interface functions ===================================
void vcom0_putc(uint8_t c)
{
	vcom_putchar(0, c);
}

void vcom0_putstring(const char *s)
{
	vcom_putstring(0, s);
}
// Serial state notification =============================================
struct cdc_SerialStateNotif_  ssnotif = {
	.bmRequestType = {.Recipient = USB_RQREC_INTERFACE, .Type = USB_RQTYPE_CLASS, .DirIn = 1},
	.bNotification = CDC_NOTIFICATION_SERIAL_STATE,
	.wIndex = 0,	// interface
	.wLength = 2,	// size of wSerialState
	.wSerialState = 0
};

// forward declaration
const struct usbdevice_ usbdev;

// inline only to avoid not used warning
static inline void send_serialstate_notif(uint8_t ch)
{
	ssnotif.wIndex = ConfigDesc.cdc[ch].cdcdesc.cdccomifdesc.bInterfaceNumber;	// interface
	ssnotif.wSerialState = cdc_data[ch].SerialState;
	if (USBdev_SendData(&usbdev, ConfigDesc.cdc[ch].cdcdesc.cdcnotif.bEndpointAddress,
		(const uint8_t *)&ssnotif, sizeof(ssnotif), 0) == 0)
	{
		cdc_data[ch].SerialStateSent = ssnotif.wSerialState & (CDC_SERIAL_STATE_TX_CARRIER | CDC_SERIAL_STATE_RX_CARRIER);	// clear all transient flags
		cdc_data[ch].SerialState ^= ssnotif.wSerialState & ~(CDC_SERIAL_STATE_TX_CARRIER | CDC_SERIAL_STATE_RX_CARRIER);	// clear transient flags sent
	}
}
#endif	// USBD_CDC_CHANNELS

// overwrite for any real-world use - this is just echo for demo application
// return 1 if prompt requested
__attribute__ ((weak)) bool vcom_process_input(uint8_t ch, uint8_t c)
{
#if USBD_CDC_CHANNELS
	vcom_putchar(ch, c);	// echo to the same channel
#endif
	return 0;
}

// enable data reception on a specified endpoint - called after received data is processed
static void allow_rx(uint8_t epn)
{
	__disable_irq();
	usbdev.hwif->EnableRx(&usbdev, epn);
	__enable_irq();
}

// vcom0 rx/out polled app interface =====================================
bool vcom0_rxrdy(void)
{
	return cdc_data[0].RxLength;
}

uint8_t vcom0_getc(void)
{
	while (!vcom0_rxrdy()) ;
	uint8_t c = cdc_data[0].RxData[cdc_data[0].RxIdx];
	if (++cdc_data[0].RxIdx == cdc_data[0].RxLength)
	{
		cdc_data[0].RxLength = 0;
		allow_rx(ConfigDesc.cdc[0].cdcdesc.cdcout.bEndpointAddress);
	}
	return c;
}
//========================================================================
uint32_t usbdev_msec;

// called from USB interrupt at 1 kHz (SOF)
void usbdev_tick(void)
{
	++usbdev_msec;
#if USBD_CDC_CHANNELS
//	static uint16_t dt;
//	if ((++dt & 0x3ff) == 0)
//	{
//		cdc_data[0].SerialState = dt >> 10 & 3;
//		//send_serialstate_notif(0);
//	}
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
	{
		struct cdc_data_ *cdcp = &cdc_data[ch];
		if (cdcp->connstart_timer && --cdcp->connstart_timer == 0)
		{
			cdcp->connected = 1;
			cdcp->signon_rq = 1;
			NVIC_SetPendingIRQ(vcomcfg[ch].rx_irqn);
			NVIC_EnableIRQ(vcomcfg[ch].tx_irqn);
		}
		if (cdcp->TxTout && --cdcp->TxTout == 0)
		{
			NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);
		}
		if (cdcp->SerialState != cdcp->SerialStateSent)
			send_serialstate_notif(ch);
	}
#endif	// USBD_CDC_CHANNELS
}

#if USBD_CDC_CHANNELS
void cdc_LineStateHandler(const struct usbdevice_ *usbd, uint8_t ch)
{
	// called from USB interrupt, overwrites the default handler in usb_class.c
	if ((cdc_data[ch].ControlLineState & (CDC_CTL_DTR | CDC_CTL_RTS)) == (CDC_CTL_DTR | CDC_CTL_RTS))
	{
		// Note: Br@y Terminal sends DTR & RTS only when DTR goes active while RTS _is_ active
		cdc_data[ch].SerialState |= CDC_SERIAL_STATE_TX_CARRIER | CDC_SERIAL_STATE_RX_CARRIER;
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

// data reception and state change handler, priority lower than USB hw interrupt
void VCOM_rx_IRQHandler(uint8_t ch)
{
	bool prompt_rq = 0;

	if (cdc_data[ch].LineCodingChanged)
	{
		cdc_data[ch].LineCodingChanged = 0;
		// if needed, handle here (reprogram UART if used)
	}
	if (cdc_data[ch].ControlLineStateChanged)
	{
		// handle if not handled by LineStateHandler
	}
#ifndef POLL
	if (cdc_data[ch].RxLength)
	{
		uint8_t *rxptr = cdc_data[ch].RxData; //
		for (uint8_t i = 0; i < cdc_data[ch].RxLength; i++)
			prompt_rq |= vcom_process_input(ch, *rxptr++);
		cdc_data[ch].RxLength = 0;
		allow_rx(ConfigDesc.cdc[ch].cdcdesc.cdcout.bEndpointAddress);
	}
#endif
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

	NVIC_DisableIRQ(vcomcfg[ch].tx_irqn);
//	if (cdp->TxLength)
	{
		if (cdp->TxLength == CDC_DATA_EP_SIZE)
			NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);	// another packet must follow, data or ZLP
		USBdev_SendData(&usbdev, ConfigDesc.cdc[ch].cdcdesc.cdcin.bEndpointAddress,
			cdp->TxData, cdp->TxLength, 0);
		cdp->TxLength = 0;	// clear counter
	}
}

void VCOM0_tx_IRQHandler(void)
{
	VCOM_tx_IRQHandler(0);
}

#endif	// USBD_CDC_CHANNELS


// Application routines ==================================================

// called form USB hw interrupt
void DataReceivedHandler(const struct usbdevice_ *usbd, uint8_t epn)
{
	uint16_t length = usbd->outep[epn].count;
	if (length)
	{
		{
			switch (epn)
			{
#if USBD_CDC_CHANNELS
			case CDC0_DATA_OUT_EP:
				cdc_data[0].RxIdx = 0;	// for polled only
				cdc_data[0].RxLength = length;
#ifndef POLL
				NVIC_SetPendingIRQ(VCOM0_rx_IRQn);
#endif	// POLL
				break;
#endif	// USBD_CDC_CHANNELS
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
#if USBD_CDC_CHANNELS
	case CDC0_DATA_IN_EP:
		NVIC_EnableIRQ(VCOM0_tx_IRQn);
		break;
#endif	// USBD_CDC_CHANNELS
	default:

	}
}

// USB descriptors =======================================================
// start with string descriptors since they are referenced in other descriptors
STRLANGID(sdLangID, USB_LANGID_US);
STRINGDESC(sdVendor, u"gbm");
STRINGDESC(sdName, u"VCP");
STRINGDESC(sdSerial, u"0001");
#if USBD_CDC_CHANNELS
STRINGDESC(sdVcom0, u"VCOM0");
#endif

// string descriptor numbering - must match the order in string desc table
enum usbd_sidx_ {
	USBD_SIDX_LANGID,
	USBD_SIDX_MFG, USBD_SIDX_PRODUCT, USBD_SIDX_SERIALNUM,
#if USBD_CDC_CHANNELS
	USBD_SIDX_FUN_VCOM0,
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
#endif
};

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
	.cdc = {
		[0] = {
			.cdcdesc = CDCVCOMDESC(IFNUM_CDC0_CONTROL, CDC0_INT_IN_EP, CDC0_DATA_IN_EP, CDC0_DATA_OUT_EP, CDCACM_FDCAP_LC_LS)
		},
	}
};

// endpoint configuration - constant =====================================
static const struct epcfg_ outcfg[USBD_NUM_EPPAIRS] = {
	{.ifidx = 0, .handler = 0},
	{.ifidx = IFNUM_CDC0_CONTROL, .handler = 0},	// unused
	{.ifidx = IFNUM_CDC0_DATA, .handler = DataReceivedHandler},
};

static const struct epcfg_ incfg[USBD_NUM_EPPAIRS] = {
	{.ifidx = 0, .handler = 0},
#if USBD_CDC_CHANNELS
	{.ifidx = IFNUM_CDC0_CONTROL, .handler = 0},
	{.ifidx = IFNUM_CDC0_DATA, .handler = DataSentHandler},
#endif	// USBD_CDC_CHANNELS
};

// class and instance index for each interface - required for handling class requests
const struct ifassoc_ if2fun[USBD_NUM_INTERFACES] = {
#if USBD_CDC_CHANNELS
	[IFNUM_CDC0_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 0},
	[IFNUM_CDC0_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 0},
#endif	// USBD_CDC_CHANNELS
};

// device config options and descriptors - constant ======================
static const struct usbdcfg_ usbdcfg = {
	.irqn = USB_IRQn,
	.irqpri = USB_IRQ_PRI,
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

static const struct cdc_services_ cdc_service = {
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
	.cdc_data = cdc_data,
};

// Init routine to start USB device =================================
void USBapp_Init(void)
{
#if USBD_CDC_CHANNELS
	// Tx interrupts are enabled when the device is connected
	NVIC_SetPriority(VCOM0_tx_IRQn, USB_IRQ_PRI);
	NVIC_SetPriority(VCOM0_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_EnableIRQ(VCOM0_rx_IRQn);
#endif	// USBD_CDC_CHANNELS

	NVIC_SetPriority((IRQn_Type)usbdev.cfg->irqn, USB_IRQ_PRI);
	usbdev.hwif->Init(&usbdev);
}

// USB interrupt routine - invokes general USB interrupt handler passing device structure pointer to it
void USB_IRQHandler(void)
{
	usbdev.hwif->IRQHandler(&usbdev);
}

void USBapp_Poll(void)
{
	uint8_t c = vcom0_getc();
	vcom0_putc(c);
}

#endif // SIMPLE_CDC
