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

#include "half_bridge.h"
#include "pcb.h"
#include "config.h"


static int _pwm_resolution;
static float _min_duty;
static float _max_duty;

static bool _enabled;

static int tim_ccr = 0;     // fake register TIM1->CCR1

void _init_registers(int freq_kHz, int deadtime_ns)
{
    _pwm_resolution = 48000000 / (freq_kHz * 1000);
}

void half_bridge_init(int freq_kHz, int deadtime_ns, float min_duty, float max_duty)
{
    _init_registers(freq_kHz, deadtime_ns);

    _min_duty = min_duty;
    _max_duty = max_duty;
    half_bridge_set_duty_cycle(_max_duty);      // init with allowed value

    _enabled = false;
}

void half_bridge_set_duty_cycle(float duty)
{
    float duty_target;
    // protection against wrong settings which could destroy the hardware
    if (duty < _min_duty) {
        duty_target = _min_duty;
    }
    else if (duty > _max_duty) {
        duty_target = _max_duty;
    }
    else {
        duty_target = duty;
    }

    tim_ccr = _pwm_resolution / 2 * duty_target;
}

void half_bridge_duty_cycle_step(int delta)
{
    float duty_target = (float)(tim_ccr + delta) / (_pwm_resolution / 2);

    // protection against wrong settings which could destroy the hardware
    if (duty_target < _min_duty) {
        half_bridge_set_duty_cycle(_min_duty);
    }
    else if (duty_target > _max_duty) {
        half_bridge_set_duty_cycle(_max_duty);
    }
    else {
        tim_ccr = tim_ccr + delta;
    }
}

float half_bridge_get_duty_cycle()
{
    return (float)(tim_ccr) / (_pwm_resolution / 2);;
}

void half_bridge_start(float pwm_duty)
{
    half_bridge_set_duty_cycle(pwm_duty);

    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    //TIM1->BDTR |= TIM_BDTR_MOE;

    _enabled = true;
}

void half_bridge_stop()
{
    // Break and Dead-Time Register
    // MOE  = 1: Main output enable
    //TIM1->BDTR &= ~(TIM_BDTR_MOE);

    _enabled = false;
}

bool half_bridge_enabled()
{
    return _enabled;
}
