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

#include "stm32h5yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

/*
 * Minimal clock setup routine for STM32H5 (teted on Nucleo-H503)
 * The routine may be replaced by any user-writen or CubeMX-generated HAL routine providing stable 48 MHz clock to USB peripheral.
 */
//#define USE_HSE
#undef HSE_VALUE
#define HSE_VALUE	24000000u	// Nucleo-H503 Xtal osc. frequency
//#define HSI_DIV2_VALUE	32000000u	// default HSI (divided) frequency after reset

#define HCLK_FREQ	240000000u	// target clock frequency

static inline void ClockSetup(void)
{
	// initial clock after reset is HSI divided by 2 -> 32 MHz
	// BUT ST-supplied SystemInit resets the divisor, so the clock freq is 64 MHz!
	PWR->VOSCR = PWR_VOSCR_VOS;	// raise core voltage; 3 -> scale 0, for highest operating frequency
	// osc and PLL setup
	// N = 240, P = 2
	RCC->PLL1DIVR = (2 - 1) << RCC_PLL1DIVR_PLL1P_Pos | (HCLK_FREQ * 2 / 48000000 - 1) << RCC_PLL1DIVR_PLL1Q_Pos | (2 - 1) << RCC_PLL1DIVR_PLL1R_Pos
		| (HCLK_FREQ / 1000000u - 1) << RCC_PLL1DIVR_PLL1N_Pos;
#ifdef USE_HSE
	RCC->CR |= RCC_CR_HSEON | RCC_CR_HSI48ON;
	while ((RCC->CR & (RCC_CR_HSERDY | RCC_CR_HSI48RDY)) != (RCC_CR_HSERDY | RCC_CR_HSI48RDY)) ;
	// 2 MHz from 24 MHz HSE -> div by 12
	RCC->PLL1CFGR = RCC_PLLxCFGR_PLLxSRC_HSE | (HSE_VALUE / 2000000u) << RCC_PLL1CFGR_PLL1M_Pos | RCC_PLL1CFGR_PLL1PEN | RCC_PLL1CFGR_PLL1QEN;
#else
	RCC->CR |= RCC_CR_HSI48ON;
	while (~RCC->CR & RCC_CR_HSI48RDY) ;
	RCC->PLL1CFGR = RCC_PLLxCFGR_PLLxSRC_HSI | (HSI_VALUE / 2000000u) << RCC_PLL1CFGR_PLL1M_Pos | RCC_PLL1CFGR_PLL1PEN | RCC_PLL1CFGR_PLL1QEN;
#endif

	RCC->CR |= RCC_CR_PLL1ON;
	RCC->APB1LENR = RCC_APB1LENR_CRSEN;
	CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// HSI48 sync to USB SOF

	// wait for correct core voltage
	while ((PWR->VOSSR & (PWR_VOSSR_ACTVOS | PWR_VOSSR_VOSRDY)) != (PWR_VOSSR_ACTVOS | PWR_VOSSR_VOSRDY)) ;
	// wait for PLL ready
	while (~RCC->CR & RCC_CR_PLL1RDY) ;
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_WRHIGHFREQ_1 | FLASH_ACR_LATENCY_5WS;
	// enable instruction cache
	while (ICACHE->SR & ICACHE_SR_BUSYF) ;
	ICACHE->CR |= ICACHE_CR_EN;

	// switch to PLL
	RCC->CFGR1 = RCC_CFGR1_SW_PLL1;
	// select USB clock
	RCC->CCIPR4 = 3 << RCC_CCIPR4_USBSEL_Pos;	// HSI48 as USB clock (default after reset = 0 - no clock selected)
	//RCC->CCIPR4 = 1 << RCC_CCIPR4_USBSEL_Pos;	// PLL1Q as USB clock
}

/*
 * USB hardware setup routine for STM32H5 - USB module enable & pin configuration
 * May be replaced by CubeMX-generated USB init routine
 */
static inline void USBhwSetup(void)
{
	// USB hardware setup
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_USBEN;

	AFRF(GPIOA, 11) = AFN_USB;
	AFRF(GPIOA, 12) = AFN_USB;
	//BF2_(GPIOA->OSPEEDR).p11 = GPIO_OSPEEDR_VHI;
	//BF2_(GPIOA->OSPEEDR).p12 = GPIO_OSPEEDR_VHI;
	BF2F(GPIOA->MODER, 11) = GPIO_MODER_AF;
	BF2F(GPIOA->MODER, 12) = GPIO_MODER_AF;
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
