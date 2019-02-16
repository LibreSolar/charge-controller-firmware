
#include "hardware.h"
#include "pcb.h"

#include "mbed.h"
#include "half_bridge.h"
#include "us_ticker_data.h"

extern bool led_states[];

#ifdef PIN_LOAD_EN
DigitalOut load_enable(PIN_LOAD_EN);
#else
DigitalOut load_disable(PIN_LOAD_DIS);
#endif

#ifdef PIN_USB_PWR_EN
DigitalOut usb_pwr_en(PIN_USB_PWR_EN);
#endif

#ifdef PIN_UEXT_DIS
DigitalOut uext_dis(PIN_UEXT_DIS);
#endif

//----------------------------------------------------------------------------
void init_load()
{
#ifdef PIN_UEXT_DIS
    uext_dis = 0;
#endif

#ifdef PIN_USB_PWR_EN
    usb_pwr_en = 1;
#endif
}


//----------------------------------------------------------------------------
void hw_load_switch(bool enabled)
{
#ifdef PIN_LOAD_EN
    if (enabled) load_enable = 1;
    else load_enable = 0;
#else
    if (enabled) load_disable = 0;
    else load_disable = 1;
#endif

#ifdef LED_LOAD
    if (enabled) led_states[LED_LOAD] = 1;
    else led_states[LED_LOAD] = 0;
#endif
}

//----------------------------------------------------------------------------
void hw_usb_out(bool enabled)
{
#ifdef PIN_USB_PWR_EN
    if (enabled) usb_pwr_en = 1;
    else usb_pwr_en = 0;
#endif
}

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
    hw_load_switch(false);
    hw_usb_out(false);

    for (int i = 0; i < NUM_LEDS; i++) {
        led_states[i] = 0;
    }

    int led_blink = 1;

    while (1) {

        for (int led = 0; led < NUM_LEDS; led++) {
            led_states[led] = (led % 2) ^ led_blink;
        }

        led_blink = !led_blink;
        wait_ms(100);

        // stay here to indicate something was wrong
        feed_the_dog();
    }
}

#if defined(STM32F0)
#define SYS_MEM_START       0x1FFFC800
#define SRAM_END            0x20003FFF  // 16k
#elif defined(STM32L0)
#define SYS_MEM_START       0x1FF00000
#define SRAM_END            0X20004FFF  // 20k
#endif

#define MAGIC_CODE          0xDEADBEEF

void start_dfu_bootloader()
{
    // place magic code at end of RAM and initiate reset
    *((unsigned long *)(SRAM_END - 0xF)) = MAGIC_CODE;
    NVIC_SystemReset();
}

// https://stackoverflow.com/questions/42020893/stm32l073rz-rev-z-iap-jump-to-bootloader-system-memory

// This function should be called at the beginning of SystemInit in system_clock.c (mbed target directory)
// As mbed does not provide this functionality, you have to patch the file manually
extern "C" void system_init_hook()
{
    if (*((unsigned long *)(SRAM_END - 0xF)) == MAGIC_CODE) {

        *((unsigned long *)(SRAM_END - 0xF)) =  0xCAFEFEED;  // reset the trigger

        // jump function pointer to be initialized and called later
        void (*SysMemJump)(void);

        // TODO for STM32L0
/*
        FLASH_EraseInitTypeDef EraseInitStruct;
        EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
        EraseInitStruct.PageAddress = Data_Address;
        EraseInitStruct.NbPages     = 1;

        First_jump = *(__IO uint32_t *)(Data_Address);

        if (First_jump == 0) {
            HAL_FLASH_Unlock();
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Data_Address, 0xAAAAAAAA);
            HAL_FLASH_Lock();

            // Reinitialize the Stack pointer and jump to application address
            //JumpAddress = *(__IO uint32_t *)(0x1FF00004);
            SysMemJump = (void (*)(void)) (*((uint32_t *) (SYS_MEM_START + 4)));
        }
        else {
            HAL_FLASH_Unlock();
            HAL_FLASHEx_Erase(&EraseInitStruct, &PAGEError);
            HAL_FLASH_Lock();

            // Reinitialize the Stack pointer and jump to application address
            //JumpAddress =  (0x1FF00369);
            SysMemJump = (void (*)(void)) (*((uint32_t *) (0x1FF00369)));
        }
*/
        //__set_MSP(0x20002250);  // set stack pointer for STM32F0
        __set_MSP(SYS_MEM_START);
        //__set_MSP(*(__IO uint32_t *)(0x1FF00000));

        // point the PC to the System Memory reset vector (+4)
        SysMemJump = (void (*)(void)) (*((uint32_t *) (SYS_MEM_START + 4))); // for STM32F0
        SysMemJump();

        while (42);
    }
}
