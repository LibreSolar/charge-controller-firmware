#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include "structs.h"

void output_sdcard();

void output_serial(dcdc_t *dcdc, battery_t *bat, load_output_t *load);

void output_oled(dcdc_t *dcdc, dcdc_port_t *solar_port,  dcdc_port_t *bat_port, battery_t *bat, load_output_t *load);



void uart_serial_init(Serial* s);
void uart_serial_process();
void uart_serial_pub();

#ifdef USB_SERIAL_ENABLED
#include "USBSerial.h"
void usb_serial_init(USBSerial* s);
void usb_serial_process();
void usb_serial_pub();
#endif


/*
void can_send_data();
void can_receive();
void can_process_outbox();
void can_process_inbox();
//void can_list_object_ids(int category = TS_C_ALL);
void can_send_object_name(int obj_id, uint8_t can_dest_id);
*/

#endif /* _INTERFACE_H_ */
