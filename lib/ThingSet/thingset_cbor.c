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

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>  // for definition of endianness

#include "cbor.h"
#include "ts_config.h"
#include "thingset.h"

const data_object_t* thingset_data_object_by_id(ts_data_t *data, uint16_t id) {
    for (unsigned int i = 0; i < data->size; i++) {
        if (data->objects[i].id == id) {
            return &(data->objects[i]);
        }
    }
    return NULL;
}

int _status_msg(bin_buffer_t *resp, uint8_t code)
{
    resp->data[0] = 0x80 + code;
    resp->pos = 1;
    return code;
}

int thingset_read_cbor(bin_buffer_t *req, bin_buffer_t *resp, ts_data_t *data)
{
    unsigned int pos = 1;       // ignore first byte for function code in request
    
    _status_msg(resp, TS_STATUS_SUCCESS);   // init response buffer

    while (pos + 1 < req->pos) {

        const data_object_t* data_obj = thingset_data_object_by_id(data,
            ((uint16_t)req->data[pos] << 8) + (uint16_t)req->data[pos+1]);

        if (data_obj == NULL) {
            return _status_msg(resp, TS_STATUS_UNKNOWN_DATA_OBJ);
        }

        if (!(data_obj->access & TS_ACCESS_READ)) {
            return _status_msg(resp, TS_STATUS_UNAUTHORIZED);
        }

        size_t num_bytes = 0;

        switch (data_obj->type) {
#ifdef TS_64BIT_TYPES_SUPPORT
            case TS_T_UINT64:
                num_bytes = cbor_serialize_int(&resp->data[resp->pos], *((uint64_t*)data_obj->data), TS_RESP_BUFFER_LEN - resp->pos);
                //printf("read INT32, id: 0x%x, value: %d, num_bytes: %d, max_len: %d\n", data_obj->id, *((int32_t*)data_obj->data), num_bytes, TS_RESP_BUFFER_LEN - resp->pos);
                break;
            case TS_T_INT64:
                num_bytes = cbor_serialize_int(&resp->data[resp->pos], *((int64_t*)data_obj->data), TS_RESP_BUFFER_LEN - resp->pos);
                //printf("read INT32, id: 0x%x, value: %d, num_bytes: %d, max_len: %d\n", data_obj->id, *((int32_t*)data_obj->data), num_bytes, TS_RESP_BUFFER_LEN - resp->pos);
                break;
#endif
            case TS_T_UINT32:
                num_bytes = cbor_serialize_int(&resp->data[resp->pos], *((uint32_t*)data_obj->data), TS_RESP_BUFFER_LEN - resp->pos);
                //printf("read INT32, id: 0x%x, value: %d, num_bytes: %d, max_len: %d\n", data_obj->id, *((int32_t*)data_obj->data), num_bytes, TS_RESP_BUFFER_LEN - resp->pos);
                break;
            case TS_T_INT32:
                num_bytes = cbor_serialize_int(&resp->data[resp->pos], *((int32_t*)data_obj->data), TS_RESP_BUFFER_LEN - resp->pos);
                //printf("read INT32, id: 0x%x, value: %d, num_bytes: %d, max_len: %d\n", data_obj->id, *((int32_t*)data_obj->data), num_bytes, TS_RESP_BUFFER_LEN - resp->pos);
                break;
            case TS_T_FLOAT32:
                num_bytes = cbor_serialize_float(&resp->data[resp->pos], *((float*)data_obj->data), TS_RESP_BUFFER_LEN - resp->pos);
                //printf("read FLOAT, id: 0x%x, value: %d, num_bytes: %d, max_len: %d\n", data_obj->id, *((int32_t*)data_obj->data), num_bytes, TS_RESP_BUFFER_LEN - resp->pos);
                break;
            case TS_T_BOOL:
                num_bytes = cbor_serialize_bool(&resp->data[resp->pos], *((bool*)data_obj->data), TS_RESP_BUFFER_LEN - resp->pos);
                break;
            case TS_T_STRING:
                num_bytes = cbor_serialize_string(&resp->data[resp->pos], (char*)data_obj->data, TS_RESP_BUFFER_LEN - resp->pos);
                //printf("read STRING, id: 0x%x, value: %s, num_bytes: %d, max_len: %d\n", data_obj->id, (char*)data_obj->data, num_bytes, TS_RESP_BUFFER_LEN - resp->pos);
                break;
        }

        if (num_bytes == 0) {
            return _status_msg(resp, TS_STATUS_RESPONSE_TOO_LONG);
        } else {
            resp->pos += num_bytes;
        }

        pos += 2;
    }

    return TS_STATUS_SUCCESS;
}

int thingset_write_cbor(bin_buffer_t *req, bin_buffer_t *resp, ts_data_t *data)
{
    unsigned int pos = 1;       // ignore first byte for function code in request

    //printf("write request, id: 0x%x, hex data: %x %x %x %x %x\n", ((uint16_t)req->data[1] << 8) + req->data[2], 
    //    req->data[3], req->data[4], req->data[5], req->data[6], req->data[7]);

    // loop through all elements to check if request is valid
    while (pos < req->pos) {

        const data_object_t* data_obj = thingset_data_object_by_id(data,
            ((uint16_t)req->data[pos] << 8) + (uint16_t)req->data[pos+1]);
        pos += 2;

        if (data_obj == NULL) {
            return _status_msg(resp, TS_STATUS_UNKNOWN_DATA_OBJ);
        }

        if (!(data_obj->access & TS_ACCESS_WRITE)) {
            return _status_msg(resp, TS_STATUS_UNAUTHORIZED);
        }

        switch (data_obj->type) {
#ifdef TS_64BIT_TYPES_SUPPORT
            case TS_T_UINT64:
                pos += cbor_deserialize_uint64(&req->data[pos], (uint64_t*)data_obj->data);
                break;
            case TS_T_INT64:
                pos += cbor_deserialize_int64(&req->data[pos], (int64_t*)data_obj->data);
                break;
#endif
            case TS_T_UINT32:
                pos += cbor_deserialize_uint32(&req->data[pos], (uint32_t*)data_obj->data);
                break;
            case TS_T_INT32:
                pos += cbor_deserialize_int32(&req->data[pos], (int32_t*)data_obj->data);
                break;
            case TS_T_FLOAT32:
                pos += cbor_deserialize_float(&req->data[pos], (float*)data_obj->data);
                break;
            case TS_T_BOOL:
                pos += cbor_deserialize_bool(&req->data[pos], (bool*)data_obj->data);
                break;
            case TS_T_STRING:
                pos += cbor_deserialize_string(&req->data[pos], (char*)data_obj->data, data_obj->detail);
                break;
            default:
                break;
        }
    }

    return _status_msg(resp, TS_STATUS_SUCCESS);
}


/*
int thingset_get_data_object_name(void)
{
    _resp->data[0] = TS_OBJ_NAME + 0x80;    // Function ID
    int data_obj_id = _req[1] + ((int)_req[2] << 8);

    for (unsigned int i = 0; i < sizeof(dataObjects)/sizeof(data_object_t); i++) {
        if (dataObjects[i].id == data_obj_id) {
            if (dataObjects[i].access & ACCESS_READ) {
                _resp->data[1] = T_STRING;
                int len = strlen(dataObjects[i].name);
                for (int j = 0; j < len; j++) {
                    _resp->data[j+2] = *(dataObjects[i].name + j);
                }
                #if DEBUG
                serial.printf("Get Data Object Name: %s (id = %d)\n", dataObjects[i].name, data_obj_id);
                #endif
                return len + 2;
            }
            else {
                _resp->data[1] = TS_STATUS_UNAUTHORIZED;
                return 2;   // length of response
            }
        }
    }

    // data object not found --> send error message
    _resp->data[1] = TS_STATUS_DATA_UNKNOWN;
    return 2;   // length of response
}


int thingset_list_cbor(bin_buffer_t *req, bin_buffer_t *resp, ts_data_t *data)
{
    _resp->data[0] = TS_LIST + 0x80;    // Function ID
    //_resp->data[1] = T_UINT16;
    int category = _req[1];
    int num_ids = 0;

    for (unsigned int i = 0; i < sizeof(dataObjects)/sizeof(data_object_t); i++) {
        if (dataObjects[i].access & ACCESS_READ
            && (dataObjects[i].category == category || category == TS_C_ALL))
        {
            if (num_ids * 2 < _resp_max_len - 4) {  // enough space left in buffer
                _resp->data[num_ids*2 + 2] = dataObjects[i].id & 0x00FF;
                _resp->data[num_ids*2 + 3] = dataObjects[i].id >> 8;
                num_ids++;
            }
            else {
                // not enough space in buffer
                _resp->data[1] = TS_STATUS_RESPONSE_TOO_LONG;
                return 2;
            }
        }
    }

    return num_ids*2 + 2;
}
*/