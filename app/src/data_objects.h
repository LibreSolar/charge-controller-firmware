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
#define ID_DEVICE   0x01
#define ID_BATTERY  0x02
#define ID_CHARGER  0x03
#define ID_SOLAR    0x04
#define ID_LOAD     0x05
#define ID_USB      0x06
#define ID_NANOGRID 0x07
#define ID_DFU      0x0F
#define ID_PUB      0x100
#define ID_CTRL     0x8000

/*
 * Subset definitions for statements and publish/subscribe
 */
#define SUBSET_SER  (1U << 0) // UART serial
#define SUBSET_CAN  (1U << 1) // CAN bus
#define SUBSET_NVM  (1U << 2) // data that should be stored in EEPROM
#define SUBSET_CTRL (1U << 3) // control data sent and received via CAN

/*
 * Data object versioning for EEPROM
 *
 * Increment the version number each time any data object IDs stored in NVM are changed. Otherwise
 * data might get corrupted.
 */
#define DATA_OBJECTS_VERSION 05

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
