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

#ifndef THINGSET_CAN_H
#define THINGSET_CAN_H

#include "config.h"

#ifdef CAN_ENABLED
#include "data_objects.h"
#include "can_msg_queue.h"
#include "thingset_interface.h"

class ThingSetCAN: public ThingSetInterface
{
    public:
        ThingSetCAN(uint8_t can_node_id, const unsigned int c);

        void process_asap();
        void process_1s();

        void enable();

    private:
        bool pub_object(const data_object_t& data_obj);
        int  pub();
        void process_outbox();

    #if defined(CAN_RECEIVE)
        void process_inbox();
        void process_input();
        void send_object_name(int data_obj_id, uint8_t can_dest_id);
        CANMsgQueue rx_queue;
    #endif

        CANMsgQueue tx_queue;
        uint8_t node_id;
        const unsigned int channel;
        CAN can;
        DigitalOut can_disable;
};

#endif /* CAN_ENABLED */
#endif
