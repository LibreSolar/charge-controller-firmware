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

#ifndef INTERFACE_H
#define INTERFACE_H

/** @file
 *
 * @brief
 * Communication interfaces connected to UEXT port
 */

#include "mbed.h"
#include <stdint.h>
#include "dcdc.h"
#include "power_port.h"
#include "battery.h"
#include "load.h"

/** OLED display based on SSD1306 IC, connected to UEXT port via I2C
 */
void oled_output(dcdc_t *dcdc, power_port_t *solar_port, power_port_t *bat_port, battery_state_t *bat, load_output_t *load);

/** UART serial interface (either in UEXT connector or from additional SWD serial)
 */
void uart_serial_init(Serial* s);
void uart_serial_process();
void uart_serial_pub();

/** Serial interface via USB CDC device class (currently only supported with STM32F0)
 */
void usb_serial_init();
void usb_serial_process();
void usb_serial_pub();

/** CAN bus interface
 */
void can_send_data();
void can_receive();
void can_process_outbox();
void can_process_inbox();
void can_send_object_name(int obj_id, uint8_t can_dest_id);

/** GSM mobile interface via UEXT connector serial interface
 *
 * Initialization (called once at program startup)
 */
void gsm_init();

/** GSM mobile interface via UEXT connector serial interface
 *
 * Process function, called every second
 */
void gsm_process();

/** LoRa interface via UEXT connector SPI interface
 *
 * Initialization (called once at program startup)
 */
void lora_init();

/** LoRa interface via UEXT connector SPI interface
 *
 * Process function, called every second
 */
void lora_process();

/** WiFi interface with ESP32
 *
 * Initialization (called once at program startup)
 */
void wifi_init();

/** WiFi interface with ESP32
 *
 * Process function, called every second
 */
void wifi_process();

/** SD card connected to UEXT connector (e.g. via Olimex adapter)
 *
 * The new mbed implementation of SD cards require RTOS --> currently not supported
 */
void output_sdcard();

#endif /* INTERFACE_H */
