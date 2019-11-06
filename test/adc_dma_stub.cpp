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

#include "config.h"
#include "pcb.h"
#include "adc_dma.h"
#include "adc_dma_stub.h"

#include <stdint.h>
#include <stdio.h>

extern uint16_t adc_readings[NUM_ADC_CH];
extern uint32_t adc_filtered[NUM_ADC_CH];

void prepare_adc_readings(AdcValues values)
{
    adc_readings[ADC_POS_VREF_MCU] = (uint16_t)(1.224 / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_V_SOLAR] = (uint16_t)((values.solar_voltage / (ADC_GAIN_V_SOLAR)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_V_BAT] = (uint16_t)((values.battery_voltage / (ADC_GAIN_V_BAT)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_I_DCDC] = (uint16_t)((values.dcdc_current / (ADC_GAIN_I_DCDC)) / 3.3 * 4096) << 4;
    adc_readings[ADC_POS_I_LOAD] = (uint16_t)((values.load_current / (ADC_GAIN_I_LOAD)) / 3.3 * 4096) << 4;
}

void prepare_adc_filtered()
{
    // initialize also filtered values
    for (int i = 0; i < NUM_ADC_CH; i++) {
        adc_filtered[i] = adc_readings[i] << ADC_FILTER_CONST;
    }
}
