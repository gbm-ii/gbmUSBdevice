/**
  ******************************************************************************
  * @file           : usbdev_main.c
  * @brief          : gbmUSBdevice example
  * gbm 2024
  ******************************************************************************
  */

/*
 * mcu_hw.h includes board.h if it is present
 * board.h defines basic board features (clocking, port connections, etc.).
 * To run demo on one of common STM32 boards, like Nucleo, Discovery or any "pill",
 * create board.h and include in it one of files found in STM32_Inc/boards.
 */
#include <stdbool.h>
#include "mcu_hw.h"		// in Example/Inc/<mcu_series> directory
#include "stm32gpioutil.h"
#include "usb_app.h"	// in USBdevice/Inc

void LED_Set(bool on)
{
	hwLED_Set(on);
}

bool BtnGet(void)
{
#ifdef BTN_PORT
	return BTN_DOWN;
#else
	return 0;
#endif
}

// usbdev demo application
static void usbdev_main(void)
{
	ClockSetup();	// Setup MCU and USB peripheral clock - may be replaced by CubeMX/HAL routine
	LED_Btn_Setup();
	USBhwSetup();	// Turn on USB peripheral and setup its pins - may be replaced by CubeMX/HAL routine

	USBapp_Init();	// Initialize and start USB stack

	for (;;)
	{
#ifdef POLLED
		USBapp_poll();
#else
		__WFI();
#endif
	}
}

/**
  * @brief  Application entry point.
  * @retval n/a
  *
  *	Defined as weak so that the full gbmUSBdev code with this demo
  *	may be included in the real application source code tree.
  */
__attribute__((weak)) int main(void)
{
	usbdev_main();
}
