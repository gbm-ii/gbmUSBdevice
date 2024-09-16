/*
 * usb_app.h
 *
 *  Created on: Dec 5, 2023
 *      Author: gbm
 */

#ifndef INC_USB_APP_H_
#define INC_USB_APP_H_

#include <stdint.h>

void usbdev_tick(void);
void USBapp_Init(void);
void USBapp_DeInit(void);
void USBapp_Poll(void);

void vcom0_putc(uint8_t c);
void vcom_write(uint8_t ch, const char *buf, uint16_t size);
void vcom_putchar(uint8_t ch, char c);
void vcom_putstring(uint8_t ch, const char *s);
void vcom_prompt_request(uint8_t ch);

#endif /* INC_USB_APP_H_ */
