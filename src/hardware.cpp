
#include "hardware.h"
#include "half_bridge.h"
#include "data_objects.h"
#include "us_ticker_data.h"

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

#ifdef PIN_LED_SOC
DigitalOut led_soc(PIN_LED_SOC);    // only one LED
#else
DigitalOut led_soc_3(PIN_LED_SOC_3);
#endif

DigitalOut led_solar(PIN_LED_SOLAR);

#ifdef PIN_LED_LOAD
DigitalOut led_load(PIN_LED_LOAD);
#endif

//extern battery_t bat;

int led1_CCR;   // CCR for TIM21 to switch LED1 on
int led2_CCR;   // CCR for TIM21 to switch LED2 on


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
void enable_load()
{
#ifdef PIN_LOAD_EN
    load_enable = 1;
#else
    load_disable = 0;
#endif

#ifdef PIN_LED_LOAD
    led_load = 1;
#endif
}

//----------------------------------------------------------------------------
void disable_load()
{
#ifdef PIN_LOAD_EN
    load_enable = 0;
#else
    load_disable = 1;
#endif

#ifdef PIN_LED_LOAD
    led_load = 0;
#endif
}

// mbed-os uses TIM21 on L0 for us-ticker
// https://github.com/ARMmbed/mbed-os/issues/6854
// fix: patch targets/TARGET_STM/TARGET_STM32L0/TARGET_NUCLEO_L073RZ/device/us_ticker_data.h

void init_leds()
{
#ifdef PIN_LED_SOC_3
    led_soc_3 = 1;      // always enabled (0-20% SOC)
#endif

#ifdef PIN_LED_LOAD
    led_load = 1;       // switch on during start-up
#endif

led_solar = 1;          // switch on during start-up

#if defined(PIN_LED_GND) && defined(STM32L0)
    // PB13 / TIM21_CH1: LED_SOC12  --> high for LED2, PWM for LED1 + 2
    // PB14 / TIM21_CH2: LED_GND    --> always PWM

    // TODO: throw error during compile time... but how?
    if (TIM_MST == TIM21) {
        error("Error: Timer conflict --> change us_ticker_data.h to use TIM22 instead of TIM21!");
    }

    const int freq = 1;     // kHz
    const float duty_target = 0.2;

    RCC->IOPENR |= RCC_IOPENR_IOPBEN;

    // Enable TIM21 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM21EN;

    // Select alternate function mode on PB13 and PB14 (first bit _1 = 1, second bit _0 = 0)
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE13)) | GPIO_MODER_MODE13_1;
    GPIOB->MODER = (GPIOB->MODER & ~(GPIO_MODER_MODE14)) | GPIO_MODER_MODE14_1;

    // Select AF6 on PB13 and PB14
    GPIOB->AFR[1] |= 0x6 << GPIO_AFRH_AFRH5_Pos;        // 5 + 8 = PB13
    GPIOB->AFR[1] |= 0x6 << GPIO_AFRH_AFRH6_Pos;        // 6 + 8 = PB14

    // No prescaler --> timer frequency = 32 MHz        // TODO!
    TIM21->PSC = 0;

    // Capture/Compare Mode Register 1
    // OCxM = 110: Select PWM mode 1 on OCx
    // OCxPE = 1:  Enable preload register on OCx (reset value)
    TIM21->CCMR1 |= TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE;
    TIM21->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;

    // Capture/Compare Enable Register
    // CCxP: Active high polarity on OCx (default = 0)
    TIM21->CCER &= ~(TIM_CCER_CC1P);     // PB13
    TIM21->CCER |= TIM_CCER_CC2P;        // PB14

    // Control Register 1
    // TIM_CR1_CMS = 01: Select center-aligned mode 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM21->CR1 |= TIM_CR1_CMS_0 | TIM_CR1_CEN;

    // Force update generation (UG = 1)
    TIM21->EGR |= TIM_EGR_UG;

    // set PWM frequency and resolution
    int _pwm_resolution = SystemCoreClock / (freq * 1000);

    // Auto Reload Register
    // center-aligned mode --> divide resolution by 2
    TIM21->ARR = _pwm_resolution / 2;

    TIM21->CCR2 = _pwm_resolution / 2 * duty_target; // LED_GND
    
    led1_CCR = TIM21->ARR - TIM21->CCR2;        // LED1 + LED2
    led2_CCR = TIM21->ARR;                      // only LED2
    TIM21->CCR1 = led1_CCR;         // start with all LEDs on
    
    TIM21->CCER |= TIM_CCER_CC1E;   // enable PWM on LED_12
    TIM21->CCER |= TIM_CCER_CC2E;   // enable PWM on LED_GND
#endif
}

void update_dcdc_led(bool enabled)
{
    led_solar = enabled;
}

//----------------------------------------------------------------------------
void update_soc_led(battery_t *bat)
{
#ifdef PIN_LED_SOC
    if (bat->full) {
        led_soc = 1;
    }
    else if (bat->soc > 80 || bat->state == CHG_STATE_CV) {
        if (us_ticker_read() % 2000000 < 1800000) {
            led_soc = 1;
        }
        else {
            led_soc = 0;
        }
    }
    else if (bat->soc <= 80 && bat->soc >= 30) {
        if (time(NULL) % 2 == 0) {
            led_soc = 1;
        }
        else {
            led_soc = 0;
        }
    }
    else {
        if (us_ticker_read() % 2000000 < 200000) {
            led_soc = 1;
        }
        else {
            led_soc = 0;
        }
    }
#elif defined(PIN_LED_SOC_3) // 3-bar SOC gauge
    if (bat->soc > 80) {
        TIM21->CCR1 = led1_CCR;
        TIM21->CCER |= TIM_CCER_CC1E;
    }
    else if (bat->soc > 20) {
        TIM21->CCR1 = led2_CCR;
        TIM21->CCER |= TIM_CCER_CC1E;        
    }
    else {
        TIM21->CCER &= ~(TIM_CCER_CC1E);
    }
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
    disable_load();

    while (1) {
        #ifdef PIN_LED_SOC
        led_soc = 1;
        #endif
        led_solar = 0;
        #ifdef PIN_LED_LOAD
        led_load = 1;
        #endif
        wait_ms(100);
        #ifdef PIN_LED_SOC
        led_soc = 0;
        #endif
        led_solar = 1;
        #ifdef PIN_LED_LOAD
        led_load = 0;
        #endif
        wait_ms(100);

        // stay here to indicate something was wrong
        feed_the_dog();
    }
}
