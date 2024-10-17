/*
* usb_hw.h - definitions for USB peripheral hardware feature
*/

#ifndef USB_HW_H_
#define USB_HW_H_

enum usb_epstate_ {USB_EPSTATE_DISABLE, USB_EPSTATE_STALL, USB_EPSTATE_NAK, USB_EPSTATE_VALID};

// implementation-specific defs
#ifdef USB_OTG_FS_PERIPH_BASE	// F4, L4 series
#define USB_BASE	USB_OTG_FS_PERIPH_BASE
#define USB_IRQn	OTG_FS_IRQn
#define USB_IRQHandler	OTG_FS_IRQHandler
#endif

#if defined(STM32F10X_MD) || defined(STM32F103xB)
//#include "stm32f10x.h"
#include "stm32f1xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ f1_fs_services;
#define usb_hw_services	f1_fs_services

#define USB_IRQn	USB_LP_CAN1_RX0_IRQn
#define USB_IRQHandler USB_LP_CAN1_RX0_IRQHandler

/* USB device FS */
#ifndef USB_BASE
#define USB_BASE              (APB1PERIPH_BASE + 0x00005C00UL) /*!< USB_IP Peripheral Registers base address */
#endif

// USB FS Device - F0, L0, L5, ...
#elif defined(STM32F072xB) || defined(STM32F042x6)
#include "stm32f0xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ l0_fs_services;
#define usb_hw_services	l0_fs_services

#elif defined(STM32L073xx)
#include "stm32l0xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ l0_fs_services;
#define usb_hw_services	l0_fs_services

#elif defined(STM32L552xx)
#include "stm32l5xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ l0_fs_services;
#define usb_hw_services	l0_fs_services

#define USB_IRQn	USB_FS_IRQn
#define USB_IRQHandler	USB_FS_IRQHandler

#elif defined(STM32G0B1xx)
#include "stm32g0xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

#define USB_IRQn	USB_UCPD1_2_IRQn
#define USB_IRQHandler	USB_UCPD1_2_IRQHandler

#define USB_PMA_OFFSET ((USB_DRD_PMAADDR) - (USB_DRD_BASE))

#elif defined(STM32U073xx) || defined(STM32U083xx)
#include "stm32u0xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

#define USB_IRQn	USB_DRD_FS_IRQn
#define USB_IRQHandler	USB_DRD_FS_IRQHandler

#define USB_BASE	USB_DRD_BASE
#define USB_PMA_OFFSET ((USB_DRD_PMAADDR) - (USB_DRD_BASE))

#elif defined(STM32C071xx)
#include "stm32c0xx.h"
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

#define USB_IRQn	USB_DRD_FS_IRQn
#define USB_IRQHandler	USB_DRD_FS_IRQHandler

#define USB_PMA_OFFSET ((USB_DRD_PMAADDR) - (USB_DRD_BASE))

#elif defined(STM32H503xx) || defined(STM32H533xx) || defined(STM32H563xx)
#include "stm32h5xx.h"
// like G0B1
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

#define USB_IRQn	USB_DRD_FS_IRQn
#define USB_IRQHandler	USB_DRD_FS_IRQHandler

#ifdef USB_DRD_BASE_NS
#define USB_BASE	USB_DRD_BASE_NS
#else
#define USB_BASE	USB_DRD_FS_BASE
#endif
#define USB_PMA_OFFSET ((USB_DRD_PMAADDR_NS) - (USB_DRD_BASE_NS))

#elif defined(STM32U535xx) || defined(STM32U545xx)
#include "stm32u5xx.h"
// like G0B1
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

//#define USB_IRQn	USB_DRD_FS_IRQn
//#define USB_IRQHandler	USB_DRD_FS_IRQHandler

#define USB_BASE	USB_DRD_BASE_NS
#define USB_PMA_OFFSET ((USB_DRD_PMAADDR_NS) - (USB_DRD_BASE_NS))


// USB OTG

#elif defined(STM32F401xC)
#include "stm32f4xx.h"
#define USB_NEPPAIRS	4u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	3u
extern const struct USBhw_services_ l4_otgfs_services;
#define usb_hw_services	l4_otgfs_services

#elif defined(STM32L476xx) || defined(STM32L496xx) || defined(STM32L4P5xx) || defined(STM32L4R5xx)
#include "stm32l4xx.h"
#define USB_NEPPAIRS	6u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ l4_otgfs_services;
#define usb_hw_services	l4_otgfs_services

#elif defined(STM32U575xx) || defined(STM32U585xx) || defined(STM32U5A5xx)
#include "stm32u5xx.h"

#if defined(STM32U5A5xx)
#define USB_NEPPAIRS	9u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	0xfu
extern const struct USBhw_services_ l4_otgfs_services;
#define usb_hw_services	l4_otgfs_services

#define USB_IRQn	OTG_HS_IRQn
#define USB_IRQHandler	OTG_HS_IRQHandler
#define USB_BASE USB_OTG_HS_BASE_NS
#else
#define USB_NEPPAIRS	6u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ l4_otgfs_services;
#define usb_hw_services	l4_otgfs_services

#define USB_IRQn	OTG_FS_IRQn
#define USB_IRQHandler	OTG_FS_IRQHandler
#define USB_BASE USB_OTG_FS_BASE_NS
#endif

#else
#error Add your MCU support in usb_hw.h
#endif

// implementation-specific defs
#ifdef USB_OTG_FS_PERIPH_BASE	// F4, L4 series
#define USB_BASE	USB_OTG_FS_PERIPH_BASE
#define USB_IRQn	OTG_FS_IRQn
#define USB_IRQHandler	OTG_FS_IRQHandler
#endif

#endif
