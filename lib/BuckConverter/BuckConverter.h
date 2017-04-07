/* mbed library for MPPT buck converter
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

class BuckConverter
{
public:

    /** Create a half bridge object
     */
    BuckConverter(int freq_kHz);

    void set_max_voltage(int voltage_mV);
    void set_max_current(int current_mA);
    void set_min_current(int current_mA);
    time_t last_time_CV ();
    void last_time_CV_reset();

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
     *
     *  @param pwm_duty Duty cycle between 0.0 and 1.0
     */
    void start(float pwm_duty);

    /** Stop the PWM generation
     */
    void stop();

    /** Updates duty cycle to match voltage levels based on measured values
     *
     *  @param input_voltage_mV Measured input voltage (mV)
     *  @param output_voltage_mV Measured output voltage (mA)
     *  @param output_current_mA Measured output current (mA)
     */
    void update(int input_voltage_mV, int output_voltage_mV, int output_current_mA);

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

    /** Initiatializes the registers to generate the PWM signal
     */
    void init_registers();

    int _pwm_resolution;
    float _min_duty;
    float _max_duty;
    int _pwm_delta;

    bool _enabled;

    int _max_voltage_mV;
    int _max_current_mA;
    int _min_current_mA;
    int _output_power_prev_mW;
    time_t _time_voltage_limit_reached;
};
