

#ifndef __HARDWARE_H_
#define __HARDWARE_H_

void init_load();
void hw_load_switch(bool enabled);
void hw_usb_out(bool enabled);

void init_watchdog(float timeout);      // timeout in seconds
void feed_the_dog();

void control_timer_start(int freq_Hz);
void system_control();  // is implemented in main.cpp

void start_dfu_bootloader();

#endif /* HARDWARE_H */