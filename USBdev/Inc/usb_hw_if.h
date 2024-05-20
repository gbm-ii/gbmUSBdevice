/*
 * lightweight USB device stack by gbm
 * usb_hw_if.h - hardware-invariant interface to USB hardware module
 * gbm 11'2022
 */

#ifndef USB_HW_IF_H_
#define USB_HW_IF_H_

#include "usb_hw.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define EP_IS_IN	0x80

// called by usb_app.c
//void USBhw_Init(const struct usbdevice_ *usbd);
//void USBhw_IRQHandler(const struct usbdevice_ *usbd);

// called by usb_dev.c
//uint16_t USBhw_GetInEPSize(const struct usbdevice_ *usbd, uint8_t epn);

//void USBhw_SetAddress(const struct usbdevice_ *usbd);
//void USBhw_SetCfg(const struct usbdevice_ *usbd);

//void USBhw_SetEPStall(const struct usbdevice_ *usbd, uint8_t epaddr);
//void USBhw_ClrEPStall(const struct usbdevice_ *usbd, uint8_t epaddr);
//bool USBhw_IsEPStalled(const struct usbdevice_ *usbd, uint8_t epaddr);

//void USBhw_EnableCtlSetup(const struct usbdevice_ *usbd);
//void USBhw_EnableRx(const struct usbdevice_ *usbd, uint8_t epn);
//void USBhw_StartTx(const struct usbdevice_ *usbd, uint8_t epn);

struct USBhw_services_ {
	void (*IRQHandler)(const struct usbdevice_ *usbd);

	void (*Init)(const struct usbdevice_ *usbd);
	void (*DeInit)(const struct usbdevice_ *usbd);
	uint16_t (*GetInEPSize)(const struct usbdevice_ *usbd, uint8_t epn);

	void (*SetCfg)(const struct usbdevice_ *usbd);
	void (*ResetCfg)(const struct usbdevice_ *usbd);

	void (*SetEPStall)(const struct usbdevice_ *usbd, uint8_t epaddr);
	void (*ClrEPStall)(const struct usbdevice_ *usbd, uint8_t epaddr);
	bool (*IsEPStalled)(const struct usbdevice_ *usbd, uint8_t epaddr);

	void (*EnableCtlSetup)(const struct usbdevice_ *usbd);
	void (*EnableRx)(const struct usbdevice_ *usbd, uint8_t epn);
	void (*StartTx)(const struct usbdevice_ *usbd, uint8_t epn);
};

#endif
