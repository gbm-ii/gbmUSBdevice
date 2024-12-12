/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32F4-specific setup routines for USB
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
#include "stm32f4yy.h"
#include "bf_reg.h"

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */

#undef HSE_VALUE

#ifdef BLACKPILL
// F401 BlackPill board, 25 MHz osc
#define HSE_VALUE 25000000u
#define RCC_CR_HSESEL	RCC_CR_HSEON

// LED active low
#define LED_PORT GPIOC
#define LED_BIT	13

// user button, active low
#define BTN_PORT	GPIOA
#define BTN_BIT	0
#define BTN_DOWN	(~BTN_PORT->IDR & 1u << BTN_BIT)

#elif defined(NUCLEO64)	// Discovery / Nucleo 64
// Nucleo-F401 board, 8 MHz ext. clock
#define HSE_VALUE 8000000u
#define RCC_CR_HSESEL	(RCC_CR_HSEON | RCC_CR_HSEBYP)	// Nucleo-64 - Ext. clock

#define LED_PORT	GPIOA
#define LED_BIT	5
// user button, active low
#define BTN_PORT	GPIOC
#define BTN_BIT	13

#elif defined(F4DISCO)
// F401 Discovery board, 8 MHz osc
#define HSE_VALUE 8000000u
#define RCC_CR_HSESEL	RCC_CR_HSEON	// Discovery - XTAL
#else
#error Board type not defined!
#endif

#define LED_MSK	(1u << LED_BIT)

#define HCLK_FREQ	84000000u
#define USB_ENUM_DELAY_ms	50u

// minimal clock setup required for USB device operation
static inline void ClockSetup(void)
{
	RCC->CR |= RCC_CR_HSESEL;
	while (!(RCC->CR & RCC_CR_HSERDY));
	RCC->PLLCFGR = (RCC->PLLCFGR & RCC_PLLCFGR_RSVD)
		| RCC_PLLCFGR_PLLSRC_HSE
		| RCC_PLLCFGR_PLLMV(HSE_VALUE / 1000000u)
		| RCC_PLLCFGR_PLLNV(336)	// 192 for F411 @ 96 MHz
		| RCC_PLLCFGR_PLLPV(4)		// 2 for F411 @ 96 MHz
		| RCC_PLLCFGR_PLLQV(7);		// 4 for F411 @ 96 MHz
	RCC->CR |= RCC_CR_PLLON;	//
	// set Flash speed
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_2WS;	// 1ws 30..64, 3 ws 90..100
	while (!(RCC->CR & RCC_CR_PLLRDY));

	RCC->CFGR = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV2 | RCC_CFGR_SW_PLL;	// APB2, APB1 prescaler = 2
	//while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

static inline void noHAL_Delay(uint8_t ms)
{
	SysTick->LOAD = HCLK_FREQ / 1000u * ms - 1u;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
	while (SysTick) ;
	SysTick->CTRL = 0;
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
//	RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
//	PWR->CR2 |= PWR_CR2_USV | PWR_CR2_IOSV;	// enable VddUSB
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

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
	BF2F(BTN_PORT->PUPDR, BTN_BIT) = GPIO_PUPDR_PU;
	BF2F(BTN_PORT->MODER, BTN_BIT) = GPIO_MODER_IN;
#endif
}

static inline void hwLED_Set(bool on)
{
#ifdef LED_PORT
	LED_PORT->BSRR = on ? LED_MSK << 16 : LED_MSK;
#endif
}

#endif /* INC_MCU_HW_H_ */
