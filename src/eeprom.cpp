/*
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "eeprom.h"

#include <stdio.h>

#ifndef UNIT_TEST
#include <zephyr.h>
#include <device.h>
#include <drivers/eeprom.h>
#endif

#include "mcu.h"
#include "board.h"
#include "thingset.h"
#include "helper.h"

// versioning of EEPROM layout (2 bytes)
// change the version number each time the data object array below is changed!
#define EEPROM_VERSION 3

#define EEPROM_HEADER_SIZE 8    // bytes

#define EEPROM_UPDATE_INTERVAL  (6*60*60)       // update every 6 hours

extern ThingSet ts;

// stores object-ids of values to be stored in EEPROM
const uint16_t eeprom_data_objects[] = {
    0x01, // timestamp
    0x18, // DeviceID
    0x08, 0x09, 0x0A, 0x0B, // input / output wh
    0x0C, 0x0D, 0x0E, // num full charge / deep-discharge / usable Ah
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3F, // battery settings
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, // resistances and min/max temperatures
    0x40, 0x41, 0x42, 0x43, 0x46, 0x47, // load settings
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9,    // V, I, T max
    0xA6 // day count
};

#ifndef UNIT_TEST

uint32_t _calc_crc(const uint8_t *buf, size_t len)
{
#if defined(CONFIG_SOC_SERIES_STM32G4X)
    RCC->AHB1ENR |= RCC_AHB1ENR_CRCEN;
#else
    RCC->AHBENR |= RCC_AHBENR_CRCEN;
#endif

    // we keep standard polynomial 0x04C11DB7 (same for STM32L0 and STM32F0)
    // and we don't care for endianness here
    CRC->CR |= CRC_CR_RESET;
    for (size_t i = 0; i < len; i += 4) {
        //printf("CRC buf: %.8x, CRC->DR: %.8x\n", *((uint32_t*)&buf[i]), CRC->DR);
        size_t remaining_bytes = len - i;
        if (remaining_bytes >= 4) {
            CRC->DR = *((uint32_t*)&buf[i]);
        }
        else {
            // ignore bytes >= len if len is not a multiple of 4
            CRC->DR = *((uint32_t*)&buf[i]) & (0xFFFFFFFFU >> (32 - remaining_bytes * 8));
        }
    }

#if defined(CONFIG_SOC_SERIES_STM32G4X)
    RCC->AHB1ENR &= ~(RCC_AHB1ENR_CRCEN);
#else
    RCC->AHBENR &= ~(RCC_AHBENR_CRCEN);
#endif

    return CRC->DR;
}

// EEPROM layout:
// bytes 0-1: Version number
// bytes 2-3: number of data bytes
// bytes 4-7: CRC32
// byte 8: start of data

void eeprom_restore_data()
{
    uint8_t buf_req[300];  // ThingSet request buffer
    int err;

	struct device *eeprom_dev = device_get_binding("EEPROM_0");

    // EEPROM header
    uint8_t buf_header[EEPROM_HEADER_SIZE] = {};    // initialize to avoid compiler warning
    err = eeprom_read(eeprom_dev, 0, buf_header, EEPROM_HEADER_SIZE);
    if (err != 0) {
        printf("EEPROM: read error: %d\n", err);
        return;
    }
    uint16_t version = *((uint16_t*)&buf_header[0]);
    uint16_t len     = *((uint16_t*)&buf_header[2]);
    uint32_t crc     = *((uint32_t*)&buf_header[4]);

    //printf("EEPROM header restore: ver %d, len %d, CRC %.8x\n", version, len, (unsigned int)crc);
    //printf("Header: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
    //    buf_header[0], buf_header[1], buf_header[2], buf_header[3],
    //    buf_header[4], buf_header[5], buf_header[6], buf_header[7]);

    if (version == EEPROM_VERSION && len <= sizeof(buf_req)) {

        err = eeprom_read(eeprom_dev, EEPROM_HEADER_SIZE, buf_req, len);

        //printf("Data (len=%d): ", len);
        //for (int i = 0; i < len; i++) printf("%.2x ", buf_req[i]);

        if (_calc_crc(buf_req, len) == crc) {
            int status = ts.init_cbor(buf_req, sizeof(buf_req));     // first byte is ignored
            printf("EEPROM: Data objects read and updated, ThingSet result: %d\n", status);
        }
        else {
            printf("EEPROM: CRC of data not correct, expected 0x%x (data_len = %d)\n",
                (unsigned int)crc, len);
        }
    }
    else {
        printf("EEPROM: Empty or data layout version changed\n");
    }
}

void eeprom_store_data()
{
    uint8_t buf[300];

	struct device *eeprom_dev = device_get_binding("EEPROM_0");

    int len = ts.pub_msg_cbor(buf + EEPROM_HEADER_SIZE, sizeof(buf) - EEPROM_HEADER_SIZE,
        eeprom_data_objects, sizeof(eeprom_data_objects)/sizeof(uint16_t));
    uint32_t crc = _calc_crc(buf + EEPROM_HEADER_SIZE, len);

    // store EEPROM_VERSION, number of bytes and CRC
    *((uint16_t*)&buf[0]) = (uint16_t)EEPROM_VERSION;
    *((uint16_t*)&buf[2]) = (uint16_t)(len);   // length of data
    *((uint32_t*)&buf[4]) = crc;

    //printf("Data (len=%d): ", len);
    //for (int i = 0; i < len; i++) printf("%.2x ", buf[i + EEPROM_HEADER_SIZE]);

    //printf("Header: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
    //    buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

    if (len == 0) {
        printf("EEPROM: Data could not be stored, ThingSet error: %d\n", len);
    }
    else {
        int err = eeprom_write(eeprom_dev, 0, buf, len + EEPROM_HEADER_SIZE);
        if (err == 0) {
            printf("EEPROM: Data successfully stored.\n");
        }
        else {
            printf("EEPROM: Write error.\n");
        }
    }
}

#else

void eeprom_store_data() {;}
void eeprom_restore_data() {;}

#endif

void eeprom_update()
{
    if (uptime() % EEPROM_UPDATE_INTERVAL == 0 && uptime() > 0) {
        eeprom_store_data();
    }
}
