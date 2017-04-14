/* mbed library for ADC readings
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

#include "mbed.h"

class AnalogValue
{
public:

    /** Create an AnalogValue object
     *
     *  @param pin ADC input pin
     *  @param multiplier Factor to calculate desired output (e.g. for voltage dividers, gain, etc.)
     *  @param num_readings Number of ADC readings to be averaged
     *  @param filter_constant Low pass filter constant (0.0 to 1.0, 0.0 for no filtering)
     */
    AnalogValue(PinName pin, float multiplier, const int num_readings = 8, float filter_constant = 0.0);

    /** Updates ADC reading and applies low-pass filter. Should be called with a regular time interval
     */
    void update();

    /** Charger state machine update, should be called exactly once per second
     *
     *  @param adc_ref AnalogIn for the pin with the reference voltage attached
     *  @param ref_voltage Rated reference voltage
     *  @param num_readings Number of ADC readings to be averaged
     */
    static void update_reference_voltage(AnalogIn& adc_ref, float ref_voltage, int num_readings = 8);

    /** Read the last updated (and filtered) ADC result (incl. multiplier)
     *
     *  @returns
     *    Saved ADC result with multiplier applied
     */
    float read();

    /** An operator shorthand for read()
     *
     * The float() operator can be used as a shorthand for read() to simplify common code sequences
     *
     * Example:
     * @code
     * float x = volume.read();
     * float x = volume;
     *
     * if(volume.read() > 0.25) { ... }
     * if(volume > 0.25) { ... }
     * @endcode
     */
    operator float() {
        return read();
    }

private:
    
    /** Read out ADC results and apply averaging and/or low pass filter
     *
     *  @param input AnalogIn for the pin to be read
     *  @param num_readings Number of ADC readings to be averaged
     *
     *  @returns
     *    Averaged and filtered ADC result
     */
    static float adc_read_avg(AnalogIn& input, int num_readings);

    static float _vcc;          // actual VCC of ADC

    AnalogIn _adc;
    float _multiplier;
    float _value;

    const int _num_readings;
    float _filter_constant;
};
