/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32U3-specific setup routines for USB
 * Copyright (c) 2024 gbm
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
#include "stm32u3yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc

#if (__STDC_VERSION__ >= 202000L) && __has_include("board.h")
	#include "board.h"
	// If board.h is present, HSE_VALUE and RCC_CR_HSESEL must be defined in board.h.
#else
#endif

// must be included after board defs!
#include "stm32gpioutil.h"

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

/*
 * Minimal clock setup routine for STM32U3
 * The routine may be replaced by any user-writen or CubeMX-generated HAL routine providing stable 48 MHz clock to USB peripheral.
 */

#ifndef HCLK_FREQ
#define HCLK_FREQ	96000000u
#endif

// frequency step for computing Flash wait states
#if defined(STM32U375xx) || defined(STM32U385xx) || defined(STM32U356xx) || defined(STM32U366xx)
#define FLASH_WS_FREQ_STEP	32000000u
#else // 3B5, 3C5
#define FLASH_WS_FREQ_STEP	25000000u
#endif

static inline void ClockSetup(void)
{
	// after reset, 12 MHz MSIS 1 div by 2 is used as HCLK
	RCC->AHB1ENR2 |= RCC_AHB1ENR2_PWREN;
//	PWR->CR3 |= PWR_CR3_REGSEL;	// turn on SMPS
//	while (~PWR->SVMSR & PWR_SVMSR_REGS) ;	// wait for SMPS ready

	RCC->CFGR4 = RCC_CFGR4_BOOSTSEL_0;	// select MSIS as booster clock
	PWR->VOSR = PWR_VOSR_BOOSTEN | PWR_VOSR_R1EN;

	// osc and PLL setup for MCUs up to U58x
	RCC->CR |= RCC_CR_HSI48ON;

	RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;

	while (~RCC->CR & RCC_CR_HSI48RDY) ;

	CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// HIS48 sync to USB SOF

	// wait for correct core voltage
	while (~PWR->VOSR & (PWR_VOSR_R1RDY | PWR_VOSR_BOOSTRDY)) ;

	FLASH->ACR = FLASH_ACR_PRFTEN | ((HCLK_FREQ - 1u) / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos;

	while (~RCC->CR & RCC_CR_MSISRDY) ;
	RCC->ICSCR1 |= RCC_ICSCR1_MSIRGSEL;
	RCC->ICSCR1 &= ~(RCC_ICSCR1_MSISSEL | RCC_ICSCR1_MSISDIV);	// MSIRC0/1
	while (~RCC->CR & RCC_CR_MSISRDY) ;

	ICACHE->CR |= ICACHE_CR_EN;
}

/*
 * USB hardware setup routine for STM32U5 - USB module enable & pin configuration
 * May be replaced by CubeMX-generated USB init routine
 */
static inline void USBhwSetup(void)
{
	RCC->AHB1ENR2 |= RCC_AHB1ENR2_PWREN;
	PWR->SVMCR |= PWR_SVMCR_IO2SV | PWR_SVMCR_USV | PWR_SVMCR_UVMEN;	// USB and IO2 power on
	RCC->APB2ENR |= RCC_APB2ENR_USB1EN;
}

#endif /* INC_MCU_HW_H_ */
