/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "ts_config.h"
#include "thingset.h"
#include "jsmn.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <cinttypes>


int ThingSet::txt_response(int code)
{
    size_t pos = 0;
#ifdef TS_VERBOSE_STATUS_MESSAGES
    switch(code) {
        // success
        case TS_STATUS_CREATED:
            pos = snprintf((char *)resp, resp_size, ":%.2X Created.", code);
            break;
        case TS_STATUS_DELETED:
            pos = snprintf((char *)resp, resp_size, ":%.2X Deleted.", code);
            break;
        case TS_STATUS_VALID:
            pos = snprintf((char *)resp, resp_size, ":%.2X Valid.", code);
            break;
        case TS_STATUS_CHANGED:
            pos = snprintf((char *)resp, resp_size, ":%.2X Changed.", code);
            break;
        case TS_STATUS_CONTENT:
            pos = snprintf((char *)resp, resp_size, ":%.2X Content.", code);
            break;
        // client errors
        case TS_STATUS_BAD_REQUEST:
            pos = snprintf((char *)resp, resp_size, ":%.2X Bad Request.", code);
            break;
        case TS_STATUS_UNAUTHORIZED:
            pos = snprintf((char *)resp, resp_size, ":%.2X Unauthorized.", code);
            break;
        case TS_STATUS_FORBIDDEN:
            pos = snprintf((char *)resp, resp_size, ":%.2X Forbidden.", code);
            break;
        case TS_STATUS_NOT_FOUND:
            pos = snprintf((char *)resp, resp_size, ":%.2X Not Found.", code);
            break;
        case TS_STATUS_METHOD_NOT_ALLOWED:
            pos = snprintf((char *)resp, resp_size, ":%.2X Method Not Allowed.", code);
            break;
        case TS_STATUS_REQUEST_INCOMPLETE:
            pos = snprintf((char *)resp, resp_size, ":%.2X Request Entity Incomplete.", code);
            break;
        case TS_STATUS_CONFLICT:
            pos = snprintf((char *)resp, resp_size, ":%.2X Conflict.", code);
            break;
        case TS_STATUS_REQUEST_TOO_LARGE:
            pos = snprintf((char *)resp, resp_size, ":%.2X Request Entity Too Large.", code);
            break;
        case TS_STATUS_UNSUPPORTED_FORMAT:
            pos = snprintf((char *)resp, resp_size, ":%.2X Unsupported Content-Format.", code);
            break;
        // server errors
        case TS_STATUS_INTERNAL_SERVER_ERR:
            pos = snprintf((char *)resp, resp_size, ":%.2X Internal Server Error.", code);
            break;
        case TS_STATUS_NOT_IMPLEMENTED:
            pos = snprintf((char *)resp, resp_size, ":%.2X Not Implemented.", code);
            break;
        default:
            pos = snprintf((char *)resp, resp_size, ":%.2X Error.", code);
            break;
    };
#else
    pos = snprintf((char *)resp, resp_size, ":%.2X.", code);
#endif
    if (pos < resp_size)
        return pos;
    else
        return 0;
}

int ThingSet::json_serialize_value(char *buf, size_t size, const DataNode *node)
{
    size_t pos = 0;
    const DataNode *sub_node;

    switch (node->type) {
#ifdef TS_64BIT_TYPES_SUPPORT
    case TS_T_UINT64:
        pos = snprintf(&buf[pos], size - pos, "%" PRIu64 ",", *((uint64_t *)node->data));
        break;
    case TS_T_INT64:
        pos = snprintf(&buf[pos], size - pos, "%" PRIi64 ",", *((int64_t *)node->data));
        break;
#endif
    case TS_T_UINT32:
        pos = snprintf(&buf[pos], size - pos, "%" PRIu32 ",", *((uint32_t *)node->data));
        break;
    case TS_T_INT32:
        pos = snprintf(&buf[pos], size - pos, "%" PRIi32 ",", *((int32_t *)node->data));
        break;
    case TS_T_UINT16:
        pos = snprintf(&buf[pos], size - pos, "%" PRIu16 ",", *((uint16_t *)node->data));
        break;
    case TS_T_INT16:
        pos = snprintf(&buf[pos], size - pos, "%" PRIi16 ",", *((int16_t *)node->data));
        break;
    case TS_T_FLOAT32:
        pos = snprintf(&buf[pos], size - pos, "%.*f,", node->detail,
                *((float *)node->data));
        break;
    case TS_T_BOOL:
        pos = snprintf(&buf[pos], size - pos, "%s,",
                (*((bool *)node->data) == true ? "true" : "false"));
        break;
    case TS_T_EXEC:
        pos = snprintf(&buf[pos], size - pos, "null,");
        break;
    case TS_T_STRING:
        pos = snprintf(&buf[pos], size - pos, "\"%s\",", (char *)node->data);
        break;
    case TS_T_PUBSUB:
        pos = snprintf(&buf[pos], size - pos, "[]") - 1;
        for (unsigned int i = 0; i < num_nodes; i++) {
            if (data_nodes[i].pubsub & (uint16_t)node->detail) {
                pos += snprintf(&buf[pos], size - pos, "\"%s\",", data_nodes[i].name);
            }
        }
        pos--; // remove trailing comma
        pos += snprintf(&buf[pos], size - pos, "],");
        break;
    case TS_T_ARRAY:
        ArrayInfo *array_info = (ArrayInfo *)node->data;
        if (!array_info) {
            return 0;
        }
        pos += snprintf(&buf[pos], size - pos, "[");
        for (int i = 0; i < array_info->num_elements; i++) {
            switch (array_info->type) {
            case TS_T_UINT64:
                pos += snprintf(&buf[pos], size - pos, "%" PRIu64 ",",
                        ((uint64_t *)array_info->ptr)[i]);
                break;
            case TS_T_INT64:
                pos += snprintf(&buf[pos], size - pos, "%" PRIi64 ",",
                        ((int64_t *)array_info->ptr)[i]);
                break;
            case TS_T_UINT32:
                pos += snprintf(&buf[pos], size - pos, "%" PRIu32 ",",
                        ((uint32_t *)array_info->ptr)[i]);
                break;
            case TS_T_INT32:
                pos += snprintf(&buf[pos], size - pos, "%" PRIi32 ",",
                        ((int32_t *)array_info->ptr)[i]);
                break;
            case TS_T_UINT16:
                pos += snprintf(&buf[pos], size - pos, "%" PRIu16 ",",
                        ((uint16_t *)array_info->ptr)[i]);
                break;
            case TS_T_INT16:
                pos += snprintf(&buf[pos], size - pos, "%" PRIi16 ",",
                        ((int16_t *)array_info->ptr)[i]);
                break;
            case TS_T_FLOAT32:
                pos += snprintf(&buf[pos], size - pos, "%.*f,", node->detail,
                        ((float *)array_info->ptr)[i]);
                break;
            case TS_T_NODE_ID:
                sub_node = get_node(((uint16_t *)array_info->ptr)[i]);
                if (sub_node) {
                    pos += snprintf(&buf[pos], size - pos, "\"%s\",", sub_node->name);
                }
                break;
            default:
                break;
            }
        }
        if (array_info->num_elements > 0) {
            pos--; // remove trailing comma
        }
        pos += snprintf(&buf[pos], size - pos, "],");
        break;
    }

    if (pos < size) {
        return pos;
    }
    else {
        return 0;
    }
}

int ThingSet::json_serialize_name_value(char *buf, size_t size, const DataNode* node)
{
    size_t pos = snprintf(buf, size, "\"%s\":", node->name);

    if (pos < size) {
        return pos + json_serialize_value(&buf[pos], size - pos, node);
    }
    else {
        return 0;
    }
}

void ThingSet::dump_json(node_id_t node_id, int level)
{
    uint8_t buf[100];
    bool first = true;
    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].parent == node_id) {
            if (!first) {
                printf(",\n");
            }
            else {
                printf("\n");
                first = false;
            }
            if (data_nodes[i].type == TS_T_PATH) {
                printf("%*s\"%s\" {", 4 * level, "", data_nodes[i].name);
                dump_json(data_nodes[i].id, level + 1);
                printf("\n%*s}", 4 * level, "");
            }
            else {
                int pos = json_serialize_name_value((char *)buf, sizeof(buf), &data_nodes[i]);
                if (pos > 0) {
                    buf[pos-1] = '\0';  // remove trailing comma
                    printf("%*s%s", 4 * level, "", (char *)buf);
                }
            }
        }
    }
    if (node_id == 0) {
        printf("\n");
    }
}

int ThingSet::txt_process()
{
    int path_len = req_len - 1;
    char *path_end = strchr((char *)req + 1, ' ');
    if (path_end) {
        path_len = (uint8_t *)path_end - req - 1;
    }

    const DataNode *endpoint = get_endpoint((char *)req + 1, path_len);
    if (!endpoint) {
        if (req[0] == '?' && req[1] == '/' && path_len == 1) {
            return txt_get(NULL, false);
        }
        else {
            return txt_response(TS_STATUS_NOT_FOUND);
        }
    }

    jsmn_parser parser;
    jsmn_init(&parser);

    json_str = (char *)req + 1 + path_len;
    tok_count = jsmn_parse(&parser, json_str, req_len - path_len - 1, tokens, sizeof(tokens));

    if (tok_count == JSMN_ERROR_NOMEM) {
        return txt_response(TS_STATUS_REQUEST_TOO_LARGE);
    }
    else if (tok_count < 0) {
        // other parsing error
        return txt_response(TS_STATUS_BAD_REQUEST);
    }
    else if (tok_count == 0) {
        if (req[0] == '?') {
            // no payload data
            if ((char)req[path_len] == '/') {
                if (endpoint->type == TS_T_PATH || endpoint->type == TS_T_EXEC) {
                    return txt_get(endpoint, false);
                }
                else {
                    // device discovery is only allowed for internal nodes
                    return txt_response(TS_STATUS_BAD_REQUEST);
                }
            }
            else {
                return txt_get(endpoint, true);
            }
        }
        else if (req[0] == '!') {
            return txt_exec(endpoint);
        }
    }
    else {
        if (req[0] == '?') {
            return txt_fetch(endpoint->id);
        }
        else if (req[0] == '=') {
            int len = txt_patch(endpoint->id);

            // check if endpoint has a callback assigned
            if (endpoint->data != NULL && strncmp((char *)resp, ":84", 3) == 0) {
                // create function pointer and call function
                void (*fun)(void) = reinterpret_cast<void(*)()>(endpoint->data);
                fun();
            }
            return len;
        }
        else if (req[0] == '!' && endpoint->type == TS_T_EXEC) {
            return txt_exec(endpoint);
        }
        else if (req[0] == '+') {
            return txt_create(endpoint);
        }
        else if (req[0] == '-') {
            return txt_delete(endpoint);
        }
    }
    return txt_response(TS_STATUS_BAD_REQUEST);
}

int ThingSet::txt_fetch(node_id_t parent_id)
{
    size_t pos = 0;
    int tok = 0;       // current token

    // initialize response with success message
    pos += txt_response(TS_STATUS_CONTENT);

    if (tokens[0].type == JSMN_ARRAY) {
        pos += snprintf((char *)&resp[pos], resp_size - pos, " [");
        tok++;
    } else {
        pos += snprintf((char *)&resp[pos], resp_size - pos, " ");
    }

    while (tok < tok_count) {

        if (tokens[tok].type != JSMN_STRING) {
            return txt_response(TS_STATUS_BAD_REQUEST);
        }

        const DataNode *node = get_node(
            json_str + tokens[tok].start,
            tokens[tok].end - tokens[tok].start, parent_id);

        if (node == NULL) {
            return txt_response(TS_STATUS_NOT_FOUND);
        }
        else if (node->type == TS_T_PATH) {
            // bad request, as we can't read internal path node's values
            return txt_response(TS_STATUS_BAD_REQUEST);
        }

        if ((node->access & TS_READ_MASK & _auth_flags) == 0) {
            if (node->access & TS_READ_MASK) {
                return txt_response(TS_STATUS_UNAUTHORIZED);
            }
            else {
                return txt_response(TS_STATUS_FORBIDDEN);
            }
        }

        pos += json_serialize_value((char *)&resp[pos], resp_size - pos, node);

        if (pos >= resp_size - 2) {
            return txt_response(TS_STATUS_RESPONSE_TOO_LARGE);
        }
        tok++;
    }

    pos--;  // remove trailing comma
    if (tokens[0].type == JSMN_ARRAY) {
        // buffer will be long enough as we dropped last 2 characters --> sprintf allowed
        pos += sprintf((char *)&resp[pos], "]");
    } else {
        resp[pos] = '\0';    // terminate string
    }

    return pos;
}

int ThingSet::json_deserialize_value(char *buf, size_t len, jsmntype_t type, const DataNode *node)
{
    if (type != JSMN_PRIMITIVE && type != JSMN_STRING) {
        return 0;
    }

    errno = 0;
    switch (node->type) {
        case TS_T_FLOAT32:
            *((float*)node->data) = strtod(buf, NULL);
            break;
        case TS_T_UINT64:
            *((uint64_t*)node->data) = strtoull(buf, NULL, 0);
            break;
        case TS_T_INT64:
            *((int64_t*)node->data) = strtoll(buf, NULL, 0);
            break;
        case TS_T_UINT32:
            *((uint32_t*)node->data) = strtoul(buf, NULL, 0);
            break;
        case TS_T_INT32:
            *((int32_t*)node->data) = strtol(buf, NULL, 0);
            break;
        case TS_T_UINT16:
            *((uint16_t*)node->data) = strtoul(buf, NULL, 0);
            break;
        case TS_T_INT16:
            *((uint16_t*)node->data) = strtol(buf, NULL, 0);
            break;
        case TS_T_BOOL:
            if (buf[0] == 't' || buf[0] == '1') {
                *((bool*)node->data) = true;
            }
            else if (buf[0] == 'f' || buf[0] == '0') {
                *((bool*)node->data) = false;
            }
            else {
                return 0;       // error
            }
            break;
        case TS_T_STRING:
            if (type != JSMN_STRING || (unsigned int)node->detail <= len) {
                return 0;
            }
            else if (node->id != 0) {     // dummy node has id = 0
                strncpy((char*)node->data, buf, len);
                ((char*)node->data)[len] = '\0';
            }
            break;
    }

    if (errno == ERANGE) {
        return 0;
    }

    return 1;   // value always contained in one token (arrays not yet supported)
}

int ThingSet::txt_patch(node_id_t parent_id)
{
    int tok = 0;       // current token

    // buffer for data node value (largest negative 64bit integer has 20 digits)
    char value_buf[21];
    size_t value_len;   // length of value in buffer

    if (tok_count < 2) {
        if (tok_count == JSMN_ERROR_NOMEM) {
            return txt_response(TS_STATUS_REQUEST_TOO_LARGE);
        } else {
            return txt_response(TS_STATUS_BAD_REQUEST);
        }
    }

    if (tokens[0].type == JSMN_OBJECT) {    // object = map
        tok++;
    }

    // loop through all elements to check if request is valid
    while (tok + 1 < tok_count) {

        if (tokens[tok].type != JSMN_STRING ||
            (tokens[tok+1].type != JSMN_PRIMITIVE && tokens[tok+1].type != JSMN_STRING)) {
            return txt_response(TS_STATUS_BAD_REQUEST);
        }

        const DataNode* node = get_node(
            json_str + tokens[tok].start,
            tokens[tok].end - tokens[tok].start, parent_id);

        if (node == NULL) {
            return txt_response(TS_STATUS_NOT_FOUND);
        }

        if ((node->access & TS_WRITE_MASK & _auth_flags) == 0) {
            if (node->access & TS_WRITE_MASK) {
                return txt_response(TS_STATUS_UNAUTHORIZED);
            }
            else {
                return txt_response(TS_STATUS_FORBIDDEN);
            }
        }

        tok++;

        // extract the value and check buffer lengths
        value_len = tokens[tok].end - tokens[tok].start;
        if ((node->type != TS_T_STRING && value_len >= sizeof(value_buf)) ||
            (node->type == TS_T_STRING && value_len >= (size_t)node->detail))
        {
            return txt_response(TS_STATUS_UNSUPPORTED_FORMAT);
        }
        else {
            strncpy(value_buf, &json_str[tokens[tok].start], value_len);
            value_buf[value_len] = '\0';
        }

        // create dummy node to test formats
        uint8_t dummy_data[8];          // enough to fit also 64-bit values
        DataNode dummy_node = {0, 0, "Dummy", (void *)dummy_data, node->type, node->detail};

        int res = json_deserialize_value(value_buf, value_len, tokens[tok].type, &dummy_node);
        if (res == 0) {
            return txt_response(TS_STATUS_UNSUPPORTED_FORMAT);
        }
        tok += res;
    }

    if (tokens[0].type == JSMN_OBJECT) {
        tok = 1;
    }
    else {
        tok = 0;
    }

    // actually write data
    while (tok + 1 < tok_count) {

        const DataNode *node = get_node(json_str + tokens[tok].start,
            tokens[tok].end - tokens[tok].start, parent_id);

        tok++;

        // extract the value again (max. size was checked before)
        value_len = tokens[tok].end - tokens[tok].start;
        if (value_len < sizeof(value_buf)) {
            strncpy(value_buf, &json_str[tokens[tok].start], value_len);
            value_buf[value_len] = '\0';
        }

        tok += json_deserialize_value(&json_str[tokens[tok].start], value_len, tokens[tok].type,
            node);
    }

    return txt_response(TS_STATUS_CHANGED);
}

int ThingSet::txt_get(const DataNode *parent_node, bool include_values)
{
    // initialize response with success message
    size_t len = txt_response(TS_STATUS_CONTENT);

    node_id_t parent_node_id = (parent_node == NULL) ? 0 : parent_node->id;

    if (parent_node != NULL && parent_node->type != TS_T_PATH &&
        parent_node->type != TS_T_EXEC)
    {
        // get value of data node
        resp[len++] = ' ';
        len += json_serialize_value((char *)&resp[len], resp_size - len, parent_node);
        resp[--len] = '\0';     // remove trailing comma again
        return len;
    }

    if (parent_node != NULL && parent_node->type == TS_T_EXEC && include_values) {
        // bad request, as we can't read exec node's values
        return txt_response(TS_STATUS_BAD_REQUEST);
    }

    len += sprintf((char *)&resp[len], include_values ? " {" : " [");
    int nodes_found = 0;
    for (unsigned int i = 0; i < num_nodes; i++) {
        if ((data_nodes[i].access & TS_READ_MASK) &&
            (data_nodes[i].parent == parent_node_id))
        {
            if (include_values) {
                if (data_nodes[i].type == TS_T_PATH) {
                    // bad request, as we can't read nternal path node's values
                    return txt_response(TS_STATUS_BAD_REQUEST);
                }
                len += json_serialize_name_value((char *)&resp[len], resp_size - len,
                    &data_nodes[i]);
            }
            else {
                len += snprintf((char *)&resp[len],
                    resp_size - len,
                    "\"%s\",", data_nodes[i].name);
            }
            nodes_found++;

            if (len >= resp_size - 1) {
                return txt_response(TS_STATUS_RESPONSE_TOO_LARGE);
            }
        }
    }

    // remove trailing comma and add closing bracket
    if (nodes_found == 0) {
        len++;
    }
    resp[len-1] = include_values ? '}' : ']';
    resp[len] = '\0';

    return len;
}

int ThingSet::txt_create(const DataNode *node)
{
    if (tok_count > 1) {
        // only single JSON primitive supported at the moment
        return txt_response(TS_STATUS_NOT_IMPLEMENTED);
    }

    if (node->type == TS_T_ARRAY) {
        ArrayInfo *arr_info = (ArrayInfo *)node->data;
        if (arr_info->num_elements < arr_info->max_elements) {

            if (arr_info->type == TS_T_NODE_ID && tokens[0].type == JSMN_STRING) {

                const DataNode *new_node = get_node(json_str + tokens[0].start,
                    tokens[0].end - tokens[0].start);

                if (new_node != NULL) {
                    node_id_t *node_ids = (node_id_t *)arr_info->ptr;
                    // check if node is already existing in array
                    for (int i = 0; i < arr_info->num_elements; i++) {
                        if (node_ids[i] == new_node->id) {
                            return txt_response(TS_STATUS_CONFLICT);
                        }
                    }
                    // otherwise append it
                    node_ids[arr_info->num_elements] = new_node->id;
                    arr_info->num_elements++;
                    return txt_response(TS_STATUS_CREATED);
                }
                else {
                    return txt_response(TS_STATUS_NOT_FOUND);
                }
            }
            else {
                return txt_response(TS_STATUS_NOT_IMPLEMENTED);
            }
        }
        else {
            return txt_response(TS_STATUS_INTERNAL_SERVER_ERR);
        }
    }
    else if (node->type == TS_T_PUBSUB) {
        if (tokens[0].type == JSMN_STRING) {
            DataNode *del_node = get_node(json_str + tokens[0].start,
                tokens[0].end - tokens[0].start);
            if (del_node != NULL) {
                del_node->pubsub |= (uint16_t)node->detail;
                return txt_response(TS_STATUS_CREATED);
            }
            return txt_response(TS_STATUS_NOT_FOUND);
        }
    }
    return txt_response(TS_STATUS_METHOD_NOT_ALLOWED);
}

int ThingSet::txt_delete(const DataNode *node)
{
    if (tok_count > 1) {
        // only single JSON primitive supported at the moment
        return txt_response(TS_STATUS_NOT_IMPLEMENTED);
    }

    if (node->type == TS_T_ARRAY) {
        ArrayInfo *arr_info = (ArrayInfo *)node->data;
        if (arr_info->type == TS_T_NODE_ID && tokens[0].type == JSMN_STRING) {
            const DataNode *del_node = get_node(json_str + tokens[0].start,
                tokens[0].end - tokens[0].start);
            if (del_node != NULL) {
                // node found in node database, now look for same ID in the array
                node_id_t *node_ids = (node_id_t *)arr_info->ptr;
                for (int i = 0; i < arr_info->num_elements; i++) {
                    if (node_ids[i] == del_node->id) {
                        // node also found in array, shift all remaining elements
                        for (int j = i; j < arr_info->num_elements - 1; j++) {
                            node_ids[j] = node_ids[j+1];
                        }
                        arr_info->num_elements--;
                        return txt_response(TS_STATUS_DELETED);
                    }
                }
            }
            return txt_response(TS_STATUS_NOT_FOUND);
        }
        else {
            return txt_response(TS_STATUS_NOT_IMPLEMENTED);
        }
    }
    else if (node->type == TS_T_PUBSUB) {
        if (tokens[0].type == JSMN_STRING) {
            DataNode *del_node = get_node(json_str + tokens[0].start,
                tokens[0].end - tokens[0].start);
            if (del_node != NULL) {
                del_node->pubsub &= ~((uint16_t)node->detail);
                return txt_response(TS_STATUS_DELETED);
            }
            return txt_response(TS_STATUS_NOT_FOUND);
        }
    }
    return txt_response(TS_STATUS_METHOD_NOT_ALLOWED);
}

int ThingSet::txt_exec(const DataNode *node)
{
    int tok = 0;            // current token
    int nodes_found = 0;    // number of child nodes found

    if (tok_count > 0 && tokens[tok].type == JSMN_ARRAY) {
        tok++;      // go to first element of array
    }

    if ((node->access & TS_WRITE_MASK) && (node->type == TS_T_EXEC)) {
        // node is generally executable, but are we authorized?
        if ((node->access & TS_WRITE_MASK & _auth_flags) == 0) {
            return txt_response(TS_STATUS_UNAUTHORIZED);
        }
    }
    else {
        return txt_response(TS_STATUS_FORBIDDEN);
    }

    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].parent == node->id) {
            if (tok >= tok_count) {
                // more child nodes found than parameters were passed
                return txt_response(TS_STATUS_BAD_REQUEST);
            }
            int res = json_deserialize_value(json_str + tokens[tok].start,
                tokens[tok].end - tokens[tok].start, tokens[tok].type, &data_nodes[i]);
            if (res == 0) {
                // deserializing the value was not successful
                return txt_response(TS_STATUS_UNSUPPORTED_FORMAT);
            }
            tok += res;
            nodes_found++;
        }
    }

    if (tok_count > tok) {
        // more parameters passed than child nodes found
        return txt_response(TS_STATUS_BAD_REQUEST);
    }

    // if we got here, finally create function pointer and call function
    void (*fun)(void) = reinterpret_cast<void(*)()>(node->data);
    fun();

    return txt_response(TS_STATUS_VALID);
}

int ThingSet::txt_pub(char *buf, size_t buf_size, const uint16_t pub_ch)
{
    unsigned int len = sprintf(buf, "# {");

    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].pubsub & pub_ch) {
            len += json_serialize_name_value(&buf[len], buf_size - len, &data_nodes[i]);
        }
        if (len >= buf_size - 1) {
            return 0;
        }
    }
    buf[len-1] = '}';    // overwrite comma

    return len;
}
