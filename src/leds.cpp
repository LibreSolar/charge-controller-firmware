/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UNIT_TEST

#include "leds.h"
#include "pcb.h"

#include "mbed.h"

static int led_states[NUM_LEDS];                // must be one of led_state_t
static int trigger_timeout[NUM_LEDS];           // seconds until trigger should end

static int blink_state = 1;                     // global state of all blinking LEDs
static bool flicker_state = 1;                  // global state of all flickering LEDs

static bool charging = false;

static void charlieplexing()
{
    static int led_count = 0;
    static int flicker_count = 0;

    // could be increased to value >NUM_LEDS to reduce on-time
    if (led_count >= NUM_LEDS) {
        led_count = 0;
    }

    if (flicker_count > 30) {
        flicker_count = 0;
        flicker_state = !flicker_state;
    }

    if (led_count < NUM_LEDS && (
            led_states[led_count] == LED_STATE_ON ||
            (led_states[led_count] == LED_STATE_FLICKER && flicker_state == 1) ||
            (led_states[led_count] == LED_STATE_BLINK && blink_state == 1)
       )) {
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
        // all pins floating
        for (int pin_number = 0; pin_number < NUM_LED_PINS; pin_number++) {
            DigitalIn pin(led_pins[pin_number]);
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
        if (enabled) {
            led_states[led] = LED_STATE_ON;
            trigger_timeout[led] = 0;           // switched off the first time the main loop is called
        }
        else {
            led_states[led] = LED_STATE_OFF;
            trigger_timeout[led] = -1;
        }
    }
    timer_start(NUM_LEDS * 60);     // 60 Hz
}

void leds_set_charging(bool enabled)
{
    charging = enabled;
#ifdef LED_DCDC
    led_states[LED_DCDC] = LED_STATE_ON;
#endif
}

void leds_set(int led, bool enabled, int timeout)
{
    if (led >= NUM_LEDS || led < 0)
        return;

    if (enabled) {
        led_states[led] = LED_STATE_ON;
    }
    else {
        led_states[led] = LED_STATE_OFF;
    }

    trigger_timeout[led] = timeout;
}

void leds_on(int led, int timeout)
{
    leds_set(led, true, timeout);
}

void leds_off(int led)
{
    leds_set(led, false, -1);
}

void leds_blink(int led, int timeout)
{
    if (led < NUM_LEDS && led >= 0) {
        led_states[led] = LED_STATE_BLINK;
        trigger_timeout[led] = timeout;
    }
}

void leds_flicker(int led, int timeout)
{
    if (led < NUM_LEDS && led >= 0) {
        led_states[led] = LED_STATE_FLICKER;
        trigger_timeout[led] = timeout;
    }
}

void leds_update_1s()
{
    blink_state = !blink_state;

    for (int led = 0; led < NUM_LEDS; led++) {
        if (trigger_timeout[led] > 0) {
            trigger_timeout[led]--;
        }
        else if (trigger_timeout[led] == 0) {
            led_states[led] = LED_STATE_OFF;
        }
    }
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
    int blink_chg = (charging) ? LED_STATE_BLINK : LED_STATE_ON;
#else
    int blink_chg = LED_STATE_ON;
#endif


#ifdef LED_PWR
    led_states[LED_PWR] = blink_chg;
    trigger_timeout[LED_PWR] = -1;
#elif defined(LED_SOC_3)  // 3-bar SOC gauge
    trigger_timeout[LED_SOC_1] = -1;
    trigger_timeout[LED_SOC_2] = -1;
    trigger_timeout[LED_SOC_3] = -1;
    if (soc > 80) {
        led_states[LED_SOC_1] = blink_chg;
        led_states[LED_SOC_2] = LED_STATE_ON;
        led_states[LED_SOC_3] = LED_STATE_ON;
    }
    else if (soc > 20) {
        led_states[LED_SOC_1] = LED_STATE_OFF;
        led_states[LED_SOC_2] = blink_chg;
        led_states[LED_SOC_3] = LED_STATE_ON;
    }
    else {
        led_states[LED_SOC_1] = LED_STATE_OFF;
        led_states[LED_SOC_2] = LED_STATE_OFF;
        led_states[LED_SOC_3] = blink_chg;
    }
#endif
}

#endif /* UNIT_TEST */
