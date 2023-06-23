/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_OBJECTS_H_
#define DATA_OBJECTS_H_

/** @file
 *
 * @brief Handling of ThingSet data objects
 */

#include <stdint.h>
#include <string.h>

/*
 * Groups / first layer data object IDs
 */
#define ID_ROOT     0x00
#define ID_DEVICE   0x04
#define ID_BATTERY  0x05
#define ID_CHARGER  0x06
#define ID_SOLAR    0x07
#define ID_LOAD     0x08
#define ID_USB      0x09
#define ID_NANOGRID 0x0A

/**
 * Callback function to be called when conf values were changed
 */
void data_objects_update_conf();

/**
 * Initializes and reads data objects from EEPROM
 */
void data_objects_init();

#endif /* DATA_OBJECTS_H */
