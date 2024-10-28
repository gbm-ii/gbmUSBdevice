/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32L4-specific setup routines for USB
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
#include "stm32l4yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc
#include "boards/stm32nucleo64.h"

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

/*
 * Minimal clock setup routine for STM32L4 (teted on Nucleo-L476, L496, L4R5)
 * The routine may be replaced by any user-writen or CubeMX-generated HAL routine providing stable 48 MHz clock to USB peripheral.
 */

#ifndef HCLK_FREQ
#define HCLK_FREQ	80000000u
#endif

// frequency step for computing Flash wait states
#ifdef DMAMUX	// present in L4+ series only
#define FLASH_WS_FREQ_STEP	20000000u	// L4Px..L4Sx - L4+
#else
#define FLASH_WS_FREQ_STEP	16000000u	// L41x..L4Ax - classic L4
#endif

static inline void ClockSetup(void)
{
#ifdef CRS
	// STM32L4x2, L49x..L4Sx - 80(L4+ - 120 max.) MHz main clock from MSI via PLL
	RCC->PLLCFGR = RCC_PLLCFGR_PLLREN | RCC_PLLCFGR_PLLNV(HCLK_FREQ / 2000000u) | RCC_PLLCFGR_PLLMV(1) | RCC_PLLCFGR_PLLSRC_MSI;
	RCC->CR |= RCC_CR_PLLON;
	// set Flash access time
	FLASH->ACR = FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_PRFTEN
		| ((HCLK_FREQ - 1u) / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos;

#if HCLK_FREQ > 80000000u
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	PWR->CR5 = 0;	// enable boost mode
#endif

	// USB clock from HSI48 synchronized to USB SOF
	RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;
	// HSI48
	RCC->CRRCR |= RCC_CRRCR_HSI48ON;	//
	while (~RCC->CRRCR & RCC_CRRCR_HSI48RDY);
	// sync HSI48 to USB SOF
//	CRS->CFGR = CRS_CFGR_SYNCSRC_USB | 34u << CRS_CFGR_FELIM_Pos | (48000000 / 1000 - 1);
	CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;

	while (~RCC->CR & RCC_CR_PLLRDY);
#else
	// STM32L47x - use MSI synchronized to LSE (compatible with Nucleo-L476)
	// USB clk from SAI1
	// Turn On LSE
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	RCC->APB1ENR1;
	PWR->CR1 |= PWR_CR1_DBP;	// Enable access to BDCR
	while (~PWR->CR1 & PWR_CR1_DBP);
	RCC->BDCR = RCC_BDCR_LSEON;
	while (~RCC->BDCR & RCC_BDCR_LSERDY);
	// LSE already ON
	RCC->CR |= RCC_CR_MSIPLLEN;	// Sync MSI to LSE
	// PLL - 80 MHz
	RCC->PLLCFGR = RCC_PLLCFGR_PLLREN | RCC_PLLCFGR_PLLNV(HCLK_FREQ / 2000000u) | RCC_PLLCFGR_PLLMV(1) | RCC_PLLCFGR_PLLSRC_MSI;
	RCC->CCIPR = 1u << RCC_CCIPR_CLK48SEL_Pos;	// select PLLSAI1 Q as 48M clock
	RCC->PLLSAI1CFGR = RCC_PLLCFGR_PLLREN | RCC_PLLCFGR_PLLNV(24) | RCC_PLLCFGR_PLLMV(1) | RCC_PLLCFGR_PLLQV(2)
			| RCC_PLLSAI1CFGR_PLLSAI1QEN;
	RCC->CR |= RCC_CR_PLLON | RCC_CR_PLLSAI1ON;	//
	// set Flash access time
	FLASH->ACR = FLASH_ACR_DCEN | FLASH_ACR_ICEN | FLASH_ACR_PRFTEN
		| ((HCLK_FREQ - 1u) / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos;
	while ((RCC->CR & (RCC_CR_PLLRDY | RCC_CR_PLLSAI1RDY)) != (RCC_CR_PLLRDY | RCC_CR_PLLSAI1RDY));
#endif
	RCC->CFGR |= RCC_CFGR_SW_PLL;
//	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/*
 * USB hardware setup routine for STM32L4 - USB module enable & pin configuration
 * May be replaced by CubeMX-generated USB init routine
 */
static inline void USBhwSetup(void)
{
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	PWR->CR2 |= PWR_CR2_USV | PWR_CR2_IOSV;	// enable VddUSB
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN	| RCC_AHB2ENR_OTGFSEN;

	AFRF(GPIOA, 11) = AFN_USB;
	AFRF(GPIOA, 12) = AFN_USB;
	BF2F(GPIOA->OSPEEDR, 11) = GPIO_OSPEEDR_HI;
	BF2F(GPIOA->OSPEEDR, 12) = GPIO_OSPEEDR_HI;
	BF2F(GPIOA->MODER, 11) = GPIO_MODER_AF;
	BF2F(GPIOA->MODER, 12) = GPIO_MODER_AF;
}

// board LED/Button setup needed for HID demo
static inline void LED_Btn_Setup(void)
{
#ifdef LED_PORT
	RCC->IOENR |= RCC_IOENR_GPIOEN(LED_PORT);
	BF2F(LED_PORT->MODER, LED_BIT) = GPIO_MODER_OUT;
#endif
#ifdef BTN_PORT
	RCC->IOENR |= RCC_IOENR_GPIOEN(BTN_PORT);
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
