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

#include <stdbool.h>
#include <stdint.h>

#include "dcdc.h"
#include "power_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// possible charger states
enum charger_state {
    CHG_STATE_IDLE,
    CHG_STATE_CC,
    CHG_STATE_CV,
    CHG_STATE_TRICKLE,
    CHG_STATE_EQUALIZATION
};

/* Charger state machine update, should be called exactly once per second
 */
void charger_state_machine(power_port_t *port, battery_t *bat, float voltage, float current);

#ifdef __cplusplus
}
#endif

#endif // CHARGER_H
