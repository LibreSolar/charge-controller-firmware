/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#ifndef __THINGSET_H_
#define __THINGSET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ts_config.h"
#include "ts_parser.h"

#include <stdint.h>

/* Protocol functions
 */
#define TS_FUNCTION_READ     0x01
#define TS_FUNCTION_WRITE    0x02
#define TS_FUNCTION_LIST     0x03
#define TS_FUNCTION_NAME     0x04
#define TS_FUNCTION_PUB      0x05
#define TS_FUNCTION_AUTH     0x06

/* Status codes
 */
#define TS_STATUS_SUCCESS            0
#define TS_STATUS_ERROR             30
#define TS_STATUS_UNKNOWN_FUNCTION  31    // Function ID unknown
#define TS_STATUS_UNKNOWN_DATA_OBJ  32    // Data Object ID unknown
#define TS_STATUS_WRONG_FORMAT      33      
#define TS_STATUS_WRONG_TYPE        34    // Data type not supported
#define TS_STATUS_DEVICE_BUSY       35    // Device busy
#define TS_STATUS_UNAUTHORIZED      36
#define TS_STATUS_REQUEST_TOO_LONG  37
#define TS_STATUS_RESPONSE_TOO_LONG 38
#define TS_STATUS_INVALID_VALUE  39       // value out of allowed range

/* Internal C data types (used to cast void* pointers)
 */
#define TS_T_BOOL    0
#define TS_T_UINT64  1
#define TS_T_INT64   2
#define TS_T_UINT32  3
#define TS_T_INT32   4
#define TS_T_UINT16  5
#define TS_T_INT16   6
#define TS_T_FLOAT32 7
#define TS_T_STRING  8

/* Internal access rights to data objects
 */
#define TS_ACCESS_READ (0x1U)
#define TS_ACCESS_WRITE (0x1U << 1)
#define TS_ACCESS_READ_AUTH (0x1U << 2)     // read after authentication
#define TS_ACCESS_WRITE_AUTH (0x1U << 3)    // write after authentication
#define TS_ACCESS_EXEC (0x1U << 4)          // execute (for RPC only)
#define TS_ACCESS_EXEC_AUTH (0x1U << 5)     // execute after authentication

/* ThingSet data object categories
 */
#define TS_CATEGORY_ALL 0
#define TS_CATEGORY_INFO 1        // read-only device information (e.g. manufacturer, etc.)
#define TS_CATEGORY_SETUP 2       // user-configurable settings (open access, maybe with user password)
#define TS_CATEGORY_INPUT 3       // free access
#define TS_CATEGORY_OUTPUT 4      // free access
#define TS_CATEGORY_RPC 5         // Remote Procedure Call
#define TS_CATEGORY_CAL 6         // factory-calibrated settings (access restricted)
#define TS_CATEGORY_DIAGNOSIS 7   // error memory, etc (at least partly access restricted)

static const char* const ts_categories[] = {
    "info",
    "setup",
    "input",
    "output",
    "rpc",
    "calibration",
    "diagnosis"
};

/* for CAN only...
 */
#define PUB_MULTIFRAME_EN (0x1U << 7)
#define PUB_TIMESTAMP_EN (0x1U << 6)



/* ThingSet data object struct
 * 
 * id = Data object ID
 * access = one of TS_ACCESS_READ, _WRITE, _EXECUTE, ...
 * type = one of TS_TYPE_INT32, _FLOAT, ...
 * detail = exponent (10^exponent = factor to convert to SI unit) for UINT / INT
 *          decimal digits to use for plotting of floats in JSON strings
 *          lenght of string buffer for string type
 */
typedef struct data_object_t {
    const uint16_t id;
    const uint8_t access;
    const uint8_t type;
    const int16_t detail;
    void *data;
    const char *name;
} data_object_t;

/* buffer for string-type data encoded as char array (necessary to use string functions without casts)
 */
typedef struct {
    //char *data;             // array containing data
    char data[TS_REQ_BUFFER_LEN];
    size_t size;            // size of the array
    size_t pos;             // index of the next free byte
} str_buffer_t;

/* buffer for binary data (unsigned on all platforms)
 */
typedef struct {
    uint8_t *data;          // array containing data
    size_t size;            // size of the array
    size_t pos;             // index of the next free byte
} bin_buffer_t;

typedef struct ts_data_t {
    const data_object_t *objects;
    size_t size;
} ts_data_t;

typedef struct {
    char *str;
    jcp_parser_t parser;
    jcptok_t tokens[TS_NUM_JSON_TOKENS];
    int tok_count;
} ts_parser_t;

/* Process ThingSet request
 *   - receives a request saved in req->data
 *   - performs an action (i.e. thingset function)
 *   - saves the response in resp->data
 *   - returns ThingSet status code
 * 
 * This function also detects if JSON or CBOR format is used
 */
void thingset_process(str_buffer_t *req, str_buffer_t *resp, ts_data_t *data);

/* ThingSet read function
 *   - appends requested data to resp buffer
 *   - returns ThingSet status code
 */
int thingset_read_json(ts_parser_t *parser, str_buffer_t *resp, ts_data_t *data);

int thingset_write_json(ts_parser_t *parser, str_buffer_t *resp, ts_data_t *data);

int thingset_list_json(ts_parser_t *parser, str_buffer_t *resp, ts_data_t *data);

int thingset_pub_json(str_buffer_t *resp, ts_data_t *data, uint16_t pub_list[], size_t list_len);

void thingset_status_message_json(str_buffer_t *resp, int code);

int thingset_read_cbor(bin_buffer_t *req, bin_buffer_t *resp, ts_data_t *data);
int thingset_write_cbor(bin_buffer_t *req, bin_buffer_t *resp, ts_data_t *data);
int thingset_list_cbor(bin_buffer_t *req, bin_buffer_t *resp, ts_data_t *data);

const data_object_t* thingset_data_object_by_id(ts_data_t *data, uint16_t id);

#ifdef __cplusplus
}
#endif

#endif
