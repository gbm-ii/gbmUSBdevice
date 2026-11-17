/*
 mcutype.h - include the mcu header based on preprocessor symbol defined
 define symbols for MCU features
 gbm 11'2026
*/

#ifndef MCUTYPE_H_
#define MCUTYPE_H_

#if defined(STM32F10X_MD) || defined(STM32F103xB)
//#include "stm32f10x.h"
#include "stm32f1xx.h"
#define USB_DEV_F1

#elif defined(STM32F072xB) || defined(STM32F042x6)
#include "stm32f0xx.h"
#define USB_DEV_L0

#elif defined(STM32L073xx)
#include "stm32l0xx.h"
#define USB_DEV_L0

#elif defined(STM32L412xx) || defined(STM32L422xx) || defined(STM32L432xx) || defined(STM32L433xx) \
	|| defined(STM32L442xx) || defined(STM32L443xx) || defined(STM32L452xx) || defined(STM32L462xx)
#include "stm32l4xx.h"
#define USB_DEV_L0

#elif defined(STM32L552xx)
#include "stm32l5xx.h"
#define USB_DEV_L0

#elif defined(STM32G474xx)
#include "stm32g4xx.h"
#define USB_DEV_L0

#elif defined(STM32G0B1xx) || defined(STM32G0C1xx)
#include "stm32g0xx.h"
#define USB_DRD_G0

#elif defined(STM32U073xx) || defined(STM32U083xx)
#include "stm32u0xx.h"
#define USB_DRD_G0

#elif defined(STM32C071xx)
#include "stm32c0xx.h"
#define USB_DRD_G0

#elif defined(STM32H503xx) || defined(STM32H523xx) || defined(STM32H533xx) \
	|| defined(STM32H543xx) || defined(STM32H553xx) \
	|| defined(STM32H562xx) || defined(STM32H563xx) || defined(STM32H573xx)
#include "stm32h5xx.h"
#define USB_DRD_G0

#elif defined(STM32U535xx) || defined(STM32U545xx) 
#include "stm32u5xx.h"
#define USB_DRD_G0

#elif defined(STM32C531xx) || defined(STM32C532xx) || defined(STM32C542xx) \
 || defined(STM32C551xx) || defined(STM32C552xx) || defined(STM32C562xx) \
 || defined(STM32C591xx) || defined(STM32C593xx) || defined(STM32C5A3xx)
#include "stm32c5xx.h"
#define USB_DRD_G0

#elif defined(STM32U375xx) || defined(STM32U385xx) || defined(STM32U356xx) || defined(STM32U366xx) \
	|| defined(STM32U3B5xx) || defined(STM32U3C5xx)
#include "stm32u3xx.h"
#define USB_DRD_G0

#elif defined(STM32F401xC) || defined(STM32F411xC)
#include "stm32f4xx.h"
#define USB_OTG_FS_L4

#elif defined(STM32L475xx) || defined(STM32L476xx) || defined(STM32L496xx) || defined(STM32L4A6xx) \
	|| defined(STM32L4P5xx) || defined(STM32L4R5xx) || defined(STM32L4S5xx)
#include "stm32l4xx.h"
#define USB_OTG_FS_L4

#elif defined(STM32U575xx) || defined(STM32U585xx) || defined(STM32U5A5xx)
#include "stm32u5xx.h"
#define USB_OTG_FS_L4

#else
#error Add your MCU support!
#endif

#endif
