/*
 * lightweight USB device stack by gbm
 * mcu_hw.h - STM32F1-specific setup routines for USB
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
#include "stm32f10y.h"
#include "bf_reg.h"

#define BLUEPILLPLUS
#include "boards/stm32f103bluepill.h"

/*
 * The routines below are supposed to be called only once, so they are defined as static inline
 * in a header file.
 */
 
// F103 BluePill board, 8 MHz osc

#define HCLK_FREQ	72000000u
#define USB_ENUM_DELAY_ms	50u

// minimal clock setup required for USB device operation
static inline void ClockSetup(void)
{
	RCC->CR |= RCC_CR_HSEON;
	while (~RCC->CR & RCC_CR_HSERDY) ;
    RCC->CFGR = RCC_CFGR_HPRE_DIV1    /* HCLK = SYSCLK */
		| RCC_CFGR_PPRE2_DIV1    /* PCLK2 = HCLK/1 */
		| RCC_CFGR_PPRE1_DIV2    /* PCLK1 = HCLK/2 */
		| RCC_CFGR_PLLSRC/*_HSE*/ | RCC_CFGR_PLLMULL9;    /*  PLL configuration: PLLCLK = HSE * 9 = 72 MHz */
    RCC->CR |= RCC_CR_PLLON;

    FLASH->ACR |= FLASH_ACR_LATENCY_2;

    while(~RCC->CR & RCC_CR_PLLRDY);    /* Wait till PLL is ready */
    RCC->CFGR |= RCC_CFGR_SW_PLL;        /* Select PLL as system clock source */
    //while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);    /* Wait till PLL is used as system clock source */
}

static inline void noHAL_Delay(uint8_t ms)
{
	SysTick->LOAD = HCLK_FREQ / 1000u * ms - 1u;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
	while (~SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) ;
	SysTick->CTRL = 0;
}

// USB peripheral enable & pin configuration
static inline void USBhwSetup(void)
{
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
	AFIO->MAPR = AFIO_MAPR_SWJ_CFG_JTAGDISABLE;	// SWD only

	// Pull down PA12 (USBDP) to disable USB pullup detection - needed for possible re-enumeration
	BF4F(GPIOA->CRH, 12) = GPIO_CR_OOD_S;
	
	// USB enumeration delay - replace with HAL_Delay if HAL is used
	noHAL_Delay(USB_ENUM_DELAY_ms);
	
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;	// activate USB, pulling up DP
}

// board LED/Button setup needed for HID demo
static inline void LED_Btn_Setup(void)
{
#ifdef LED_PORT
	RCC->APB2ENR |= RCC_IOENR_GPIOEN(LED_PORT);
	CRF(LED_PORT, LED_BIT) = GPIO_CR_OPP_S;
#endif
#ifdef BTN_PORT
	RCC->APB2ENR |= RCC_IOENR_GPIOEN(BTN_PORT);
	CRF(BTN_PORT, BTN_BIT) = GPIO_CR_INP;
	BTN_PORT->BRR = BTN_MSK;
#endif
}

static inline void hwLED_Set(bool on)
{
#ifdef LED_PORT
	LED_PORT->BSRR = on ^ LED_ACTIVE_LEVEL ? LED_MSK << 16 : LED_MSK;
#endif
}


#endif /* INC_MCU_HW_H_ */
