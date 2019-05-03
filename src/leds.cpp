
#include "leds.h"
#include "pcb.h"

#include "mbed.h"

static bool led_states[NUM_LEDS];

static int blink_state = 1;
static bool flicker_state = 1;

static bool charging = false;
static bool rx_flag = false;
static bool tx_flag = false;
static int rxtx_trigger_timestamp;

static void charlieplexing()
{
    static int led_count = 0;
    static int flicker_count = 0;

    // could be increased to value >NUM_LEDS to reduce on-time
    if (led_count >= NUM_LEDS) {
        led_count = 0;
    }

    if (flicker_count > 200) {
        flicker_count = 0;
        flicker_state = !flicker_state;
    }

    if (led_count < NUM_LEDS && led_states[led_count] == true) {
        for (int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
            DigitalInOut pin(led_pins[pin_number]);
            switch (led_pin_setup[led_count][pin_number]) {
            case PIN_HIGH:
                pin.output();
                pin = 1;
                break;
            case PIN_LOW:
                pin.output();
                pin = 0;
                break;
            case PIN_FLOAT:
                pin.input();
                break;
            }
        }
    }
    else {
        for (int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
            DigitalOut pin(led_pins[pin_number]);
            pin = 0;
        }
    }

    led_count++;
    flicker_count++;
}

#if defined(STM32F0)

static void timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM17 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;

    // Set timer clock to 10 kHz
    TIM17->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM17->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM17->ARR = 10000 / freq_Hz - 1;

    // 3 = lowest priority of STM32L0/F0
    NVIC_SetPriority(TIM17_IRQn, 3);
    NVIC_EnableIRQ(TIM17_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM17->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM17_IRQHandler(void)
{
    TIM17->SR &= ~TIM_SR_UIF;       // clear update interrupt flag to restart timer
    charlieplexing();
}

#elif defined(STM32L0)

static void timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM22 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM22EN;

    // Set timer clock to 10 kHz
    TIM22->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM22->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM22->ARR = 10000 / freq_Hz - 1;

    // 3 = lowest priority of STM32L0/F0
    NVIC_SetPriority(TIM22_IRQn, 3);
    NVIC_EnableIRQ(TIM22_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM22->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM22_IRQHandler(void)
{
    TIM22->SR &= ~(1 << 0);
    charlieplexing();
}

#endif

void leds_init(bool enabled)
{
    for (int led = 0; led < NUM_LEDS; led++) {
        led_states[led] = enabled;
    }
    timer_start(NUM_LEDS * 60);     // 60 Hz
}

void leds_set_charging(bool enabled)
{
    charging = enabled;
#ifdef LED_DCDC
    led_states[LED_DCDC] = enabled;
#endif
}

void leds_set_load(bool enabled)
{
#ifdef LED_LOAD
    if (enabled) led_states[LED_LOAD] = 1;
    else led_states[LED_LOAD] = 0;
#endif
}

void leds_update_rxtx()
{
#ifdef LED_RXTX
    if (time(NULL) >= rxtx_trigger_timestamp + 2) {
        led_states[LED_RXTX] = false;
        tx_flag = false;
        rx_flag = false;
    }
    else {
        if (tx_flag) {
            led_states[LED_RXTX] = true;               // TX: constant on
        }
        else if (rx_flag) {
            led_states[LED_RXTX] = flicker_state;       // RX: flicker
        }
    }
#endif
}

void leds_trigger_rx()
{
    rx_flag = true;
    tx_flag = false;
    rxtx_trigger_timestamp = time(NULL);
}

void leds_trigger_tx()
{
    rx_flag = false;
    tx_flag = true;
    rxtx_trigger_timestamp = time(NULL);
}

void leds_toggle_blink()
{
    blink_state = !blink_state;
}

void leds_toggle_error()
{
    for (int led = 0; led < NUM_LEDS; led++) {
        led_states[led] = (led % 2) ^ blink_state;
    }
    blink_state = !blink_state;
}

//----------------------------------------------------------------------------
void leds_update_soc(int soc)
{
#ifndef LED_DCDC
    // if there is no dedicated LED for the DC/DC converter status, blink SOC
    // or power LED when charger is enabled
    int blink_on = (charging) ? blink_state : 1;
#else
    int blink_on = 1;
#endif

#ifdef LED_PWR

    led_states[LED_PWR] = blink_on;

#elif defined(LED_SOC_3)  // 3-bar SOC gauge

    if (soc > 80) {
        if (blink_on) {
            led_states[LED_SOC_1] = true;
        }
        else {
            led_states[LED_SOC_1] = false;
        }
        led_states[LED_SOC_2] = true;
        led_states[LED_SOC_3] = true;
    }
    else if (soc > 20) {
        led_states[LED_SOC_1] = false;
        if (blink_on) {
            led_states[LED_SOC_2] = true;
        }
        else {
            led_states[LED_SOC_2] = false;
        }
        led_states[LED_SOC_3] = true;
    }
    else {
        led_states[LED_SOC_1] = false;
        led_states[LED_SOC_2] = false;
        if (blink_on) {
            led_states[LED_SOC_3] = true;
        }
        else {
            led_states[LED_SOC_3] = false;
        }
    }
#endif
}
