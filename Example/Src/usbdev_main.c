/**
  ******************************************************************************
  * @file           : usbdev_main.c
  * @brief          : gbmUSBdevice example
  * gbm 2024
  ******************************************************************************
  */

#include "mcu_hw.h"		// in Example/Inc/<mcu_series> directory
#include "usb_app.h"	// in USBdevice/Inc

// usbdev demo application
static void usbdev_main(void)
{
	ClockSetup();	// Setup MCU and USB peripheral clock - may be replaced by CubeMX/HAL routine
	USBhwSetup();	// Turn on USB peripheral and setup its pins - may be replaced by CubeMX/HAL routine

	USBapp_Init();	// Initialize and start USB stack

	for (;;)
		__WFI();
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