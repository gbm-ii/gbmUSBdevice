/*
 * usbdev_binding.h
 *
 *  Created on: Oct 22, 2023
 *      Author: gbm
 */

#ifndef INC_USBDEV_BINDING_H_
#define INC_USBDEV_BINDING_H_

// binding for U5xx series

#define USB_IRQ_PRI	12

#define VCOM0_rx_IRQn	RCC_IRQn
#define VCOM0_rx_IRQHandler	RCC_IRQHandler

#define VCOM0_tx_IRQn	RAMCFG_IRQn
#define VCOM0_tx_IRQHandler	RAMCFG_IRQHandler

#define VCOM1_rx_IRQn	FDCAN1_IT1_IRQn
#define VCOM1_rx_IRQHandler	FDCAN1_IT1_IRQHandler

#define VCOM1_tx_IRQn	FDCAN1_IT0_IRQn
#define VCOM1_tx_IRQHandler	FDCAN1_IT0_IRQHandler

#define PRN_rx_IRQn	FLASH_IRQn
#define PRN_rx_IRQHandler	FLASH_IRQHandler

#endif /* INC_USBDEV_BINDING_H_ */
