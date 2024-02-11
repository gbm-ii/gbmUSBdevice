/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32G0-specific setup routines for USB
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

#include "stm32g0yy.h"

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

// minimal clock setup required for USB device operation
static inline void ClockSetup(void)
{
	// set HCLK to 64 MHz PLL fed by HSI, use HSI48 synchronized to SOF for USB
	// VCO = 64..344 MHz
	// PWR Range 1 is the default - ok for 64 MHz
	RCC->PLLCFGR = RCC_PLLCFGR_PLLNV(8) | RCC_PLLCFGR_PLLRV(2) | RCC_PLLCFGR_PLLREN | RCC_PLLCFGR_PLLSRC_HSI;
	RCC->CR |= RCC_CR_HSI48ON | RCC_CR_PLLON;
	FLASH->ACR |= FLASH_ACR_PRFTEN | 2u << FLASH_ACR_LATENCY_Pos; // 2 WS up to 64 MHz
    RCC->APBENR1 |= RCC_APBENR1_CRSEN;
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// sync HSI48 to USB SOF

	while (~RCC->CR & RCC_CR_PLLRDY);
	RCC->CFGR = RCC_CFGR_SW_PLLRCLK;
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
    RCC->APBENR1 |= RCC_APBENR1_PWREN | RCC_APBENR1_USBEN;
	PWR->CR2 |= PWR_CR2_USV;
	// With G0 series, MODER and OSPEEEDR value is "don't care" for USB data pins
}

// board LED/Button setup needed for HID demo
static inline void LED_Btn_Setup(void)
{
#ifdef LED_PORT
	RCC->IOPENR |= RCC_IOPENR_GPIOEN(LED_PORT);
	BF2F(LED_PORT->MODER, LED_BIT) = GPIO_MODER_OUT;
#endif
}

#endif /* INC_MCU_HW_H_ */
