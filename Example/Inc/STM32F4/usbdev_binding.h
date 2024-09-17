/*
 * usbdev_binding.h
 *
 *  Created on: Oct 22, 2023
 *      Author: gbm
 */

#ifndef INC_USBDEV_BINDING_H_
#define INC_USBDEV_BINDING_H_

// binding for F401

#define USB_IRQ_PRI	12

#define VCOM0_rx_IRQn	FPU_IRQn
#define VCOM0_rx_IRQHandler	FPU_IRQHandler

#define VCOM0_tx_IRQn	SDIO_IRQn
#define VCOM0_tx_IRQHandler	SDIO_IRQHandler

#define VCOM1_tx_IRQn	I2C1_ER_IRQn
#define VCOM1_tx_IRQHandler	I2C1_ER_IRQHandler

#define VCOM1_rx_IRQn	I2C1_EV_IRQn
#define VCOM1_rx_IRQHandler	I2C1_EV_IRQHandler

#define PRN_rx_IRQn	FLASH_IRQn
#define PRN_rx_IRQHandler	FLASH_IRQHandler

#endif /* INC_USBDEV_BINDING_H_ */
