/* mbed library for a battery charge controller
 * Copyright (c) 2017 Martin Jäger (www.libre.solar)
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

#include "charger.h"


static ChargingProfile *_profile;          // all charging profile variables
static int _state;                         // valid states: enum charger_states
static int _time_state_changed;            // timestamp of last state change
static float _target_voltage;              // target voltage for current state
static float _target_current;              // target current for current state
static int _time_voltage_limit_reached;    // last time the CV limit was reached
static bool _charging_enabled;
static bool _discharging_enabled;


// private function

/** Enter a different charger state
 *
 *  @param next_state Next state (e.g. CHG_CC)
 */
void charger_enter_state(int next_state);


void charger_init(ChargingProfile *profile)
{
    _profile = profile;
    _charging_enabled = false;
    _time_state_changed = -profile->time_limit_recharge;     // start immediately
}

/*****************************************************************************
 *  Charger state machine

 ## Idle
 Initial state of the charge controller. If the solar voltage is high enough
 and the battery is not full, charging in CC mode is started.

 ### CC / bulk charging
 The battery is charged with maximum possible current (MPPT algorithm is
 active) until the CV voltage limit is reached.

 ### CV / absorption charging
 Lead-acid batteries are charged for some time using a slightly higher charge
 voltage. After a current cutoff limit or a time limit is reached, the charger
 goes into trickle or equalization mode for lead-acid batteries or back into
 Standby for Li-ion batteries.

 ### Trickle charging
 This mode is kept forever for a lead-acid battery and keeps the battery at
 full state of charge. If too much power is drawn from the battery, the
 charger switches back into CC / bulk charging mode.

 ### Equalization charging
 This mode is only used for lead-acid batteries after several deep-discharge
 cycles or a very long period of time with no equalization. Voltage is
 increased to 15V or above, so care must be taken for the other system
 components attached to the battery. (currently, no equalization charging is
 enabled in the software)

 */

void charger_update(float battery_voltage, float battery_current) {

    //printf("_time_state_change = %d, time = %d, v_bat = %f, i_bat = %f\n", _time_state_changed, time(NULL), battery_voltage, battery_current);

    // Load management
    if (battery_voltage < _profile->cell_voltage_load_disconnect * _profile->num_cells) {
        _discharging_enabled = false;
    }
    if (battery_voltage >= _profile->cell_voltage_load_reconnect * _profile->num_cells) {
        _discharging_enabled = true;
    }

    // state machine
    switch (_state) {

        case CHG_IDLE: {
            if  (battery_voltage < _profile->num_cells * _profile->cell_voltage_recharge
                 && (time(NULL) - _time_state_changed) > _profile->time_limit_recharge)
            {
                _target_current = _profile->charge_current_max;
                _target_voltage = _profile->num_cells * _profile->cell_voltage_max;
                _charging_enabled = true;
                charger_enter_state(CHG_CC);
            }
            break;
        }

        case CHG_CC: {
            if (battery_voltage > _target_voltage) {
                _target_voltage = _profile->num_cells * _profile->cell_voltage_max;
                charger_enter_state(CHG_CV);
            }
            break;
        }

        case CHG_CV: {
            if (battery_voltage >= _target_voltage) {
                _time_voltage_limit_reached = time(NULL);
            }

            // cut-off limit reached because battery full (i.e. CV mode still
            // reached by available solar power within last 2s) or CV period long enough?
            if ((battery_current < _profile->current_cutoff_CV && (time(NULL) - _time_voltage_limit_reached) < 2)
                || (time(NULL) - _time_state_changed) > _profile->time_limit_CV)
            {
                if (_profile->equalization_enabled) {
                    // TODO: additional conditions!
                    _target_voltage = _profile->num_cells * _profile->cell_voltage_equalization;
                    _target_current = _profile->current_limit_equalization;
                    charger_enter_state(CHG_EQUALIZATION);
                }
                else if (_profile->trickle_enabled) {
                    _target_voltage = _profile->num_cells * _profile->cell_voltage_trickle;
                    charger_enter_state(CHG_TRICKLE);
                }
                else {
                    _target_current = 0;
                    _charging_enabled = false;
                    charger_enter_state(CHG_IDLE);
                }
            }
            break;
        }

        case CHG_TRICKLE: {
            if (battery_voltage >= _target_voltage) {
                _time_voltage_limit_reached = time(NULL);
            }

            if (time(NULL) - _time_voltage_limit_reached > _profile->time_trickle_recharge)
            {
                _target_current = _profile->charge_current_max;
                _target_voltage = _profile->num_cells * _profile->cell_voltage_max;
                charger_enter_state(CHG_CC);
            }
            // assumtion: trickle does not harm the battery --> never go back to idle
            // (for Li-ion battery: disable trickle!)
            break;
        }
    }
}

void charger_enter_state(int next_state)
{
    printf("Enter State: %d\n", next_state);
    _time_state_changed = time(NULL);
    _state = next_state;
}

bool charger_discharging_enabled()
{
    return _discharging_enabled;
}

bool charger_charging_enabled()
{
    return _charging_enabled;
}

int charger_get_state()
{
    return _state;
}

float charger_read_target_current()
{
    return _target_current;
}

float charger_read_target_voltage()
{
    return _target_voltage;
}
