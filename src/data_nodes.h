/*
 * Copyright (c) 2016 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DATA_NODES_H_
#define DATA_NODES_H_

/** @file
 *
 * @brief Handling of ThingSet data nodes
 */

/*
 * Categories / first layer node IDs
 */
#define ID_ROOT     0x00
#define ID_INFO     0x18        // read-only device information (e.g. manufacturer, device ID)
#define ID_CONF     0x30        // configurable settings
#define ID_INPUT    0x60        // input data (e.g. set-points)
#define ID_OUTPUT   0x70        // output data (e.g. measurement values)
#define ID_REC      0xA0        // recorded data (history-dependent)
#define ID_CAL      0xD0        // calibration
#define ID_EXEC     0xE0        // function call
#define ID_AUTH     0xEA
#define ID_PUB      0xF0        // publication setup
#define ID_SUB      0xF1        // subscription setup
#define ID_LOG      0x100       // access log data

/*
 * Publish/subscribe channels
 */
#define PUB_SER     (1U << 0)   // UART serial
#define PUB_CAN     (1U << 1)   // CAN bus
#define PUB_NVM     (1U << 2)   // data that should be stored in EEPROM

/*
 * Data node versioning for EEPROM
 *
 * Increment the version number each time any data node IDs stored in NVM are changed. Otherwise
 * data might get corrupted.
 */
#define DATA_NODES_VERSION 4

extern bool pub_serial_enable;
extern bool pub_can_enable;

void data_objects_update_conf();
void data_objects_read_eeprom();

void thingset_auth();

#endif /* DATA_OBJECTS_H */
