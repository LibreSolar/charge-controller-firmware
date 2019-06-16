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

#include "pcb.h"
#include "thingset.h"
#include "eeprom.h"
#include <inttypes.h>
#include <time.h>

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
    0x40, 0x41, 0x42, 0x43,  // load settings
    0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA,    // V, I, T max
    0xA6 // day count
};

#ifndef UNIT_TEST

uint32_t _calc_crc(uint8_t *buf, size_t len)
{
    RCC->AHBENR |= RCC_AHBENR_CRCEN;

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

    RCC->AHBENR &= ~(RCC_AHBENR_CRCEN);
    return CRC->DR;
}

#endif // UNIT_TEST

#if defined(EEPROM_24AA01) || defined(EEPROM_24AA32)

#ifdef EEPROM_24AA01
#define EEPROM_PAGE_SIZE 8      // see datasheet of 24AA01
#define EEPROM_ADDRESS_SIZE 1   // bytes
#elif defined(EEPROM_24AA32)
#define EEPROM_PAGE_SIZE 32     // see datasheet of 24AA32
#define EEPROM_ADDRESS_SIZE 2   // bytes
#endif

int device = 0b1010000 << 1;	// 8-bit address
I2C i2c_eeprom(PIN_EEPROM_SDA, PIN_EEPROM_SCL);

void eeprom_init (int i2c_address)
{
	device = i2c_address;
}

int eeprom_write (unsigned int addr, uint8_t* data, int len)
{
	int err = 0;
	uint8_t buf[EEPROM_PAGE_SIZE + 2];  // page size + 2 address bytes

    for (uint16_t pos = 0; pos < len; pos += EEPROM_PAGE_SIZE) {
        if (EEPROM_ADDRESS_SIZE == 1) {
            buf[0] = (addr + pos) & 0xFF;
        }
        else {
            buf[0] = (addr + pos) >> 8;    // high byte
            buf[1] = (addr + pos) & 0xFF;  // low byte
        }

        int page_len = 0;
        for (int i = 0; i < EEPROM_PAGE_SIZE && pos + i < len; i++) {
            buf[i+EEPROM_ADDRESS_SIZE] = data[pos + i];
            page_len++;
        }

        // send I2C start + device address + memory address + data + I2C stop
        err =  i2c_eeprom.write(device, (char*)buf, page_len + 2);

        if (err != 0)
            return -1;	// write error

        // ACK polling as described in datasheet.
        // EEPROM won't send an ACK from a new request until it finished writing.
        for (int counter = 0; counter < 100; counter++) {
            err =  i2c_eeprom.write(device, (char*)buf, 2);
            if (err == 0) {
                //printf("EEPROM write finished, counter: %d\n", counter);
                break;
            }
        }

    }
	return err;
 }

int eeprom_read (unsigned int addr, uint8_t* ret, int len)
{
	uint8_t buf[2];
    int err = 0;

    if (EEPROM_ADDRESS_SIZE == 1) {
    	// write the address we want to read
        buf[0] = addr & 0xFF;
    	// send memory address without stop (repeated = true)
        err = i2c_eeprom.write(device, (char*)buf, 1, true);
    }
    else {
        // same as above, but with 2 bytes address
        buf[0] = addr >> 8;    // high byte
        buf[1] = addr & 0xFF;  // low byte
        err = i2c_eeprom.write(device, (char*)buf, 2, true);
    }

	if (err != 0)
		return -1;  // read error

	wait(0.02);

	return i2c_eeprom.read(device, (char*)ret, len);
}

#elif defined(STM32L0)  // internal EEPROM

int eeprom_write (unsigned int addr, uint8_t* data, int len)
{
    int timeout = 0;

    // Wait till no operation is on going
    while ((FLASH->SR & FLASH_SR_BSY) != 0 && timeout < 100) {
        timeout++;
    }

    // Perform unlock sequence if EEPROM is locked
    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0) {
        FLASH->PEKEYR = FLASH_PEKEY1;
        FLASH->PEKEYR = FLASH_PEKEY2;
    }

    if (addr + len > DATA_EEPROM_BANK1_END - DATA_EEPROM_BASE)
        return -1;

    // write data byte-wise to EEPROM
	for (int i = 0; i < len; i++) {
        *(uint8_t *)(DATA_EEPROM_BASE + addr + i) = data[i];
	}

    // Wait till no operation is on going
    while ((FLASH->SR & FLASH_SR_BSY) != 0 && timeout < 200) {
        timeout++;
    }
    // Lock the NVM again by setting PELOCK in PECR
    FLASH->PECR |= FLASH_PECR_PELOCK;

	return 0;
}

int eeprom_read (unsigned int addr, uint8_t* ret, int len)
{
    if (addr + len > DATA_EEPROM_BANK1_END - DATA_EEPROM_BASE)
        return -1;

    memcpy(ret, ((uint8_t*)addr) + DATA_EEPROM_BASE, len);
    return 0;
}

#endif // STM32L0


#if (defined(PIN_EEPROM_SDA) && defined(PIN_EEPROM_SCL)) || defined(STM32L0)

// EEPROM layout:
// bytes 0-1: Version number
// bytes 2-3: number of data bytes
// bytes 4-7: CRC32
// byte 8: start of data

void eeprom_restore_data()
{
    uint8_t buf_req[300];  // ThingSet request buffer

    // EEPROM header
    uint8_t buf_header[EEPROM_HEADER_SIZE];
    if (eeprom_read(0, buf_header, EEPROM_HEADER_SIZE) < 0) {
        printf("EEPROM: read error!\n");
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
        eeprom_read(EEPROM_HEADER_SIZE, buf_req, len);

        //printf("Data (len=%d): ", len);
        //for (int i = 0; i < len; i++) printf("%.2x ", buf_req[i]);

        if (_calc_crc(buf_req, len) == crc) {
            int status = ts.init_cbor(buf_req, sizeof(buf_req));     // first byte is ignored
            printf("EEPROM: Data objects read and updated, ThingSet result: %d\n", status);
        }
        else {
            printf("EEPROM: CRC of data not correct, expected 0x%x (data_len = %d)\n", (unsigned int)crc, len);
        }
    }
    else {
        printf("EEPROM: Empty or data layout version changed\n");
    }
}

void eeprom_store_data()
{
    uint8_t buf[300];

    int len = ts.pub_msg_cbor(buf + EEPROM_HEADER_SIZE, sizeof(buf) - EEPROM_HEADER_SIZE, eeprom_data_objects, sizeof(eeprom_data_objects)/sizeof(uint16_t));
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
    else if (eeprom_write(0, buf, len + EEPROM_HEADER_SIZE) < 0) {
        printf("EEPROM: Write error.\n");
    }
    else {
        printf("EEPROM: Data successfully stored.\n");
    }
}

#else

void eeprom_store_data() {;}
void eeprom_restore_data() {;}

#endif

void eeprom_update()
{
    if (time(NULL) % EEPROM_UPDATE_INTERVAL == 0 && time(NULL) > 0) {
        eeprom_store_data();
    }
}
