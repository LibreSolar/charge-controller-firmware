/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UNIT_TEST

#if CONFIG_EXT_THINGSET_CAN

#include "ext.h"

#include <zephyr.h>
#include <device.h>
#include <drivers/gpio.h>
#include <drivers/can.h>
#include <stm32f0xx_ll_system.h>        // needed for debug printing of register contents

#include "board.h"
#include "thingset.h"
#include "data_nodes.h"
#include "can_msg_queue.h"

#ifndef CONFIG_CAN
#error The hardware does not support CAN, please disable CONFIG_EXT_THINGSET_CAN
#endif

#ifndef CAN_SPEED
#define CAN_SPEED 250000    // 250 kHz
#endif

extern ThingSet ts;

class ThingSetCAN: public ExtInterface
{
public:
    ThingSetCAN(uint8_t can_node_id);

    void process_asap();
    void process_1s();

    void enable();

private:
    /**
     * Enqueues all data nodes of CAN publication channel
     *
     * @returns number of data nodes added to queue
     */
    int pub();

    /**
     * Try to send out all data in TX queue
     */
    void process_outbox();

#if defined(CAN_RECEIVE) && defined(__MBED__)
    void process_inbox();
    void process_input();
    void send_object_name(int data_node_id, uint8_t can_dest_id);
    CanMsgQueue rx_queue;
#endif

    CanMsgQueue tx_queue;
    uint8_t node_id;

    struct device *can_dev;
    struct device *can_en_dev;
};

#ifndef CAN_NODE_ID
#define CAN_NODE_ID 10
#endif

ThingSetCAN ts_can(CAN_NODE_ID);

//----------------------------------------------------------------------------
// preliminary simple CAN functions to send data to the bus for logging
// Data format based on CBOR specification (except for first byte, which uses
// only 6 bit to specify type and transport protocol)
//
// Protocol details:
// https://libre.solar/thingset/

ThingSetCAN::ThingSetCAN(uint8_t can_node_id):
    node_id(can_node_id)
{
    can_en_dev = device_get_binding(DT_OUTPUTS_CAN_EN_GPIOS_CONTROLLER);
    gpio_pin_configure(can_en_dev, DT_OUTPUTS_CAN_EN_GPIOS_PIN,
        DT_OUTPUTS_CAN_EN_GPIOS_FLAGS | GPIO_OUTPUT_INACTIVE);

    can_dev = device_get_binding("CAN_1");
}

void ThingSetCAN::enable()
{
    gpio_pin_set(can_en_dev, DT_OUTPUTS_CAN_EN_GPIOS_PIN, 1);
}

void ThingSetCAN::process_1s()
{
    pub();
    process_asap();
}

int ThingSetCAN::pub()
{
    int retval = 0;
    unsigned int can_id;
    uint8_t can_data[8];

    if (pub_can_enable) {
        int data_len = 0;
        int start_pos = 0;
        while ((data_len = ts.bin_pub_can(start_pos, PUB_CAN, node_id, can_id, can_data)) != -1) {

            struct zcan_frame frame = {
                .id_type = CAN_EXTENDED_IDENTIFIER,
                .rtr = CAN_DATAFRAME
            };

            frame.ext_id = can_id;
            memcpy(frame.data, can_data, 8);

            if (data_len >= 0) {
                frame.dlc = data_len;
                tx_queue.enqueue(frame);
            }
            retval++;
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

void can_pub_isr(uint32_t err_flags, void *arg)
{
	// Do nothing. Publication messages are fire and forget.
}

void ThingSetCAN::process_outbox()
{
    int max_attempts = 15;
    while (!tx_queue.empty() && max_attempts > 0) {
        CanFrame msg;
        tx_queue.first(msg);
        if (can_send(can_dev, &msg, K_MSEC(10), can_pub_isr, NULL) == CAN_TX_OK) {
            tx_queue.dequeue();
        }
        else {
            //printk("Sending CAN message failed");
        }
        max_attempts--;
    }
}

/**
 * CAN receive currently only working with mbed
 */
#if defined(CAN_RECEIVE) && defined(__MBED__)

// TODO: Move encoding to ThingSet class
void ThingSetCAN::send_object_name(int data_node_id, uint8_t can_dest_id)
{
    uint8_t msg_priority = 7;   // low priority service message
    uint8_t function_id = 0x84;
    CanFrame msg;
    msg.format = CANExtended;
    msg.type = CANData;
    msg.id = msg_priority << 26 | function_id << 16 |(can_dest_id << 8)| node_id;      // TODO: add destination node ID

    const DataNode *dop = ts.get_data_nodeect(data_node_id);

    if (dop != NULL) {
        if (dop->access & TS_ACCESS_READ) {
            msg.data[2] = TS_T_STRING;
            int len = strlen(dop->name);
            for (int i = 0; i < len && i < (8-3); i++) {
                msg.data[i+3] = *(dop->name + i);
            }
            msg.len = ((len < 5) ? 3 + len : 8);
            // serial.printf("TS Send Object Name: %s (id = %d)\n", dataObjects[arr_id].name, data_node_id);
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
        CanFrame msg;
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
            int data_node_id;

            switch (function_id) {
                case TS_OUTPUT:
                {
                    if (msg.len >= 2) {
                        data_node_id = msg.data[1] + (msg.data[2] << 8);
                        const DataNode *dop = ts.get_data_node(data_node_id);
                        pub_object(*dop);
                    }
                    break;
                }
                case TS_INPUT:
                {
                    if (msg.len >= 8)
                    {
                        data_node_id = msg.data[1] + (msg.data[2] << 8);
                        // int value = msg.data[6] + (msg.data[7] << 8);
                        const DataNode *dop = ts.get_data_node(data_node_id);

                        if (dop != NULL)
                        {
                            if (dop->access & TS_ACCESS_WRITE)
                            {
                                // TODO: write data
                                // serial.printf("ThingSet Write: %d to %s (id = %d)\n", value, dataObjects[i].name, data_node_id);
                            }
                            else
                            {
                                // serial.printf("No write allowed to data object %s (id = %d)\n", dataObjects[i].name, data_node_id);
                            }
                        }
                    }
                    break;
                }
                case TS_NAME:
                    data_node_id = msg.data[1] + (msg.data[2] << 8);
                    send_object_name(data_node_id, can_dest_id);
                    // serial.printf("Get Data Object Name: %d\n", data_node_id);
                    break;
            }
        }
        max_attempts--;
    }
}

void ThingSetCAN::process_input()
{
    CanFrame msg;
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

#endif /* CONFIG_EXT_THINGSET_CAN */

#endif /* UNIT_TEST */
