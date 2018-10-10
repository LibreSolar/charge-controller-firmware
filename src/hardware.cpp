

#ifndef UNIT_TEST

#include "hardware.h"

#include "half_bridge.h"

#include "data_objects.h"
//#include "config.h"

//#include "structs.h"


// LED1 (red, top): Duty cycle shows state of charge, i.e. continuously on = full, 3s on/1s off = 75% full, short flash with long pauses in between = almost empty
// LED2 (green, middle): Solar indicator: on = charging, off = not charging
// LED3 (green, bottom close to USB port): Load indicator: on = load switch and USB charging on, off = both switched off because of low battery

#ifdef PIN_LOAD_EN
DigitalOut load_enable(PIN_LOAD_EN);
#else
DigitalOut load_disable(PIN_LOAD_DIS);
#endif

#ifdef PIN_LED_SOC
DigitalOut led_soc(PIN_LED_SOC);
#else
DigitalOut led_soc_12(PIN_LED_SOC_12);
DigitalOut led_soc_3(PIN_LED_SOC_3);
#endif

DigitalOut led_solar(PIN_LED_SOLAR);

#ifdef PIN_LED_LOAD
DigitalOut led_load(PIN_LED_LOAD);
#endif

#ifdef PIN_LED_GND
DigitalOut led_gnd(PIN_LED_GND);
#endif

//extern battery_t bat;

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

void init_leds()
{
#ifdef PIN_LED_GND
    // TODO: generate PWM signal
    led_gnd = 0;
#endif
}

void update_solar_led(bool enabled)
{
    led_solar = enabled;
}

//----------------------------------------------------------------------------
void flash_led_soc(battery_t *bat)
{
#ifdef PIN_LED_SOC
    if (bat->full) {
        led_soc = 1;
    }
    else if (bat->soc > 80 || bat->state == CHG_CV) {
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
#endif
}

// Reset the watchdog timer
void feed_the_dog()
{
    IWDG->KR = 0xAAAA;
}

// configures IWDG (timeout in seconds)
void init_watchdog(float timeout) {

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

#endif /* UNIT_TEST */