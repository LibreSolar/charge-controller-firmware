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

#ifndef DATA_OBJECTS_H
#define DATA_OBJECTS_H

/** @file
 *
 * @brief Handling of ThingSet data objects
 */

#include <stdbool.h>
#include <stdint.h>

void data_objects_update_conf();
void data_objects_read_eeprom();

#endif /* DATA_OBJECTS_H */
