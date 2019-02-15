
#include "pcb.h"
#include "thingset.h"
#include "mbed.h"
#include "eeprom.h"

// versioning of EEPROM layout (2 bytes)
// change the version number each time the data object array below is changed!
#define EEPROM_VERSION 2

#define EEPROM_HEADER_SIZE 8    // bytes

#define EEPROM_UPDATE_INTERVAL  (6*60*60)       // update every 6 hours

extern ThingSet ts;

// stores object-ids of values to be stored in EEPROM
const uint16_t eeprom_data_objects[] = {
    0x01, // timestamp
    0x03, 0x04, // load settings
    0x07, 0x08, // input / output wh
    0x10, 0x11, // num full charge / deep-discharge
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,   // config data
    0x42, 0x44, 0x46, 0x48, 0x49,     // V, I, T max
    0x51 // day count
};

uint32_t _calc_crc(uint8_t *buf, size_t len)
{
    RCC->AHBENR |= RCC_AHBENR_CRCEN;

    // we keep standard polynomial 0x04C11DB7 (same for STM32L0 and STM32F0)
    // and we don't care for endianness here
    CRC->CR |= CRC_CR_RESET;
    for (size_t i = 0; i < len; i += 4) {
        //printf("CRC buf: %.8x, CRC->DR: %.8x\n", *((uint32_t*)&buf[i]), CRC->DR);
        CRC->DR = *((uint32_t*)&buf[i]);
    }

    RCC->AHBENR &= ~(RCC_AHBENR_CRCEN);
    return CRC->DR;
}

#if defined(PIN_EEPROM_SDA) && defined(PIN_EEPROM_SCL)

#ifdef PCB_LS_010
#define EEPROM_PAGE_SIZE 8      // see datasheet of 24AA01
#define EEPROM_ADDRESS_SIZE 1   // bytes
#else
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
    uint8_t buf_req[200] = {0};  // ThingSet request buffer, initialize with zeros

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
        //printf("Data: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n",
        //    ts_req.data.bin[0], ts_req.data.bin[1], ts_req.data.bin[2], ts_req.data.bin[3],
        //    ts_req.data.bin[4], ts_req.data.bin[5], ts_req.data.bin[6], ts_req.data.bin[7]);

        if (_calc_crc(buf_req, len) == crc) {
            int status = ts.init_cbor(buf_req, sizeof(buf_req));     // first byte is ignored
            printf("EEPROM: Data objects read and updated, ThingSet result: %d\n", status);
        }
        else {
            printf("EEPROM: CRC of data not correct, expected 0x%x (data_len = %d)\n", (unsigned int)crc, len);
        }
    }
}

void eeprom_store_data()
{
    uint8_t buf[200] = {0};  // initialize with zeros

    int len = ts.pub_msg_cbor(buf + EEPROM_HEADER_SIZE, sizeof(buf) - EEPROM_HEADER_SIZE, eeprom_data_objects, sizeof(eeprom_data_objects)/sizeof(uint16_t));
    uint32_t crc = _calc_crc(buf + EEPROM_HEADER_SIZE, len);

    // store EEPROM_VERSION, number of bytes and CRC
    *((uint16_t*)&buf[0]) = (uint16_t)EEPROM_VERSION;
    *((uint16_t*)&buf[2]) = (uint16_t)(len);   // length of data
    *((uint32_t*)&buf[4]) = crc;

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
    if (time(NULL) % EEPROM_UPDATE_INTERVAL == 0) {
        eeprom_store_data();
    }
}