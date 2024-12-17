/* 
 * lightweight USB device stack by gbm
 * usb_app.c - event-driven composite device application example
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

#ifndef SIMPLE_CDC

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

//uint32_t usbstat;	// temporary diags
#define EVTREC(a)	usbstat = usbstat << 4 | a

// forward def
#define SINGLE_CDC (USBD_CDC_CHANNELS == 1 && (USBD_MSC + USBD_PRINTER + USBD_HID == 0))
#if SINGLE_CDC
static const struct cfgdesc_cdc_ ConfigDesc;
#else
static const struct cfgdesc_msc_ncdc_prn_ ConfigDesc;
#endif

#define SIGNON_DELAY	50u

#ifndef SIGNON0
#define SIGNON0	"\r\nVCOM0 ready\r\n"
#endif
#define SIGNON1	"\r\nVCOM1 ready\r\n"
#define SIGNON2	"\r\nVCOM2 ready\r\n"
#ifndef PROMPT
#define PROMPT	">"
#endif

#define TX_TOUT	2u	// Tx timeout when buffer not empty in ms

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
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
};
#endif	// USBD_CDC_CHANNELS

#if USBD_PRINTER
static struct prn_data_ prn_data;
#endif

#if USBD_HID
static struct hid_data_ hid_data;

__attribute__ ((weak)) bool BtnGet(void)
{
	return 0;	// redefine to get button state
}

static bool HIDupdateKB(const struct usbdevice_ *usbd)
{
#ifdef HID_PWR
	bool change = (bool)hid_data.InReport[0] ^ BtnGet();
	if (change)
		hid_data.InReport[0] ^= 1;
#else
	bool change = (bool)hid_data.InReport[2] ^ BtnGet();
	if (change)
		hid_data.InReport[2] ^= HIDKB_KPADSTAR;
#endif
	return change;
}

__attribute__ ((weak)) void LED_Set(bool on)
{
	// redefine to control board's LED
}

static void HIDsetLEDs(const struct usbdevice_ *usbd)
{
	// set onboard LED to ScrollLock status
	LED_Set(hid_data.OutReport[0] & HIDKB_MSK_SCROLLLOCK);
}
#endif

// endpoint data =========================================================
static _Alignas(USB_SetupPacket) uint8_t ep0outpkt[USBD_CTRL_EP_SIZE];	// Control EP Rx buffer

static struct epdata_ out_epdata[USBD_NUM_EPPAIRS] = {
	{.ptr = ep0outpkt, .count = 0},	// control
#if USBD_CDC_CHANNELS
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
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS
#if USBD_PRINTER
	{.ptr = prn_data.RxData, .count = 0},
#endif
#ifdef USBD_HID_OUT_EP
	{.ptr = hid_data.OutReport}
#endif
};

static struct epdata_ in_epdata[USBD_NUM_EPPAIRS]; // no need to init
//========================================================================

#if USBD_CDC_CHANNELS
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
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
};

//
void vcom_write(uint8_t ch, const char *buf, uint16_t size)
{
	if (ch < USBD_CDC_CHANNELS)
	{
		struct cdc_data_ *cdp = &cdc_data[ch];
		struct cdc_session_ *cds = &cdp->session;

		while (cds->connected && size)
		{
			while (cds->connected && cds->TxLength == CDC_DATA_EP_SIZE) ;	// buffer full -> wait

			if (cds->connected)
			{
				__disable_irq();
				uint16_t bfree = CDC_DATA_EP_SIZE - cds->TxLength;
				uint16_t chunksize = size < bfree ? size : bfree;
				memcpy(&cdp->TxData[cds->TxLength], buf, chunksize);
				buf += chunksize;
				size -= chunksize;
				cds->TxLength += chunksize;
				if (cds->TxLength == CDC_DATA_EP_SIZE)
				{
					cds->TxTout = 0;
					NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);
				}
				else
					cds->TxTout = TX_TOUT;
				__enable_irq();
			}
		}
	}
}

// put character into sendbuf, generate send packet request event
void vcom_putchar(uint8_t ch, char c)
{
	vcom_write(ch, &c, 1);
}

void vcom_putstring(uint8_t ch, const char *s)
{
	if (s)
		vcom_write(ch, s, strlen(s));
}

void vcom0_putc(uint8_t c)
{
	vcom_putchar(0, c);
}

void vcom0_putstring(const char *s)
{
	vcom_putstring(0, s);
}

#if USBD_CDC_CHANNELS > 1
void vcom1_putc(uint8_t c)
{
	vcom_putchar(1, c);
}

void vcom1_putstring(const char *s)
{
	vcom_putstring(1, s);
}

#if USBD_CDC_CHANNELS > 2
void vcom2_putc(uint8_t c)
{
	vcom_putchar(2, c);
}

void vcom2_putstring(const char *s)
{
	vcom_putstring(2, s);
}

#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1

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

//========================================================================
// overwrite for any real-world use - this is just echo for demo application
// return 1 if prompt requested
__attribute__ ((weak)) uint8_t vcom_process_input(uint8_t ch, uint8_t c)
{
#if USBD_CDC_CHANNELS
	vcom_putchar(ch, c);	// echo to the same channel
#endif
	return 0;
}

__attribute__ ((weak)) void VCP_ConnStatus(uint8_t ch, bool on)
{
	// define to control board's LED for VCP connection status signaling
}

#endif	// USBD_CDC_CHANNELS

// enable data reception on a specified endpoint - called after received data is processed
static void allow_rx(uint8_t epn)
{
	__disable_irq();
	usbdev.hwif->EnableRx(&usbdev, epn);
	__enable_irq();
}

#if USBD_PRINTER

__attribute__ ((weak)) uint8_t prn_process_input(uint8_t c)
{
	return vcom_process_input(0, c);	// same handling as vcom0
}

void PRN_rx_IRQHandler(void)
{
	if (prn_data.RxLength)
	{
		uint8_t *rxptr = prn_data.RxData; //
		for (uint8_t i = 0; i < prn_data.RxLength; i++)
			prn_process_input(*rxptr++);	// same handling as vcom0
		prn_data.RxLength = 0;
		allow_rx(PRN_DATA_OUT_EP);
	}
}
#endif

//========================================================================
// called on reset, suspend, resume
static void usbdev_session_init(void)
{
#if USBD_CDC_CHANNELS
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
	{
		struct cdc_data_ *cdcp = &cdc_data[ch];
		cdcp->session = (struct cdc_session_) {0};
		VCP_ConnStatus(ch, 0);
	}
#endif
}

static void usbdev_reset(void)
{
	usbdev_session_init();
#if USBD_CDC_CHANNELS
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
	{
		struct cdc_data_ *cdcp = &cdc_data[ch];
		cdcp->ControlLineState = 0;
		cdcp->ControlLineStateChanged = 0;
		cdcp->LineCodingChanged = 0;
	}
#endif
}

/* sequence recorded during Win hibernation and wakeup with TeraTerm active:
 * resume with old linestate, reset, resume, reset, reset
 *  -> no linestate update
 */

static void usbdev_resume(void)
{
	usbdev_session_init();
#if USBD_CDC_CHANNELS
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
	{
		struct cdc_data_ *cdcp = &cdc_data[ch];
		if ((cdcp->ControlLineState & (CDC_CTL_DTR | CDC_CTL_RTS)) == (CDC_CTL_DTR | CDC_CTL_RTS))
		{
			cdcp->session.connstart_timer = SIGNON_DELAY;	// display prompt after 50 ms
		}
	}
#endif
}

//========================================================================
volatile uint32_t usbdev_msec;

// called from USB interrupt at 1 kHz (SOF)
void usbdev_tick(void)
{
	++usbdev_msec;
#if USBD_CDC_CHANNELS
#ifdef SSNOTIF_TEST
	static uint16_t dt;
	if ((++dt & 0x3ff) == 0)
	{
		cdc_data[0].SerialState = dt >> 10 & 3;
		//send_serialstate_notif(0);
	}
#endif
	for (uint8_t ch = 0; ch < USBD_CDC_CHANNELS; ch++)
	{
		struct cdc_data_ *cdcp = &cdc_data[ch];
		struct cdc_session_ *cds = &cdcp->session;

		if (cds->connstart_timer && --cds->connstart_timer == 0)
		{
			cds->connected = 1;
			VCP_ConnStatus(ch, 1);
			cds->signon_rq = 1;
			NVIC_SetPendingIRQ(vcomcfg[ch].rx_irqn);
			NVIC_EnableIRQ(vcomcfg[ch].tx_irqn);
		}
		if (cds->autonul_timer && --cds->autonul_timer == 0)
		{
			cds->autonul = 1;
			NVIC_SetPendingIRQ(vcomcfg[ch].rx_irqn);
		}
		if (cds->TxTout && --cds->TxTout == 0)
		{
			NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);
		}
		if (cdcp->SerialState != cdcp->SerialStateSent)
			send_serialstate_notif(ch);
	}
#endif	// USBD_CDC_CHANNELS
#if USBD_HID
	if (hid_data.SampleTimer == 0)
	{
		// initialize once
		hid_data.SampleTimer = HID_POLLING_INTERVAL;
		hid_data.Idle = HID_DEFAULT_IDLE;
		hid_data.ReportTimer = HID_POLLING_INTERVAL;
	}
	else if (--hid_data.SampleTimer == 0)
	{
		hid_data.SampleTimer = HID_POLLING_INTERVAL;
		if (HIDupdateKB(&usbdev))
			hid_data.InRq = 1;
	}
	if (((hid_data.ReportTimer && --hid_data.ReportTimer == 0) || hid_data.InRq)
		&& USBdev_SendData(&usbdev, HID_IN_EP, (const uint8_t *)hid_data.InReport, sizeof(hid_data.InReport), 0) == 0)
	{
		hid_data.ReportTimer = hid_data.Idle * 4 > HID_POLLING_INTERVAL ? hid_data.Idle * 4 : HID_POLLING_INTERVAL;
		hid_data.InRq = 0;
	}

#endif	// USBD_HID
}

#if USBD_CDC_CHANNELS
void cdc_LineStateHandler(const struct usbdevice_ *usbd, uint8_t ch)
{
	// called from USB interrupt, overwrites the default handler in usb_class.c
	if ((cdc_data[ch].ControlLineState & (CDC_CTL_DTR | CDC_CTL_RTS)) == (CDC_CTL_DTR | CDC_CTL_RTS))
	{
		// Note: Br@y Terminal sends DTR & RTS only when DTR goes active while RTS _is_ active
		cdc_data[ch].SerialState |= CDC_SERIAL_STATE_TX_CARRIER | CDC_SERIAL_STATE_RX_CARRIER;
		cdc_data[ch].session.connstart_timer = SIGNON_DELAY;	// display prompt after 50 ms
	}
	else
	{
		//NVIC_DisableIRQ(VCOM0_tx_IRQn);
		// should reset the state
		cdc_data[ch].session.connstart_timer = 0;	// possible hazard w/USB interrupt
		cdc_data[ch].session.connected = 0;
		VCP_ConnStatus(ch, 0);
	}
	cdc_data[ch].ControlLineStateChanged = 0;
}

// to be called by app when prompt should be displayed not as a result of command processing
void vcom_prompt_request(uint8_t ch)
{
	cdc_data[ch].session.prompt_rq = 1;
	NVIC_SetPendingIRQ(vcomcfg[ch].rx_irqn);
}

// signon/prompt display, may be customized
__attribute__ ((weak)) void vcom_prompt(uint8_t ch, bool signon)
{
	vcom_putstring(ch, signon ? vcomcfg[ch].signon : vcomcfg[ch].prompt);
}

// data reception and state change handler, priority lower than USB hw interrupt
void VCOM_rx_IRQHandler(uint8_t ch)
{
	if (cdc_data[ch].LineCodingChanged)
	{
		cdc_data[ch].LineCodingChanged = 0;
		// if needed, handle here (reprogram UART if used)
	}
	if (cdc_data[ch].ControlLineStateChanged)
	{
		// handle if not handled by LineStateHandler
	}
	if (cdc_data[ch].session.RxLength)
	{
		cdc_data[ch].session.connected = 1;
		VCP_ConnStatus(ch, 1);
		uint8_t *rxptr = cdc_data[ch].RxData; //
		uint8_t pival = 0;
		for (uint8_t i = 0; i < cdc_data[ch].session.RxLength; i++)
		{
			pival = vcom_process_input(ch, *rxptr++);
			cdc_data[ch].session.prompt_rq |= pival & PIRET_PROMPTRQ;
		}
		cdc_data[ch].session.RxLength = 0;
		cdc_data[ch].session.autonul = 0;
		cdc_data[ch].session.autonul_timer = (pival & PIRET_AUTONUL) ? AUTONUL_TOUT : 0;
		allow_rx(ConfigDesc.cdc[ch].cdcdesc.cdcout.bEndpointAddress);
	}
	else if (cdc_data[ch].session.autonul)
	{
		cdc_data[ch].session.autonul = 0;
		cdc_data[ch].session.prompt_rq |= vcom_process_input(ch, 0) & PIRET_PROMPTRQ;
	}
	if (cdc_data[ch].session.signon_rq)
	{
		cdc_data[ch].session.signon_rq = 0;
		vcom_prompt(ch, 1);
		cdc_data[ch].session.prompt_rq = 1;
	}
	if (cdc_data[ch].session.prompt_rq)
	{
		cdc_data[ch].session.prompt_rq = 0;
		vcom_prompt(ch, 0);
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
		if (cdp->session.TxLength == CDC_DATA_EP_SIZE)
			NVIC_SetPendingIRQ(vcomcfg[ch].tx_irqn);	// another packet must follow, data or ZLP
		USBdev_SendData(&usbdev, ConfigDesc.cdc[ch].cdcdesc.cdcin.bEndpointAddress,
			cdp->TxData, cdp->session.TxLength, 0);
		cdp->session.TxLength = 0;	// clear counter
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

#if USBD_HID
// HID keyboard report descriptor
const uint8_t hid_report_desc[] = {
#ifdef HID_PWR
		// experimental: power keys
		0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
		0x09, 0x80,                    // USAGE (System Control)
		0xa1, 0x01,                    // COLLECTION (Application)
		0x19, 0x81,                    //   USAGE_MINIMUM (System Sleep)
		0x29, 0x83,                    //   USAGE_MAXIMUM (System Wake Up)
		0x15, 0x00,                    //   LOGICAL_MINIMUM (0)   <---------- Add these three lines
		0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)   <----------
		0x75, 0x01,                    //   REPORT_SIZE (1)       <----------
		0x95, 0x03,                    //   REPORT_COUNT (3)
		0x81, 0x06,                    //   INPUT (Data,Var,Rel)
		0x95, 0x05,                    //   REPORT_COUNT (5)
		0x81, 0x03,                    //   INPUT (Cnst,Var,Abs)
		0xc0                           // END_COLLECTION
#else
	0x05, 0x01,	//	Usage Page (Generic Desktop)
	0x09, 0x06,	//	Usage (Keyboard)
	0xA1, 0x01,	//	Collection (Application)

	// input report for modifier keys
	0x05, 0x07,	//	Usage Page (Key Codes);
	0x19, 0xe0,	//	Usage Minimum (224) - Left Control
	0x29, 0xE7,	//	Usage Maximum (231) - Right GUI
	0x15, 0x00,	//	Logical Minimum (0)
	0x25, 0x01,	//	Logical Maximum (1)
	0x75, 0x01,	//	Report Size (1) - one bit
	0x95, 0x08,	//	Report Count (8) - times 8
	0x81, 0x02,	//	Input (Data, Variable, Absolute); Modifier byte
	0x95, 0x01,	//	Report Count (1) - one spare byte
	0x75, 0x08,	//	Report Size (8)
	0x81, 0x01,	//	Input (Constant), reserved byte

	// output report for keyboard LEDs
	0x95, 0x05,	//	Report Count (was 5)
	0x75, 0x01,	//	Report Size (1)
	0x05, 0x08,	//	Usage Page (Page# for LEDs)
	0x19, HID_LED_NUMLOCK,	//	Usage Minimum (1)
	0x29, HID_LED_KANA,	//	Usage Maximum (originally 5 - Kana)
	0x91, 0x02,	//	Output (Data, Variable, Absolute); LED report
	0x95, 0x01,	//	Report Count (1)
	0x75, 0x03,	//	Report Size (was 3)
	0x91, 0x01,	//	Output (Constant); LED report padding

	// input report for std keys
	0x95, 0x06,	//	Report Count (6)
	0x75, 0x08,	//	Report Size (8)
	0x15, 0x00,	//	Logical Minimum (0)
	0x25, 0x65,	//	Logical Maximum(101)
	0x05, 0x07,	//	Usage Page (Key Codes)
	0x19, 0x00,	//	Usage Minimum (0)
	0x29, 0x65,	//	Usage Maximum (101)
	0x81, 0x00,	//	Input (Data Array); (6 bytes)

	0xC0	//	End Collection
#endif
};

// not used, report send via control pipe
static void HIDoutHandler(const struct usbdevice_ *usbd, uint8_t epn)
{
	hid_data.OutReport[2] = hid_data.OutReport[0];
}
#endif


// Application routines ==================================================

// called form USB hw interrupt
void DataReceivedHandler(const struct usbdevice_ *usbd, uint8_t epn)
{
	uint16_t length = usbd->outep[epn].count;
	if (length)
	{
#ifdef xUSBLOG
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
#if USBD_CDC_CHANNELS
			case CDC0_DATA_OUT_EP:
				cdc_data[0].session.RxLength = length;
				NVIC_SetPendingIRQ(VCOM0_rx_IRQn);
				break;
#if USBD_CDC_CHANNELS > 1
			case CDC1_DATA_OUT_EP:
				cdc_data[1].session.RxLength = length;
				NVIC_SetPendingIRQ(VCOM1_rx_IRQn);
				break;
#if USBD_CDC_CHANNELS > 2
			case CDC2_DATA_OUT_EP:
				cdc_data[2].session.RxLength = length;
				NVIC_SetPendingIRQ(VCOM2_rx_IRQn);
				break;
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS
#if USBD_PRINTER
			case PRN_DATA_OUT_EP:
				prn_data.RxLength = length;
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
	switch (epn)
	{
#if USBD_CDC_CHANNELS
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
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS
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
#if USBD_HID
STRINGDESC(sdHID, u"gbmHID");
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
#if USBD_HID
	USBD_SIDX_HID,
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
#if USBD_HID
	&sdHID.bLength,
#endif
};

#define SINGLE_CDC (USBD_CDC_CHANNELS == 1 && (USBD_MSC + USBD_PRINTER + USBD_HID == 0))
#if SINGLE_CDC
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
		}
	}
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
#if USBD_CDC_CHANNELS
	.cdc = {
		[0] = {
			.cdciad = CDCVCOMIAD(IFNUM_CDC0_CONTROL, USBD_SIDX_FUN_VCOM0),
			.cdcdesc = CDCVCOMDESC(IFNUM_CDC0_CONTROL, CDC0_INT_IN_EP, CDC0_DATA_IN_EP, CDC0_DATA_OUT_EP, CDCACM_FDCAP_LC_LS)
		},
#if USBD_CDC_CHANNELS > 1
		[1] = {
			.cdciad = CDCVCOMIAD(IFNUM_CDC1_CONTROL, USBD_SIDX_FUN_VCOM1),
			.cdcdesc = CDCVCOMDESC(IFNUM_CDC1_CONTROL, CDC1_INT_IN_EP, CDC1_DATA_IN_EP, CDC1_DATA_OUT_EP, CDCACM_FDCAP_LC_LS)
		},
#if USBD_CDC_CHANNELS > 2
		[2] = {
			.cdciad = CDCVCOMIAD(IFNUM_CDC2_CONTROL, USBD_SIDX_FUN_VCOM2),
			.cdcdesc = CDCVCOMDESC(IFNUM_CDC2_CONTROL, CDC2_INT_IN_EP, CDC2_DATA_IN_EP, CDC2_DATA_OUT_EP, CDCACM_FDCAP_LC_LS)
		},
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
	},
#endif	// USBD_CDC_CHANNELS
#if USBD_PRINTER
	.prn = {
		.prnifdesc = IFDESC(IFNUM_PRN, 1, USB_CLASS_PRINTER, PRN_SUBCLASS_PRINTER, PRN_PROTOCOL_UNIDIR, USBD_SIDX_PRINTER),
//		.prnin = EPDESC(PRN_DATA_IN_EP, USB_EPTYPE_BULK, PRN_DATA_EP_SIZE, 0),
		.prnout = EPDESC(PRN_DATA_OUT_EP, USB_EPTYPE_BULK, PRN_DATA_EP_SIZE, 0)
	},
#endif
#if USBD_HID
	.hid = {
#ifdef HID_PWR
		.hidifdesc = IFDESC(IFNUM_HID, 1, USB_CLASS_HID, HID_SUBCLASS_NONE, HID_PROTOCOL_NONE, USBD_SIDX_HID),
#else
		.hidifdesc = IFDESC(IFNUM_HID, 1, USB_CLASS_HID, HID_SUBCLASS_NONE, HID_PROTOCOL_KB, USBD_SIDX_HID),
#endif
		.hiddesc = {
			.bLength = sizeof(struct USBdesc_hid_), .bDescriptorType = USB_DESCTYPE_HID, .bcdHID = USB16(0x101),
			.bCountryCode = 0, .bNumDescriptors = 1, .bHidDescriptorType = USB_DESCTYPE_HIDREPORT,
			.wDescriptorLength = USB16(sizeof(hid_report_desc))
		},
		.hidin = EPDESC(HID_IN_EP, USBD_EP_TYPE_INTR, HID_IN_EP_SIZE, HID_POLLING_INTERVAL),
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
#if USBD_HID
	{.ifidx = IFNUM_HID, .handler = HIDoutHandler},	// HID out ep, not used
#endif
};
static const struct epcfg_ incfg[USBD_NUM_EPPAIRS] = {
	{.ifidx = 0, .handler = 0},
#if USBD_MSC
#endif
#if USBD_CDC_CHANNELS
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
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS
#if USBD_PRINTER
	{.ifidx = IFNUM_PRN, .handler = DataSentHandler},
#endif
#if USBD_HID
	{.ifidx = IFNUM_HID, },
#endif
};

// class and instance index for each interface - required for handling class requests
const struct ifassoc_ if2fun[USBD_NUM_INTERFACES] = {
#if USBD_MSC
	[IFNUM_MSC] = {.classid = USB_CLASS_STORAGE, .funidx = 0},
#endif
#if USBD_CDC_CHANNELS
	[IFNUM_CDC0_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 0},
	[IFNUM_CDC0_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 0},
#if USBD_CDC_CHANNELS > 1
	[IFNUM_CDC1_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 1},
	[IFNUM_CDC1_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 1},
#if USBD_CDC_CHANNELS > 2
	[IFNUM_CDC2_CONTROL] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 2},
	[IFNUM_CDC2_DATA] = {.classid = USB_CLASS_COMMUNICATIONS, .funidx = 2},
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS
#if USBD_PRINTER
	[IFNUM_PRN] = {.classid = USB_CLASS_PRINTER, .funidx = 0},
#endif
#if USBD_HID
	[IFNUM_HID] = {.classid = USB_CLASS_HID, .funidx = 0},
#endif
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
#if USBD_HID
	.hidrepdescsize = sizeof(hid_report_desc),
	.hidrepdesc = hid_report_desc
#endif
};

static struct usbdevdata_ uddata;	// device data and status

static const struct cdc_services_ cdc_service = {
	.SetLineCoding = 0,
	.SetControlLineState = cdc_LineStateHandler,
	// todo: add get status call when implementing notifications
};

#if USBD_PRINTER
static const struct prn_services_ prn_service = {
	.SoftReset = 0,
	.UpdateStatus = 0
};
#endif

#if USBD_HID
static const struct hid_services_ hid_service = {
	.UpdateIn = HIDupdateKB,
	.UpdateOut = HIDsetLEDs,
};
#endif

// main device data structure - const with pointers to const & variable structures
const struct usbdevice_ usbdev = {
	.usb = (void *)USB_BASE,
	.hwif = &usb_hw_services,
	.cfg = &usbdcfg,
	.devdata = &uddata,
	.outep = out_epdata,
	.inep = in_epdata,
	.Reset_Handler = usbdev_reset,
	.Suspend_Handler = usbdev_session_init,
	.Resume_Handler = usbdev_resume,
	.SOF_Handler = usbdev_tick,
//	.ESOF_Handler = usbdev_notick,
	.cdc_service = &cdc_service,
	.cdc_data = cdc_data,
#if USBD_PRINTER
	.prn_service = &prn_service,
	.prn_data = &prn_data,
#endif
#if USBD_HID
	.hid_service = &hid_service,
	.hid_data = &hid_data,
#endif
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
#if USBD_CDC_CHANNELS > 2
	NVIC_SetPriority(VCOM2_tx_IRQn, USB_IRQ_PRI);
	NVIC_SetPriority(VCOM2_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_EnableIRQ(VCOM2_rx_IRQn);
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
#endif	// USBD_CDC_CHANNELS

#if USBD_PRINTER
	NVIC_SetPriority(PRN_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_EnableIRQ(PRN_rx_IRQn);
#endif

	NVIC_SetPriority((IRQn_Type)usbdev.cfg->irqn, USB_IRQ_PRI);
	usbdev.hwif->Init(&usbdev);
}

// DeInit routine to stop USB device =================================
void USBapp_DeInit(void)
{
#if USBD_CDC_CHANNELS
	// Tx interrupts are enabled when the device is connected
	NVIC_DisableIRQ(VCOM0_rx_IRQn);
#if USBD_CDC_CHANNELS > 1
	NVIC_DisableIRQ(VCOM1_rx_IRQn);
#if USBD_CDC_CHANNELS > 2
	NVIC_DisableIRQ(VCOM2_rx_IRQn);
#endif	// USBD_CDC_CHANNELS > 2
#endif	// USBD_CDC_CHANNELS > 1
	for (uint8_t i = 0; i < USBD_CDC_CHANNELS; i++)
	{
		cdc_data[i] = (struct cdc_data_){.LineCoding = {.dwDTERate = 115200, .bDataBits = 8}};
	}
#endif	// USBD_CDC_CHANNELS

#if USBD_PRINTER
	NVIC_SetPriority(PRN_rx_IRQn, USB_IRQ_PRI + 1);
	NVIC_DisableIRQ(PRN_rx_IRQn);
	prn_data = (struct prn_data_){0};
#endif
	usbdev.hwif->DeInit(&usbdev);
}

// USB interrupt routine - invokes general USB interrupt handler passing device structure pointer to it
void USB_IRQHandler(void)
{
	usbdev.hwif->IRQHandler(&usbdev);
}

#endif // SIMPLE_CDC

_Static_assert(USBD_NUM_EPPAIRS <= USB_NEPPAIRS, "Too many endpoints - not supported by USB hardware");
