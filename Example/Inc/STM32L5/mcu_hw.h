/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32L5-specific setup routines for USB
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

#include "stm32l5yy.h"
#include "bf_reg.h"		// from github.com/gbm-ii/STM32_Inc

#if (__STDC_VERSION__ >= 202000L) && __has_include("board.h")
	#include "board.h"
#endif

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

/*
 * Minimal clock setup routine for STM32L4 (teted on Nucleo-L476, L496, L4R5)
 * The routine may be replaced by any user-writen or CubeMX-generated HAL routine providing stable 48 MHz clock to USB peripheral.
 */
#ifndef HCLK_FREQ
#define	HCLK_FREQ	110000000u
#endif

// frequency step for computing Flash wait states
#define FLASH_WS_FREQ_STEP	20000000u

/**
  \brief   MCU Clock Configuration
  \details Initializes the MCU main clock to the defined <b>HCLK_FREQ</b> and USB clock to 48 MHz
  \param [in]  none
  \return      none
  \note		The main clock source is 4 MHz MSI.
			For L47x, MSI is synchronized to LSE andUSB clock is generated form it.
			For others (STM32L4x2, L49x..L4Sx), USB clock is set to HSI48 synchronized to USB SOF.
 */
static inline void ClockSetup(void)
{
	// STM32L5x2 - 110 MHz main clock from MSI via PLL
	// USB clock from HSI48 synchronized to USB SOF
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN | RCC_APB1ENR1_CRSEN;
	PWR->CR1 = 0;	// VOS range 0

	// PLL - 110 MHz
	RCC->PLLCFGR = RCC_PLLCFGR_PLLREN | RCC_PLLCFGR_PLLNV(HCLK_FREQ / 2000000u) | RCC_PLLCFGR_PLLMV(1) | RCC_PLLCFGR_PLLSRC_MSI;
	RCC->CR |= RCC_CR_PLLON;// | RCC_CR_MSIRGSEL;
	// set Flash speed
	FLASH->ACR = ((HCLK_FREQ - 1u) / FLASH_WS_FREQ_STEP) << FLASH_ACR_LATENCY_Pos;
	ICACHE->CR |= ICACHE_CR_EN;
	// HSI48
	RCC->CRRCR |= RCC_CRRCR_HSI48ON;	//
	while (~RCC->CRRCR & RCC_CRRCR_HSI48RDY);
	// sync HSI48 to USB SOF
	//CRS->CFGR = CRS_CFGR_SYNCSRC_USB | 34u << CRS_CFGR_FELIM_Pos | (48000000u / 1000u - 1);
	CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;

	while (~RCC->CR & RCC_CR_PLLRDY) ;
	RCC->CFGR |= RCC_CFGR_SW_PLL;
//	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/*
 * USB hardware setup routine for STM32L4 - USB module enable & pin configuration
 * May be replaced by CubeMX-generated USB init routine
 */
static inline void USBhwSetup(void)
{
	// USB hardware setup
	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
	PWR->CR2 |= PWR_CR2_USV;	// enable VddUSB
	RCC->APB1ENR2 |= RCC_APB1ENR2_USBFSEN;
#if 0	// not required
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	AFRF(GPIOA, 11) = AFN_USB;
	AFRF(GPIOA, 12) = AFN_USB;
	BF2F(GPIOA->OSPEEDR, 11) = GPIO_OSPEEDR_HI;
	BF2F(GPIOA->OSPEEDR, 12) = GPIO_OSPEEDR_HI;
	BF2F(GPIOA->MODER, 11) = GPIO_MODER_AF;
	BF2F(GPIOA->MODER, 12) = GPIO_MODER_AF;
#endif
}

#endif /* INC_MCU_HW_H_ */
