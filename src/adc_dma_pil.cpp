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

#include "mbed.h"
#include "config.h"

#ifdef PIL_TESTING     // processor-in-the-loop test

#include "adc_dma.h"
#include "pcb.h"        // contains defines for pins
#include <math.h>       // log for thermistor calculation
#include "log.h"
#include "pwm_switch.h"

#include "pil_test.h"

extern Serial serial;
extern log_data_t log_data;
extern float mcu_temp;

extern pil_test_data_t sim_data;
bool sim_data_initialized = false;

void _init_sim_data()
{
    sim_data.solar_voltage = 12.0;
    sim_data.battery_voltage = 12.6;
    sim_data.dcdc_current = 0;
    sim_data.load_current = 0;
    sim_data.mcu_temperature = 25;
    sim_data.bat_temperature = 25;
    sim_data.internal_temperature = 25;
    sim_data_initialized = true;
}

//----------------------------------------------------------------------------
void update_measurements(dcdc_t *dcdc, battery_state_t *bat, load_output_t *load, power_port_t *hs, power_port_t *ls)
{
    if (sim_data_initialized == false)
        _init_sim_data();

    ls->voltage = sim_data.battery_voltage;
    load->voltage = ls->voltage;

    hs->voltage = sim_data.solar_voltage;

    load->current = sim_data.load_current;

    dcdc->ls_current = sim_data.dcdc_current;
    ls->current = dcdc->ls_current - load->current;
    hs->current = -dcdc->ls_current * ls->voltage / hs->voltage;

    bat->temperature = sim_data.bat_temperature;
    dcdc->temp_mosfets = sim_data.internal_temperature;
    mcu_temp = sim_data.mcu_temperature;

    if (ls->voltage > log_data.battery_voltage_max) {
        log_data.battery_voltage_max = ls->voltage;
    }

    if (hs->voltage > log_data.solar_voltage_max) {
        log_data.solar_voltage_max = hs->voltage;
    }

    if (ls->current > log_data.dcdc_current_max) {
        log_data.dcdc_current_max = ls->current;
    }

    if (load->current > log_data.load_current_max) {
        log_data.load_current_max = load->current;
    }

    if (ls->current > 0) {
        uint16_t solar_power = ls->voltage * ls->current;
        if (solar_power > log_data.solar_power_max_day) {
            log_data.solar_power_max_day = solar_power;
            if (log_data.solar_power_max_day > log_data.solar_power_max_total) {
                log_data.solar_power_max_total = log_data.solar_power_max_day;
            }
        }
    }

    if (load->current > 0) {
        uint16_t load_power = ls->voltage * load->current;
        if (load_power > log_data.load_power_max_day) {
            log_data.load_power_max_day = load_power;
            if (log_data.load_power_max_day > log_data.load_power_max_total) {
                log_data.load_power_max_total = log_data.load_power_max_day;
            }
        }
    }

    if (dcdc->temp_mosfets > log_data.mosfet_temp_max) {
        log_data.mosfet_temp_max = dcdc->temp_mosfets;
    }

    if (bat->temperature > log_data.bat_temp_max) {
        log_data.bat_temp_max = bat->temperature;
    }

    if (mcu_temp > log_data.int_temp_max) {
        log_data.int_temp_max = mcu_temp;
    }
}

// dummy functions
void calibrate_current_sensors(dcdc_t *dcdc, load_output_t *load) {;}
void detect_battery_temperature(battery_state_t *bat, float bat_temp) {;}
void dma_setup() {;}
void adc_setup() {;}
void adc_timer_start(int freq_Hz) {;}

#endif /* TESTING_PIL */
