#ifndef _INTERFACE_H_
#define _INTERFACE_H_

#include "mbed.h"
#include <stdint.h>
#include "dcdc.h"
#include "power_port.h"
#include "battery.h"
#include "load.h"

/* OLED display based on SSD1306 IC, connected to UEXT port via I2C
 */
void oled_output(dcdc_t *dcdc, power_port_t *solar_port, power_port_t *bat_port, battery_t *bat, load_output_t *load);

/* UART serial interface (either in UEXT connector or from additional SWD serial)
 */
void uart_serial_init(Serial* s);
void uart_serial_process();
void uart_serial_pub();
void output_serial(dcdc_t *dcdc, battery_t *bat, load_output_t *load);

/* Serial interface via USB CDC device class (currently only supported with STM32F0)
 */
void usb_serial_init();
void usb_serial_process();
void usb_serial_pub();

/* CAN bus interface
 */
void can_send_data();
void can_receive();
void can_process_outbox();
void can_process_inbox();
void can_send_object_name(int obj_id, uint8_t can_dest_id);

/* GSM mobile interface via UEXT connector serial interface
 */
void gsm_init();
void gsm_process();

/* LoRa interface via UEXT connector SPI interface
 */
void lora_init();
void lora_process();

/* GSM mobile interface via UEXT connector serial interface
 */
void wifi_init();
void wifi_process();

/* SD card connected to UEXT connector (e.g. via Olimex adapter)
 *
 * The new mbed implementation of SD cards require RTOS --> currently not supported
 */
void output_sdcard();


#endif /* _INTERFACE_H_ */
