
#include "mbed.h"
#include "thingset.h"

#include "interface.h"
#include "data_objects.h"

#ifdef USB_SERIAL_ENABLED
#include "USBSerial.h"
extern USBSerial usbSerial;
#endif

uint8_t buf_req_uart[TS_REQ_BUFFER_LEN];
uint8_t buf_req_usb[TS_REQ_BUFFER_LEN];
uint8_t buf_resp[TS_RESP_BUFFER_LEN];

ts_data_t data;

str_buffer_t req_uart;
str_buffer_t req_usb;
str_buffer_t resp;


int uart_pos = 0;
int usb_pos = 0;

Serial* ser_uart;

#if PCB_VERSION < 003
//USBSerial* ser_usb;
#endif

bool uart_serial_command_flag = false;
bool usb_serial_command_flag = false;

void uart_serial_isr()
{
    while (ser_uart->readable() && uart_serial_command_flag == false) {
        if (req_uart.pos < TS_REQ_BUFFER_LEN) {
            req_uart.data[req_uart.pos] = ser_uart->getc();

            // echo back the character
            //ser_uart->putc(req_uart.data[req_uart.pos]);

            if (req_uart.data[req_uart.pos] == '\n') {
                if (req_uart.pos > 0 && req_uart.data[req_uart.pos-1] == '\r')
                    req_uart.data[req_uart.pos-1] = '\0';
                else
                    req_uart.data[req_uart.pos] = '\0';
                
                // start processing
                uart_serial_command_flag = true;
            }
            else if (req_uart.pos > 0 && req_uart.data[req_uart.pos] == '\b') { // backspace
                req_uart.pos--;
            }
            else {
                req_uart.pos++;
            }
        }
    }
}

void uart_serial_init(Serial* s)
{
    ser_uart = s;
    s->attach(uart_serial_isr);
    data.objects = dataObjects;
    data.size = dataObjectsCount;
}

void uart_serial_process()
{
    if (uart_serial_command_flag) {
        if (req_uart.pos > 0) {
            ser_uart->printf("Received Request: %s\n", req_uart.data);
            thingset_process(&req_uart, &resp, &data);
            ser_uart->printf("%s\n", resp.data);
        }

        // start listening for new commands
        uart_serial_command_flag = false;
        req_uart.pos = 0;
    }
}

void uart_serial_pub() {
    //thingset_pub_json(buf_resp, TS_RESP_BUFFER_LEN);
    //ser_uart->printf("%s", buf_resp);
}

#ifdef USB_SERIAL_ENABLED

void usb_serial_isr()
{
    while (ser_usb->readable() && usb_serial_command_flag == false) {
        if (req_usb.pos < 100) {
            req_usb.data[req_usb.pos] = ser_usb->getc();
            //ser_usb->putc(req_usb.data[req_usb.pos]);     // echo back the character
            if (req_usb.data[req_usb.pos] == '\n') {
                if (req_usb.pos > 0 && req_usb.data[req_usb.pos-1] == '\r')
                    req_usb.data[req_usb.pos-1] = '\0';
                else
                    req_usb.data[req_usb.pos] = '\0';
                req_usb.pos = 0;
                usb_serial_command_flag = true;
            }
            else if (req_usb.pos > 0 && req_usb.data[req_usb.pos] == '\b') { // backspace
                req_usb.pos--;
            }
            else {
                req_usb.pos++;
            }
        }
    }
}

void usb_serial_init(USBSerial* s)
{
    ser_usb = s;
    s->attach(usb_serial_isr);
    data.objects = dataObjects;
    data.size = sizeof(dataObjects) / sizeof(data_object_t);
    //req_usb.data = buf_req_usb;
}

void usb_serial_process()
{
    if (usb_serial_command_flag) {
        //ser->printf("Received Command: %s\n", buf_serial);
        //int len_req = sizeof(req_usb.data);
        if (req_usb.pos > 0) {
            //int len_resp = thingset_request(req_usb.data, len_req, buf_resp, TS_RESP_BUFFER_LEN);
            thingset_process(&req_usb, &resp, &data);
            ser_usb->printf("%s\n", resp.data);
        }
        usb_serial_command_flag = false;
    }
}


void usb_serial_pub() {
    //thingset_pub_json(buf_resp, TS_RESP_BUFFER_LEN);
    //ser_usb->printf("%s", buf_resp);
}

#endif /* USB_SERIAL_ENABLED */
