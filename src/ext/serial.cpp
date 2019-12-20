/*
 * Copyright (c) 2018 Martin Jäger / Libre Solar
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __MBED__

#include "mbed.h"
#include "config.h"
#include "thingset.h"
#include "ext.h"

class ThingSetStream: public ExtInterface
{
    public:
        ThingSetStream(Stream& s, const unsigned int c): channel(c), stream(&s) {};

        virtual void process_asap();
        virtual void process_1s();

    protected:
        virtual void process_input(); // this is called from the ISR typically
        const unsigned int channel;

        virtual bool readable()
        {
            return stream->readable();
        }

    private:
        Stream* stream;

        static char buf_resp[1000];           // only one response buffer needed for all objects
        char buf_req[500];
        size_t req_pos = 0;
        bool command_flag = false;
};

template<typename T> class ThingSetSerial: public ThingSetStream
{
    public:
        ThingSetSerial(T& s, const unsigned int c): ThingSetStream(s,c), ser(s) {}

        void enable() {
            Callback<void()>  cb([this]() -> void { this->process_input();});
            ser.attach(cb);
        }

    private:
        bool readable()
        {
            return ser.readable();
        }

    private:
        T& ser;
};


/*
 * Construct all global ExtInterfaces here.
 * All of these are added to the list of devices
 * for later processing in the normal operation
 */

#ifdef UART_SERIAL_ENABLED
    extern const int pub_channel_serial;
    extern Serial serial;

    ThingSetSerial ts_uart(serial, pub_channel_serial);
#endif /* UART_SERIAL_ENABLED */

// ToDo: Add USB serial again (previous library was broken with recent mbed releases)

char ThingSetStream::buf_resp[1000];

extern ThingSet ts;

void ThingSetStream::process_1s()
{
    if (ts.get_pub_channel(channel)->enabled) {
        ts.pub_msg_json(buf_resp, sizeof(buf_resp), channel);
        stream->puts(buf_resp);
        stream->putc('\n');
    }
}

void ThingSetStream::process_asap()
{
    if (command_flag) {
        // commands must have 2 or more characters
        if (req_pos > 1) {
            stream->printf("Received Request (%d bytes): %s\n", strlen(buf_req), buf_req);
            ts.process((uint8_t *)buf_req, strlen(buf_req), (uint8_t *)buf_resp, sizeof(buf_resp));
            stream->puts(buf_resp);
            stream->putc('\n');
            fflush(*stream);
        }

        // start listening for new commands
        command_flag = false;
        req_pos = 0;
    }
}

/**
 * Read characters from stream until line end \n detected, signal command available then
 * and wait for processing
 */
void ThingSetStream::process_input()
{
    while (readable() && command_flag == false) {

        int c = stream->getc();

        // \r\n and \n are markers for line end, i.e. command end
        // we accept this at any time, even if the buffer is 'full', since
        // there is always one last character left for the \0
        if (c == '\n') {
            if (req_pos > 0 && buf_req[req_pos-1] == '\r') {
                buf_req[req_pos-1] = '\0';
            }
            else {
                buf_req[req_pos] = '\0';
            }
            // start processing
            command_flag = true;
        }

        // backspace
        // can be done always unless there is nothing in the buffer
        else if (req_pos > 0 && c == '\b') {
            req_pos--;
        }
        // we fill the buffer up to all but 1 character
        // the last character is reserved for '\0'
        // more read characters are simply dropped, unless it is \n
        // which ends the command input and triggers processing
        else if (req_pos < (sizeof(buf_req)-1)) {
            buf_req[req_pos++] = c;
        }
    }
}
#endif /* __MBED__ */
