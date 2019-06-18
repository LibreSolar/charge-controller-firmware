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

#ifndef UNIT_TEST

#include "mbed.h"
#include "thingset.h"
#include "config.h"

#include "thingset_serial.h"

#ifdef USB_SERIAL_ENABLED
#include "USBSerial.h"
USBSerial ser_usb(0x1f00, 0x2012, 0x0001,  false);    // connection is not blocked when USB is not plugged in
uint8_t buf_req_usb[500];
size_t req_usb_pos = 0;
#endif

#ifdef UART_SERIAL_ENABLED
Serial* ser_uart;
uint8_t buf_req_uart[500];
size_t req_uart_pos = 0;
#endif

static uint8_t buf_resp[1000];           // only one response buffer needed for USB and UART

extern ThingSet ts;
extern ts_pub_channel_t pub_channels[];
extern const int pub_channel_serial;

void thingset_serial_init(Serial* s)
{
#ifdef UART_SERIAL_ENABLED
    uart_serial_init(s);
#endif
#ifdef USB_SERIAL_ENABLED
    usb_serial_init();
#endif
}

void thingset_serial_process_asap()
{
#ifdef UART_SERIAL_ENABLED
    uart_serial_process();
#endif
#ifdef USB_SERIAL_ENABLED
    usb_serial_process();
#endif
}

void thingset_serial_process_1s()
{
#ifdef UART_SERIAL_ENABLED
    uart_serial_pub();
#endif
#ifdef USB_SERIAL_ENABLED
    usb_serial_pub();
#endif

    fflush(stdout);
}

#ifdef UART_SERIAL_ENABLED

static bool uart_serial_command_flag = false;

void uart_serial_isr()
{
    while (ser_uart->readable() && uart_serial_command_flag == false) {
        if (req_uart_pos < sizeof(buf_req_uart)) {
            buf_req_uart[req_uart_pos] = ser_uart->getc();

            if (buf_req_uart[req_uart_pos] == '\n') {
                if (req_uart_pos > 0 && buf_req_uart[req_uart_pos-1] == '\r')
                    buf_req_uart[req_uart_pos-1] = '\0';
                else
                    buf_req_uart[req_uart_pos] = '\0';

                // start processing
                uart_serial_command_flag = true;
            }
            else if (req_uart_pos > 0 && buf_req_uart[req_uart_pos] == '\b') { // backspace
                req_uart_pos--;
            }
            else {
                req_uart_pos++;
            }
        }
    }
}

void uart_serial_init(Serial* s)
{
    ser_uart = s;
    s->attach(uart_serial_isr);
}

void uart_serial_pub()
{
    if (pub_channels[pub_channel_serial].enabled) {
        ts.pub_msg_json((char *)buf_resp, sizeof(buf_resp), 0);
        ser_uart->printf("%s\n", buf_resp);
    }
}

void uart_serial_process()
{
    if (uart_serial_command_flag) {
        if (req_uart_pos > 1) {
            ser_uart->printf("Received Request (%d bytes): %s\n", strlen((char *)buf_req_uart), buf_req_uart);
            ts.process(buf_req_uart, strlen((char *)buf_req_uart), buf_resp, sizeof(buf_resp));
            ser_uart->printf("%s\n", buf_resp);
        }

        // start listening for new commands
        uart_serial_command_flag = false;
        req_uart_pos = 0;
    }
}

#endif /* UART_SERIAL_ENABLED */


#ifdef USB_SERIAL_ENABLED

static bool usb_serial_command_flag = false;

void usb_serial_isr()
{
    while (ser_usb.readable() && usb_serial_command_flag == false) {
        if (req_usb_pos < 100) {
            buf_req_usb[req_usb_pos] = ser_usb.getc();
            //ser_usb->putc(req_usb.data[req_usb_pos]);     // echo back the character
            if (buf_req_usb[req_usb_pos] == '\n') {
                if (req_usb_pos > 0 && buf_req_usb[req_usb_pos-1] == '\r')
                    buf_req_usb[req_usb_pos-1] = '\0';
                else
                    buf_req_usb[req_usb_pos] = '\0';
                req_usb_pos = 0;
                usb_serial_command_flag = true;
            }
            else if (req_usb_pos > 0 && buf_req_usb[req_usb_pos] == '\b') { // backspace
                req_usb_pos--;
            }
            else {
                req_usb_pos++;
            }
        }
    }
}

void usb_serial_init()
{
    ser_usb.attach(usb_serial_isr);
//    buf_req_usb = buf_req_uart;
}

void usb_serial_process()
{
    if (usb_serial_command_flag) {
        //ser->printf("Received Command: %s\n", buf_serial);
        //int len_req = sizeof(req_usb.data);
        if (req_usb_pos > 1) {
            ser_usb.printf("Received Request (%d bytes): %s\n", strlen((char *)buf_req_usb), buf_req_usb);
            ts.process(buf_req_usb, strlen((char *)buf_req_usb), buf_resp, sizeof(buf_resp));
            ser_usb.printf("%s\n", buf_resp);
        }
        usb_serial_command_flag = false;
        req_usb_pos = 0;
    }
}

void usb_serial_pub() {
    if (pub_channels[pub_channel_serial].enabled) {
        ts.pub_msg_json((char *)buf_resp, sizeof(buf_resp), pub_channel_serial);
        ser_usb.printf("%s\n", buf_resp);
    }
}

#endif /* USB_SERIAL_ENABLED */

#endif /* UNIT_TEST */
