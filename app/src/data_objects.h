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

extern bool pub_serial_enable;
extern bool pub_can_enable;

/**
 * Callback function to be called when conf values were changed
 */
void data_objects_update_conf();

/**
 * Initializes and reads data objects from EEPROM
 */
void data_objects_init();

/**
 * Callback to provide authentication mechanism via ThingSet
 */
void thingset_auth();

/**
 * Alphabet used for base32 encoding
 *
 * https://en.wikipedia.org/wiki/Base32#Crockford's_Base32
 */
const char alphabet_crockford[] = "0123456789abcdefghjkmnpqrstvwxyz";

/**
 * Convert numeric device ID to base32 string
 */
void uint64_to_base32(uint64_t in, char *out, size_t size, const char *alphabet);

/**
 * Update control values received via CAN
 */
void update_control();

#endif /* DATA_OBJECTS_H */
