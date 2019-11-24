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

#include "ext.h"
#include <algorithm>

// we use code to self register objects at construction time.
// It relies on the fact that initially this pointer will be initialized
// with NULL ! This is only true for global variables or static members of classes
// so this must remain a static class member
std::vector<ExtInterface*>* ExtManager::interfaces;

// run the respective function on all objects in the "devices" list
// use the c++11 lambda expressions here for the for_each loop, keeps things compact

void ExtManager::process_asap() {
    for_each(std::begin(*interfaces),std::end(*interfaces), [](ExtInterface* tsif) {
        tsif->process_asap();
    });
}

void ExtManager::enable_all() {
    for_each(std::begin(*interfaces),std::end(*interfaces), [](ExtInterface* tsif) {
        tsif->enable();
    });
}

void ExtManager::process_1s() {
    for_each(std::begin(*interfaces),std::end(*interfaces), [](ExtInterface* tsif) {
        tsif->process_1s();
    });
}

void ExtManager::check_list() {
    if (ExtManager::interfaces == NULL)
    {
        ExtManager::interfaces = new std::vector<ExtInterface*>;
    }
}

void ExtManager::add_ext(ExtInterface* member) {
    check_list();
    interfaces->push_back(member);
}

ExtManager::ExtManager() {
    check_list();
}

ExtInterface::ExtInterface() {
    ext_mgr.add_ext(this);
}

ExtManager ext_mgr;
