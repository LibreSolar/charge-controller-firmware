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
    dcdc->hs_voltage_max = HIGH_SIDE_VOLTAGE_MAX;
    dcdc->ls_voltage_max = LOW_SIDE_VOLTAGE_MAX;
    dcdc->ls_voltage_min = 9.0;
    dcdc->output_power_min = 1;         // switch off iff power < 1 W
    dcdc->restart_interval = 60;
    dcdc->off_timestamp = -10000;       // start immediately
    dcdc->pwm_delta = 1;                // start-condition of duty cycle pwr_inc_pwm_direction
}

/** Calculates the duty cycle change depending on operating mode and actual measurements
 *
 * @returns duty cycle step (-1 or 1) or 0 if DC/DC should be switched off
 */
int duty_cycle_delta(Dcdc *dcdc)
{
    int pwr_inc_pwm_direction;      // direction of PWM duty cycle change for increasing power
    DcBus *in;
    DcBus *out;

    if (dcdc->mode == MODE_MPPT_BUCK ||
        (dcdc->mode == MODE_NANOGRID && dcdc->lv_bus->current > 0.1))
    {
        // buck mode
        pwr_inc_pwm_direction = 1;
        in = dcdc->hv_bus;
        out = dcdc->lv_bus;
    }
    else {
        // boost mode
        pwr_inc_pwm_direction = -1;
        in = dcdc->lv_bus;
        out = dcdc->hv_bus;
    }

    if (out->power >= dcdc->output_power_min) {
        dcdc->power_good_timestamp = time(NULL);     // reset the time
    }

    int pwr_inc_goal = 0;     // stores if we want to increase (+1) or decrease (-1) power

#if 0
    printf("P: %.2f, P_prev: %.2f, v_in: %.2f, v_out: %.2f, i_in: %.2f, i_out: %.2f, v_chg_target: %.2f, i_chg_max: %.2f, PWM: %.1f, chg_state: %d\n",
         out->power, dcdc->power_prev, in->voltage, out->voltage, in->current, out->current, out->chg_voltage_target,
         out->chg_current_max, half_bridge_get_duty_cycle() * 100.0, dcdc->state);
#endif

    if  (time(NULL) - dcdc->power_good_timestamp > 10 ||      // more than 10s low power
         out->power < -1.0)           // or already generating negative power
    {
        pwr_inc_goal = 0;     // switch off
    }
    else if (out->voltage > (out->chg_voltage_target - out->chg_droop_res * out->current)                     // output voltage above target
        || (in->voltage < (in->dis_voltage_start - in->dis_droop_res * in->current) && out->current > 0.1))     // input voltage below limit
    {
        dcdc->state = DCDC_STATE_CV;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->current > out->chg_current_max    // output charge current limit exceeded
        || in->current < in->dis_current_max)       // input current (negative signs) limit exceeded
    {
        dcdc->state = DCDC_STATE_CC;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (fabs(dcdc->lv_bus->current) > dcdc->ls_current_max   // current above hardware maximum
        || dcdc->temp_mosfets > 80)                               // temperature limits exceeded
    {
        dcdc->state = DCDC_STATE_DERATING;
        pwr_inc_goal = -1;  // decrease output power
    }
    else if (out->power < dcdc->output_power_min
        && out->voltage < out->dis_voltage_start)   // no load condition (e.g. start-up of nanogrid)
                                                    // --> raise voltage
    {
        pwr_inc_goal = 1;   // increase output power
    }
    else {
        // start MPPT
        dcdc->state = DCDC_STATE_MPPT;
        if (dcdc->power_prev > out->power) {
            dcdc->pwm_delta = -dcdc->pwm_delta;
        }
        pwr_inc_goal = dcdc->pwm_delta;
    }
    // store the power BEFORE the executing the current change
    dcdc->power_prev = out->power;
    return pwr_inc_goal * pwr_inc_pwm_direction;
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
        int step = duty_cycle_delta(dcdc);
        half_bridge_duty_cycle_step(step);

        if (step == 0) {
            half_bridge_stop();
            dcdc->state = DCDC_STATE_OFF;
            dcdc->off_timestamp = time(NULL);
            printf("DC/DC stop (low power).\n");
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
                dcdc->power_good_timestamp = time(NULL);
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
                dcdc->power_good_timestamp = time(NULL);
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
