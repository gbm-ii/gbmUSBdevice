/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32F0-specific setup routines for USB
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

#include "stm32f0yy.h"

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

// minimal clock setup required for USB device operation
static inline void ClockSetup(void)
{
	// set HCLK to HSI48 synchronized to SOF for USB
	RCC->CR2 |= RCC_CR2_HSI48ON;
    RCC->APB1ENR |= RCC_APB1ENR_CRSEN;
	FLASH->ACR |= FLASH_ACR_PRFTBE | 1u << FLASH_ACR_LATENCY_Pos; // 1 WS if > 24 MHz

	while (~RCC->CR2 & RCC_CR2_HSI48RDY);
	RCC->CFGR = RCC_CFGR_SW_HSI48;
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
#ifdef SYSCFG_CFGR1_PA11_PA12_RMP
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
	SYSCFG->CFGR1 |= SYSCFG_CFGR1_PA11_PA12_RMP;
#endif
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
	// With F0 series, MODER and OSPEEEDR value is "don't care" for USB data pins
}

// board LED/Button setup needed for HID demo
static inline void LED_Btn_Setup(void)
{
#ifdef LED_PORT
	RCC->AHBENR |= RCC_IOENR_GPIOEN(LED_PORT);
	BF2F(LED_PORT->MODER, LED_BIT) = GPIO_MODER_OUT;
#endif
#ifdef BTN_PORT
	RCC->AHBENR |= RCC_IOENR_GPIOEN(BTN_PORT);
	BF2F(BTN_PORT->PUPDR, BTN_BIT) = GPIO_PUPDR_PD;
	BF2F(BTN_PORT->MODER, BTN_BIT) = GPIO_MODER_IN;
#endif
}

static inline void hwLED_Set(bool on)
{
#ifdef LED_PORT
	LED_PORT->BSRR = on ? LED_MSK : LED_MSK << 16;
#endif
}

#endif /* INC_MCU_HW_H_ */
