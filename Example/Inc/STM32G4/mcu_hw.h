/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32H5-specific setup routines for USB
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

#include "stm32g4yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc
#if (__STDC_VERSION__ >= 202000L) && __has_include("board.h")
	#include "board.h"
#endif

/*
 * board definition file may include the definitions:
   #define USE_HSE	// define to use HSE
   #define HSE_VALUE	24000000u	// board's Xtal osc. frequency
   #define HCLK_FREQ	240000000u	// target clock frequency
   #define HSI_DIV2_VALUE	32000000u	// default HSI (divided) frequency after reset, changed by ST setup

   The proper defs for Nucleo-64 may be found in "boards/stm32nucleo64.h" from github.com/gbm-ii/STM32_Inc
 */

#ifndef HCLK_FREQ	// may be defined in board.h
#define HCLK_FREQ	150000000u	// default target clock frequency
#endif

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */
#define FLASH_WS_FREQ_STEP_B	34000000u	// range 1 boost mode - required above 150 MHz
#define FLASH_WS_FREQ_STEP_N	30000000u
#if HCLK_FREQ > 150000000u
#define FLASH_WS_FREQ_STEP FLASH_WS_FREQ_STEP_B
#else
#define FLASH_WS_FREQ_STEP FLASH_WS_FREQ_STEP_N
#endif
/*
 * Minimal clock setup routine for STM32H5 (tested on Nucleo-H503)
 * The routine may be replaced by any user-writen or CubeMX-generated HAL routine providing stable 48 MHz clock to USB peripheral.
 */

// the routine must be modified to set HCLK above 150 MHz -> RM 6.1.5
static inline void ClockSetup(void)
{
	// initial G4 clock after reset is HSI 16 MHz
	// BUT ST-supplied SystemInit resets the divisor in RCC->CR HSIDIV field, so the clock freq is 64 MHz!
	RCC->APB1ENR1 = RCC_APB1ENR1_PWREN | RCC_APB1ENR1_CRSEN;
#if HCLK_FREQ > 150000000u
	PWR->CR5 = 0;	// raise core voltage; reset normal mode
#endif
	// osc and PLL setup
	RCC->CRRCR |= RCC_CRRCR_HSI48ON;
	while (~RCC->CRRCR & RCC_CRRCR_HSI48RDY) ;
	// PLL R output /2
	RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSI | (HSI_VALUE / 4000000u - 1) << RCC_PLLCFGR_PLLM_Pos
			| (HCLK_FREQ / 2000000u) << RCC_PLLCFGR_PLLN_Pos | RCC_PLLCFGR_PLLREN;

	RCC->CR |= RCC_CR_PLLON;
	CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// HSI48 sync to USB SOF

	// wait for PLL ready
	while (~RCC->CR & RCC_CR_PLLRDY) ;
	// no. of wait states = freq / 42 MHz; WRHIGHFREQ = freq / 84 MHz ? (See STM32H7B0 RM.)
	FLASH->ACR = FLASH_ACR_DBG_SWEN | FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_PRFTEN
			| ((HCLK_FREQ - 1u) / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos;

	// switch to PLL
	RCC->CFGR = RCC_CFGR_SW_PLL;
	// HSI48 as USB clock (default)
}

/*
 * USB hardware setup routine for STM32H5 - USB module enable & pin configuration
 * May be replaced by CubeMX-generated USB init routine
 */
static inline void USBhwSetup(void)
{
	// USB hardware setup
	RCC->APB1ENR1 |= RCC_APB1ENR1_USBEN;
	// With G4 series, MODER and OSPEEEDR value is "don't care" for USB data pins
}

#endif /* INC_MCU_HW_H_ */
