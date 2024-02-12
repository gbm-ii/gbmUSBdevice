
# gbmUSBdevice

A compact, simple, HAL-free USB device stack for easy creation of composite devices with STM32 MCUs.

Supported MCUs: STM32: F042, F072, L0 series, L4 series, F4 series (OTG FS only), G0B1, H503, L5 series, U535/545, U575/585.
Planned support: STM32F103, STM32U59x and above, STM32H7, CH32X03.

Supported USB classes: CDC ACM (VCP), Printer.
Planned: HID, MSC BOT SCSI.

## Quick start using STM32CubeIDE

Out-of-the-box demo is currently available for STM32G0B1, L4, H503, U535, U575. Example code for other MCUs will be added soon.
By default, the example code creates a generic text printer device and two VCPs (only one VCP on F4 series due to only 3 application endpoints available in its USB OTG FS peripheral).
The demo compiles to little over 5 kB with `-Os` option.

To compile and run the example using STM32CubeIDE:

- Download or clone the github.com/gbm-ii/gbmUSBdevice and github.com/gbm-ii/STM32_Inc repositories to your computer.
- Create a *New STM32 project* (STM32Cube type) for your MCU model. Do not change any settings; just generate code for the default configuration.
- Copy or link the `gbmUSBdevice` folder to include it in your project. Set the folder as source folder
 (right click on the folder name, *Properties - C/C++ Build* - uncheck *Exclude resource from build*).
- Copy or link the `STM32_Inc folder` to your project.
- Remove or rename the main function in CubeMX-generated `main.c` file - `main()` from gbmUSBdevice example will be used instead of it.
- Add the `gbmUSBdevice/Example/Inc/<your_MCU_series>`, `gbmUSBdevice/USBdev` and `STM32_Inc` folders to the inlcude path; they should appear in the include path in exactly that order.

To see the demo in action:

- Connect the MCU's USB interface to a PC.
- Open the terminal sessions for the VCPs (TeraTerm seems to be good choice for Windows); once connected, the demo displays prompt with the VCOM logical number in a terminal window.
- The text typed in a terminal window is echoed. The text printed to the printer is displayed in VCOM0 terminal window.
