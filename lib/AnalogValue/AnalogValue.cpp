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

#include "AnalogValue.h"

float AnalogValue::_vcc = 3.3;

AnalogValue::AnalogValue(PinName pin, float multiplier, const int num_readings, float filter_constant) :
    _adc(pin), _num_readings(num_readings)
{
    _multiplier = multiplier;
    _offset = 0;
    update();   // initialize value without filter
    _filter_constant = filter_constant;
}

void AnalogValue::update()
{
    float tmp = adc_read_avg(_adc, _num_readings) * _vcc * _multiplier + _offset;
    _value = (1.0 - _filter_constant) * tmp + _filter_constant * _value;
}

float AnalogValue::adc_read_avg(AnalogIn& input, int num_readings)
{
    // averaging: use read_u16 for faster calculation
    unsigned int sum_adc_readings = 0;
    for (int i = 0; i < num_readings; i++) {
        sum_adc_readings += input.read_u16(); // normalized to 0xFFFF
    }
    return (float) (sum_adc_readings / num_readings) / (float)0xFFFF;
}

float AnalogValue::read()
{
    return _value;
}

void AnalogValue::update_reference_voltage(AnalogIn& adc_ref, float ref_voltage, int num_readings)
{
    _vcc = ref_voltage / adc_read_avg(adc_ref, num_readings);
}

void AnalogValue::calibrate_offset(float expected_value)
{
    float tmp = adc_read_avg(_adc, _num_readings) * _vcc * _multiplier;
    _offset = expected_value - tmp;
}
