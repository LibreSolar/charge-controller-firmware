/* ThingSet protocol library
 * Copyright (c) 2017-2018 Martin Jäger (www.libre.solar)
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

#include "thingset.h"
#include "ts_parser.h"

#include <string.h>
#include <stdio.h>

#define DEBUG 0

void thingset_process(str_buffer_t *req, str_buffer_t *resp, ts_data_t *data)
{
    static ts_parser_t tsp;

    if (req->pos < 1) {
        thingset_status_message_json(resp, TS_STATUS_UNKNOWN_FUNCTION);
        return;
    }

    jcp_init(&(tsp.parser));

    if (req->data[0] == TS_FUNCTION_READ) {
        bin_buffer_t req_bin, resp_bin;
        req_bin.data = (uint8_t*)req->data;
        req_bin.pos = req->pos;
        req_bin.size = req->size;
        resp_bin.data = (uint8_t*)resp->data;
        resp_bin.pos = 0;
        resp_bin.size = resp->size;
        thingset_read_cbor(&req_bin, &resp_bin, data);
        resp->pos = resp_bin.pos;
    }
    else if (req->data[0] == TS_FUNCTION_WRITE) {
        bin_buffer_t req_bin, resp_bin;
        req_bin.data = (uint8_t*)req->data;
        req_bin.pos = req->pos;
        req_bin.size = req->size;
        resp_bin.data = (uint8_t*)resp->data;
        resp_bin.pos = 0;
        resp_bin.size = resp->size;
        thingset_write_cbor(&req_bin, &resp_bin, data);
        resp->pos = resp_bin.pos;
    }
    else if (req->data[0] == '!') {      // JSON request
        if (req->pos > 4 && strncmp(req->data, "!read", 5) == 0) {
            tsp.str = req->data+6;
            tsp.tok_count = jcp_parse(&(tsp.parser), tsp.str, req->pos-6, tsp.tokens, TS_NUM_JSON_TOKENS);
            //printf("read command, data: %s, num_tok: %d\n", tsp.str, tsp.tok_count);
            thingset_read_json(&tsp, resp, data);
        }
        else if (req->pos > 5 && strncmp(req->data, "!write", 6) == 0) {
            tsp.str = req->data+7;
            tsp.tok_count = jcp_parse(&(tsp.parser), tsp.str, req->pos-7, tsp.tokens, TS_NUM_JSON_TOKENS);
            //printf("write command, data: %s, num_tok: %d\n", tsp.str, tsp.tok_count);
            thingset_write_json(&tsp, resp, data);
        }
        else if (req->pos > 5 && strncmp(req->data, "!list", 5) == 0) {
            tsp.str = req->data+6;
            tsp.tok_count = jcp_parse(&(tsp.parser), tsp.str, req->pos-6, tsp.tokens, TS_NUM_JSON_TOKENS);
            thingset_list_json(&tsp, resp, data);
        }
        // quick and dirty hack to go into DFU mode!
        //else if (req_len >= 4 && strncmp(req, "!dfu", 4) == 0) {
            //dfu_run_bootloader();
        //}
        else if (req->pos >= 4 && strncmp(req->data, "!pub", 4) == 0) {
            /* TODO!!
            cal.pub_data_enabled = !cal.pub_data_enabled;
            sprintf(resp->data, ":0 Success.\n");
            return strlen(resp->data);
            */
        }
        else {
            thingset_status_message_json(resp, TS_STATUS_UNKNOWN_FUNCTION);
        }
    }
    else {
        // not a thingset command --> ignore and set response to empty string
        resp->data[0] = 0;
        resp->pos = 0;
        //thingset_status_message(ts, TS_STATUS_UNKNOWN_FUNCTION);
    }
}


