/*
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef EEPROM_H
#define EEPROM_H

#include <inttypes.h>

/** @file
 *
 * @brief Handling of internal or external EEPROM to store device configuration
 */

/** Write data to EEPROM address
 *
 * @returns 0 for success
 */
int eeprom_write(unsigned int addr, const uint8_t* data, int len);

/** Read data from EEPROM address
 *
 * @returns 0 for success
 */
int eeprom_read(unsigned int addr, const uint8_t* ret, int len);

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
