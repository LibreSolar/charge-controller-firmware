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

#ifndef CHARGER_TYPE_PWM

#include "half_bridge.h"

#include <time.h>       // for time(NULL) function
#include <math.h>       // for fabs function
#include <stdlib.h>     // for min/max function
#include <stdio.h>

extern LogData log_data;

void dcdc_init(Dcdc *dcdc, DcBus *hv, DcBus *lv, DcdcOperationMode mode)
{
    dcdc->hv_bus = hv;
    dcdc->lv_bus = lv;
    dcdc->mode           = mode;
    dcdc->enabled        = true;
    dcdc->state          = DCDC_STATE_OFF;
    dcdc->ls_current_max = DCDC_CURRENT_MAX;
    dcdc->ls_current_min = 0.05;                // A   if lower, charger is switched off
    dcdc->hs_voltage_max = HIGH_SIDE_VOLTAGE_MAX;   // V
    dcdc->ls_voltage_max = LOW_SIDE_VOLTAGE_MAX;    // V
    dcdc->ls_voltage_min = 9.0;
    //dcdc->offset_voltage_start = 4.0;         // V  charging switched on if Vsolar > Vbat + offset
    //dcdc->offset_voltage_stop = 1.0;          // V  charging switched off if Vsolar < Vbat + offset
    dcdc->restart_interval = 60;                // s    --> when should we retry to start charging after low solar power cut-off?
    dcdc->off_timestamp = -10000;               // start immediately
    dcdc->pwm_delta = 1;
}

// returns if output power should be increased (1), decreased (-1) or switched off (0)
/**
 * @param dcdc_dir 1 =>  current flows from ls to hs through the dcdc, -1 opposite
 */
int _dcdc_output_control(Dcdc *dcdc, DcBus *out, DcBus *in, const int dcdc_dir)
{
    float dcdc_power_new = dcdc->lv_bus->voltage * dcdc->lv_bus->current * dcdc_dir;

    static uint32_t low_dcdc_power_count = 0;

    // less than 1W dcdc power
    bool dcdc_power_low = dcdc_power_new < 1.0;

    // count how long low power condition remains
    // reset counter if no lower power condition
    low_dcdc_power_count  = dcdc_power_low ? low_dcdc_power_count + 1 : 0;

    int retval = 0;

    //printf("P: %.2f, P_prev: %.2f, v_in: %.2f, v_out: %.2f, i_in: %.2f, i_out: %.2f, v_chg_target: %.2f, i_chg_max: %.2f, PWM: %.1f, chg_state: %d\n",
    //     dcdc_power_new, dcdc->power, in->voltage, out->voltage, in->current, out->current, out->chg_voltage_target,
    //     out->chg_current_max, half_bridge_get_duty_cycle() * 100.0, dcdc->state);
    if  (
            (out->chg_allowed == false && out->dis_allowed == false)        // we are not supposed to transfer any energy into the outputs
            || low_dcdc_power_count > 100                                 // the transfered energy drops below a threshold for more than 10s, we make a small averaging here
            || in->dis_allowed == false                                     // we are not allowed to take energy from the input
            // || (in->voltage < in->dis_voltage_stop && out->current < 0.1)   // input voltage is too low and we have no output consumption (battery or grid)
        )
    {
        low_dcdc_power_count = 0; // reset low dcdc current "timer"
        retval = 0;
    }
    else if (out->voltage > (out->chg_voltage_target - out->chg_droop_res * out->current)                     // output voltage above target
        || (in->voltage < (in->dis_voltage_start - in->dis_droop_res * in->current) && out->current > 0.1))     // input voltage below limit
    {
        dcdc->state = DCDC_STATE_CV;
        retval = -1;  // decrease output power
    }
    else if (out->current > out->chg_current_max         // output charge current limit exceeded
        || in->current < in->dis_current_max)            // input current (negative signs) limit exceeded
    {
        dcdc->state = DCDC_STATE_CC;
        retval = -1;  // decrease output power
    }
    else if (fabs(dcdc->lv_bus->current) > dcdc->ls_current_max          // current above hardware maximum
        || dcdc->temp_mosfets > 80)                                 // temperature limits exceeded
    {
        dcdc->state = DCDC_STATE_DERATING;
        retval = -1;  // decrease output power
    }
    else if (dcdc_power_low == true && out->voltage < out->dis_voltage_start)  // no load condition (e.g. start-up of nanogrid) --> raise voltage
    {
        retval = 1;   // increase output power
    }
    else {
        // start MPPT
        dcdc->state = DCDC_STATE_MPPT;
        if (dcdc->power > dcdc_power_new) {
            dcdc->pwm_delta = -dcdc->pwm_delta;
        }
        retval = dcdc->pwm_delta;
    }
    // store the power BEFORE the executing the current change
    dcdc->power = dcdc_power_new;
    return retval;
}

int dcdc_check_start_conditions(Dcdc *dcdc)
{
    if (dcdc->enabled == false ||
        dcdc->hv_bus->voltage > dcdc->hs_voltage_max ||   // also critical for buck mode because of ringing
        dcdc->lv_bus->voltage > dcdc->ls_voltage_max ||
        dcdc->lv_bus->voltage < dcdc->ls_voltage_min ||
        time(NULL) < (dcdc->off_timestamp + dcdc->restart_interval))
    {
        return 0;       // no energy transfer allowed
    }

    if (dcdc->lv_bus->chg_current_max > 0 &&
        dcdc->lv_bus->voltage < dcdc->lv_bus->chg_voltage_target &&
        dcdc->lv_bus->voltage > dcdc->lv_bus->chg_voltage_min &&
        dcdc->hv_bus->dis_current_max < 0 &&
        dcdc->hv_bus->voltage > dcdc->hv_bus->dis_voltage_start)
    {
        return 1;       // start in buck mode allowed
    }

    if (dcdc->hv_bus->chg_current_max > 0 &&
        dcdc->hv_bus->voltage < dcdc->hv_bus->chg_voltage_target &&
        dcdc->hv_bus->voltage > dcdc->hv_bus->chg_voltage_min &&
        dcdc->lv_bus->dis_current_max < 0 &&
        dcdc->lv_bus->voltage > dcdc->lv_bus->dis_voltage_start)
    {
        return -1;      // start in boost mode allowed
    }

    return 0;
}

void dcdc_control(Dcdc *dcdc)
{
    if (half_bridge_enabled()) {
        int step;

        if (dcdc->mode == MODE_MPPT_BUCK || dcdc->lv_bus->current > 0.1 ) {
            // always in buck mode or when boost or nano grid are receiving energy from high side enegry producer
            // ( grid [nanogrid] or battery [boost] )
            // for low side energy consumers (battery and/or load).

            //printf("-");
            step = _dcdc_output_control(dcdc, dcdc->lv_bus, dcdc->hv_bus, 1);
            half_bridge_duty_cycle_step(step);
        }
        else {
            //printf("+");
            step = _dcdc_output_control(dcdc, dcdc->hv_bus, dcdc->lv_bus, -1);
            half_bridge_duty_cycle_step(-step);
        }

        if (step == 0) {
            half_bridge_stop();
            dcdc->state = DCDC_STATE_OFF;
            dcdc->off_timestamp = time(NULL);
            printf("DC/DC stop.\n");
        }
        else if (dcdc->lv_bus->voltage > dcdc->ls_voltage_max || dcdc->hv_bus->voltage > dcdc->hs_voltage_max) {
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
        static int current_debounce_counter = 0;
        if (dcdc->lv_bus->current > 0.5) {
            // if there is current even though the DC/DC is switched off, the
            // high-side MOSFET must be broken --> set flag and let main() decide
            // what to do... (e.g. call dcdc_self_destruction)
            current_debounce_counter++;
            if (current_debounce_counter > CONTROL_FREQUENCY) {      // waited 1s before setting the flag
                log_data.error_flags |= (1 << ERR_HS_MOSFET_SHORT);
            }
            return;
        }
        else {
            current_debounce_counter = 0;
        }

        static int startup_delay_counter = 0;
        static const int num_wait_calls = (CONTROL_FREQUENCY / 10 >= 1) ? (CONTROL_FREQUENCY / 10) : 1; // wait at least 100 ms for voltages to settle

        int startup_mode = dcdc_check_start_conditions(dcdc);
        if (startup_mode == 1) {
            if (startup_delay_counter >= num_wait_calls) {
                // Don't start directly at Vmpp (approx. 0.8 * Voc) to prevent high inrush currents and stress on MOSFETs
                half_bridge_start(dcdc->lv_bus->voltage / (dcdc->hv_bus->voltage - 1));
                printf("DC/DC buck mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", dcdc->hv_bus->voltage, dcdc->lv_bus->voltage, half_bridge_get_duty_cycle() * 100);
            }
            else {
                startup_delay_counter++;
            }
        }
        else if (startup_mode == -1) {
            if (startup_delay_counter >= num_wait_calls) {
                // will automatically start with max. duty (0.97) if connected to a nanogrid not yet started up (zero voltage)
                half_bridge_start(dcdc->lv_bus->voltage / (dcdc->hv_bus->voltage + 1));
                printf("DC/DC boost mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", dcdc->hv_bus->voltage, dcdc->lv_bus->voltage, half_bridge_get_duty_cycle() * 100);
            }
            else {
                startup_delay_counter++;
            }
        }
        else {
            startup_delay_counter = 0;    // reset counter
        }
    }
}

void dcdc_test(Dcdc *dcdc)
{
    if (half_bridge_enabled()) {
        if (half_bridge_get_duty_cycle() > 0.5) {
            half_bridge_duty_cycle_step(-1);
        }
    }
    else {
        static int startup_delay_counter = 0;
        static const int num_wait_calls = (CONTROL_FREQUENCY / 10 >= 1) ? (CONTROL_FREQUENCY / 10) : 1; // wait at least 100 ms for voltages to settle

        int startup_mode = dcdc_check_start_conditions(dcdc);
        if (startup_mode == 1) {
            if (startup_delay_counter >= num_wait_calls) {
                // Don't start directly at Vmpp (approx. 0.8 * Voc) to prevent high inrush currents and stress on MOSFETs
                half_bridge_start(dcdc->lv_bus->voltage / (dcdc->hv_bus->voltage - 1));
                printf("DC/DC buck mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", dcdc->hv_bus->voltage, dcdc->lv_bus->voltage, half_bridge_get_duty_cycle() * 100);
            }
            else {
                startup_delay_counter++;
            }
        }
        else if (startup_mode == -1) {
            if (startup_delay_counter >= num_wait_calls) {
                // will automatically start with max. duty (0.97) if connected to a nanogrid not yet started up (zero voltage)
                half_bridge_start(dcdc->lv_bus->voltage / (dcdc->hv_bus->voltage + 1));
                printf("DC/DC boost mode start (HV: %.2fV, LV: %.2fV, PWM: %.1f).\n", dcdc->hv_bus->voltage, dcdc->lv_bus->voltage, half_bridge_get_duty_cycle() * 100);
            }
            else {
                startup_delay_counter++;
            }
        }
        else {
            startup_delay_counter = 0;    // reset counter
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

#endif /* CHARGER_TYPE_PWM */
