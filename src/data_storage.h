/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_STORAGE_H_
#define DATA_STORAGE_H_

#define DATA_UPDATE_INTERVAL  (6*60*60)       // update every 6 hours

/**
 * @file
 *
 * @brief Handling of internal or external EEPROM to store device configuration
 */

/**
 * Store current charge controller data to EEPROM
 */
void data_storage_write();

/**
 * Restore charge controller data from EEPROM and write to variables in RAM
 */
void data_storage_read();

/**
 * Stores data to EEPROM every 6 hours (can be called regularly)
 */
void data_storage_update();

#endif /* DATA_STORAGE_H_ */
