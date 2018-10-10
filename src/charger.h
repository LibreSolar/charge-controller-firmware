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

#ifndef __CHARGER_H_
#define __CHARGER_H_

#include "data_objects.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void battery_init(battery_t* bat, int type, int capacity_Ah, int num_cells_series);

/* Initialize DC/DC and DC/DC port structs
 *
 * See http://libre.solar/docs/dcdc_control for detailed information
 */
//void dcdc_init(dcdc_t *dcdc);
//void dcdc_port_init_bat(dcdc_port_t *port, battery_t *bat);
//void dcdc_port_init_solar(dcdc_port_t *port);
//void dcdc_port_init_nanogrid(dcdc_port_t *port);

/* Charger state machine update, should be called exactly once per second
 */
void charger_state_machine(dcdc_port_t *port, battery_t *bat, float voltage, float current);

void bat_calculate_soc(battery_t *bat, float voltage, float current);

//void dcdc_control(dcdc_t *dcdc, dcdc_port_t *high_side, dcdc_port_t *low_side);

#ifdef __cplusplus
}
#endif

#endif // CHARGER_H
