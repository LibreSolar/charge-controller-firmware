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
#ifndef _THINGSET_CAN_H_
#define _THINGSET_CAN_H_

#include "data_objects.h"

class ThingSetCAN
{
    public:
        bool pub_object(const data_object_t& data_obj);
        void process_asap();
        void process_1s();

        void init_hw();
        int  pub();

    private:
        void process_outbox();
};

extern ThingSetCAN ts_can;
#endif