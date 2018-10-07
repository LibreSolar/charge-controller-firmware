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

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

const data_object_t* thingset_data_object_by_name(ts_data_t *data, char *str, size_t len) 
{
    for (unsigned int i = 0; i < data->size; i++) {
        if (strncmp(data->objects[i].name, str, len) == 0 
            && strlen(data->objects[i].name) == len) {  // otherwise e.g. foo and fooBar would be recognized as equal
            return &(data->objects[i]);
        }
    }
    return NULL;
}

void thingset_status_message_json(str_buffer_t *resp, int code)
{
#ifdef TS_VERBOSE_STATUS_MESSAGES
    switch(code) {
    case TS_STATUS_SUCCESS:
        sprintf(resp->data, ":%d Success.", code);
        break;
    case TS_STATUS_UNKNOWN_FUNCTION: // 31
        sprintf(resp->data, ":%d Unknown function.", code);
        break;
    case TS_STATUS_UNKNOWN_DATA_OBJ: // 32
        sprintf(resp->data, ":%d Data object not found.", code);
        break;
    case TS_STATUS_WRONG_FORMAT: // 33
        sprintf(resp->data, ":%d Wrong format.", code);
        break;
    case TS_STATUS_WRONG_TYPE: // 34
        sprintf(resp->data, ":%d Data type not supported.", code);
        break;
    case TS_STATUS_DEVICE_BUSY:
        sprintf(resp->data, ":%d Device busy.", code);
        break;
    case TS_STATUS_UNAUTHORIZED:
        sprintf(resp->data, ":%d Unauthorized.", code);
        break;
    case TS_STATUS_REQUEST_TOO_LONG:
        sprintf(resp->data, ":%d Request too long.", code);
        break;
    case TS_STATUS_RESPONSE_TOO_LONG:
        sprintf(resp->data, ":%d Response too long.", code);
        break;
    case TS_STATUS_INVALID_VALUE:
        sprintf(resp->data, ":%d Invalid or too large value.", code);
        break;
    default:
        sprintf(resp->data, ":%d Error.", code);
        break;
    };
#else
    sprintf(resp->data, ":%d.", code);
#endif
    resp->pos = strlen(resp->data);
}

int thingset_read_json(ts_parser_t *parser, str_buffer_t *resp, ts_data_t *data)
{
    int tok = 0;       // current token

    // TODO: throw error if tok_count = JCP_ERROR_NOMEM

    // initialize response with success message
    thingset_status_message_json(resp, TS_STATUS_SUCCESS);

    if (parser->tokens[0].type == JCP_ARRAY) {
        resp->pos += sprintf(&resp->data[resp->pos], " [");
        tok++;
    } else {
        resp->pos += sprintf(&resp->data[resp->pos], " ");
    }

    while (tok < parser->tok_count) {

        if (parser->tokens[tok].type != JCP_STRING) {
            thingset_status_message_json(resp, TS_STATUS_WRONG_FORMAT);
            return TS_STATUS_WRONG_FORMAT;
        }

        const data_object_t *data_obj = thingset_data_object_by_name(data,
            parser->str + parser->tokens[tok].start, 
            parser->tokens[tok].end - parser->tokens[tok].start);

        if (data_obj == NULL) {
            thingset_status_message_json(resp, TS_STATUS_UNKNOWN_DATA_OBJ);
            return TS_STATUS_UNKNOWN_DATA_OBJ;
        }

        if (!(data_obj->access & TS_ACCESS_READ)) {
            thingset_status_message_json(resp, TS_STATUS_UNAUTHORIZED);
            return TS_STATUS_UNAUTHORIZED;
        }

        switch (data_obj->type) {
        case TS_T_FLOAT32:
            resp->pos += snprintf(&resp->data[resp->pos], TS_RESP_BUFFER_LEN - resp->pos, 
                "%.*f, ", data_obj->detail, *((float*)data_obj->data));
            break;
        case TS_T_UINT64:
            resp->pos += snprintf(&resp->data[resp->pos], TS_RESP_BUFFER_LEN - resp->pos, 
                "%" PRIu64 ", ", *((uint64_t*)data_obj->data));
            break;
        case TS_T_INT64:
            resp->pos += snprintf(&resp->data[resp->pos], TS_RESP_BUFFER_LEN - resp->pos, 
                "%" PRIi64 ", ", *((int64_t*)data_obj->data));
            break;
        case TS_T_UINT32:
        case TS_T_INT32:
        case TS_T_UINT16:
        case TS_T_INT16:
            resp->pos += snprintf(&resp->data[resp->pos], TS_RESP_BUFFER_LEN - resp->pos, 
                "%d, ", *((int*)data_obj->data));
            break;
        case TS_T_BOOL:
            resp->pos += snprintf(&resp->data[resp->pos], TS_RESP_BUFFER_LEN - resp->pos, 
                "%s, ", (*((bool*)data_obj->data) == true ? "true" : "false"));
            break;
        case TS_T_STRING:
            resp->pos += snprintf(&resp->data[resp->pos], TS_RESP_BUFFER_LEN - resp->pos, 
                "\"%s\", ", (char*)data_obj->data);
            break;
        }

        if (resp->pos >= TS_RESP_BUFFER_LEN - 2) {
            thingset_status_message_json(resp, TS_STATUS_RESPONSE_TOO_LONG);
            return TS_STATUS_RESPONSE_TOO_LONG;
        }
        tok++;
    }

    resp->pos -= 2;  // remove trailing comma and blank
    if (parser->tokens[0].type == JCP_ARRAY) {
        resp->pos += sprintf(&resp->data[resp->pos], "]");
    } else {
        resp->data[resp->pos] = '\0';    // terminate string
    }

    return TS_STATUS_SUCCESS;
}

int thingset_write_json(ts_parser_t *parser, str_buffer_t *resp, ts_data_t *data)
{
    int tok = 0;        // current token
    char buf[21];       // buffer for data object value (largest negative 64bit integer has 20 digits)
    size_t value_len;   // length of value in buffer

    if (parser->tok_count < 2) {
        if (parser->tok_count == JCP_ERROR_NOMEM) {
            thingset_status_message_json(resp, TS_STATUS_WRONG_FORMAT);
            return TS_STATUS_REQUEST_TOO_LONG;
        } else {
            thingset_status_message_json(resp, TS_STATUS_WRONG_FORMAT);
            return TS_STATUS_WRONG_FORMAT;
        }
    }

    if (parser->tokens[0].type == JCP_MAP) {
        tok++;
    }

    // loop through all elements to check if request is valid
    while (tok + 1 < parser->tok_count) {

        if (parser->tokens[tok].type != JCP_STRING || 
            (parser->tokens[tok+1].type != JCP_PRIMITIVE && parser->tokens[tok+1].type != JCP_STRING)) {
            thingset_status_message_json(resp, TS_STATUS_WRONG_FORMAT);
            return TS_STATUS_WRONG_FORMAT;
        }

        const data_object_t* data_obj = thingset_data_object_by_name(data,
            parser->str + parser->tokens[tok].start, 
            parser->tokens[tok].end - parser->tokens[tok].start);

        if (data_obj == NULL) {
            thingset_status_message_json(resp, TS_STATUS_UNKNOWN_DATA_OBJ);
            return TS_STATUS_UNKNOWN_DATA_OBJ;
        }

        if (!(data_obj->access & TS_ACCESS_WRITE)) {
            thingset_status_message_json(resp, TS_STATUS_UNAUTHORIZED);
            return TS_STATUS_UNAUTHORIZED;
        }

        // extract the value and check buffer lengths
        value_len = parser->tokens[tok+1].end - parser->tokens[tok+1].start;
        if (value_len >= sizeof(buf) ||
            (data_obj->type == TS_T_STRING && value_len >= (size_t)data_obj->detail)) {
            thingset_status_message_json(resp, TS_STATUS_INVALID_VALUE);
            return TS_STATUS_INVALID_VALUE;
        } else {
            strncpy(buf, &parser->str[parser->tokens[tok+1].start], value_len);
            buf[value_len] = '\0';
        }

        errno = 0;
        float tmp_f;
        int tmp_i;
        switch (data_obj->type) {
            case TS_T_STRING:
                if (parser->tokens[tok+1].type != JCP_STRING) {
                    thingset_status_message_json(resp, TS_STATUS_WRONG_TYPE);
                    return TS_STATUS_WRONG_TYPE;
                }
                // data object buffer buffer length already checked above
                break;
            case TS_T_FLOAT32:
                if (parser->tokens[tok+1].type != JCP_PRIMITIVE) {
                    thingset_status_message_json(resp, TS_STATUS_WRONG_TYPE);
                    return TS_STATUS_WRONG_TYPE;
                }
                tmp_f = strtod(buf, NULL);
                if (errno == ERANGE) {
                    thingset_status_message_json(resp, TS_STATUS_INVALID_VALUE);
                    return TS_STATUS_INVALID_VALUE;
                }
                break;
            case TS_T_INT64:
                // TODO
                break;
            case TS_T_INT32:
                if (parser->tokens[tok+1].type != JCP_PRIMITIVE) {
                    thingset_status_message_json(resp, TS_STATUS_WRONG_TYPE);
                    return TS_STATUS_WRONG_TYPE;
                }
                tmp_i = strtol(buf, NULL, 0);
                if (errno == ERANGE) {
                    thingset_status_message_json(resp, TS_STATUS_INVALID_VALUE);
                    return TS_STATUS_INVALID_VALUE;
                }
                break;
            case TS_T_BOOL:
                if (!(buf[0] == 't' || buf[0] == '1' || buf[0] == 'f' || buf[0] == '0')) {
                    thingset_status_message_json(resp, TS_STATUS_WRONG_TYPE);
                    return TS_STATUS_WRONG_TYPE;
                }
                break;
            default:
                thingset_status_message_json(resp, TS_STATUS_WRONG_TYPE);
                return TS_STATUS_WRONG_TYPE;
                break;
        }
        tok += 2;   // map expected --> always one string + one value
    }

    if (parser->tokens[0].type == JCP_MAP)
        tok = 1;
    else
        tok = 0;

    // actually write data
    while (tok + 1 < parser->tok_count) {

        const data_object_t *data_obj = thingset_data_object_by_name(data, 
            parser->str + parser->tokens[tok].start, 
            parser->tokens[tok].end - parser->tokens[tok].start);

        // extract the value again (max. size was checked before)
        value_len = parser->tokens[tok+1].end - parser->tokens[tok+1].start;
        if (value_len < sizeof(buf)) {
            strncpy(buf, &parser->str[parser->tokens[tok+1].start], value_len);
            buf[value_len] = '\0';
        }

        switch (data_obj->type) {
        case TS_T_FLOAT32:
            *((float*)data_obj->data) = strtod(buf, NULL);
            break;
        case TS_T_UINT64:
            *((uint64_t*)data_obj->data) = strtoull(buf, NULL, 0);
            break;
        case TS_T_INT64:
            //printf("%s\n", buf);
            *((int64_t*)data_obj->data) = strtoll(buf, NULL, 0);
            break;
        case TS_T_UINT32:
            *((uint32_t*)data_obj->data) = strtol(buf, NULL, 0);
            break;
        case TS_T_INT32:
            *((int32_t*)data_obj->data) = strtoul(buf, NULL, 0);
            break;
        case TS_T_BOOL:
            if (buf[0] == 't' || buf[0] == '1') {
                *((bool*)data_obj->data) = true;
            }
            else if (buf[0] == 'f' || buf[0] == '0') {
                *((bool*)data_obj->data) = false;
            }
            break;
        case TS_T_STRING:
            strncpy((char*)data_obj->data, &parser->str[parser->tokens[tok+1].start], value_len);
            ((char*)data_obj->data)[value_len] = '\0';
            break;
        }
        tok += 2;   // map expected --> always one string + one value
    }

    thingset_status_message_json(resp, TS_STATUS_SUCCESS);
    return TS_STATUS_SUCCESS;
}

int thingset_list_json(ts_parser_t *parser, str_buffer_t *resp, ts_data_t *data)
{
    uint16_t mask = 0;

    // initialize response with success message
    thingset_status_message_json(resp, TS_STATUS_SUCCESS);

    if (parser->tok_count == 0) {
        mask = 0;
    }
    else if (parser->tok_count == 1 && parser->tokens[0].type == JCP_STRING) {
        for (uint8_t i = 0; i < sizeof(ts_categories)/sizeof(ts_categories[0]); i++) {
            if (strncmp(ts_categories[i], &parser->str[parser->tokens[0].start],
                parser->tokens[0].end - parser->tokens[0].start) == 0)
            {
                // array starts at 0 for category ID 1 --> increment i by one to get ID
                mask = (i + 1) << 12;
                break;
            }
        }
    }
    else {
        thingset_status_message_json(resp, TS_STATUS_WRONG_FORMAT);
        return TS_STATUS_WRONG_FORMAT;
    }

    resp->pos += sprintf(&resp->data[resp->pos], " [");

    for (unsigned int i = 0; i < data->size; i++) {
        if ((data->objects[i].access & TS_ACCESS_READ) &&
            ((data->objects[i].id & mask) == mask))
        {
            resp->pos += snprintf(&resp->data[resp->pos],
                TS_RESP_BUFFER_LEN - resp->pos,
                "\"%s\", ", data->objects[i].name);
    
            if (resp->pos >= TS_RESP_BUFFER_LEN - 2) {
                thingset_status_message_json(resp, TS_STATUS_RESPONSE_TOO_LONG);
                return TS_STATUS_RESPONSE_TOO_LONG;
            }
        }
    }

    // remove trailing comma and add closing bracket
    sprintf(&resp->data[resp->pos-2], "]");
    resp->pos--;

    return TS_STATUS_SUCCESS;
}

int thingset_pub_json(str_buffer_t *resp, ts_data_t *data, uint16_t pub_list[], size_t list_len)
{
    resp->pos = sprintf(resp->data, "# {");
 
    for (unsigned int i = 0; i < list_len; ++i) {

        const data_object_t* data_obj = thingset_data_object_by_id(data, pub_list[i]);

        switch (data_obj->type) {
        case TS_T_FLOAT32:
            resp->pos += snprintf(&resp->data[resp->pos], resp->size - resp->pos, 
                "\"%s\":%.*f, ", data_obj->name, data_obj->detail, *((float*)data_obj->data));
            break;
        case TS_T_STRING:
            resp->pos += snprintf(&resp->data[resp->pos], resp->size - resp->pos, 
                "\"%s\":\"%s\", ", data_obj->name, (char*)data_obj->data);
            break;
        case TS_T_INT32:
            resp->pos += snprintf(&resp->data[resp->pos], resp->size - resp->pos, 
                "\"%s\":%d, ", data_obj->name, *((int*)data_obj->data));
            break;
        case TS_T_BOOL:
            resp->pos += snprintf(&resp->data[resp->pos], resp->size - resp->pos, 
                "\"%s\":%s, ", data_obj->name, (*((bool*)data_obj->data) == true ? "true" : "false"));
            break;
        }
        if (resp->pos >= resp->size - 2) {
            return TS_STATUS_RESPONSE_TOO_LONG;
        }
    }
    sprintf(&resp->data[resp->pos-2], "}");    // overwrite comma + blank

    return TS_STATUS_SUCCESS;
}
