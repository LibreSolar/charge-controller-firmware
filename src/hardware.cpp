/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hardware.h"

#include "mcu.h"
#include "pcb.h"
#include "load.h"
#include "half_bridge.h"
#include "leds.h"
#include "setup.h"

#if defined(STM32F0)
#define SYS_MEM_START       0x1FFFC800
#define SRAM_END            0x20003FFF  // 16k
#elif defined(STM32L0)
#define SYS_MEM_START       0x1FF00000
#define SRAM_END            0X20004FFF  // 20k
#define FLASH_LAST_PAGE     0x0802FF80  // start address of last page (192 kbyte cat 5 devices)
#endif

#define MAGIC_CODE_ADDR     (SRAM_END - 0xF)    // where the magic code is stored

#if defined(__MBED__)

#include "mbed.h"

#if defined(STM32F0)

void control_timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM16 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

    // Set timer clock to 10 kHz
    TIM16->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM16->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM16->ARR = 10000 / freq_Hz - 1;

    // 1 = second-highest priority of STM32L0/F0
    NVIC_SetPriority(TIM16_IRQn, 1);
    NVIC_EnableIRQ(TIM16_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM16->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM16_IRQHandler(void)
{
    TIM16->SR &= ~TIM_SR_UIF;       // clear update interrupt flag to restart timer
    system_control();
}

#elif defined(STM32L0)

void control_timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM7 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    // Set timer clock to 10 kHz
    TIM7->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM7->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM7->ARR = 10000 / freq_Hz - 1;

    // 1 = second-highest priority of STM32L0/F0
    NVIC_SetPriority(TIM7_IRQn, 1);
    NVIC_EnableIRQ(TIM7_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM7->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM7_IRQHandler(void)
{
    TIM7->SR &= ~(1 << 0);
    system_control();
}

#endif


// Reset the watchdog timer
void feed_the_dog()
{
    IWDG->KR = 0xAAAA;
}

// configures IWDG (timeout in seconds)
void init_watchdog(float timeout) {

    // TODO for STM32L0
    #define LSI_FREQ 40000     // approx. 40 kHz according to RM0091

    uint16_t prescaler;

    IWDG->KR = 0x5555; // Disable write protection of IWDG registers

    // set prescaler
    if ((timeout * (LSI_FREQ/4)) < 0x7FF) {
        IWDG->PR = IWDG_PRESCALER_4;
        prescaler = 4;
    }
    else if ((timeout * (LSI_FREQ/8)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_8;
        prescaler = 8;
    }
    else if ((timeout * (LSI_FREQ/16)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_16;
        prescaler = 16;
    }
    else if ((timeout * (LSI_FREQ/32)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_32;
        prescaler = 32;
    }
    else if ((timeout * (LSI_FREQ/64)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_64;
        prescaler = 64;
    }
    else if ((timeout * (LSI_FREQ/128)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_128;
        prescaler = 128;
    }
    else {
        IWDG->PR = IWDG_PRESCALER_256;
        prescaler = 256;
    }

    // set reload value (between 0 and 0x0FFF)
    IWDG->RLR = (uint32_t)(timeout * (LSI_FREQ/prescaler));

    //float calculated_timeout = ((float)(prescaler * IWDG->RLR)) / LSI_FREQ;
    //printf("WATCHDOG set with prescaler:%d reload value: 0x%X - timeout:%f\n", prescaler, (unsigned int)IWDG->RLR, calculated_timeout);

    IWDG->KR = 0xAAAA;  // reload
    IWDG->KR = 0xCCCC;  // start

    feed_the_dog();
}

// this function is called by mbed if a serious error occured: error() function called
void mbed_die(void)
{
    half_bridge_stop();
    load.stop();
    usb_pwr.stop();

    leds_init(false);

    while (1) {

        leds_toggle_error();
        wait_ms(100);

        // stay here to indicate something was wrong
        feed_the_dog();
    }
}

void start_stm32_bootloader()
{
#ifdef PIN_BOOT0_EN
    // pin is connected to BOOT0 via resistor and capacitor
    DigitalOut boot0_en(PIN_BOOT0_EN);

    boot0_en = 1;
    wait_ms(100);   // wait for capacitor at BOOT0 pin to charge up

    NVIC_SystemReset();
#elif defined (STM32F0)
    // place magic code at end of RAM and initiate reset
    *((uint32_t *)(MAGIC_CODE_ADDR)) = 0xDEADBEEF;
    NVIC_SystemReset();
#endif
}

// This function should be called at the beginning of SystemInit in system_clock.c (mbed target directory)
// As mbed does not provide this functionality, you have to patch the file manually
extern "C" void system_init_hook()
{
    if (*((uint32_t *)(MAGIC_CODE_ADDR)) == 0xDEADBEEF) {

        uint32_t jump_address;

        *((uint32_t *)(MAGIC_CODE_ADDR)) =  0x00000000;  // reset the trigger

        jump_address = (*((uint32_t *) (SYS_MEM_START + 4)));

        // reinitialize stack pointer
        __set_MSP(SYS_MEM_START);

        // jump to specified address
        void (*jump)(void);
        jump = (void (*)(void)) jump_address;
        jump();

        while (42); // should never be reached
    }
}

void reset_device()
{
    NVIC_SystemReset();
}

#elif defined(__ZEPHYR__)

#include <power/reboot.h>
#include <drivers/gpio.h>

void start_stm32_bootloader()
{
#ifdef DT_SWITCH_BOOT0_GPIOS_CONTROLLER
    // pin is connected to BOOT0 via resistor and capacitor
    struct device *dev = device_get_binding(DT_SWITCH_BOOT0_GPIOS_CONTROLLER);
    gpio_pin_configure(dev, DT_SWITCH_BOOT0_GPIOS_PIN,
        DT_SWITCH_BOOT0_GPIOS_FLAGS | GPIO_OUTPUT_ACTIVE);

    k_sleep(100);   // wait for capacitor at BOOT0 pin to charge up
    reset_device();
#elif defined (STM32F0)
    // place magic code at end of RAM and initiate reset
    *((uint32_t *)(MAGIC_CODE_ADDR)) = 0xDEADBEEF;
    reset_device();
#endif
}

void reset_device()
{
    sys_reboot(SYS_REBOOT_COLD);
}

#else

// dummy functions for unit tests
void start_stm32_bootloader() {}
void reset_device() {}

#endif /* UNIT_TEST */
