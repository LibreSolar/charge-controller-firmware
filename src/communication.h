#include "mbed.h"

void uart_serial_init(Serial* s);
void uart_serial_process();
void uart_serial_pub();

#ifdef USB_SERIAL_ENABLED
#include "USBSerial.h"
void usb_serial_init(USBSerial* s);
void usb_serial_process();
void usb_serial_pub();
#endif
