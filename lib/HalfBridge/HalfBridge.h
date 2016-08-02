/* mbed library for half bridge driver PWM generation
 * Copyright (c) 2016 Martin Jäger (www.libre.solar)
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

class HalfBridge
{
public:

    /** Create a half bridge class for the specified pins
     *
     *  @param pin_HS PinName of the high-side output
     *  @param pin_LS PinName of the low-side output
     */
    HalfBridge(PinName pin_HS, PinName pin_LS);

    /** Set the PWM frequency in kHz
     *
     *  @param freq Frequency in kHz
     */
    void frequency_kHz(int freq);

    /** Set the deadtime between switching the two FETs on/off
     *
     *  @param deadtime Deadtime in ns
     */
    void deadtime_ns(int deadtime);

    /** Lock the settings of PWM generation to prevent accidental changes
     *  (does not work properly, yet! --> TODO)
     */
    void lock_settings(void);

    /** Start the PWM generation
     */
    void start();

    /** Stop the PWM generation
     */
    void stop();

    /** Set the duty cycle of the PWM signal between 0.0 and 1.0
     *
     *  @param duty Duty cycle between 0.0 and 1.0
     */
    void set_duty_cycle(float duty);

    /** Adjust the duty cycle with minimum step size
     *
     *  @param delta Number of steps (positive or negative)
     */
    void duty_cycle_step(int delta);

    /** Read the currently set duty cycle
     *
     *  @returns
     *    Duty cycle between 0.0 and 1.0
     */
    float get_duty_cycle();

    /** Set limits for the duty cycle to prevent hardware damages
     *
     *  @param min_duty Minimum duty cycle (e.g. 0.5 for limiting input voltage)
     *  @param max_duty Maximum duty cycle (e.g. 0.97 for charge pump)
     */
    void duty_cycle_limits(float min_duty, float max_duty);

private:
    int _pwm_resolution;
    float _min_duty;
    float _max_duty;

};
