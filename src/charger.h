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

/** @file
 *
 * @brief Battery charger state machine.
 */

#ifndef CHARGER_H
#define CHARGER_H

#include <stdbool.h>
#include <stdint.h>

#include "dcdc.h"
#include "power_port.h"

/** Possible charger states

 ## Idle
 Initial state of the charge controller. If the solar voltage is high enough
 and the battery is not full, bulk charging mode is started.

 ## Bulk / CC / MPPT charging
 The battery is charged with maximum possible current (MPPT algorithm is
 active) until the CV voltage limit is reached.

 ## Topping / CV / absorption charging
 Lead-acid batteries are charged for some time using a slightly higher charge
 voltage. After a current cutoff limit or a time limit is reached, the charger
 goes into trickle or equalization mode for lead-acid batteries or back into
 Standby for Li-ion batteries.

 ## Trickle charging
 This mode is kept forever for a lead-acid battery and keeps the battery at
 full state of charge. If too much power is drawn from the battery, the
 charger switches back into CC / bulk charging mode.

 ## Equalization charging
 This mode is only used for lead-acid batteries after several deep-discharge
 cycles or a very long period of time with no equalization. Voltage is
 increased to 15V or above, so care must be taken for the other system
 components attached to the battery. (currently, no equalization charging is
 enabled in the software)

 Further information:
 https://en.wikipedia.org/wiki/IUoU_battery_charging
 https://batteryuniversity.com/learn/article/charging_the_lead_acid_battery

 */
enum charger_state {
    CHG_STATE_IDLE,
    CHG_STATE_BULK,
    CHG_STATE_TOPPING,
    CHG_STATE_TRICKLE,
    CHG_STATE_EQUALIZATION
};

/** Charger state machine update, should be called once per second
 */
void charger_state_machine(power_port_t *port, battery_conf_t *bat_conf, battery_state_t *bat_state, float voltage, float current);

#endif /* CHARGER_H */
