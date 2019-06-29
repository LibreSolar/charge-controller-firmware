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

#ifndef THINGSET_SERIAL_H
#define THINGSET_SERIAL_H

/** @file
 *
 * @brief
 * ThingSet protocol based communication via UART or USB serial port
 */

#include "mbed.h"
#include "thingset_device.h"

class ThingSetStream: public ThingSetDevice
{
    public:
        ThingSetStream(Stream& s, const unsigned int c): channel(c), stream(&s) {};

        virtual void process_asap();
        virtual void process_1s();

    protected:                
        virtual void process_input(); // this is called from the ISR typically
        const unsigned int channel;

    private:
        Stream* stream;

        static uint8_t buf_resp[1000];           // only one response buffer needed for all objects
        uint8_t buf_req[500];
        size_t req_pos = 0;
        bool command_flag = false;
};

template<typename T> class ThingSetSerial: public ThingSetStream
{
    public:
        ThingSetSerial(T& s, const unsigned int c): ThingSetStream(s,c), ser(&s) {}

        virtual void enable() { {
            Callback<void()>  cb([this]() -> void { this->process_input();});
            // ser->attach(cb);
        }
}
        
    private:
        T* ser;
};
#endif /* THINGSET_SERIAL_H */
