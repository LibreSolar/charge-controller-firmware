/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if CONFIG_THINGSET_CAN

#include <device.h>
#include <drivers/can.h>
#include <drivers/gpio.h>
#include <task_wdt/task_wdt.h>
#include <zephyr.h>

#ifdef CONFIG_ISOTP
#include <canbus/isotp.h>
#endif

#include <logging/log.h>
LOG_MODULE_REGISTER(ext_can, CONFIG_CAN_LOG_LEVEL);

#include "bat_charger.h"
#include "data_objects.h"
#include "hardware.h"
#include "helper.h"
#include "thingset.h"

#if DT_NODE_EXISTS(DT_CHILD(DT_PATH(outputs), can_en))
#define CAN_EN_GPIO DT_CHILD(DT_PATH(outputs), can_en)
#endif

extern ThingSet ts;
extern uint16_t can_node_addr;
extern Charger charger;

static const struct device *can_dev = DEVICE_DT_GET(DT_NODELABEL(can1));

#ifdef CONFIG_ISOTP

#define RX_THREAD_STACK_SIZE 1024
#define RX_THREAD_PRIORITY   2

const struct isotp_fc_opts fc_opts = {
    .bs = 8,    // block size
    .stmin = 1, // minimum separation time = 100 ms
};

struct isotp_msg_id rx_addr = {
    .id_type = CAN_EXTENDED_IDENTIFIER,
    .use_ext_addr = 0,   // Normal ISO-TP addressing (using only CAN ID)
    .use_fixed_addr = 1, // enable SAE J1939 compatible addressing
};

struct isotp_msg_id tx_addr = {
    .id_type = CAN_EXTENDED_IDENTIFIER,
    .use_ext_addr = 0,   // Normal ISO-TP addressing (using only CAN ID)
    .use_fixed_addr = 1, // enable SAE J1939 compatible addressing
};

static struct isotp_recv_ctx recv_ctx;
static struct isotp_send_ctx send_ctx;

void send_complete_cb(int error_nr, void *arg)
{
    ARG_UNUSED(arg);
    LOG_DBG("TX complete callback, err: %d", error_nr);
}

void can_isotp_thread()
{
    int ret, rem_len, resp_len;
    unsigned int req_len;
    struct net_buf *buf;
    static uint8_t rx_buffer[600]; // large enough to receive a 512k flash page for DFU
    static uint8_t tx_buffer[1000];

    if (!device_is_ready(can_dev)) {
        return;
    }

    while (1) {
        /* re-assign address in every loop as it may have been changed via ThingSet */
        rx_addr.ext_id =
            TS_CAN_BASE_REQRESP | TS_CAN_PRIO_REQRESP | TS_CAN_TARGET_SET(can_node_addr);
        tx_addr.ext_id =
            TS_CAN_BASE_REQRESP | TS_CAN_PRIO_REQRESP | TS_CAN_SOURCE_SET(can_node_addr);

        ret = isotp_bind(&recv_ctx, can_dev, &rx_addr, &tx_addr, &fc_opts, K_FOREVER);
        if (ret != ISOTP_N_OK) {
            LOG_DBG("Failed to bind to rx ID %d [%d]", rx_addr.ext_id, ret);
            return;
        }

        req_len = 0;
        do {
            rem_len = isotp_recv_net(&recv_ctx, &buf, K_FOREVER);
            if (rem_len < 0) {
                LOG_DBG("Receiving error [%d]", rem_len);
                break;
            }
            if (req_len + buf->len <= sizeof(rx_buffer)) {
                memcpy(&rx_buffer[req_len], buf->data, buf->len);
            }
            req_len += buf->len;
            net_buf_unref(buf);
        } while (rem_len);

        // we need to unbind the receive ctx so that control frames are received in the send ctx
        isotp_unbind(&recv_ctx);

        if (req_len > sizeof(rx_buffer)) {
            LOG_DBG("RX buffer too small");
            tx_buffer[0] = TS_STATUS_REQUEST_TOO_LARGE;
            resp_len = 1;
        }
        else if (req_len > 0 && rem_len == 0) {
            LOG_INF("Got %d bytes via ISO-TP. Processing ThingSet message.", req_len);
            resp_len = ts.process(rx_buffer, req_len, tx_buffer, sizeof(tx_buffer));
            LOG_DBG("TX buf: %x %x %x %x", tx_buffer[0], tx_buffer[1], tx_buffer[2], tx_buffer[3]);
        }
        else {
            tx_buffer[0] = TS_STATUS_INTERNAL_SERVER_ERR;
            resp_len = 1;
        }

        if (resp_len > 0) {
            ret = isotp_send(&send_ctx, can_dev, tx_buffer, resp_len, &recv_ctx.tx_addr,
                             &recv_ctx.rx_addr, send_complete_cb, NULL);
            if (ret != ISOTP_N_OK) {
                LOG_DBG("Error while sending data to ID %d [%d]", tx_addr.ext_id, ret);
            }
        }
    }
}

K_THREAD_DEFINE(can_isotp, RX_THREAD_STACK_SIZE, can_isotp_thread, NULL, NULL, NULL,
                RX_THREAD_PRIORITY, 0, 1500);

#endif /* CONFIG_ISOTP */

// below defines should go into the ThingSet library
#define TS_CAN_SOURCE_GET(id)  (((uint32_t)id & TS_CAN_SOURCE_MASK) >> TS_CAN_SOURCE_POS)
#define TS_CAN_DATA_ID_GET(id) (((uint32_t)id & TS_CAN_DATA_ID_MASK) >> TS_CAN_DATA_ID_POS)

CAN_MSGQ_DEFINE(sub_msgq, 10);

const struct zcan_filter ctrl_filter = {
    .id = TS_CAN_BASE_CONTROL,
    .rtr = CAN_DATAFRAME,
    .id_type = CAN_EXTENDED_IDENTIFIER,
    .id_mask = TS_CAN_TYPE_MASK,
    .rtr_mask = 1,
};

void can_pub_isr(int error, void *arg)
{
    // Do nothing. Publication messages are fire and forget.
}

void can_pub_send(uint32_t can_id, uint8_t can_data[8], uint8_t data_len)
{
    struct zcan_frame frame = { 0 };
    frame.id_type = CAN_EXTENDED_IDENTIFIER;
    frame.rtr = CAN_DATAFRAME;
    frame.id = can_id;
    memcpy(frame.data, can_data, 8);

    if (data_len >= 0) {
        frame.dlc = data_len;

        if (can_send(can_dev, &frame, K_MSEC(10), can_pub_isr, NULL) != 0) {
            LOG_DBG("Error sending CAN frame");
        }
    }
}

void can_pubsub_thread()
{
    int wdt_channel = task_wdt_add(2000, task_wdt_callback, (void *)k_current_get());

    unsigned int can_id;
    uint8_t can_data[8];
    struct zcan_frame rx_frame;

    const struct device *can_en_dev = device_get_binding(DT_GPIO_LABEL(CAN_EN_GPIO, gpios));
    gpio_pin_configure(can_en_dev, DT_GPIO_PIN(CAN_EN_GPIO, gpios),
                       DT_GPIO_FLAGS(CAN_EN_GPIO, gpios) | GPIO_OUTPUT_ACTIVE);

    if (!device_is_ready(can_dev)) {
        return;
    }

    int filter_id = can_add_rx_filter_msgq(can_dev, &sub_msgq, &ctrl_filter);
    if (filter_id < 0) {
        LOG_ERR("Unable to attach ISR [%d]", filter_id);
        return;
    }

    int64_t next_pub = k_uptime_get();
    uint32_t count = 0;

    while (1) {

        task_wdt_feed(wdt_channel);

        if (pub_can_enable) {
            int data_len = 0;
            int start_pos = 0;
            while (count % 10 == 0) {
                // normal objects: only every second
                data_len = ts.bin_pub_can(start_pos, SUBSET_CAN, can_node_addr, can_id, can_data);
                if (data_len < 0) {
                    // finished with all IDs
                    break;
                }
                else {
                    can_pub_send(can_id, can_data, data_len);
                }
            }

            data_len = 0;
            start_pos = 0;
            while (true) {
                // control objects: every 100 ms
                data_len = ts.bin_pub_can(start_pos, SUBSET_CTRL, can_node_addr, can_id, can_data);
                if (data_len < 0) {
                    break;
                }
                else {
                    can_pub_send(can_id, can_data, data_len);
                }
            }
        }

        // wait for incoming messages until the next pub message has to be sent out
        while (k_msgq_get(&sub_msgq, &rx_frame, K_TIMEOUT_ABS_MS(next_pub)) != -EAGAIN) {
            // process message
            uint16_t data_id = TS_CAN_DATA_ID_GET(rx_frame.id);
            uint8_t sender_addr = TS_CAN_SOURCE_GET(rx_frame.id);

            // control message received?
            if (data_id > 0x8000 && sender_addr < can_node_addr) {
                uint8_t buf[4 + 8]; // ThingSet bin headers + CAN frame payload
                buf[0] = 0xA1;      // CBOR: map with 1 element
                buf[1] = 0x19;      // CBOR: uint16 follows
                buf[2] = data_id >> 8;
                buf[3] = data_id;
                memcpy(&buf[4], rx_frame.data, 8);

                int status = ts.bin_import(buf, 4 + rx_frame.dlc, TS_WRITE_MASK, SUBSET_CTRL);
                if (status == TS_STATUS_CHANGED) {
                    charger.time_last_ctrl_msg = uptime();
                }
            }
        }

        next_pub += 100; // 100 ms period (currently fixed)
        count++;
    }
}

K_THREAD_DEFINE(can_pubsub, 1024, can_pubsub_thread, NULL, NULL, NULL, 6, 0, 1000);

#endif /* CONFIG_THINGSET_CAN */
