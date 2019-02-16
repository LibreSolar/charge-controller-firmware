
#include "mbed.h"
#include "thingset.h"
#include "config.h"

#include "interface.h"

#ifdef USB_SERIAL_ENABLED
#include "USBSerial.h"
USBSerial ser_usb(0x1f00, 0x2012, 0x0001,  false);    // connection is not blocked when USB is not plugged in
uint8_t buf_req_usb[500;
#endif

#ifdef UART_SERIAL_ENABLED
Serial* ser_uart;
uint8_t buf_req_uart[500];
size_t req_uart_pos = 0;
#endif

uint8_t buf_resp[1000];           // only one response buffer needed for USB and UART

extern ThingSet ts;

bool uart_serial_command_flag = false;
bool pub_uart_enabled = false;       // start with sending data disabled

bool usb_serial_command_flag = false;
bool pub_usb_enabled = true;

#ifdef UART_SERIAL_ENABLED

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
    if (pub_uart_enabled) {
        ts.pub_msg_json((char *)buf_resp, sizeof(buf_resp), 4);
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

#else
// dummy functions
void uart_serial_init(Serial* s) {;}
void uart_serial_pub() {;}
void uart_serial_process() {;}
#endif /* UART_SERIAL_ENABLED */


#ifdef USB_SERIAL_ENABLED

void usb_serial_isr()
{
    while (ser_usb.readable() && usb_serial_command_flag == false) {
        if (req_usb.pos < 100) {
            req_usb.data.bin[req_usb.pos] = ser_usb.getc();
            //ser_usb->putc(req_usb.data[req_usb.pos]);     // echo back the character
            if (req_usb.data.bin[req_usb.pos] == '\n') {
                if (req_usb.pos > 0 && req_usb.data.bin[req_usb.pos-1] == '\r')
                    req_usb.data.bin[req_usb.pos-1] = '\0';
                else
                    req_usb.data.bin[req_usb.pos] = '\0';
                req_usb.pos = 0;
                usb_serial_command_flag = true;
            }
            else if (req_usb.pos > 0 && req_usb.data.bin[req_usb.pos] == '\b') { // backspace
                req_usb.pos--;
            }
            else {
                req_usb.pos++;
            }
        }
    }
}

void usb_serial_init()
{
    ser_usb.attach(usb_serial_isr);
    req_usb.data.bin = buf_req_uart;
    req_usb.size = sizeof(buf_req_uart);
}

void usb_serial_process()
{
    if (usb_serial_command_flag) {
        //ser->printf("Received Command: %s\n", buf_serial);
        //int len_req = sizeof(req_usb.data);
        if (req_usb.pos > 0) {
            //int len_resp = thingset_request(req_usb.data, len_req, buf_resp, TS_RESP_BUFFER_LEN);
            thingset_process(&req_usb, &resp, &ts_data);
            ser_usb.printf("%s\n", resp.data);
        }
        usb_serial_command_flag = false;
    }
}

void usb_serial_pub() {
    if (pub_usb_enabled) {
        thingset_pub_msg_json(&resp, &ts_data, pub_serial_data_objects, sizeof(pub_serial_data_objects)/sizeof(uint16_t));
        ser_usb.printf("%s\n", resp.data.str);
    }
}

#else
// dummy functions
void usb_serial_init() {;}
void usb_serial_pub() {;}
void usb_serial_process() {;}
#endif /* USB_SERIAL_ENABLED */
