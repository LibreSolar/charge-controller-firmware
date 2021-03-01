/*
 * Copyright (c) The Libre Solar Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "data_storage.h"

#include <zephyr.h>

#include "mcu.h"
#include "thingset.h"
#include "data_nodes.h"
#include "helper.h"

#include <stdio.h>

#ifndef UNIT_TEST

#include <device.h>
#include <logging/log.h>
LOG_MODULE_REGISTER(nvs, CONFIG_DATA_STORAGE_LOG_LEVEL);

K_MUTEX_DEFINE(data_buf_lock);

// Buffer used by store and restore functions (must be word-aligned for hardware CRC calculation)
static uint8_t buf[512] __aligned(sizeof(uint32_t));

extern ThingSet ts;

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

#endif /* UNIT_TEST */

#ifdef CONFIG_EEPROM

#include <drivers/eeprom.h>

/*
 * EEPROM header bytes:
 * - 0-1: Data nodes version number
 * - 2-3: Number of data bytes
 * - 4-7: CRC32
 *
 * Data starts from byte 8
 */
#define EEPROM_HEADER_SIZE 8

void data_storage_read()
{
    int err;

	const struct device *eeprom_dev = device_get_binding("EEPROM_0");

    uint8_t buf_header[EEPROM_HEADER_SIZE] = {};
    err = eeprom_read(eeprom_dev, 0, buf_header, EEPROM_HEADER_SIZE);
    if (err != 0) {
        LOG_ERR("EEPROM read error %d", err);
        return;
    }
    uint16_t version = *((uint16_t*)&buf_header[0]);
    uint16_t len     = *((uint16_t*)&buf_header[2]);
    uint32_t crc     = *((uint32_t*)&buf_header[4]);

    LOG_DBG("EEPROM header restore: ver %d, len %d, CRC %.8x", version, len, (unsigned int)crc);
    LOG_DBG("Header: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x",
        buf_header[0], buf_header[1], buf_header[2], buf_header[3],
        buf_header[4], buf_header[5], buf_header[6], buf_header[7]);

    if (version == DATA_NODES_VERSION && len <= sizeof(buf)) {
        k_mutex_lock(&data_buf_lock, K_FOREVER);
        err = eeprom_read(eeprom_dev, EEPROM_HEADER_SIZE, buf, len);

        //printf("Data (len=%d): ", len);
        //for (int i = 0; i < len; i++) printf("%.2x ", buf[i]);

        if (_calc_crc(buf, len) == crc) {
            int status = ts.bin_sub(buf, sizeof(buf), TS_WRITE_MASK, PUB_NVM);
            LOG_INF("EEPROM read and data nodes updated, ThingSet result: 0x%x", status);
        }
        else {
            LOG_ERR("EEPROM data CRC invalid, expected 0x%x (data_len = %d)",
                (unsigned int)crc, len);
        }
        k_mutex_unlock(&data_buf_lock);
    }
    else {
        LOG_INF("EEPROM empty or data layout version changed");
    }
}

void data_storage_write()
{
    int err;

	const struct device *eeprom_dev = device_get_binding("EEPROM_0");

    k_mutex_lock(&data_buf_lock, K_FOREVER);

    int len = ts.bin_pub(buf + EEPROM_HEADER_SIZE, sizeof(buf) - EEPROM_HEADER_SIZE, PUB_NVM);
    uint32_t crc = _calc_crc(buf + EEPROM_HEADER_SIZE, len);

    *((uint16_t*)&buf[0]) = (uint16_t)DATA_NODES_VERSION;
    *((uint16_t*)&buf[2]) = (uint16_t)(len);
    *((uint32_t*)&buf[4]) = crc;

    //printf("Data (len=%d): ", len);
    //for (int i = 0; i < len; i++) printf("%.2x ", buf[i + EEPROM_HEADER_SIZE]);

    LOG_DBG("Header: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

    if (len == 0) {
        LOG_ERR("EEPROM data could not be stored. ThingSet error (len = %d)", len);
    }
    else {
        err = eeprom_write(eeprom_dev, 0, buf, len + EEPROM_HEADER_SIZE);
        if (err == 0) {
            LOG_INF("EEPROM data successfully stored");
        }
        else {
            LOG_ERR("EEPROM write error %d", err);
        }
    }
    k_mutex_unlock(&data_buf_lock);
}

#else

void data_storage_write() {;}
void data_storage_read() {;}

#endif

void data_storage_update()
{
    if (uptime() % DATA_UPDATE_INTERVAL == 0 && uptime() > 0) {
        data_storage_write();
    }
}
