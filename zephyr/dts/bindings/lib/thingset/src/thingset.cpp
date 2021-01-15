/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "ts_config.h"
#include "thingset.h"
#include "jsmn.h"
#include "cbor.h"

#include <string.h>
#include <stdio.h>

#define DEBUG 0

static void _check_id_duplicates(const DataNode *data, size_t num)
{
    for (unsigned int i = 0; i < num; i++) {
        for (unsigned int j = i + 1; j < num; j++) {
            if (data[i].id == data[j].id) {
                printf("ThingSet error: Duplicate data node ID 0x%X.\n", data[i].id);
            }
        }
    }
}

/*
 * Counts the number of elements in an an array of node IDs by looking for the first non-zero
 * elements starting from the back.
 *
 * Currently only supporting uint16_t (node_id_t) arrays as we need the size of each element
 * to iterate through the array.
 */
static void _count_array_elements(const DataNode *data, size_t num)
{
    for (unsigned int i = 0; i < num; i++) {
        if (data[i].type == TS_T_ARRAY) {
            ArrayInfo *arr = (ArrayInfo *)data[i].data;
            if (arr->num_elements == TS_AUTODETECT_ARRLEN) {
                arr->num_elements = 0;  // set to safe default
                if (arr->type == TS_T_NODE_ID) {
                    for (int elem = arr->max_elements - 1; elem >= 0; elem--) {
                        if (((node_id_t *)arr->ptr)[elem] != 0) {
                            arr->num_elements = elem + 1;
                            //printf("%s num elements: %d\n", data[i].name, arr->num_elements);
                            break;
                        }
                    }
                }
                else {
                    printf("Autodetecting array length of node 0x%X not possible.\n", data[i].id);
                }
            }
        }
    }
}

ThingSet::ThingSet(DataNode *data, size_t num)
{
    _check_id_duplicates(data, num);

    _count_array_elements(data, num);

    data_nodes = data;
    num_nodes = num;
}

int ThingSet::process(uint8_t *request, size_t request_len, uint8_t *response, size_t response_size)
{
    // check if proper request was set before asking for a response
    if (request == NULL || request_len < 1)
        return 0;

    // assign private variables
    req = request;
    req_len = request_len;
    resp = response;
    resp_size = response_size;

    if (req[0] < 0x20) {
        // binary mode request
        return bin_process();
    }
    else if (req[0] == '?' || req[0] == '=' || req[0] == '+' || req[0] == '-' || req[0] == '!') {
        // text mode request
        return txt_process();
    }
    else {
        // not a thingset command --> ignore and set response to empty string
        response[0] = 0;
        return 0;
    }
}

DataNode *const ThingSet::get_node(const char *str, size_t len, int32_t parent)
{
    for (unsigned int i = 0; i < num_nodes; i++) {
        if (parent != -1 && data_nodes[i].parent != parent) {
            continue;
        }
        else if (strncmp(data_nodes[i].name, str, len) == 0
            && strlen(data_nodes[i].name) == len)  // otherwise e.g. foo and fooBar would be recognized as equal
        {
            return &(data_nodes[i]);
        }
    }
    return NULL;
}

DataNode *const ThingSet::get_node(node_id_t id)
{
    for (unsigned int i = 0; i < num_nodes; i++) {
        if (data_nodes[i].id == id) {
            return &(data_nodes[i]);
        }
    }
    return NULL;
}

DataNode *const ThingSet::get_endpoint(const char *path, size_t len)
{
    const DataNode *node;
    const char *start = path;
    const char *end = strchr(path, '/');
    uint16_t parent = 0;

    // maximum depth of 10 assumed
    for (int i = 0; i < 10; i++) {
        if (end != NULL) {
            if (end - path != (int)len - 1) {
                node = get_node(start, end - start, parent);
                if (!node) {
                    return NULL;
                }
                parent = node->id;
                start = end + 1;
                end = strchr(start, '/');
            }
            else {
                // resource ends with trailing slash
                return get_node(start, end - start, parent);
            }
        }
        else {
            return get_node(start, path + len - start, parent);
        }
    }
    return NULL;
}
