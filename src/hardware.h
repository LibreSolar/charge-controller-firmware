

#ifndef __HARDWARE_H_
#define __HARDWARE_H_

#include "mbed.h"
//#include "structs.h"
#include "data_objects.h"

void enable_load();
void disable_load();

void enable_usb();
void disable_usb();

void init_leds();
void update_solar_led(bool enabled);
void flash_led_soc(battery_t *bat);

void init_watchdog(float timeout);      // timeout in seconds
void feed_the_dog();

#endif /* HARDWARE_H */