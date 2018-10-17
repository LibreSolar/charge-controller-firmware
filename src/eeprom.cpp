
#include "config.h"
#include "thingset.h"
#include "data_objects.h"
#include "mbed.h"
#include "eeprom.h"

#if defined(PIN_EEPROM_SDA) && defined(PIN_EEPROM_SCL)

int device = 0b1010000 << 1;	// 8-bit address
I2C i2c_eeprom(PIN_EEPROM_SDA, PIN_EEPROM_SCL);

void eeprom_init (int i2c_address)
{
	device = i2c_address;
}

int eeprom_write (int addr, char* data, int len)
{
	int err;
	char buf[PAGE_SIZE + 2];  // page size + 2 address bytes

	if (len > PAGE_SIZE)
		return -1;  // too long
    
    buf[0] = addr >> 8;    // high byte
    buf[1] = addr & 0xFF;  // low byte

	for (int i = 0; i < len; i++) {
		buf[i+2] = data[i];
	}

	// send I2C start + device address + memory address + data + I2C stop
	err =  i2c_eeprom.write(device, buf, len + 2);

	if (err != 0)
		return -1;	// write error
    
	// ACK polling as described in datasheet.
	// EEPROM wont send an ACK from a new request until it finished writing.
	for (int counter = 0; counter < 100; counter++) {
		err =  i2c_eeprom.write(device, buf, 2);
		if (err == 0) {
			//printf("EEPROM write finished, counter: %d\n", counter);
			break;
		}
    }
	return err;
 } 
  
int eeprom_read (int addr, char* ret, int len )
{
	char buf[2];

	// write the address we want to read
    buf[0] = addr >> 8;    // high byte
    buf[1] = addr & 0xFF;  // low byte
    
	// send memory address without stop (repeated = true)
	if (i2c_eeprom.write(device, buf, 2, true) != 0)
		return -1;  // read error
  
	wait(0.02);

	return i2c_eeprom.read(device, ret, len);
} 

// EEPROM layout:
// bytes 0-1: Version number
// bytes 2-3: number of saved data bytes
// byte 10: start of data

void eeprom_restore_data(ts_data_t *ts)
{
    union float2bytes { float f; char b[4]; } f2b;     // for conversion of bytes to float
    char buf[6];
    int stored_bytes;
    int addr = 10;

    eeprom_read(0, buf, 4);

    if (((buf[0] << 8) + buf[1]) != EEPROM_VERSION)
        return;

    stored_bytes = (buf[2] << 8) + buf[3];
    //printf("Stored bytes: %d\n", stored_bytes);

    while (addr < stored_bytes + 10) {

        eeprom_read(addr, buf, 6);
        const data_object_t* data_obj = thingset_data_object_by_id(ts, (buf[0] << 8) + buf[1]);
        if (data_obj == NULL) {
            //printf("ID %d not found\n", (buf[0] << 8) + buf[1]);
            return;
        }
    
        switch (data_obj->type) {
            case TS_T_FLOAT32:
                f2b.b[0] = buf[5];
                f2b.b[1] = buf[4];
                f2b.b[2] = buf[3];
                f2b.b[3] = buf[2];
                *((float*)data_obj->data) = f2b.f;
                addr += 6;
                //printf("Restored %s = %f\n", data_obj->name, f2b.f);
                break;
            case TS_T_INT32:
                *((int*)data_obj->data) =
                    (buf[2] << 24) + (buf[3] << 16) + (buf[4] << 8) + buf[5];
                addr += 6;
                //printf("Restored %s = %d\n", data_obj->name, *((int*)data_obj->data));
                break;
            case TS_T_BOOL:
                *((bool*)data_obj->data) = buf[2];
                addr += 3;
                //printf("Restored %s = %d\n", data_obj->name, *((bool*)data_obj->data));
                break;
        }
    }
}

void eeprom_store_data(ts_data_t *ts)
{
    union float2bytes { float f; char b[4]; } f2b;     // for conversion of bytes to float
    char buf[6];
    int len = 0, value;
    int addr = 10;      // first 10 bytes reserved

    for (unsigned int i = 0; i < sizeof(eeprom_data_objects)/sizeof(eeprom_data_objects[0]); i++) {
        const data_object_t* data_obj = thingset_data_object_by_id(ts, eeprom_data_objects[i]);
        if (data_obj == NULL) {
            continue;
        }
        buf[0] = data_obj->id >> 8;
        buf[1] = data_obj->id;

        // byte order: most significant byte first (big endian) like CBOR
        switch (data_obj->type) {
            case TS_T_FLOAT32:
                f2b.f = *((float*)data_obj->data);
                buf[2] = f2b.b[3];
                buf[3] = f2b.b[2];
                buf[4] = f2b.b[1];
                buf[5] = f2b.b[0];
                len = 6;
                break;
            case TS_T_INT32:
                value = *((int*)data_obj->data);
                buf[2] = value >> 24;
                buf[3] = value >> 16;
                buf[4] = value >> 8;
                buf[5] = value;
                len = 6;
                break;
            case TS_T_BOOL:
                buf[2] = *((bool*)data_obj->data);
                len = 3;
                break;
        }
        eeprom_write(addr, buf, len);
        //printf("Written %s to address %d\n", data_obj->name, addr);
        addr += len;
    }

    // store EEPROM_VERSION and amount of used bytes
    buf[0] = EEPROM_VERSION >> 8;
    buf[1] = EEPROM_VERSION;
    buf[2] = (addr - 10) >> 8;
    buf[3] = (addr - 10);
    eeprom_write(0, buf, 4);
}

#else

void eeprom_store_data(ts_data_t *ts) {;}
void eeprom_restore_data(ts_data_t *ts) {;}

#endif 
