/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#ifdef __MBED__

#include "mbed.h"

#include "config.h"

#ifdef CAN_ENABLED
#include "pcb.h"

#ifndef HAS_CAN
#error The hardware does not support CAN, please undefine CAN_ENABLED
#endif

#include "can_msg_queue.h"
//#include "can_iso_tp.h"

#include "thingset.h"

#ifndef CAN_SPEED
#define CAN_SPEED 250000    // 250 kHz
#endif

#include "data_objects.h"
#include "can_msg_queue.h"
#include "ext.h"

class ThingSetCAN: public ExtInterface
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


extern const unsigned int PUB_CHANNEL_CAN;
#ifndef CAN_NODE_ID
    #define CAN_NODE_ID 10
#endif
ThingSetCAN ts_can(CAN_NODE_ID, PUB_CHANNEL_CAN);


class ThingSetCAN_Device
{
    CAN can;
    DigitalOut can_disable;

    ThingSetCAN_Device(PinName rd, PinName td, int hz);
};


//----------------------------------------------------------------------------
// preliminary simple CAN functions to send data to the bus for logging
// Data format based on CBOR specification (except for first byte, which uses
// only 6 bit to specify type and transport protocol)
//
// Protocol details:
// https://github.com/LibreSolar/ThingSet/blob/master/can.md

extern ThingSet ts;

ThingSetCAN::ThingSetCAN(uint8_t can_node_id, const unsigned int c):
    node_id(can_node_id),
    channel(c),
    can(PIN_CAN_RX, PIN_CAN_TX, CAN_SPEED),
    can_disable(PIN_CAN_STB)
{
    can_disable = 1; // we disable the transceiver
    can.mode(CAN::Normal);
    // TXFP: Transmit FIFO priority driven by request order (chronologically)
    // NART: No automatic retransmission
    CAN1->MCR |= CAN_MCR_TXFP | CAN_MCR_NART;
}

void ThingSetCAN::enable()
{
    can_disable = 0; // we enable the transceiver
    #if defined(CAN_RECEIVE)
    can.attach([this](){ this->process_input(); } );
    #endif
}

void ThingSetCAN::process_1s()
{
    pub();
    process_asap();
}

bool ThingSetCAN::pub_object(const data_object_t& data_obj)
{
    CANMessage msg;
    msg.format = CANExtended;
    msg.type = CANData;

    int encode_len = ts.encode_msg_can(data_obj, node_id, msg.id, msg.data);

    if (encode_len >= 0)
    {
        msg.len = encode_len;
        tx_queue.enqueue(msg);
    }
    return (encode_len >= 0);
}


/**
 * returns number of can objects added to queue
 */
int ThingSetCAN::pub()
{
    int retval = 0;
    ts_pub_channel_t* can_chan = ts.get_pub_channel(channel);

    if (can_chan != NULL)
    {
        for (unsigned int element = 0; element < can_chan->num; element++)
        {
            const data_object_t *data_obj = ts.get_data_object(can_chan->object_ids[element]);
            if (data_obj != NULL && data_obj->access & TS_ACCESS_READ)
            {
                if (pub_object(*data_obj))
                {
                    retval++;
                }
            }
        }
    }
    return retval;
}

void ThingSetCAN::process_asap()
{
    process_outbox();
    #if defined(CAN_RECEIVE)
    process_inbox();
    #endif
}

void ThingSetCAN::process_outbox()
{
    int max_attempts = 15;
    while (!tx_queue.empty() && max_attempts > 0) {
        CANMessage msg;
        tx_queue.first(msg);
        if(can.write(msg)) {
            tx_queue.dequeue();
        }
        else {
            //serial.printf("Sending CAN message failed, err: %u, MCR: %u, MSR: %u, TSR: %u, id: %u\n", can.tderror(), (uint32_t)CAN1->MCR, (uint32_t)CAN1->MSR, (uint32_t)CAN1->TSR, msg.id);
        }
        max_attempts--;
    }
}

#if defined(CAN_RECEIVE)
// TODO: Move encoding to ThingSet class
void ThingSetCAN::send_object_name(int data_obj_id, uint8_t can_dest_id)
{
    uint8_t msg_priority = 7;   // low priority service message
    uint8_t function_id = 0x84;
    CANMessage msg;
    msg.format = CANExtended;
    msg.type = CANData;
    msg.id = msg_priority << 26 | function_id << 16 |(can_dest_id << 8)| node_id;      // TODO: add destination node ID

    const data_object_t *dop = ts.get_data_object(data_obj_id);

    if (dop != NULL) {
        if (dop->access & TS_ACCESS_READ) {
            msg.data[2] = TS_T_STRING;
            int len = strlen(dop->name);
            for (int i = 0; i < len && i < (8-3); i++) {
                msg.data[i+3] = *(dop->name + i);
            }
            msg.len = ((len < 5) ? 3 + len : 8);
            // serial.printf("TS Send Object Name: %s (id = %d)\n", dataObjects[arr_id].name, data_obj_id);
        }
    }
    else {
        // send error message
        // data[0] : ISO-TP header
        msg.data[1] = 1;    // TODO: Define error code numbers
        msg.len = 2;
    }

    tx_queue.enqueue(msg);
}

void ThingSetCAN::process_inbox()
{
    int max_attempts = 15;
    while (!rx_queue.empty() && max_attempts >0) {
        CANMessage msg;
        rx_queue.dequeue(msg);

        if (!(msg.id & (0x1U<<25))) {
            // serial.printf("CAN ID bit 25 = 1 --> ignored\n");
            continue;  // might be SAE J1939 or NMEA 2000 message --> ignore
        }

        if (msg.id & (0x1U<<24)) {
            // serial.printf("Data object publication frame\n");
            // data object publication frame
        } else {
            // serial.printf("Service frame\n");
            // service frame
            int function_id = (msg.id >> 16) & (int)0xFF;
            uint8_t can_dest_id = msg.id & (int)0xFF;
            int data_obj_id;

            switch (function_id) {
                case TS_OUTPUT:
                {
                    if (msg.len >= 2) {
                        data_obj_id = msg.data[1] + (msg.data[2] << 8);
                        const data_object_t *dop = ts.get_data_object(data_obj_id);
                        pub_object(*dop);
                    }
                    break;
                }
                case TS_INPUT:
                {
                    if (msg.len >= 8)
                    {
                        data_obj_id = msg.data[1] + (msg.data[2] << 8);
                        // int value = msg.data[6] + (msg.data[7] << 8);
                        const data_object_t *dop = ts.get_data_object(data_obj_id);

                        if (dop != NULL)
                        {
                            if (dop->access & TS_ACCESS_WRITE)
                            {
                                // TODO: write data
                                // serial.printf("ThingSet Write: %d to %s (id = %d)\n", value, dataObjects[i].name, data_obj_id);
                            }
                            else
                            {
                                // serial.printf("No write allowed to data object %s (id = %d)\n", dataObjects[i].name, data_obj_id);
                            }
                        }
                    }
                    break;
                }
                case TS_NAME:
                    data_obj_id = msg.data[1] + (msg.data[2] << 8);
                    send_object_name(data_obj_id, can_dest_id);
                    // serial.printf("Get Data Object Name: %d\n", data_obj_id);
                    break;
            }
        }
        max_attempts--;
    }
}

void ThingSetCAN::process_input()
{
    CANMessage msg;
    while (can.read(msg)) {
        if (!rx_queue.full()) {
            rx_queue.enqueue(msg);
            // serial.printf("Message received. id: %d, data: %d\n", msg.id, msg.data[0]);
        } else {
            // serial.printf("CAN rx queue full\n");
        }
    }
}
#endif /* CAN_RECEIVE */
#endif /* CAN_ENABLED */

#endif /* __MBED__ */
