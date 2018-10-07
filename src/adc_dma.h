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

#ifndef ADC_DMA_H
#define ADC_DMA_H

#include "mbed.h"
#include "data_objects.h"
#include "structs.h"

extern bool new_reading_available;

void calibrate_current_sensors(dcdc_t *dcdc, load_output_t *load);
void update_measurements(dcdc_t *dcdc, battery_t *bat, load_output_t *load, dcdc_port_t *hs, dcdc_port_t *ls);

void setup_adc_timer(void);
void setup_adc(void);
void start_dma(void);

#endif // ADC_DMA

#endif /* UNIT_TEST */
