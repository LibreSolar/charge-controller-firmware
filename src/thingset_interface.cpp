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

#ifndef UNIT_TEST

#include "config.h"
#include "mbed.h"
#include "thingset.h"
#include "thingset_interface.h"

/*
 * Construct all global ThingSetInterfaces here.
 * All of these are added to the list of devices
 * for later processing in the normal operation
 */

#ifdef CAN_ENABLED
    #include "thingset_can.h"
    extern const unsigned int PUB_CHANNEL_CAN;
    #ifndef CAN_NODE_ID
        #define CAN_NODE_ID 10
    #endif
    ThingSetCAN ts_can(CAN_NODE_ID, PUB_CHANNEL_CAN);
#endif

#ifdef UART_SERIAL_ENABLED
    #include "thingset_serial.h"
    extern const int pub_channel_serial;
    extern Serial serial;

    ThingSetSerial ts_uart(serial, pub_channel_serial);
#endif /* UART_SERIAL_ENABLED */


#ifdef USB_SERIAL_ENABLED
    #include "thingset_serial.h"
    #include "USBSerial.h"
    extern const int pub_channel_serial;

    USBSerial ser_usb(0x1f00, 0x2012, 0x0001,  false);    // connection is not blocked when USB is not plugged in

    ThingSetSerial ts_usb(ser_usb, pub_channel_serial);
#endif /* USB_SERIAL_ENABLED */


// we use ifdef here in order to avoid using some dynamically
// allocated list. Only std::vector works with this code if NO devices are enabled
// std::array and also plain C arrays work if at least one array element is there.

std::vector<ThingSetInterface*> ThingSetInterfaceManager::interfaces =
{
#ifdef UART_SERIAL_ENABLED
    &ts_uart,
#endif
#ifdef USB_SERIAL_ENABLED
    &ts_usb,
#endif
#ifdef CAN_ENABLED
    &ts_can,
#endif
};

// run the respective function on all objects in the "devices" list
// use the c++11 lambda expressions here for the for_each loop, keeps things compact

void ThingSetInterfaceManager::process_asap()
{
    for_each(std::begin(interfaces),std::end(interfaces), [](ThingSetInterface* tsif) {
        tsif->process_asap();
    });
}

void ThingSetInterfaceManager::enable()
{
    for_each(std::begin(interfaces),std::end(interfaces), [](ThingSetInterface* tsif) {
        tsif->enable();
    });
}

void ThingSetInterfaceManager::process_1s()
{
    for_each(std::begin(interfaces),std::end(interfaces), [](ThingSetInterface* tsif) {
        tsif->process_1s();
    });
}

ThingSetInterfaceManager ts_interfaces;

#endif /* UNIT_TEST */
