
#include "mbed.h"

#define PAGE_SIZE 32    // see datasheet of 24LCxx

void eeprom_init(int address);

// return values: 0 for success
int eeprom_write (int, char *, int);
int eeprom_read (int, char *, int);

void eeprom_store_data(ts_data_t *ts);
void eeprom_restore_data(ts_data_t *ts);
