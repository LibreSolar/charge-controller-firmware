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

#ifdef __MBED__

#include "thingset_serial.h"

#include "config.h"
#include "thingset.h"

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
