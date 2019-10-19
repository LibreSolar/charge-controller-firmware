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

#include "dcdc.h"
#include "pcb.h"
#include "log.h"

#include "half_bridge.h"

#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function
#include <stdlib.h>     // for min/max function
#include <stdio.h>

extern LogData log_data;

#if FEATURE_DCDC_CONVERTER == 0

Dcdc::Dcdc(PowerPort *hv_side, PowerPort *lv_side, DcdcOperationMode op_mode) {}

#else

Dcdc::Dcdc(PowerPort *hv_side, PowerPort *lv_side, DcdcOperationMode op_mode)
{
    hvs = hv_side;
    lvs = lv_side;
    mode           = op_mode;
    enabled        = true;
    state          = DCDC_STATE_OFF;
    ls_current_max = DCDC_CURRENT_MAX;
    hs_voltage_max = HIGH_SIDE_VOLTAGE_MAX;
    ls_voltage_max = LOW_SIDE_VOLTAGE_MAX;
    ls_voltage_min = 9.0;
    output_power_min = 1;         // switch off iff power < 1 W
    restart_interval = 60;
    off_timestamp = -10000;       // start immediately
    pwm_delta = 1;                // start-condition of duty cycle pwr_inc_pwm_direction

    // lower duty limit might have to be adjusted dynamically depending on LS voltage
    half_bridge_init(PWM_FREQUENCY, PWM_DEADTIME, 12 / hs_voltage_max, 0.97);
}

int Dcdc::duty_cycle_delta()
{
    int pwr_inc_pwm_direction;      // direction of PWM duty cycle change for increasing power
    PowerPort *in;
    PowerPort *out;

    if (mode == MODE_MPPT_BUCK ||
        (mode == MODE_NANOGRID && lvs->current > 0.1))
    {
        // buck mode
        pwr_inc_pwm_direction = 1;
        in = hvs;
        out = lvs;
    }
    else {
        // boost mode
        pwr_inc_pwm_direction = -1;
        in = lvs;
        out = hvs;
    }

    if (out->power >= output_power_min) {
        power_good_timestamp = time(NULL);     // reset the time
    }

    int pwr_inc_goal = 0;     // stores if we want to increase (+1) or decrease (-1) power

#if 0
    printf("P: %.2f, P_prev: %.2f, v_in: %.2f, v_out: %.2f, i_in: %.2f, i_out: %.2f, "
        "v_chg_target: %.2f, i_chg_max: %.2f, PWM: %.1f, chg_state: %d\n",
        out->power, power_prev, in->voltage, out->voltage, in->current, out->current,
        out->sink_voltage_max, out->pos_current_limit, half_bridge_get_duty_cycle() * 100.0,
        state);
#endif

    if  (time(NULL) - power_good_timestamp > 10 ||      // more than 10s low power
         out->power < -1.0)           // or already generating negative power
    {
        pwr_inc_goal = 0;     // switch off
    }
    else if (out->voltage > (out->sink_voltage_max - out->pos_droop_res * out->current))
    {
       // output voltage target reached
        state = DCDC_STATE_CV;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->current > out->pos_current_limit)
    {
        // output charge current limit reached
        state = DCDC_STATE_CC;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (fabs(lvs->current) > ls_current_max    // current above hardware maximum
        || temp_mosfets > 80                        // temperature limits exceeded
        || (in->voltage < (in->src_voltage_start - in->neg_droop_res * in->current)
            && out->current > 0.1)                  // input voltage below limit
        || in->current < in->neg_current_limit)     // input current (negative signs) limit exceeded
    {
        state = DCDC_STATE_DERATING;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->power < output_power_min && out->voltage < out->src_voltage_start)
    {
        // no load condition (e.g. start-up of nanogrid) --> raise voltage
        pwr_inc_goal = 1;   // increase output power
    }
    else {
        // start MPPT
        state = DCDC_STATE_MPPT;
        if (power_prev > out->power) {
            pwm_delta = -pwm_delta;
        }
        pwr_inc_goal = pwm_delta;
    }

    power_prev = out->power;
    return pwr_inc_goal * pwr_inc_pwm_direction;
}

int Dcdc::check_start_conditions()
{
    if (enabled == false ||
        hvs->voltage > hs_voltage_max ||   // also critical for buck mode because of ringing
        lvs->voltage > ls_voltage_max ||
        lvs->voltage < ls_voltage_min ||
        time(NULL) < (off_timestamp + restart_interval))
    {
        return 0;       // no energy transfer allowed
    }

    if (lvs->pos_current_limit > 0 &&
        lvs->voltage < lvs->sink_voltage_max &&
        lvs->voltage > lvs->sink_voltage_min &&
        hvs->neg_current_limit < 0 &&
        hvs->voltage > hvs->src_voltage_start)
    {
        return 1;       // start in buck mode allowed
    }

    if (hvs->pos_current_limit > 0 &&
        hvs->voltage < hvs->sink_voltage_max &&
        hvs->voltage > hvs->sink_voltage_min &&
        lvs->neg_current_limit < 0 &&
        lvs->voltage > lvs->src_voltage_start)
    {
        return -1;      // start in boost mode allowed
    }

    return 0;
}

void Dcdc::control()
{
    if (half_bridge_enabled()) {
        const char *stop_reason = NULL;
        if (lvs->voltage > ls_voltage_max || hvs->voltage > hs_voltage_max) {
            stop_reason = "emergency (voltage limits exceeded)";
        }
        else if (enabled == false) {
            stop_reason = "disabled";
        }
        else {
            int step = duty_cycle_delta();
            if (step == 0) {
                stop_reason =  "low power";
            }
            else {
                half_bridge_duty_cycle_step(step);
            }
        }

        if (stop_reason != NULL) {
            half_bridge_stop();
            state = DCDC_STATE_OFF;
            off_timestamp = time(NULL);
            printf("DC/DC Stop: %s.\n",stop_reason);
        }
    }
    else { // half bridge is off

        static int current_debounce_counter = 0;
        if (lvs->current > 0.5) {
            // if there is current even though the DC/DC is switched off, the
            // high-side MOSFET must be broken --> set flag and let main() decide
            // what to do... (e.g. call dcdc_self_destruction)
            current_debounce_counter++;
            if (current_debounce_counter > CONTROL_FREQUENCY) {      // waited 1s before setting the flag
                log_data.error_flags |= (1 << ERR_HS_MOSFET_SHORT);
            }
        }
        else {  // hardware is fine
            current_debounce_counter = 0;

            static int startup_delay_counter = 0;

            // wait at least 100 ms for voltages to settle
            static const int num_wait_calls =
                (CONTROL_FREQUENCY / 10 >= 1) ? (CONTROL_FREQUENCY / 10) : 1;

            int startup_mode = check_start_conditions();

            if (startup_mode == 0) {
                startup_delay_counter = 0; // reset counter
            }
            else {
                startup_delay_counter++;
                if (startup_delay_counter > num_wait_calls) {
                    const char *mode_name = NULL;
                    if (startup_mode == 1) {
                        mode_name = "buck";
                        // Don't start directly at Vmpp (approx. 0.8 * Voc) to prevent high inrush
                        // currents and stress on MOSFETs
                        half_bridge_start(lvs->voltage / (hvs->voltage - 1));
                    }
                    else {
                        mode_name = "boost";
                        // Will automatically start with max. duty (0.97) if connected to a
                        // nanogrid not yet started up (zero voltage)
                        half_bridge_start(lvs->voltage / (hvs->voltage + 1));
                    }
                    power_good_timestamp = time(NULL);
                    printf("DC/DC %s mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", mode_name,
                        hvs->voltage, lvs->voltage, half_bridge_get_duty_cycle() * 100);
                    startup_delay_counter = 0; // ensure we will have a delay before next start
                }
            }
        }
    }
}

void Dcdc::test()
{
    if (half_bridge_enabled()) {
        if (half_bridge_get_duty_cycle() > 0.5) {
            half_bridge_duty_cycle_step(-1);
        }
    }
    else {
        static int startup_delay_counter = 0;

        // wait at least 100 ms for voltages to settle
        static const int num_wait_calls =
            (CONTROL_FREQUENCY / 10 >= 1) ? (CONTROL_FREQUENCY / 10) : 1;

        if (startup_delay_counter > num_wait_calls) {
            if (check_start_conditions() != 0) {
                half_bridge_start(lvs->voltage / hvs->voltage);
                printf("DC/DC test mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n",
                    hvs->voltage, lvs->voltage, half_bridge_get_duty_cycle() * 100);
            }
        }
        else {
            startup_delay_counter++;
        }
    }
}

void Dcdc::self_destruction()
{
    printf("Charge controller self-destruction called!\n");
    //half_bridge_stop();
    //half_bridge_init(50, 0, 0, 0.98);   // reset safety limits to allow 0% duty cycle
    //half_bridge_start(0);
    // now the fuse should be triggered and we disappear
}

#endif /* CHARGER_TYPE_PWM */
