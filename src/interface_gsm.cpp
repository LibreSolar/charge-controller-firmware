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

#include "config.h"

#ifdef GSM_ENABLED     // otherwise don't compile code to reduce firmware size

#include "pcb.h"
#include "interface.h"
#include "thingset.h"
#include "gprs.h"
#include "MQTTPacket.h"
#include "hardware.h"
#include "eeprom.h"
#include "leds.h"
#include <inttypes.h>

GPRS  SIM800(PIN_UEXT_TX, PIN_UEXT_RX, 9600, (char *)GSM_NUMBER);

DigitalOut gsm_en(PIN_UEXT_MOSI);

int mqtt_connect();
int mqtt_send_pub_packet();
int mqtt_send_sub_packet();
int mqtt_disconnect();
int mqtt_state_machine();

extern uint32_t deviceID;

extern ThingSet ts;
extern const int PUB_CHANNEL_MQTT;

// for publish messages
const int qos = 1;
const int dup_flag = 0;
const int retain_flag = 0;
int packet_id = 0;

// for subscribe messages
int qos_sub = 0;            // is it different to qos of publish messages?
int packet_id_sub = 1;      // can we use above packet_id also for publish messages?

int last_call = 0;                  // Initialise the sending interval timer
const int sleep_time = 300 ;        // GSM Sleep period

enum gsm_states {
    GSM_STATE_INIT,                 // initial state
    GSM_STATE_SIM_READY,            // SIM card recognized and initialized
    GSM_STATE_SSL_READY,            // SSL certificate imported and state set
    GSM_STATE_NETWORK_AVAILABLE,    // successfully registered in GSM network
    GSM_STATE_IP_CONNECTED,         // got IP address
    GSM_STATE_TCP_MQTT,             // TCP connection has been established, MQTT ongoing
    GSM_STATE_SLEEP,                // sleeping and waiting for wake-up
};

enum mqtt_states {
    MQTT_STATE_READY,               // Initial state
    MQTT_CONNECTED,                 // MQTT connection established
    MQTT_PUBLISHED,                 // Publish the thingset message to the server
    MQTT_SUBSCRIBED,                // MQTT subscribed and waiting for incoming messages
    MQTT_THINGSET_PROCESSED,        // Incoming messages have been processed
};

void gsm_init(void)
{
    // only enable GSM module, rest is done in state machine
    gsm_en = 1;
}

// State machine for GSM module
void gsm_process(void)
{
    static gsm_states state = GSM_STATE_INIT;

    switch (state)
    {
        case GSM_STATE_INIT:
        {
            printf("\n IN GSM INIT");
            if (SIM800.init() == -1)
            return;                             // try again next time
            /* NEED TO SET PIN?
            if (SIM800.check_pin() == false) {
                // pin needed --> set it
                if (SIM800.set_pin(SIM_PIN) == false)
                    return;
            }
            */
            state = GSM_STATE_SIM_READY;
            break;
        }

        case GSM_STATE_SIM_READY:
        {
            if (SIM800.activateSSL() == -1)
            return;
            if (SIM800.enableSSL() == -1)       // import certificate
            return;
            state = GSM_STATE_SSL_READY;
            break;
        }

        case GSM_STATE_SSL_READY:
        {
            if (SIM800.network_availablity() == -1)
                return;                      // try again next time
            if (SIM800.checkSignalStrength() <= 0)
                return;                         // try again next time
            state = GSM_STATE_NETWORK_AVAILABLE;
            break;
        }

        case GSM_STATE_NETWORK_AVAILABLE:
        {
            if (SIM800.attach1() == -1)
                return;                         // try again next time*/
            /* APN SETTINGS BLOCKING TCP CONNECTION
            if (SIM800.networkInit(APN, USERNAME, PASSWORD) == -1)
                return; // try again next time
                */
            if (SIM800.getIP() == -1)
                return;                         // try again next time
            state = GSM_STATE_IP_CONNECTED;     // if everything was successful
            break;
        }

        case GSM_STATE_IP_CONNECTED:
        {
            if (SIM800.connectTCP((char *)MQTT_HOST, (char *)MQTT_PORT) == -1)
                return;
            state = GSM_STATE_TCP_MQTT;
            break;
        }

        case GSM_STATE_TCP_MQTT:
        {
            if (mqtt_state_machine() == -1)
                return;
            SIM800.closeTCP();
            SIM800.GSM_sleep();
            last_call = time(NULL);
            state = GSM_STATE_SLEEP;
            break;
        }

        case GSM_STATE_SLEEP:
        {
            // do something
            break;
        }
    }
}

// State machine for MQTT
int mqtt_state_machine()
{
    static mqtt_states state = MQTT_STATE_READY;

    switch (state) {
    case MQTT_STATE_READY:
        if (mqtt_connect())
            state = MQTT_CONNECTED;
        break;

    case MQTT_CONNECTED:
        if (mqtt_send_pub_packet())
            state = MQTT_PUBLISHED;
        break;

    case MQTT_PUBLISHED:
        if (mqtt_send_sub_packet())
            state = MQTT_SUBSCRIBED;
        break;

    case MQTT_SUBSCRIBED:
        // Some code to read packets and pass to thingset
        state = MQTT_THINGSET_PROCESSED;
        break;

    case MQTT_THINGSET_PROCESSED:
        //Disconnect from MQTT
        break;
    }
    return 0;
}

int mqtt_connect()
{
    char id_buf[20];
    snprintf(id_buf, sizeof(id_buf), "%" PRIu32, deviceID);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID.cstring = id_buf;
    data.keepAliveInterval = 60;
    data.cleansession = 0;
    data.username.cstring = (char *)MQTT_USER;
    data.password.cstring = (char *)MQTT_PASS;
    data.MQTTVersion = 4;

    unsigned char buf[100];
    int len = MQTTSerialize_connect(buf, sizeof(buf), &data);

    return (SIM800.sendTCPData(buf, len) != -1);
}

int mqtt_send_pub_packet()
{
    char topic_buf[50];
    MQTTString topic = MQTTString_initializer;
    snprintf(topic_buf, sizeof(topic_buf), "%s/%" PRIu32, MQTT_PUBLISH_TOPIC, deviceID);
    topic.cstring = topic_buf;

    uint8_t pub_data[100];
    int len_cbor = ts.pub_msg_cbor(pub_data, sizeof(pub_data), PUB_CHANNEL_MQTT);

    unsigned char mqtt_packet[200];
    int len_mqtt = MQTTSerialize_publish(mqtt_packet, sizeof(mqtt_packet), dup_flag, qos, retain_flag, packet_id, topic, pub_data+1, len_cbor-1);
    packet_id++;

    return (SIM800.sendTCPData(mqtt_packet, len_mqtt) != -1);
}

int mqtt_send_sub_packet()
{
    char topic_buf[50];
    MQTTString topic = MQTTString_initializer;
    snprintf(topic_buf, sizeof(topic_buf), "%s/%" PRIu32, MQTT_SUBSCRIBE_TOPIC, deviceID);
    topic.cstring = topic_buf;

    unsigned char buf[200];
    int len = MQTTSerialize_subscribe(buf, sizeof(buf), 0, packet_id_sub, 1, &topic, &qos_sub);
    packet_id++;

    return (SIM800.sendTCPData(buf, len) != -1);
}

int mqtt_disconnect()
{
    unsigned char buf[50];
    int len = MQTTSerialize_disconnect(buf, sizeof(buf));

    return (SIM800.sendTCPData(buf, len) != -1);
}

#endif /* GSM_ENABLED */
