/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

#ifndef TEST_H_
#define TEST_H_

#include <stdint.h>
#include <string.h>

/*
 * Same as in test_data.h
 */
#define ID_ROOT     0x00
#define ID_INFO     0x18        // read-only device information (e.g. manufacturer, device ID)
#define ID_CONF     0x30        // configurable settings
#define ID_INPUT    0x60        // input data (e.g. set-points)
#define ID_OUTPUT   0x70        // output data (e.g. measurement values)
#define ID_REC      0xA0        // recorded data (history-dependent)
#define ID_CAL      0xD0        // calibration
#define ID_EXEC     0xE0        // function call
#define ID_PUB      0xF0        // publication setup
#define ID_SUB      0xF1        // subscription setup
#define ID_LOG      0x100       // access log data

#define PUB_SER     (1U << 0)   // UART serial
#define PUB_CAN     (1U << 1)   // CAN bus
#define PUB_NVM     (1U << 2)   // data that should be stored in EEPROM

#define TS_REQ_BUFFER_LEN 500
#define TS_RESP_BUFFER_LEN 500

void tests_common();
void tests_text_mode();
void tests_binary_mode();

int hex2bin(char *const hex, uint8_t *bin, size_t bin_size);

#endif /* TEST_H_ */
