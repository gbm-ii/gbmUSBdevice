/*
 * usbdev_binding.h
 *
 *  Created on: Oct 22, 2023
 *      Author: Grzegorz
 */

#ifndef INC_USBDEV_BINDING_H_
#define INC_USBDEV_BINDING_H_

// binding for G0B1

#define USB_IRQ_PRI	2
//#define VCOM_TX_IRQn	I2C1_ER_IRQn
//#define VCOM_TX_IRQHandler	I2C1_ER_IRQHandler
//#define VCOM_RX_IRQn	I2C1_EV_IRQn
//#define VCOM_RX_IRQHandler	I2C1_EV_IRQHandler

#if USBD_CDC_CHANNELS
#define VCOM0_rx_IRQn	RCC_CRS_IRQn
#define VCOM0_rx_IRQHandler	RCC_CRS_IRQHandler

#define VCOM0_tx_IRQn	PVD_PVM_IRQn
#define VCOM0_tx_IRQHandler	PVD_PVM_IRQHandler

#if USBD_CDC_CHANNELS > 1
#define VCOM1_rx_IRQn	RTC_TAMP_IRQn
#define VCOM1_rx_IRQHandler	RTC_TAMP_IRQHandler

#define VCOM1_tx_IRQn	RNG_CRYP_IRQn
#define VCOM1_tx_IRQHandler	RNG_CRYP_IRQHandler
#endif	// > 1
#endif

#if USBD_PRINTER
#define PRN_rx_IRQn	FLASH_ECC_IRQn
#define PRN_rx_IRQHandler	FLASH_ECC_IRQHandler
#endif

#endif /* INC_USBDEV_BINDING_H_ */
