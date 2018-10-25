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

/* Basic initialization of battery
 *
 * Configures battery based on settings defined in config.h
 */
void battery_init(battery_t *bat);

/* User settings of battery
 *
 * This function should be implemented in config.cpp
 */
void battery_init_user(battery_t *bat);

/* Update battery settings
 *
 * Most important settings specified in update will be copied to actual battery_t,
 * if suggested updates are valid (includes plausibility check!)
 */
bool battery_update_settings(battery_t *actual, battery_t *update);

/* Charger state machine update, should be called exactly once per second
 */
void charger_state_machine(dcdc_port_t *port, battery_t *bat, float voltage, float current);

void battery_update_energy(battery_t *bat, float voltage, float current, float timespan);

#ifdef __cplusplus
}
#endif

#endif // CHARGER_H
