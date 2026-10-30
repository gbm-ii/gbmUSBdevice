/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32C5-specific setup routines for USB
 * Copyright (c) 2026 gbm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INC_MCU_HW_H_
#define INC_MCU_HW_H_

#include <stdbool.h>
#include "stm32c5yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc
#if (__STDC_VERSION__ >= 202000L) && __has_include("board.h")
	#include "board.h"
#endif

// must be included after board defs!
#include "stm32gpioutil.h"

#ifndef HCLK_FREQ
#define HCLK_FREQ	144000000u
#endif

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

// minimal clock setup required for USB device operation
static inline void ClockSetup(void)
{
	// set HCLK to 48 MHz HSISDIV3 synchronized to SOF
	RCC->CR1 |= RCC_CR1_HSISON;
	FLASH->ACR |= FLASH_ACR_PRFTEN | (HCLK_FREQ / 34000000u) << FLASH_ACR_LATENCY_Pos
		| (HCLK_FREQ / 68000000u) << FLASH_ACR_WRHIGHFREQ_Pos;
    RCC->APB1LENR |= RCC_APB1LENR_CRSEN;
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// sync HSI48 to USB SOF

	while (~RCC->CR1 & RCC_CR1_HSISRDY);
	RCC->CFGR1 = RCC_CFGR1_SW_HSIS;
	RCC->CCIPR2 = RCC_CCIPR2_CK48SEL_1;
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USBEN;
	// With C0 series, MODER and OSPEEEDR value is "don't care" for USB data pins
	BF2F(GPIOA->MODER, 11) = GPIO_MODER_IN;
	BF2F(GPIOA->MODER, 12) = GPIO_MODER_IN;
	BF2F(GPIOA->OSPEEDR, 11) = GPIO_OSPEEDR_VHI;
	BF2F(GPIOA->OSPEEDR, 12) = GPIO_OSPEEDR_VHI;
}

#endif /* INC_MCU_HW_H_ */
