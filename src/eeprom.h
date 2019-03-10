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

#ifndef EEPROM_H
#define EEPROM_H

/** @file
 *
 * @brief Handling of internal or external EEPROM to store device configuration
 */

#include "mbed.h"

/** Write data to EEPROM address
 *
 * @returns 0 for success
 */
int eeprom_write(unsigned int addr, uint8_t* data, int len);

/** Read data from EEPROM address
 *
 * @returns 0 for success
 */
int eeprom_read(unsigned int addr, uint8_t* ret, int len);

/** Store current charge controller data to EEPROM
 */
void eeprom_store_data();

/** Restore charge controller data from EEPROM and write to variables in RAM
 */
void eeprom_restore_data();

/** Stores data to EEPROM every 6 hours (can be called regularly)
 */
void eeprom_update();

#endif /* EEPROM_H */
