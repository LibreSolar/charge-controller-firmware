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

#include "config.h"
#include "mbed.h"
#include "thingset.h"
#include "thingset_serial.h"

uint8_t ThingSetStream::buf_resp[1000];

extern ThingSet ts;

void ThingSetStream::process_1s()
{
    if (ts.get_pub_channel(channel)->enabled) {
        ts.pub_msg_json((char *)buf_resp, sizeof(buf_resp), channel);
        stream->puts((const char*)buf_resp);
        stream->putc('\n');
    }
    stream->puts(".\n");
}

void ThingSetStream::process_asap()
{
    if (command_flag) {
        if (req_pos > 1) {
            stream->printf("Received Request (%d bytes): %s\n", strlen((char *)buf_req), buf_req);
            ts.process(buf_req, strlen((char *)buf_req), buf_resp, sizeof(buf_resp));
            stream->puts((const char*)buf_resp);
            stream->putc('\n');
        }

        // start listening for new commands
        command_flag = false;
        req_pos = 0;
    }
}

void ThingSetStream::process_input()
{
        while (stream->readable() && command_flag == false) {
        if (req_pos < sizeof(buf_req)) {
            buf_req[req_pos] = stream->getc();

            if (buf_req[req_pos] == '\n') {
                if (req_pos > 0 && buf_req[req_pos-1] == '\r')
                    buf_req[req_pos-1] = '\0';
                else
                    buf_req[req_pos] = '\0';

                // start processing
                command_flag = true;
            }
            else if (req_pos > 0 && buf_req[req_pos] == '\b') { // backspace
                req_pos--;
            }
            else {
                req_pos++;
            }
        }
    }
}
#endif /* UNIT_TEST */
