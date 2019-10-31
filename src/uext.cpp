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
#include "uext.h"

/*
 * Construct all global UExtInterfaces here.
 * All of these are added to the list of devices
 * for later processing in the normal operation
 */

#ifdef WIFI_ENABLED
    #include "uext/uext_wifi.h"
    UExtWifi uext_wifi;
#endif

#ifdef OLED_ENABLED
    #include "uext/uext_oled.h"
    UExtOled uext_oled;
#endif 

// we use ifdef here in order to avoid using some dynamically
// allocated list. Only std::vector works with this code if NO devices are enabled
// std::array and also plain C arrays work if at least one array element is there.

std::vector<UExtInterface*> UExtInterfaceManager::interfaces =
{
#ifdef WIFI_ENABLED
    &uext_wifi,
#endif
#ifdef OLED_ENABLED
    &uext_oled,
#endif
};

// run the respective function on all objects in the "devices" list
// use the c++11 lambda expressions here for the for_each loop, keeps things compact

void UExtInterfaceManager::process_asap()
{
    for_each(std::begin(interfaces),std::end(interfaces), [](UExtInterface* tsif) {
        tsif->process_asap();
    });
}

void UExtInterfaceManager::enable()
{
    for_each(std::begin(interfaces),std::end(interfaces), [](UExtInterface* tsif) {
        tsif->enable();
    });
}

void UExtInterfaceManager::process_1s()
{
    for_each(std::begin(interfaces),std::end(interfaces), [](UExtInterface* tsif) {
        tsif->process_1s();
    });
}

UExtInterfaceManager uext;

#endif /* UNIT_TEST */
