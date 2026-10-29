# file-format: 1.0.0
cmake_minimum_required(VERSION 3.30)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
	usb_app.c
	usb_class.c
	usb_dev.c
	usb_hw_f1.c
	usb_hw_g0.c
	usb_hw_l0.c
	usb_hw_l4.c
	usb_log.c
	msc_bot_scsi.c
)
