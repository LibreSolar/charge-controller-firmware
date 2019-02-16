
#include "mbed.h"

void eeprom_init(int address);

// return values: 0 for success
int eeprom_write (unsigned int, char *, int);
int eeprom_read (unsigned int, char *, int);

void eeprom_store_data();
void eeprom_restore_data();

void eeprom_update();