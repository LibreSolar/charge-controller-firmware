/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#include "dcdc.h"

#include "half_bridge.h"

//#include "data_objects.h"
#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function

void dcdc_init(dcdc_t *dcdc)
{
    dcdc->ls_current_max = DCDC_CURRENT_MAX;
    dcdc->ls_current_min = 0.05;                // A   if lower, charger is switched off
    dcdc->hs_voltage_max = 55.0;                // V
    dcdc->ls_voltage_max = 30.0;                // V
    //dcdc->offset_voltage_start = 4.0;         // V  charging switched on if Vsolar > Vbat + offset
    //dcdc->offset_voltage_stop = 1.0;          // V  charging switched off if Vsolar < Vbat + offset
    dcdc->restart_interval = 60;                // s    --> when should we retry to start charging after low solar power cut-off?
    dcdc->off_timestamp = -10000;               // start immediately
    dcdc->pwm_delta = 1;
}

void dcdc_port_init_bat(dcdc_port_t *port, battery_t *bat)
{
    port->input_allowed = true;     // discharging allowed
    port->output_allowed = true;    // charging allowed


    port->voltage_input_target = bat->cell_voltage_load_reconnect * bat->num_cells;
    port->voltage_input_stop = bat->cell_voltage_load_disconnect * bat->num_cells;
    port->current_input_max = -bat->charge_current_max;          // TODO: discharge current

    port->voltage_output_target = bat->cell_voltage_max * bat->num_cells;
    port->voltage_output_min = bat->cell_voltage_absolute_min * bat->num_cells;
    port->current_output_max = bat->charge_current_max;

}

void dcdc_port_init_solar(dcdc_port_t *port)
{
    port->input_allowed = true;         // PV panel may provide power to solar input of DC/DC
    port->output_allowed = false;
    
    port->voltage_input_target = 16.0;
    port->voltage_input_stop = 14.0;
    port->current_input_max = -18.0;
}

void dcdc_port_init_nanogrid(dcdc_port_t *port)
{
    port->input_allowed = true;
    port->output_allowed = true;

    port->voltage_input_target = 25.0;      // starting buck mode above this point
    port->voltage_input_stop = 20.0;        // stopping buck mode below this point
    port->current_input_max = -5.0;
    
    port->voltage_output_target = 23.0;        // starting idle mode above this point
    port->current_output_max = 5.0;
    port->voltage_output_min = 10.0;
    port->droop_resistance = 1.0;           // 1 Ohm means 1V change of target voltage per amp
}


// returns if output power should be increased (1), decreased (-1) or switched off (0)
int _dcdc_output_control(dcdc_t *dcdc, dcdc_port_t *out, dcdc_port_t *in)
{
    //static float dcdc_power;
    float dcdc_power_new = out->voltage * out->current;
    static int pwm_delta = 1;

    //printf("P: %.2f, P_prev: %.2f, v_in: %.2f, v_out: %.2f, i_in: %.2f, i_out: %.2f, v_target: %.2f, i_max: %.2f, PWM: %.1f, chg_en: %d",
    //     dcdc_power_new, dcdc->power, in->voltage, out->voltage, in->current, out->current,
    //     target_voltage, out->current_output_max, half_bridge_get_duty_cycle() * 100.0, out->output_allowed);

    if (out->output_allowed == false || in->input_allowed == false
        || (in->voltage < in->voltage_input_stop && out->current < 0.1))
    {
        return 0;
    }
    else if (out->voltage > (out->voltage_output_target - out->droop_resistance * out->current)    // output voltage above target
        || out->current > out->current_output_max                                               // output current limit exceeded
        || in->voltage < (in->voltage_input_target - in->droop_resistance * in->current)        // input voltage below limit
        || in->current < in->current_input_max                                                  // input current (negative signs) above limit
        || fabs(dcdc->ls_current) > dcdc->ls_current_max                                        // current above hardware maximum
        || dcdc->temp_mosfets > 80)                                                             // temperature limits exceeded
    {
        //printf("   dec\n");
        return -1;  // decrease output power
    }
    else if (out->current < 0.1 && out->voltage < out->voltage_input_target)  // no load condition (e.g. start-up of nanogrid) --> raise voltage
    {
        //printf("   inc\n");
        return 1;   // increase output power
    }
    else {
        //printf("   MPPT\n");
        // start MPPT
        if (dcdc->power > dcdc_power_new) {
            pwm_delta = -pwm_delta;
        }
        dcdc->power = dcdc_power_new;
        return pwm_delta;
    }
}

bool _dcdc_check_start_conditions(dcdc_t *dcdc, dcdc_port_t *out, dcdc_port_t *in)
{
    return out->output_allowed == true
        && out->voltage < out->voltage_output_target
        && out->voltage > out->voltage_output_min
        && in->input_allowed == true
        && in->voltage > in->voltage_input_target
        //&& dcdc->hs_voltage - dcdc->ls_voltage > dcdc->offset_voltage_start
        && time(NULL) > (dcdc->off_timestamp + dcdc->restart_interval);
}

void dcdc_control(dcdc_t *dcdc, dcdc_port_t *hs, dcdc_port_t *ls)
{
    if (half_bridge_enabled()) {
        int step;
        if (ls->current > 0.1) {    // buck mode
            printf("-");
            step = _dcdc_output_control(dcdc, ls, hs);
            half_bridge_duty_cycle_step(step);
        }
        else {
            printf("+");
            step = _dcdc_output_control(dcdc, hs, ls);
            half_bridge_duty_cycle_step(-step);
        }
        if (step == 0) {
            half_bridge_stop();
            dcdc->off_timestamp = time(NULL);
            printf("DC/DC stop.\n");
        }
    } 
    else {
        if (_dcdc_check_start_conditions(dcdc, ls, hs)) {
            // Vmpp at approx. 0.8 * Voc --> take 0.9 as starting point for MPP tracking             
            half_bridge_start(ls->voltage / (hs->voltage * 0.9));
            printf("DC/DC buck mode start.\n");
        }
        else if (_dcdc_check_start_conditions(dcdc, hs, ls)) {
            // will automatically start with max. duty (0.97) if connected to a nanogrid not yet started up (zero voltage)
            half_bridge_start((ls->voltage * 0.9) / hs->voltage);
            printf("DC/DC boost mode start.\n");
        }
    }
}

#endif /* UNIT_TEST */
