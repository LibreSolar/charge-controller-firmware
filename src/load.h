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

#ifndef LOAD_H
#define LOAD_H

/** @file
 *
 * @brief
 * Load/USB output functions and data types
 */

#include <stdint.h>
#include <stdbool.h>

#include "power_port.h"

/** Load/USB output states
 *
 * Used for USB or normal load outputs connecting to battery via switch.
 */
enum LoadState {
    LOAD_STATE_DISABLED = 0,        ///< Actively disabled
    LOAD_STATE_ON,                  ///< Normal state: On
    LOAD_STATE_OFF_LOW_SOC,         ///< Off to protect battery (overrules target setting)
    LOAD_STATE_OFF_OVERCURRENT,     ///< Off to protect charge controller (overrules target setting)
    LOAD_STATE_OFF_OVERVOLTAGE,     ///< Off to protect loads (overrules target setting)
    LOAD_STATE_OFF_SHORT_CIRCUIT    ///< Off to protect charge controller (overrules target setting)
};

/** Load output type
 *
 * Stores status of load output incl. 5V USB output (if existing on PCB)
 */
class LoadOutput
{
public:
    /** Initialize LoadOutput struct and overcurrent / short circuit protection comparator (if existing)
     *
     * @param load Load output struct
     * @param bus_int Internal DC bus the load switch is connected to (e.g. battery and DC/DC junction)
     * @param terminal Load terminal DC bus (external interface)
     */
    LoadOutput(PowerPort *pwr_port);

    /** Enable/disable load switch
     */
    void switch_set(bool enabled);

    /** Enable/disable USB output
     */
    void usb_set(bool enabled);

    /** State machine, called every second.
     */
    void state_machine();

    /** USB state machine, called every second.
     */
    void usb_state_machine();

    /** Main load control function, should be called by control timer
     *
     * Performs time-critical checks like overcurrent and overvoltage
     */
    void control(float load_max_voltage);

    uint16_t switch_state;      ///< Current state of load output switch
    uint16_t usb_state;         ///< Current state of USB output

    DcBus *bus;                 ///< Pointer to internal DC bus where the load output
                                ///< current is coming from
    PowerPort *port;            ///< Pointer to DC bus containting actual voltage and current
                                ///< measurement of (external) load output terminal

    float current_max;          ///< maximum allowed current
    int overcurrent_timestamp;  ///< time at which an overcurrent event occured

    float junction_temperature; ///< calculated using thermal model based on current and ambient
                                ///< temperature measurement (unit: °C)

    bool enabled_target;        ///< target setting defined via communication port (overruled if
                                ///< battery is empty)
    bool usb_enabled_target;    ///< same for USB output
};

#endif /* LOAD_H */
