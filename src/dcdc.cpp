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
#include "config.h"
#include "pcb.h"
#include "log.h"

#include "half_bridge.h"

#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function

extern log_data_t log_data;

void dcdc_init(dcdc_t *dcdc)
{
    dcdc->mode           = DCDC_MODE_INIT;
    dcdc->enabled        = true;
    dcdc->state          = DCDC_STATE_OFF;
    dcdc->ls_current_max = DCDC_CURRENT_MAX;
    dcdc->ls_current_min = 0.05;                // A   if lower, charger is switched off
    dcdc->hs_voltage_max = HIGH_SIDE_VOLTAGE_MAX;   // V
    dcdc->ls_voltage_max = LOW_SIDE_VOLTAGE_MAX;    // V
    //dcdc->offset_voltage_start = 4.0;         // V  charging switched on if Vsolar > Vbat + offset
    //dcdc->offset_voltage_stop = 1.0;          // V  charging switched off if Vsolar < Vbat + offset
    dcdc->restart_interval = 60;                // s    --> when should we retry to start charging after low solar power cut-off?
    dcdc->off_timestamp = -10000;               // start immediately
    dcdc->pwm_delta = 1;
}

// returns if output power should be increased (1), decreased (-1) or switched off (0)
int _dcdc_output_control(dcdc_t *dcdc, power_port_t *out, power_port_t *in)
{
    float dcdc_power_new = out->voltage * out->current;
    static int pwm_delta = 1;

    //printf("P: %.2f, P_prev: %.2f, v_in: %.2f, v_out: %.2f, i_in: %.2f, i_out: %.2f, i_max: %.2f, PWM: %.1f, chg_en: %d\n",
    //     dcdc_power_new, dcdc->power, in->voltage, out->voltage, in->current, out->current,
    //     out->current_output_max, half_bridge_get_duty_cycle() * 100.0, out->output_allowed);

    if (out->output_allowed == false || in->input_allowed == false
        || (in->voltage < in->voltage_input_stop && out->current < 0.1))
    {
        return 0;
    }
    else if (out->voltage > (out->voltage_output_target - out->droop_res_output * out->current)                     // output voltage above target
        || (in->voltage < (in->voltage_input_start - in->droop_res_input * in->current) && out->current > 0.1))     // input voltage below limit
    {
        dcdc->state = DCDC_STATE_CV;
        return -1;  // decrease output power
    }
    else if (out->current > out->current_output_max         // output current limit exceeded
        || in->current < in->current_input_max)             // input current (negative signs) limit exceeded
    {
        dcdc->state = DCDC_STATE_CC;
        return -1;  // decrease output power
    }
    else if (fabs(dcdc->ls_current) > dcdc->ls_current_max          // current above hardware maximum
        || dcdc->temp_mosfets > 80)                                 // temperature limits exceeded
    {
        dcdc->state = DCDC_STATE_DERATING;
        return -1;  // decrease output power
    }
    else if (out->current < 0.1 && out->voltage < out->voltage_input_start)  // no load condition (e.g. start-up of nanogrid) --> raise voltage
    {
        return 1;   // increase output power
    }
    else {
        // start MPPT
        dcdc->state = DCDC_STATE_MPPT;
        if (dcdc->power > dcdc_power_new) {
            pwm_delta = -pwm_delta;
        }
        dcdc->power = dcdc_power_new;
        return pwm_delta;
    }
}

bool _dcdc_check_start_conditions(dcdc_t *dcdc, power_port_t *out, power_port_t *in)
{
    return dcdc->enabled == true
        && out->output_allowed == true
        && out->voltage < out->voltage_output_target
        && out->voltage > out->voltage_output_min
        && in->input_allowed == true
        && in->voltage > in->voltage_input_start
        //&& dcdc->hs_voltage - dcdc->ls_voltage > dcdc->offset_voltage_start
        && time(NULL) > (dcdc->off_timestamp + dcdc->restart_interval);
}

void dcdc_control(dcdc_t *dcdc, power_port_t *hs, power_port_t *ls)
{
    if (half_bridge_enabled()) {
        int step;
        if (ls->current > 0.1) {    // buck mode
            //printf("-");
            step = _dcdc_output_control(dcdc, ls, hs);
            half_bridge_duty_cycle_step(step);
        }
        else {
            //printf("+");
            step = _dcdc_output_control(dcdc, hs, ls);
            half_bridge_duty_cycle_step(-step);
        }

        if (step == 0) {
            half_bridge_stop();
            dcdc->state = DCDC_STATE_OFF;
            dcdc->off_timestamp = time(NULL);
            printf("DC/DC stop.\n");
        }
        else if (ls->voltage > dcdc->ls_voltage_max || hs->voltage > dcdc->hs_voltage_max) {
            half_bridge_stop();
            dcdc->state = DCDC_STATE_OFF;
            dcdc->off_timestamp = time(NULL);
            printf("DC/DC emergency stop (voltage limits exceeded).\n");
        }
        else if (dcdc->enabled == false) {
            half_bridge_stop();
            dcdc->state = DCDC_STATE_OFF;
            printf("DC/DC stop (disabled).\n");
        }
    }
    else {
        static int debounce_counter = 0;
        if (dcdc->ls_current > 0.5) {
            // if there is current even though the DC/DC is switched off, the
            // high-side MOSFET must be broken --> set flag and let main() decide
            // what to do... (e.g. call dcdc_self_destruction)
            debounce_counter++;
            if (debounce_counter > CONTROL_FREQUENCY) {      // waited 1s before setting the flag
                log_data.error_flags |= (1 << ERR_HS_MOSFET_SHORT);
            }
            return;
        }
        debounce_counter = 0;

        if (_dcdc_check_start_conditions(dcdc, ls, hs) && ls->voltage < dcdc->ls_voltage_max) {
            // Vmpp at approx. 0.8 * Voc --> take 0.9 as starting point for MPP tracking
            half_bridge_start(ls->voltage / (hs->voltage * 0.9));
            printf("DC/DC buck mode start.\n");
        }
        else if (_dcdc_check_start_conditions(dcdc, hs, ls) && hs->voltage < dcdc->hs_voltage_max) {
            // will automatically start with max. duty (0.97) if connected to a nanogrid not yet started up (zero voltage)
            half_bridge_start((ls->voltage * 0.9) / hs->voltage);
            printf("DC/DC boost mode start.\n");
        }
    }
}

void dcdc_self_destruction()
{
    printf("Charge controller self-destruction called!\n");
    //half_bridge_stop();
    //half_bridge_init(50, 0, 0, 0.98);   // reset safety limits to allow 0% duty cycle
    //half_bridge_start(0);
    // now the fuse should be triggered and we disappear
}
