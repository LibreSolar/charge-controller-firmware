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

#include "uext.h"
#include <algorithm>

// we use code to self register objects at construction time.
// It relies on the fact that initially this pointer will be initialized
// with NULL ! This is only true for global variables or static members of classes
// so this must remain a static class member
std::vector<UExtInterface*>* UExtInterfaceManager::interfaces;

// run the respective function on all objects in the "devices" list
// use the c++11 lambda expressions here for the for_each loop, keeps things compact

void UExtInterfaceManager::process_asap() {
    for_each(std::begin(*interfaces),std::end(*interfaces), [](UExtInterface* tsif) {
        tsif->process_asap();
    });
}

void UExtInterfaceManager::enable() {
    for_each(std::begin(*interfaces),std::end(*interfaces), [](UExtInterface* tsif) {
        tsif->enable();
    });
}

void UExtInterfaceManager::process_1s() {
    for_each(std::begin(*interfaces),std::end(*interfaces), [](UExtInterface* tsif) {
        tsif->process_1s();
    });
}

void UExtInterfaceManager::check_list() {
    if (UExtInterfaceManager::interfaces == NULL)
    {
        UExtInterfaceManager::interfaces = new std::vector<UExtInterface*>;
    }
}

void UExtInterfaceManager::add_ext(UExtInterface* member) {
    check_list();
    interfaces->push_back(member);
}

UExtInterfaceManager::UExtInterfaceManager() {
    check_list();
}

UExtInterface::UExtInterface() {
    uext.add_ext(this);
}

UExtInterfaceManager uext;
