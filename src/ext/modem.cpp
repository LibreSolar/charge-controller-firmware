/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

#ifndef UNIT_TEST

#include "config.h"

#if CONFIG_EXT_MODEM

#include "ext.h"

#include <string.h>
#include <stdio.h>

#include <zephyr.h>
#include <sys/printk.h>
#include <drivers/uart.h>
#include <drivers/gpio.h>

// Cicada modem library developed by Okra Solar
#include "cicada/commdevices/blockingcommdev.h"
#include "cicada/commdevices/sim800.h"
#include "cicada/mqttcountdown.h"
#include "cicada/platform/zephyr/zephyrserial.h"
#include "cicada/scheduler.h"
#include "cicada/tick.h"

// Eclipse Paho MQTTCLient library, necessary for Cicada
#include <MQTTClient.h>

#include "thingset.h"

/*
 * Zephyr thread settings: Thread must be a cooperative thread (negative priority), as Cicada is
 * not thread-safe and does not support preemption according to comments in other examples.
 */
#define MODEM_THREAD_STACK_SIZE 4048
#define MODEM_THREAD_PRIORITY -1        // lowest cooperative priority

//K_THREAD_STACK_DEFINE(modem_thread_stack_area, MODEM_THREAD_STACK_SIZE);
struct k_thread modem_thread_data;

extern ThingSet ts;
extern const int pub_channel_serial;

using namespace Cicada;

void yieldFunction(void *sched)
{
    k_yield();
}

void modem_task(void *, void *, void *)
{
    static struct device *dev_gsm_en;
    dev_gsm_en = device_get_binding(DT_SWITCH_MOSI_GPIOS_CONTROLLER);
    gpio_pin_configure(dev_gsm_en, DT_SWITCH_MOSI_GPIOS_PIN,
        DT_SWITCH_MOSI_GPIOS_FLAGS | GPIO_OUTPUT_ACTIVE);

    // Set up serial port
    const uint16_t serialBufferSize = 512;
    char serialReadBuffer[serialBufferSize];
    char serialWriteBuffer[serialBufferSize];
    ZephyrSerial serial(serialReadBuffer, serialWriteBuffer, serialBufferSize, serialBufferSize,
                     DT_ALIAS_UART_UEXT_LABEL);

    // Set up modem driver connected to serial port
    const uint16_t commBufferSize = 1024;
    uint8_t commReadBuffer[commBufferSize];
    uint8_t commWriteBuffer[commBufferSize];
    Sim800CommDevice commDev(serial, commReadBuffer, commWriteBuffer, commBufferSize);

    // Set up MQTT client
    BlockingCommDevice bld(commDev, eTickFunction, yieldFunction, NULL);
    MQTT::Client<BlockingCommDevice, MQTTCountdown> client
        = MQTT::Client<BlockingCommDevice, MQTTCountdown>(bld);

    // Connect modem and IP channel
    commDev.setApn(CONFIG_EXT_MODEM_APN);
    commDev.setHostPort(CONFIG_EXT_MQTT_HOST, CONFIG_EXT_MQTT_PORT);
    commDev.connect();
    while (!commDev.isConnected()) {
        yieldFunction(NULL);
    }

    // Connect MQTT client
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = (char*)"enaccess";
    client.connect(data);

    // Send a message
    MQTT::Message message;
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)"Hello World!";
    message.payloadlen = 13;
    client.publish("enaccess/test", message);

    // Disconnect everything
    client.disconnect();
    commDev.disconnect();
    while (!commDev.isIdle()) {
        yieldFunction(NULL);
    }
}

// start thread after 3 seconds
K_THREAD_DEFINE(modem_thread_id, MODEM_THREAD_STACK_SIZE, modem_task, NULL, NULL, NULL,
    MODEM_THREAD_PRIORITY, 0, 3000);

#endif /* CONFIG_EXT_MODEM */

#endif /* UNIT_TEST */
