/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32U5-specific setup routines for USB
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

#include "stm32u5yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

/*
 * Minimal clock setup routine for STM32L4 (teted on Nucleo-L476, L496, L4R5)
 * The routine may be replaced by any user-writen or CubeMX-generated HAL routine providing stable 48 MHz clock to USB peripheral.
 */

#ifndef HCLK_FREQ
#define HCLK_FREQ	240000000u
#endif

// frequency step for computing Flash wait states
#define FLASH_WS_FREQ_STEP	32000000u

static inline void ClockSetup(void)
{
	// after reset, 4 MHz MSIS is used as HCLK
	RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
	PWR->VOSR |= PWR_VOSR_BOOSTEN | PWR_VOSR_VOS;	// raise core voltage; 3 -> scale 1, for highest operating frequency
	PWR->CR3 |= PWR_CR3_REGSEL;	// turn on SMPS
	while (~PWR->SVMSR & PWR_SVMSR_REGS) ;

	// osc and PLL setup
	RCC->CR |= RCC_CR_HSI48ON;

	RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;

	while (~RCC->CR & RCC_CR_HSI48RDY) ;
	// HCLK from PLL1R, PLL input clock 4..16 MHz, PLL freq 128..544 MHz
	// M = 1, N = 80, P = 2
	RCC->PLL1DIVR = (2u - 1) << RCC_PLL1DIVR_PLL1P_Pos | (2u - 1) << RCC_PLL1DIVR_PLL1Q_Pos | (2u - 1) << RCC_PLL1DIVR_PLL1R_Pos
		| (HCLK_FREQ / 2000000u - 1) << RCC_PLL1DIVR_PLL1N_Pos;
	RCC->PLL1CFGR = RCC_PLL1CFGR_PLL1SRC_0 | (1u - 1) << RCC_PLL1CFGR_PLL1M_Pos | RCC_PLL1CFGR_PLL1REN;	// MSIS, no div
	RCC->CR |= RCC_CR_PLL1ON;

	CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;	// HIS48 sync to USB SOF

	// wait for correct core voltage
	while (~PWR->VOSR & (PWR_VOSR_VOSRDY | PWR_VOSR_BOOSTRDY)) ;
	// wait for PLL ready
	while (~RCC->CR & RCC_CR_PLL1RDY) ;
	FLASH->ACR = FLASH_ACR_PRFTEN | ((HCLK_FREQ - 1u) / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos;

	while (ICACHE->SR & ICACHE_SR_BUSYF) ;
	ICACHE->CR |= ICACHE_CR_EN;
	// switch to PLL
	RCC->CFGR1 = RCC_CFGR1_SW_PLL1;
	// select USB clock
	//RCC->CCIPR1 = 0 << RCC_CCIPR1_ICLKSEL_Pos;	// HSI48 as USB clock - default after reset
	//RCC->CCIPR4 = 1 << RCC_CCIPR4_USBSEL_Pos;	// PLL1Q as USB clock
}

/*
 * USB hardware setup routine for STM32U5 - USB module enable & pin configuration
 * May be replaced by CubeMX-generated USB init routine
 */
static inline void USBhwSetup(void)
{
	RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
	PWR->SVMCR |= PWR_SVMCR_IO2SV | PWR_SVMCR_USV;	// USB and IO2 power on
#ifdef RCC_APB2ENR_USBEN	// U535/U545 USB device
	RCC->APB2ENR |= RCC_APB2ENR_USBEN;
#else	// U575/585 USB OTG
	RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN | RCC_AHB2ENR1_OTGEN;

	AFRF(GPIOA, 11) = AFN_USB;
	AFRF(GPIOA, 12) = AFN_USB;
	BF2F(GPIOA->OSPEEDR, 11) = GPIO_OSPEEDR_VHI;
	BF2F(GPIOA->OSPEEDR, 12) = GPIO_OSPEEDR_VHI;
	BF2F(GPIOA->MODER, 11) = GPIO_MODER_AF;
	BF2F(GPIOA->MODER, 12) = GPIO_MODER_AF;
#endif

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
