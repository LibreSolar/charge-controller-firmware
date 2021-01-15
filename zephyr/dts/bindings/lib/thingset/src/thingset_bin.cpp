/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "ts_config.h"
#include "thingset.h"
#include "cbor.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>  // for definition of endianness
#include <math.h>       // for rounding of floats

int cbor_deserialize_array_type(uint8_t *buf, const DataNode *data_node);
int cbor_serialize_array_type(uint8_t *buf, size_t size, const DataNode *data_node);

static int cbor_deserialize_data_node(uint8_t *buf, const DataNode *data_node)
{
    switch (data_node->type) {
#if (TS_64BIT_TYPES_SUPPORT == 1)
    case TS_T_UINT64:
        return cbor_deserialize_uint64(buf, (uint64_t *)data_node->data);
    case TS_T_INT64:
        return cbor_deserialize_int64(buf, (int64_t *)data_node->data);
#endif
    case TS_T_UINT32:
        return cbor_deserialize_uint32(buf, (uint32_t *)data_node->data);
    case TS_T_INT32:
        return cbor_deserialize_int32(buf, (int32_t *)data_node->data);
    case TS_T_UINT16:
        return cbor_deserialize_uint16(buf, (uint16_t *)data_node->data);
    case TS_T_INT16:
        return cbor_deserialize_int16(buf, (int16_t *)data_node->data);
    case TS_T_FLOAT32:
        return cbor_deserialize_float(buf, (float *)data_node->data);
    case TS_T_BOOL:
        return cbor_deserialize_bool(buf, (bool *)data_node->data);
    case TS_T_STRING:
        return cbor_deserialize_string(buf, (char *)data_node->data, data_node->detail);
    case TS_T_ARRAY:
        return cbor_deserialize_array_type(buf, data_node);
    default:
        return 0;
    }
}

int cbor_deserialize_array_type(uint8_t *buf, const DataNode *data_node)
{
    uint16_t num_elements;
    int pos = 0; // Index of the next value in the buffer
    ArrayInfo *array_info;
    array_info = (ArrayInfo *)data_node->data;

    if (!array_info) {
        return 0;
    }

    // Deserialize the buffer length, and calculate the actual number of array elements
    pos = cbor_num_elements(buf, &num_elements);

    if (num_elements > array_info->max_elements) {
        return 0;
    }

    for (int i = 0; i < num_elements; i++) {
        switch (array_info->type) {
#if (TS_64BIT_TYPES_SUPPORT == 1)
        case TS_T_UINT64:
            pos += cbor_deserialize_uint64(&(buf[pos]), &(((uint64_t *)array_info->ptr)[i]));
            break;
        case TS_T_INT64:
            pos += cbor_deserialize_int64(&(buf[pos]), &(((int64_t *)array_info->ptr)[i]));
            break;
#endif
        case TS_T_UINT32:
            pos += cbor_deserialize_uint32(&(buf[pos]), &(((uint32_t *)array_info->ptr)[i]));
            break;
        case TS_T_INT32:
            pos += cbor_deserialize_int32(&(buf[pos]), &(((int32_t *)array_info->ptr)[i]));
            break;
        case TS_T_UINT16:
            pos += cbor_deserialize_uint16(&(buf[pos]), &(((uint16_t *)array_info->ptr)[i]));
            break;
        case TS_T_INT16:
            pos += cbor_deserialize_int16(&(buf[pos]), &(((int16_t *)array_info->ptr)[i]));
            break;
        case TS_T_FLOAT32:
            pos += cbor_deserialize_float(&(buf[pos]), &(((float *)array_info->ptr)[i]));
            break;
        default:
            break;
        }
    }
    return pos;
}

static int cbor_serialize_data_node(uint8_t *buf, size_t size, const DataNode *data_node)
{
    switch (data_node->type) {
#ifdef TS_64BIT_TYPES_SUPPORT
    case TS_T_UINT64:
        return cbor_serialize_uint(buf, *((uint64_t *)data_node->data), size);
    case TS_T_INT64:
        return cbor_serialize_int(buf, *((int64_t *)data_node->data), size);
#endif
    case TS_T_UINT32:
        return cbor_serialize_uint(buf, *((uint32_t *)data_node->data), size);
    case TS_T_INT32:
        return cbor_serialize_int(buf, *((int32_t *)data_node->data), size);
    case TS_T_UINT16:
        return cbor_serialize_uint(buf, *((uint16_t *)data_node->data), size);
    case TS_T_INT16:
        return cbor_serialize_int(buf, *((int16_t *)data_node->data), size);
    case TS_T_FLOAT32:
        if (data_node->detail == 0) { // round to 0 digits: use int
#ifdef TS_64BIT_TYPES_SUPPORT
            return cbor_serialize_int(buf, llroundf(*((float *)data_node->data)), size);
#else
            return cbor_serialize_int(buf, lroundf(*((float *)data_node->data)), size);
#endif
        }
        else {
            return cbor_serialize_float(buf, *((float *)data_node->data), size);
        }
    case TS_T_BOOL:
        return cbor_serialize_bool(buf, *((bool *)data_node->data), size);
    case TS_T_STRING:
        return cbor_serialize_string(buf, (char *)data_node->data, size);
    case TS_T_ARRAY:
        return cbor_serialize_array_type(buf, size, data_node);
    default:
        return 0;
    }
}

int cbor_serialize_array_type(uint8_t *buf, size_t size, const DataNode *data_node)
{
    int pos = 0; // Index of the next value in the buffer
    ArrayInfo *array_info;
    array_info = (ArrayInfo *)data_node->data;

    if (!array_info) {
        return 0;
    }

    // Add the length field to the beginning of the CBOR buffer and update the CBOR buffer index
    pos = cbor_serialize_array(buf, array_info->num_elements, size);

    for (int i = 0; i < array_info->num_elements; i++) {
        switch (array_info->type) {
#ifdef TS_64BIT_TYPES_SUPPORT
        case TS_T_UINT64:
            pos += cbor_serialize_uint(&(buf[pos]), ((uint64_t *)array_info->ptr)[i], size);
            break;
        case TS_T_INT64:
            pos += cbor_serialize_int(&(buf[pos]), ((int64_t *)array_info->ptr)[i], size);
            break;
#endif
        case TS_T_UINT32:
            pos += cbor_serialize_uint(&(buf[pos]), ((uint32_t *)array_info->ptr)[i], size);
            break;
        case TS_T_INT32:
            pos += cbor_serialize_int(&(buf[pos]), ((int32_t *)array_info->ptr)[i], size);
            break;
        case TS_T_UINT16:
            pos += cbor_serialize_uint(&(buf[pos]), ((uint16_t *)array_info->ptr)[i], size);
            break;
        case TS_T_INT16:
            pos += cbor_serialize_int(&(buf[pos]), ((int16_t *)array_info->ptr)[i], size);
            break;
        case TS_T_FLOAT32:
            if (data_node->detail == 0) { // round to 0 digits: use int
#ifdef TS_64BIT_TYPES_SUPPORT
                pos += cbor_serialize_int(&(buf[pos]),
                    llroundf(((float *)array_info->ptr)[i]), size);
#else
                pos += cbor_serialize_int(&(buf[pos]),
                    lroundf(((float *)array_info->ptr)[i]), size);
#endif
            }
            else {
                pos += cbor_serialize_float(&(buf[pos]), ((float *)array_info->ptr)[i], size);
            }
            break;
        default:
            break;
        }
    }
    return pos;
}

int ThingSet::bin_response(uint8_t code)
{
    if (resp_size > 0) {
        resp[0] = code;
        return 1;
    }
    else {
        return 0;
    }
}

int ThingSet::bin_process()
{
    int pos = 1;    // current position during data processing

    // get endpoint (first parameter of the request)
    const DataNode *endpoint = NULL;
    if ((req[pos] & CBOR_TYPE_MASK) == CBOR_TEXT) {
        uint16_t path_len;
        pos += cbor_num_elements(&req[pos], &path_len);
        endpoint = get_endpoint((char *)req + pos, path_len);
    }
    else if ((req[pos] & CBOR_TYPE_MASK) == CBOR_UINT) {
        node_id_t id = 0;
        pos += cbor_deserialize_uint16(&req[pos], &id);
        endpoint = get_node(id);
    }
    else if (req[pos] == CBOR_UNDEFINED) {
        pos++;
    }
    else {
        return bin_response(TS_STATUS_BAD_REQUEST);
    }

    // process data
    if (req[0] == TS_GET && endpoint) {
        return bin_get(endpoint, req[pos] == 0xA0, req[pos] == 0xF7);
    }
    else if (req[0] == TS_FETCH) {
        return bin_fetch(endpoint, pos);
    }
    else if (req[0] == TS_PATCH && endpoint) {
        int response = bin_patch(endpoint, pos, _auth_flags, 0);

        // check if endpoint has a callback assigned
        if (endpoint->data != NULL && resp[0] == TS_STATUS_CHANGED) {
            // create function pointer and call function
            void (*fun)(void) = reinterpret_cast<void(*)()>(endpoint->data);
            fun();
        }
        return response;
    }
    else if (req[0] == TS_POST) {
        return bin_exec(endpoint, pos);
    }
    return bin_response(TS_STATUS_BAD_REQUEST);
}

int ThingSet::bin_fetch(const DataNode *parent, unsigned int pos_payload)
{
    /*
     * Remark: the parent node is currently still ignored. Any found data object is fetched.
     */

    unsigned int pos_req = pos_payload;
    unsigned int pos_resp = 0;
    uint16_t num_elements, element = 0;

    pos_resp += bin_response(TS_STATUS_CONTENT);   // init response buffer

    pos_req += cbor_num_elements(&req[pos_req], &num_elements);
    if (num_elements != 1 && (req[pos_payload] & CBOR_TYPE_MASK) != CBOR_ARRAY) {
        return bin_response(TS_STATUS_BAD_REQUEST);
    }

    //printf("fetch request, elements: %d, hex data: %x %x %x %x %x %x %x %x\n", num_elements,
    //    req[pos_req], req[pos_req+1], req[pos_req+2], req[pos_req+3],
    //    req[pos_req+4], req[pos_req+5], req[pos_req+6], req[pos_req+7]);

    if (num_elements > 1) {
        pos_resp += cbor_serialize_array(&resp[pos_resp], num_elements, resp_size - pos_resp);
    }

    while (pos_req + 1 < req_len && element < num_elements) {

        size_t num_bytes = 0;       // temporary storage of cbor data length (req and resp)

        node_id_t id;
        num_bytes = cbor_deserialize_uint16(&req[pos_req], &id);
        if (num_bytes == 0) {
            return bin_response(TS_STATUS_BAD_REQUEST);
        }
        pos_req += num_bytes;

        const DataNode* data_node = get_node(id);
        if (data_node == NULL) {
            return bin_response(TS_STATUS_NOT_FOUND);
        }
        if (!(data_node->access & TS_READ_MASK)) {
            return bin_response(TS_STATUS_UNAUTHORIZED);
        }

        num_bytes = cbor_serialize_data_node(&resp[pos_resp], resp_size - pos_resp, data_node);
        if (num_bytes == 0) {
            return bin_response(TS_STATUS_RESPONSE_TOO_LARGE);
        }
        pos_resp += num_bytes;
        element++;
    }

    if (element == num_elements) {
        return pos_resp;
    }
    else {
        return bin_response(TS_STATUS_BAD_REQUEST);
    }
}

int ThingSet::bin_sub(uint8_t *cbor_data, size_t len, uint16_t auth_flags, uint16_t sub_ch)
{
    uint8_t resp_tmp[1] = {};   // only one character as response expected
    req = cbor_data;
    req_len = len;
    resp = resp_tmp;
    resp_size = sizeof(resp_tmp);
    bin_patch(NULL, 1, auth_flags, sub_ch);
    return resp[0];
}

int ThingSet::bin_patch(const DataNode *parent, unsigned int pos_payload, uint16_t auth_flags,
    uint16_t sub_ch)
{
    unsigned int pos_req = pos_payload;
    uint16_t num_elements, element = 0;

    if ((req[pos_req] & CBOR_TYPE_MASK) != CBOR_MAP) {
        return bin_response(TS_STATUS_BAD_REQUEST);
    }
    pos_req += cbor_num_elements(&req[pos_req], &num_elements);

    //printf("patch request, elements: %d, hex data: %x %x %x %x %x %x %x %x\n", num_elements,
    //    req[pos_req], req[pos_req+1], req[pos_req+2], req[pos_req+3],
    //    req[pos_req+4], req[pos_req+5], req[pos_req+6], req[pos_req+7]);

    while (pos_req < req_len && element < num_elements) {

        size_t num_bytes = 0;       // temporary storage of cbor data length (req and resp)

        node_id_t id;
        num_bytes = cbor_deserialize_uint16(&req[pos_req], &id);
        if (num_bytes == 0) {
            return bin_response(TS_STATUS_BAD_REQUEST);
        }
        pos_req += num_bytes;

        const DataNode* node = get_node(id);
        if (node) {
            if ((node->access & TS_WRITE_MASK & auth_flags) == 0) {
                if (node->access & TS_WRITE_MASK) {
                    return bin_response(TS_STATUS_UNAUTHORIZED);
                }
                else {
                    return bin_response(TS_STATUS_FORBIDDEN);
                }
            }
            else if (parent && node->parent != parent->id) {
                return bin_response(TS_STATUS_NOT_FOUND);
            }
            else if (sub_ch && !(node->pubsub & sub_ch)) {
                // ignore element
                num_bytes = cbor_size(&req[pos_req]);
            }
            else {
                // actually deserialize the data and update node
                num_bytes = cbor_deserialize_data_node(&req[pos_req], node);
            }
        }
        else {
            // node not found
            if (sub_ch) {
                // ignore element
                num_bytes = cbor_size(&req[pos_req]);
            }
            else {
                return bin_response(TS_STATUS_NOT_FOUND);
            }
        }

        if (num_bytes == 0) {
            return bin_response(TS_STATUS_BAD_REQUEST);
        }
        pos_req += num_bytes;

        element++;
    }

    if (element == num_elements) {
        return bin_response(TS_STATUS_CHANGED);
    } else {
        return bin_response(TS_STATUS_BAD_REQUEST);
    }
}

int ThingSet::bin_exec(const DataNode *node, unsigned int pos_payload)
{
    unsigned int pos_req = pos_payload;
    uint16_t num_elements, element = 0;

    if ((req[pos_req] & CBOR_TYPE_MASK) != CBOR_ARRAY) {
        return bin_response(TS_STATUS_BAD_REQUEST);
    }
    pos_req += cbor_num_elements(&req[pos_req], &num_elements);

    if ((node->access & TS_WRITE_MASK) && (node->type == TS_T_EXEC)) {
        // node is generally executable, but are we authorized?
        if ((node->access & TS_WRITE_MASK & _auth_flags) == 0) {
            return bin_response(TS_STATUS_UNAUTHORIZED);
        }
    }
    else {
        return bin_response(TS_STATUS_FORBIDDEN);
    }

    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].parent == node->id) {
            if (element >= num_elements) {
                // more child nodes found than parameters were passed
                return bin_response(TS_STATUS_BAD_REQUEST);
            }
            int num_bytes = cbor_deserialize_data_node(&req[pos_req], &data_nodes[i]);
            if (num_bytes == 0) {
                // deserializing the value was not successful
                return bin_response(TS_STATUS_UNSUPPORTED_FORMAT);
            }
            pos_req += num_bytes;
            element++;
        }
    }

    if (num_elements > element) {
        // more parameters passed than child nodes found
        return bin_response(TS_STATUS_BAD_REQUEST);
    }

    // if we got here, finally create function pointer and call function
    void (*fun)(void) = reinterpret_cast<void(*)()>(node->data);
    fun();

    return bin_response(TS_STATUS_VALID);
}

int ThingSet::bin_pub(uint8_t *buf, size_t buf_size, const uint16_t pub_ch)
{
    buf[0] = TS_PUBMSG;
    int len = 1;

    // find out number of elements to be published
    int num_ids = 0;
    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].pubsub & pub_ch) {
            num_ids++;
        }
    }

    len += cbor_serialize_map(&buf[len], num_ids, buf_size - len);

    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].pubsub & pub_ch) {
            len += cbor_serialize_uint(&buf[len], data_nodes[i].id, buf_size - len);
            size_t num_bytes = cbor_serialize_data_node(&buf[len], buf_size - len, &data_nodes[i]);
            if (num_bytes == 0) {
                return 0;
            }
            else {
                len += num_bytes;
            }
        }
    }
    return len;
}

int ThingSet::bin_pub_can(int &start_pos, uint16_t pub_ch, uint8_t can_dev_id,
    uint32_t &msg_id, uint8_t (&msg_data)[8])
{
    int msg_len = -1;
    const int msg_priority = 6;

    for (unsigned int i = start_pos; i < num_nodes; i++) {
        if (data_nodes[i].pubsub & pub_ch) {
            msg_id = msg_priority << 26
                | (1U << 24) | (1U << 25)   // identify as publication message
                | data_nodes[i].id << 8
                | can_dev_id;

            msg_len = cbor_serialize_data_node(msg_data, 8, &data_nodes[i]);

            if (msg_len > 0) {
                // node found and successfully encoded, increase start pos for next run
                start_pos = i + 1;
                break;
            }
            // else: data too long, take next node
        }
    }

    if (msg_len <= 0) {
        // no more nodes found, reset position
        start_pos = 0;
    }

    return msg_len;
}

/*
int ThingSet::name_cbor(void)
{
    resp[0] = TS_OBJ_NAME + 0x80;    // Function ID
    int data_node_id = _req[1] + ((int)_req[2] << 8);

    for (unsigned int i = 0; i < sizeof(data_nodes)/sizeof(DataNode); i++) {
        if (data_nodes[i].id == data_node_id) {
            if (data_nodes[i].access & ACCESS_READ) {
                resp[1] = T_STRING;
                int len = strlen(data_nodes[i].name);
                for (int j = 0; j < len; j++) {
                    resp[j+2] = *(data_nodes[i].name + j);
                }
                #if DEBUG
                serial.printf("Get Data Object Name: %s (id = %d)\n", data_nodes[i].name, data_node_id);
                #endif
                return len + 2;
            }
            else {
                resp[1] = TS_STATUS_UNAUTHORIZED;
                return 2;   // length of response
            }
        }
    }

    // data node not found --> send error message
    resp[1] = TS_STATUS_DATA_UNKNOWN;
    return 2;   // length of response
}
*/

int ThingSet::bin_get(const DataNode *parent, bool values, bool ids_only)
{
    unsigned int len = 0;       // current length of response
    len += bin_response(TS_STATUS_CONTENT);   // init response buffer

    // find out number of elements
    int num_elements = 0;
    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].access & TS_READ_MASK
            && (data_nodes[i].parent == parent->id))
        {
            num_elements++;
        }
    }

    if (values && !ids_only) {
        len += cbor_serialize_map(&resp[len], num_elements, resp_size - len);
    }
    else {
        len += cbor_serialize_array(&resp[len], num_elements, resp_size - len);
    }

    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].access & TS_READ_MASK
            && (data_nodes[i].parent == parent->id))
        {
            int num_bytes = 0;
            if (ids_only) {
                num_bytes = cbor_serialize_uint(&resp[len], data_nodes[i].id, resp_size - len);
            }
            else {
                num_bytes = cbor_serialize_string(&resp[len], data_nodes[i].name,
                    resp_size - len);
                if (values) {
                    num_bytes += cbor_serialize_data_node(&resp[len + num_bytes],
                        resp_size - len - num_bytes, &data_nodes[i]);
                }
            }

            if (num_bytes == 0) {
                return bin_response(TS_STATUS_RESPONSE_TOO_LARGE);
            } else {
                len += num_bytes;
            }
        }
    }

    return len;
}
