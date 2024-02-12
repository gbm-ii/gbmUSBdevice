/*
 * usbdev_binding.h
 *
 *  Created: 2024
 *   Author:
 */

#ifndef INC_USBDEV_BINDING_H_
#define INC_USBDEV_BINDING_H_

// binding for L47x and above

#define USB_IRQ_PRI	12

#define VCOM0_rx_IRQn	DFSDM1_FLT0_IRQn
#define VCOM0_rx_IRQHandler	DFSDM1_FLT0_IRQHandler

#define VCOM0_tx_IRQn	DFSDM1_FLT1_IRQn
#define VCOM0_tx_IRQHandler	DFSDM1_FLT1_IRQHandler

#define VCOM1_rx_IRQn	FPU_IRQn
#define VCOM1_rx_IRQHandler	FPU_IRQHandler

#define VCOM1_tx_IRQn	SAI1_IRQn
#define VCOM1_tx_IRQHandler	SAI1_IRQHandler

#define PRN_rx_IRQn	FLASH_IRQn
#define PRN_rx_IRQHandler	FLASH_IRQHandler

#endif /* INC_USBDEV_BINDING_H_ */
