/*
* usb_hw.h - definitions for USB peripheral hardware feature
* gbm 2022..2026
*
* The main purpose of this file is to provide uniform interface to USB hardware peripheral,
* regardless of particular MCU version - define common names for peripheral access,
* interrupt request and interrupt handler.
* The names tend to change with new revisions of chip support packages,
* so they need periodical updates.
*/

#ifndef USB_HW_H_
#define USB_HW_H_

#include "mcutype.h"

#ifdef USB_DEV_F1
// Classic STM32F1 USB device peripheral - the old version
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ f1_fs_services;
#define usb_hw_services	f1_fs_services

#define USB_IRQn	USB_LP_CAN1_RX0_IRQn
#define USB_IRQHandler USB_LP_CAN1_RX0_IRQHandler

#ifndef USB_BASE
#define USB_BASE (APB1PERIPH_BASE + 0x00005C00UL) /*!< USB_IP Peripheral Registers base address */
#endif

#elif defined(USB_DEV_L0)
// USB FS Device - F0, L0, L4x2, L5, G4, ... - intermediate version
// 1024 B PMA, 16-bit access
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u

extern const struct USBhw_services_ l0_fs_services;
#define usb_hw_services	l0_fs_services

#ifdef STM32L5
#define USB_IRQn	USB_FS_IRQn
#define USB_IRQHandler	USB_FS_IRQHandler
#elif defined(STM32G4)
#define USB_IRQn	USB_LP_IRQn
#define USB_IRQHandler	USB_LP_IRQHandler
#endif

#elif defined(USB_DRD_G0)
// Modern version of USB dual role device/host - C0, C5, G0, H5, U0, U535/545
#define USB_NEPPAIRS	8u	// no. of endpoint pairs supported by hardware
#define EPNUMMSK	7u
extern const struct USBhw_services_ g0_fs_services;
#define usb_hw_services	g0_fs_services

#ifdef STM32G0
#define USB_IRQn	USB_UCPD1_2_IRQn
#define USB_IRQHandler	USB_UCPD1_2_IRQHandler

#elif defined(STM32C0) || defined(STM32C5) || defined(STM32G5) || defined(STM32H5) || defined(STM32U0)
#define USB_IRQn	USB_DRD_FS_IRQn
#define USB_IRQHandler	USB_DRD_FS_IRQHandler

#ifdef STM32G5
// G5 headers introduce new bit names
#define USB_CNTR_RESETM USB_CNTR_RST_DCONM
#define USB_BCDR_DPPU USB_BCDR_DPPU_DPD
#define USB_CHEP_ADDR USB_CHEP_EA
#define	USB_EP_KIND	USB_CHEP_EPKIND
#define USB_CHEP_ERRTX USB_CHEP_ERR_TX
#define USB_CHEP_ERRRX USB_CHEP_ERR_RX
#define USB_EP_DTOG_TX USB_EP_DTOGTX
#define USB_EP_DTOG_RX USB_EP_DTOGRX
#define USB_EP_TX_STTX USB_EP_STATTX
#define USB_CHEP_TX_STTX_Pos USB_CHEP_STATTX_Pos
#define USB_EP_RX_STRX USB_EP_STATRX
#define USB_CHEP_RX_STRX_Pos USB_CHEP_STATRX_Pos
#endif

#elif defined(STM32U3)
#define USB_IRQn	USB_FS_IRQn
#define USB_IRQHandler	USB_FS_IRQHandler

#endif

#ifndef USB_BASE

#ifdef USB_DRD_BASE_NS	// C5, H5, U5, G5
#define USB_BASE	USB_DRD_BASE_NS
#elif defined(USB_DRD_FS_BASE)
#define USB_BASE	USB_DRD_FS_BASE
#else
#define USB_BASE	USB_DRD_BASE
#endif

#endif

#ifndef USB_PMAADDR
#ifdef USB_DRD_PMADDR_NS	// C5, H5, U5, G5
#define USB_PMAADDR USB_DRD_PMAADDR_NS
#elif defined(USB_DRD_PMAADDR)
#define USB_PMAADDR USB_DRD_PMAADDR
#endif
#endif

#define USB_PMA_OFFSET ((USB_PMAADDR) - (USB_BASE))


#elif defined(USB_OTG_FS_L4) //================================================

// USB OTG - F4, L4x5/6, U575
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
#else	// FS - U575

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
