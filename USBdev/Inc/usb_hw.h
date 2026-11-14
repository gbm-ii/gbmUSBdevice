/*
* usb_hw.h - definitions for USB peripheral hardware feature
*/

#ifndef USB_HW_H_
#define USB_HW_H_

enum usb_epstate_ {USB_EPSTATE_DISABLE, USB_EPSTATE_STALL, USB_EPSTATE_NAK, USB_EPSTATE_VALID};

#include "mcutype.h"

#ifdef USB_DEV_F1
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ f1_fs_services;
#define usb_hw_services	f1_fs_services

#define USB_IRQn	USB_LP_CAN1_RX0_IRQn
#define USB_IRQHandler USB_LP_CAN1_RX0_IRQHandler

#ifndef USB_BASE
#define USB_BASE              (APB1PERIPH_BASE + 0x00005C00UL) /*!< USB_IP Peripheral Registers base address */
#endif

#elif defined(USB_DEV_L0)
// USB FS Device - F0, L0, L5, ...
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u

extern const struct USBhw_services_ l0_fs_services;
#define usb_hw_services	l0_fs_services

#ifdef STM32L5
#define USB_IRQn	USB_FS_IRQn
#define USB_IRQHandler	USB_FS_IRQHandler
#endif

#elif defined(USB_DRD_G0)

#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

#ifdef STM32G0
#define USB_IRQn	USB_UCPD1_2_IRQn
#define USB_IRQHandler	USB_UCPD1_2_IRQHandler

#elif defined(STM32H5)

#ifdef USB_DRD_BASE_NS
#define USB_BASE	USB_DRD_BASE_NS
#else
#define USB_BASE	USB_DRD_FS_BASE
#endif

#define USB_PMA_OFFSET ((USB_DRD_PMAADDR_NS) - (USB_DRD_BASE_NS))

#elif defined(STM32U5)

#define USB_BASE	USB_DRD_BASE_NS
#define USB_PMA_OFFSET ((USB_DRD_PMAADDR_NS) - (USB_DRD_BASE_NS))

#elif defined(STM32U3)
#define USB_BASE	USB_DRD_BASE
#define USB_IRQn	USB_FS_IRQn
#define USB_IRQHandler	USB_FS_IRQHandler

#elif defined(STM32C5)
#define USB_PMA_OFFSET ((USB_DRD_PMAADDR) - (USB_DRD_FS_BASE))
#define USB_BASE	USB_DRD_FS_BASE
#endif

#ifndef USB_PMA_OFFSET
#define USB_PMA_OFFSET ((USB_DRD_PMAADDR) - (USB_DRD_BASE))
#endif

#elif defined(USB_OTG_FS_L4)

// USB OTG
extern const struct USBhw_services_ l4_otgfs_services;
#define usb_hw_services	l4_otgfs_services

#if defined(STM32F401xC)
#define USB_NEPPAIRS	4u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	3u

#elif defined(STM32L4)
#define USB_NEPPAIRS	6u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u

#elif defined(STM32U5)


//#if 0	// debug ----------------------------

#if defined(STM32U5A5xx)
#define USB_NEPPAIRS	9u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	0xfu

#define USB_IRQn	OTG_HS_IRQn
#define USB_IRQHandler	OTG_HS_IRQHandler
#define USB_BASE USB_OTG_HS_BASE_NS
#else	// FS

#define USB_NEPPAIRS	6u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u

#define USB_IRQn	OTG_FS_IRQn
#define USB_IRQHandler	OTG_FS_IRQHandler
#define USB_BASE USB_OTG_FS_BASE_NS
#endif

//#endif	//debug -----------------------------------

#endif

#ifdef USB_OTG_FS_PERIPH_BASE	// F4, L4 series
#define USB_BASE	USB_OTG_FS_PERIPH_BASE
#define USB_IRQn	OTG_FS_IRQn
#define USB_IRQHandler	OTG_FS_IRQHandler
#endif


#else
#error USB interface type not supported in usb_hw.h
#endif

#endif
