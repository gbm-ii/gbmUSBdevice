
# gbmUSBdevice

A compact, simple, HAL-free USB device stack for easy creation of composite devices with STM32 MCUs.

Supported MCUs: STM32: F0x2, L0, L4, F4 (OTG FS only), G0B1, H503, L5, U535/545, U575/585.
Planned support: STM32F103, STM32U59x and above, STM32H7, CH32X03.

Supported USB classes: CDC ACM (VCP), Printer, HID.
Planned: MSC BOT SCSI.

## Quick start using STM32CubeIDE

Out-of-the-box demo is currently available for STM32F0x2, G0B1, L4, H503, U535, U575. The demo uses pure event-driven approach with software-generated interrupts.
Example code for other MCUs as well as more conventional, event loop-based version of demo will be added soon.
By default, the example code creates a generic text printer device and two VCPs (only one VCP on F4 series due to only 3 application endpoints available in its USB OTG FS peripheral).
The demo compiles to little over 5 kB with `-Os` option.

To compile and run the example using STM32CubeIDE:

- Download or clone the github.com/gbm-ii/gbmUSBdevice and github.com/gbm-ii/STM32_Inc repositories to your computer.
- Create a *New STM32 project* (STM32Cube type) for your MCU model. Do not change any settings; just generate code for the default configuration.
- Copy or link the `gbmUSBdevice` folder to include it in your project. Set the folder as source folder
 (right click on the folder name, *Properties - C/C++ Build* - uncheck *Exclude resource from build*).
- Copy or link the `STM32_Inc` folder to your project.
- Remove or rename the `main()` function in CubeMX-generated `main.c` file - `main()` from gbmUSBdevice example will be used instead of it.
- Add the `gbmUSBdevice/Example/Inc/<MCU_series>`, `gbmUSBdevice/USBdev` and `STM32_Inc` folders to the inlcude path; they should appear in the include path in exactly that order.

To see the demo in action:

- Connect the MCU's USB interface to a PC.
- Open the terminal sessions for the VCPs (TeraTerm seems to be good choice for Windows); once connected, the demo displays prompt with the VCOM logical number in a terminal window.
- The text typed in a terminal window is echoed. The text printed to the printer is displayed in VCOM0 terminal window.

Composite device configuration may be changed by editing `usb_dev_config.h` file. Newly added HID keyboard demo was tested on U545.

## Using gbmUSBdevice with HAL & CubeMX-generated code

Before calling `USBapp_Init()`:
- MCU and USB clocking must be set properly. This may be achieved using CubeMX-generated routine, replacing the `ClockSetup()` supplied with the examples. USB clock must be generated using:
	- PLL fed by HSE (all STM32; the only option for F0, F4)
	- HSI48 synchronized via CRS to USB SOF (F0x2, G0B1, L0, L4 excluding L47x, L5, U5, H5, H7
	- PLL fed by HSI synchronized to LSE (STM32L47x, esp. Nucleo-L476 board)
- USB peripheral pins must be configured properly and the USB module must be activated. 
 In the examples it is done by `USBhwSetup()`. The replacement code may be generated by CubeMX if USB module is activated in device mode without selecting any USB-related middleware.

