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
#ifndef ADC_DMA_STUB_H
#define ADC_DMA_STUB_H

#include "config.h"

/** Data to fill adc_filtered array during unit-tests
 */
typedef struct {
    float solar_voltage;
    float battery_voltage;
    float dcdc_current;
    float load_current;
    float bat_temperature;
    float internal_temperature;
} AdcValues;

void prepare_adc_readings(AdcValues values);

void prepare_adc_filtered();
void clear_adc_filtered();
uint32_t get_adc_filtered(uint32_t channel);
uint16_t adc_get_alert_limit(float scale, float limit);

#endif