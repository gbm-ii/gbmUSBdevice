/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32F4-specific setup routines for USB
 * Copyright (c) 2024..26 gbm
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

#include "stm32f4yy.h"
#include "bf_reg.h"
#if (__STDC_VERSION__ >= 202000L) && __has_include("board.h")
	#include "board.h"
	// If board.h is present, HSE_VALUE and RCC_CR_HSESEL should be defined in it.
#endif

// must be included after board defs!
#include "stm32gpioutil.h"

#ifndef HCLK_FREQ
#define HCLK_FREQ	84000000u
#endif

#define USB_ENUM_DELAY_ms	50u

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

static inline void ClockSetup(void)
{
	// minimal clock setup required for USB device operation
#ifdef RCC_CR_HSESEL	// bit field combination for acivating HSE, crystal or generator - board-dependent
	RCC->CR |= RCC_CR_HSESEL;
	while (!(RCC->CR & RCC_CR_HSERDY));
#else	// try both BYPASS and XTAL
#define HSE_START_TOUT	4000u	// startup time is 2 ms typ., the loop must take at least 9 instr
	// try HSE bypass first
	RCC->CR |= RCC_CR_HSEON | RCC_CR_HSEBYP;
	for (volatile uint32_t t = 0; t < HSE_START_TOUT && !(RCC->CR & RCC_CR_HSERDY); t++) ;
	if (!(RCC->CR & RCC_CR_HSERDY))
	{
		// no ext. generator -> try oscillator
		RCC->CR &= ~RCC_CR_HSEON;
		RCC->CR &= ~RCC_CR_HSEBYP;
		while (RCC->CR & (RCC_CR_HSEBYP | RCC_CR_HSEON)) ;
		RCC->CR |= RCC_CR_HSEON;
		for (volatile uint32_t t = 0; t < HSE_START_TOUT && !(RCC->CR & RCC_CR_HSERDY); t++) ;
	}
#endif

	if (RCC->CR & RCC_CR_HSERDY)
	{
#ifdef HSE_VALUE
		uint8_t hs_freq_MHz = HSE_VALUE / 1000000u;
#else
		// measure HSE frequency using TIM11 - RefMan RM0368 section 6.2.11
		RCC->APB2ENR |= RCC_APB2ENR_TIM11EN;
#define HSEDIV	31u
		// set prescaler for HSE_RTC
		RCC->CFGR = HSEDIV << RCC_CFGR_RTCPRE_Pos;	// 1..31
		// HSE_FREQ = 4..26 MHz, so HSE_RTC is between 130 kHz and 840 kHz

		TIM11->ARR = HSI_VALUE / 1000000u * HSEDIV * 8u * 2u - 1;	// cap prescaler 8, *2 for rounding
		TIM11->OR = TIM_OR_TI1_RMP_1;	// set TI1 to HSE_RTC
		TIM11->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_IC1PSC;			// TIM1CH1 in capture mode, prescale by 8
		TIM11->CCER = TIM_CCER_CC1E;
		TIM11->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;

		// count captures until overflow
		uint8_t caps = 0;
		uint32_t sr;
		do {
			sr = TIM11->SR;
			if (sr & TIM_SR_CC1IF)
			{
				TIM11->SR = ~TIM_SR_CC1IF;
				++caps;
			}
		} while (~sr & TIM_SR_UIF);

		const uint32_t hs_freq_MHz = (caps + 1) / 2;	// round

		RCC->APB2ENR = 0;
		RCC->APB2RSTR = RCC_APB2RSTR_TIM11RST;	// RST is independent from EN
		RCC->APB2RSTR = 0;
#endif	// HSE_VALUE

		// setup PLL for HSE operation
#if HCLK_FREQ == 84000000u
		RCC->PLLCFGR = (RCC->PLLCFGR & RCC_PLLCFGR_RSVD)
			| RCC_PLLCFGR_PLLSRC_HSE
			| RCC_PLLCFGR_PLLMV(hs_freq_MHz)
			| RCC_PLLCFGR_PLLNV(336)	// 192 for F411 @ 96 MHz
			| RCC_PLLCFGR_PLLPV(4)		// 2 for F411 @ 96 MHz
			| RCC_PLLCFGR_PLLQV(7);		// 4 for F411 @ 96 MHz
		// set Flash speed
		FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;	// 1ws 30..64, 3 ws 90..100
#elif HCLK_FREQ == 96000000u
		RCC->PLLCFGR = (RCC->PLLCFGR & RCC_PLLCFGR_RSVD)
			| RCC_PLLCFGR_PLLSRC_HSE
			| RCC_PLLCFGR_PLLMV(hs_freq_MHz)
			| RCC_PLLCFGR_PLLNV(192)	// 192 for F411 @ 96 MHz
			| RCC_PLLCFGR_PLLPV(2)		// 2 for F411 @ 96 MHz
			| RCC_PLLCFGR_PLLQV(4);		// 4 for F411 @ 96 MHz
		// set Flash speed
		FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_3WS;	// 1ws 30..64, 3 ws 90..100
#else
#error HCLK_FREQ value not supported
#endif

	}
	else
	{
		// use HSI - not reliable
		RCC->PLLCFGR = (RCC->PLLCFGR & RCC_PLLCFGR_RSVD)
			| RCC_PLLCFGR_PLLSRC_HSI
			| RCC_PLLCFGR_PLLMV(HSI_VALUE / 1000000u)
			| RCC_PLLCFGR_PLLNV(336)	// 192 for F411 @ 96 MHz
			| RCC_PLLCFGR_PLLPV(4)		// 2 for F411 @ 96 MHz
			| RCC_PLLCFGR_PLLQV(7);		// 4 for F411 @ 96 MHz
	}
#if (HCLK_FREQ > 84000000u/* && (DBGMCU->IDCODE & DBGMCU_IDCODE_DEV_ID) == 0x431*/)
	{
		// after reset VOS in PWR->CR is set for 84 MHz operation in F401
		// for F411, set voltage scaling to mode 1 (11) for > 84 MHz
		// (for F401 & F411, the default setting of 10 supports 84 MHz operation)
		RCC->APB1ENR |= RCC_APB1ENR_PWREN;
		PWR->CR = PWR_CR_VOS;	// scale 1
		while (~PWR->CSR & PWR_CSR_VOSRDY) ;
	}
#endif
	RCC->CR |= RCC_CR_PLLON;	//
	while (!(RCC->CR & RCC_CR_PLLRDY));

	RCC->CFGR = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_SW_PLL;	// APB2, APB1 prescaler = 2
	//while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
	RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

	RCC->IOENR |= RCC_IOENR_GPIOEN(GPIOA);
	AFRF(GPIOA, 11) = AFN_USB;
	AFRF(GPIOA, 12) = AFN_USB;
	BF2F(GPIOA->OSPEEDR, 11) = GPIO_OSPEEDR_HI;
	BF2F(GPIOA->OSPEEDR, 12) = GPIO_OSPEEDR_HI;
	BF2F(GPIOA->MODER, 11) = GPIO_MODER_AF;
	BF2F(GPIOA->MODER, 12) = GPIO_MODER_AF;
}

#endif /* INC_MCU_HW_H_ */
