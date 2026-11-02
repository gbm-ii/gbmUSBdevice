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

#include "stm32c5yy.h"		// from github.com/gbm-ii/STM32_Inc
#if (__STDC_VERSION__ >= 202000L) && __has_include("board.h")
	#include "board.h"
#endif

#ifndef HCLK_FREQ
#define HCLK_FREQ	144000000u
#endif

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */
#define FLASH_WS_FREQ_STEP	34000000u

// minimal clock setup required for USB device operation
static inline void ClockSetup(void)
{
	// set HCLK to 144 MHz HSIS synchronized to SOF
	RCC->CR1 |= RCC_CR1_HSISON;
    RCC->APB1LENR |= RCC_APB1LENR_CRSEN;
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// sync HSIS to USB SOF

	FLASH->ACR |= FLASH_ACR_PRFTEN | (HCLK_FREQ / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos
		| (HCLK_FREQ / FLASH_WS_FREQ_STEP / 2) << FLASH_ACR_WRHIGHFREQ_Pos;
	ICACHE->CR |= ICACHE_CR_EN;

	while (~RCC->CR1 & RCC_CR1_HSISRDY);
	RCC->CFGR1 = RCC_CFGR1_SW_HSIS;
	RCC->CCIPR2 = RCC_CCIPR2_CK48SEL_1;	// select HSIDIV3 as USB clock
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_USBEN;
	// With C0, C5 series, MODER and OSPEEEDR value is "don't care" for USB data pins
}

#endif /* INC_MCU_HW_H_ */
