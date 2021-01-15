/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#ifndef THINGSET_H_
#define THINGSET_H_

#include "ts_config.h"
#include "jsmn.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * Protocol function codes (same as CoAP)
 */
#define TS_GET      0x01
#define TS_POST     0x02
#define TS_DELETE   0x04
#define TS_FETCH    0x05
#define TS_PATCH    0x07        // it's actually iPATCH

#define TS_PUBMSG   0x1F

/*
 * Status codes (same as CoAP)
 */

// success
#define TS_STATUS_CREATED               0x81
#define TS_STATUS_DELETED               0x82
#define TS_STATUS_VALID                 0x83
#define TS_STATUS_CHANGED               0x84
#define TS_STATUS_CONTENT               0x85

// client errors
#define TS_STATUS_BAD_REQUEST           0xA0
#define TS_STATUS_UNAUTHORIZED          0xA1        // need to authenticate
#define TS_STATUS_FORBIDDEN             0xA3        // trying to write read-only value
#define TS_STATUS_NOT_FOUND             0xA4
#define TS_STATUS_METHOD_NOT_ALLOWED    0xA5
#define TS_STATUS_REQUEST_INCOMPLETE    0xA8
#define TS_STATUS_CONFLICT              0xA9
#define TS_STATUS_REQUEST_TOO_LARGE     0xAD
#define TS_STATUS_UNSUPPORTED_FORMAT    0xAF

// server errors
#define TS_STATUS_INTERNAL_SERVER_ERR   0xC0
#define TS_STATUS_NOT_IMPLEMENTED       0xC1

// ThingSet specific errors
#define TS_STATUS_RESPONSE_TOO_LARGE    0xE1

/**
 * Internal C data types (used to cast void* pointers)
 */
enum TsType {
    TS_T_BOOL,
    TS_T_UINT64,
    TS_T_INT64,
    TS_T_UINT32,
    TS_T_INT32,
    TS_T_UINT16,
    TS_T_INT16,
    TS_T_FLOAT32,
    TS_T_STRING,
    TS_T_ARRAY,
    TS_T_DECFRAC,       // CBOR decimal fraction
    TS_T_PATH,          // internal node to describe URI path
    TS_T_NODE_ID,       // internally equal to uint16_t
    TS_T_EXEC,          // for exec data objects
    TS_T_PUBSUB
};

/**
 * Data structure to specify an array data node
 */
typedef struct {
    void *ptr;                  ///< Pointer to the array
    uint16_t max_elements;      ///< Maximum number of elements in the array
    uint16_t num_elements;      ///< Actual number of elements in the array
    uint8_t type;               ///< Type of the array elements
} ArrayInfo;

/**
 * If TS_AUTODETECT_ARRLEN is assigned to num_elements, the number of elements in the array is
 * detected in the constructor by counting downwards till the first non-zero element is found.
 */
#define TS_AUTODETECT_ARRLEN    UINT16_MAX

/*
 * Functions to generate data_node map and make compiler complain if wrong
 * type is passed
 */

static inline void *_bool_to_void(bool *ptr) { return (void*) ptr; }
#define TS_NODE_BOOL(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _bool_to_void(_data_ptr), TS_T_BOOL, 0, _acc, _pubsub}

static inline void *_uint64_to_void(uint64_t *ptr) { return (void*) ptr; }
#define TS_NODE_UINT64(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _uint64_to_void(_data_ptr), TS_T_UINT64, 0, _acc, _pubsub}

static inline void *_int64_to_void(int64_t *ptr) { return (void*) ptr; }
#define TS_NODE_INT64(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _int64_to_void(_data_ptr), TS_T_INT64, 0, _acc, _pubsub}

static inline void *_uint32_to_void(uint32_t *ptr) { return (void*) ptr; }
#define TS_NODE_UINT32(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _uint32_to_void(_data_ptr), TS_T_UINT32, 0, _acc, _pubsub}

static inline void *_int32_to_void(int32_t *ptr) { return (void*) ptr; }
#define TS_NODE_INT32(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _int32_to_void(_data_ptr), TS_T_INT32, 0, _acc, _pubsub}

static inline void *_uint16_to_void(uint16_t *ptr) { return (void*) ptr; }
#define TS_NODE_UINT16(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _uint16_to_void(_data_ptr), TS_T_UINT16, 0, _acc, _pubsub}

static inline void *_int16_to_void(int16_t *ptr) { return (void*) ptr; }
#define TS_NODE_INT16(_id, _name, _data_ptr, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _int16_to_void(_data_ptr), TS_T_INT16, 0, _acc, _pubsub}

static inline void *_float_to_void(float *ptr) { return (void*) ptr; }
#define TS_NODE_FLOAT(_id, _name, _data_ptr, _digits, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _float_to_void(_data_ptr), TS_T_FLOAT32, _digits, _acc, _pubsub}

static inline void *_string_to_void(const char *ptr) { return (void*) ptr; }
#define TS_NODE_STRING(_id, _name, _data_ptr, _buf_size, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _string_to_void(_data_ptr), TS_T_STRING, _buf_size, _acc, _pubsub}

static inline void *_function_to_void(void (*fnptr)()) { return (void*) fnptr; }
#define TS_NODE_EXEC(_id, _name, _function_ptr, _parent, _acc) \
    {_id, _parent, _name, _function_to_void(_function_ptr), TS_T_EXEC, 0, _acc, 0}

static inline void *_array_to_void(ArrayInfo *ptr) { return (void *) ptr; }
#define TS_NODE_ARRAY(_id, _name, _data_ptr, _digits, _parent, _acc, _pubsub) \
    {_id, _parent, _name, _array_to_void(_data_ptr), TS_T_ARRAY, _digits, _acc, _pubsub}

#define TS_NODE_PUBSUB(_id, _name, _pubsub_channel, _parent, _acc, _pubsub) \
    {_id, _parent, _name, NULL, TS_T_PUBSUB, _pubsub_channel, _acc, _pubsub}

#define TS_NODE_PATH(_id, _name, _parent, _callback) \
    {_id, _parent, _name, _function_to_void(_callback), TS_T_PATH, 0, TS_READ_MASK, 0}

/*
 * Access right macros for data nodes
 */
#define TS_ROLE_USR     (1U << 0)       // normal user
#define TS_ROLE_EXP     (1U << 1)       // expert user
#define TS_ROLE_MKR     (1U << 2)       // maker

#define TS_READ_MASK    0x00FF          // read flags stored in 4 least-significant bits
#define TS_WRITE_MASK   0xFF00          // write flags stored in 4 most-significant bits

#define TS_USR_MASK     (TS_ROLE_USR << 8 | TS_ROLE_USR)
#define TS_EXP_MASK     (TS_ROLE_EXP << 8 | TS_ROLE_EXP)
#define TS_MKR_MASK     (TS_ROLE_MKR << 8 | TS_ROLE_MKR)

#define TS_READ(roles)          ((roles) & TS_READ_MASK)
#define TS_WRITE(roles)         (((roles) << 8) & TS_WRITE_MASK)
#define TS_READ_WRITE(roles)    (TS_READ(roles) | TS_WRITE(roles))

#define TS_USR_R        TS_READ(TS_ROLE_USR)
#define TS_EXP_R        TS_READ(TS_ROLE_EXP)
#define TS_MKR_R        TS_READ(TS_ROLE_MKR)
#define TS_ANY_R        (TS_USR_R | TS_EXP_R | TS_MKR_R)

#define TS_USR_W        TS_WRITE(TS_ROLE_USR)
#define TS_EXP_W        TS_WRITE(TS_ROLE_EXP)
#define TS_MKR_W        TS_WRITE(TS_ROLE_MKR)
#define TS_ANY_W        (TS_USR_W | TS_EXP_W | TS_MKR_W)

#define TS_USR_RW       TS_READ_WRITE(TS_ROLE_USR)
#define TS_EXP_RW       TS_READ_WRITE(TS_ROLE_EXP)
#define TS_MKR_RW       TS_READ_WRITE(TS_ROLE_MKR)
#define TS_ANY_RW       (TS_USR_RW | TS_EXP_RW | TS_MKR_RW)


typedef uint16_t node_id_t;

/**
 * ThingSet data node struct
 */
typedef struct DataNode {
    /**
     * Data node ID
     */
    const node_id_t id;

    /**
     * ID of parent node
     */
    const node_id_t parent;

    /**
     * Data Node name
     */
    const char *name;

    /**
     * Pointer to the variable containing the data. The variable type must match the type as
     * specified
     */
    void *const data;

    /**
     * One of TS_TYPE_INT32, _FLOAT, ...
     */
    const uint8_t type;

    /**
     * Exponent (10^exponent = factor to convert to SI unit) for decimal fraction type,
     * decimal digits to use for printing of floats in JSON strings or
     * lenght of string buffer for string type
     */
    const int16_t detail;

    /**
     * Flags to define read/write access
     */
    const uint16_t access;

    /**
     * Flags to add this node to different pub/sub channels
     */
    uint16_t pubsub;

} DataNode;

/**
 * Main ThingSet class
 *
 * Stores and handles all data exposed to different communication interfaces
 */
class ThingSet
{
public:
    /**
     * Initialize a ThingSet object
     *
     * @param data Pointer to array of DataNode type containing the entire node database
     * @param num Number of elements in that array
     */
    ThingSet(DataNode *data, size_t num);

    /**
     * Process ThingSet request
     *
     * This function also detects if JSON or CBOR format is used
     *
     * @param request Pointer to the ThingSet request buffer
     * @param req_len Length of the data in the request buffer
     * @param response Pointer to the buffer where the ThingSet response should be stored
     * @param resp_size Size of the response buffer, i.e. maximum allowed length of the response

     * @returns Actual length of the response written to the buffer or 0 in case of error
     */
    int process(uint8_t *request, size_t req_len, uint8_t *response, size_t resp_size);

    /**
     * Print all data nodes as a structured JSON text to stdout
     *
     * WARNING: This is a recursive function and might cause stack overflows if run in constrained
     *          devices with large data node tree. Use with care and for testing only!
     *
     * @param node_id Root node ID where to start with printing
     * @param level Indentation level (=depth inside the data node tree)
     */
    void dump_json(node_id_t node_id = 0, int level = 0);

    /**
     * Sets current authentication level
     *
     * The authentication flags must match with access flags specified in DataNode to allow
     * read/write access to a data node.
     *
     * @param flags Flags to define authentication level (1 = access allowed)
     */
    void set_authentication(uint16_t flags)
    {
        _auth_flags = flags;
    }

    /**
     * Generate publication message in JSON format
     *
     * @param buf Pointer to the buffer where the publication message should be stored
     * @param size Size of the message buffer, i.e. maximum allowed length of the message
     * @param pub_ch Flag to select publication channel (must match pubsub of data node)
     *
     * @returns Actual length of the message written to the buffer or 0 in case of error
     */
    int txt_pub(char *buf, size_t size, const uint16_t pub_ch);

    /**
     * Generate publication message in CBOR format
     *
     * @param buf Pointer to the buffer where the publication message should be stored
     * @param size Size of the message buffer, i.e. maximum allowed length of the message
     * @param pub_ch Flag to select publication channel (must match pubsub of data node)
     *
     * @returns Actual length of the message written to the buffer or 0 in case of error
     */
    int bin_pub(uint8_t *buf, size_t size, const uint16_t pub_ch);

    /**
     * Encode a publication message in CAN message format for supplied data node
     *
     * The data may only be 8 bytes long. If the actual length of a node exceeds the available
     * length, the node is silently ignored and the function continues with the next one.
     *
     * @param start_pos Position in data_nodes array to start searching
     *                  This value is updated with the next node found to allow iterating over all
     *                  nodes for this channel. It should be set to 0 to start from the beginning.
     * @param pub_ch Flag to select publication channel (must match pubsub of data node)
     * @param can_dev_id Device ID on the CAN bus
     * @param msg_id reference to can message id storage
     * @param msg_data reference to the buffer where the publication message should be stored
     *
     * @returns Actual length of the message_data or -1 if not encodable / in case of error
     */
    int bin_pub_can(int &start_pos, uint16_t pub_ch, uint8_t can_dev_id, uint32_t &msg_id,
        uint8_t (&msg_data)[8]);

    /**
     * Update data nodes based on values provided in payload data (e.g. from other pub msg)
     *
     * @param cbor_data Buffer containing key/value map that should be written to the data nodes
     * @param len Length of the data in the buffer
     * @param auth_flags Authentication flags to be used in this function (to override _auth_flags)
     * @param sub_ch Subscribe channel (as bitfield)
     *
     * @returns ThingSet status code
     */
    int bin_sub(uint8_t *cbor_data, size_t len, uint16_t auth_flags, uint16_t sub_ch);

    /**
     * Get data node by ID
     *
     * @param id Node ID
     *
     * @returns Pointer to data node or NULL if node is not found
     */
    DataNode *const get_node(node_id_t id);

    /**
     * Get data node by name
     *
     * As the names are not necessarily unique in the entire data tree, the parent is needed
     *
     * @param name Node name
     * @param len Length of the node name
     * @param parent Node ID of the parent or -1 for global search
     *
     * @returns Pointer to data node or NULL if node is not found
     */
    DataNode *const get_node(const char *name, size_t len, int32_t parent = -1);

    /**
     * Get the endpoint node of a provided path
     *
     * @param path Path of with multiple node names divided by forward slash
     * @param len Length of the node name
     *
     * @returns Pointer to data node or NULL if node is not found
     */
    DataNode *const get_endpoint(const char *path, size_t len);

private:
    /**
     * Prepares JSMN parser, performs initial check of payload data and calls get/fetch/patch
     * functions
     */
    int txt_process();

    /**
     * Performs initial check of payload data and calls get/fetch/patch functions
     */
    int bin_process();

    /**
     * GET request (text mode)
     *
     * List child data nodes (function called without content / parameters)
     */
    int txt_get(const DataNode *parent, bool include_values = false);

    /**
     * GET request (binary mode)
     *
     * List child data nodes (function called without content)
     */
    int bin_get(const DataNode *parent, bool values = false, bool ids_only = true);

    /**
     * FETCH request (text mode)
     *
     * Read data node values (function called with an array as argument)
     */
    int txt_fetch(node_id_t parent_id);

    /**
     * FETCH request (binary mode)
     *
     * Read data node values (function called with an array as argument)
     */
    int bin_fetch(const DataNode *parent, unsigned int pos_payload);

    /**
     * PATCH request (text mode)
     *
     * Write data node values in text mode (function called with a map as argument)
     */
    int txt_patch(node_id_t parent_id);

    /**
     * PATCH request (binary mode)
     *
     * Write data node values in binary mode (function called with a map as payload)
     *
     * If sub_ch is specified, nodes not found are silently ignored. Otherwise, a NOT_FOUND
     * error is raised.
     *
     * @param parent Pointer to path / parent node or NULL to consider any node
     * @param pos_payload Position of payload in req buffer
     * @param auth_flags Bitset to specify authentication status for different roles
     * @param sub_ch Bitset to specifiy subscribe channel to be considered, 0 to ignore
     */
    int bin_patch(const DataNode *parent, unsigned int pos_payload, uint16_t auth_flags,
        uint16_t sub_ch);

    /**
     * POST request to append data
     */
    int txt_create(const DataNode *node);

    /**
     * DELETE request to delete data from node
     */
    int txt_delete(const DataNode *node);

    /**
     * Execute command in text mode (function called with a single data node name as argument)
     */
    int txt_exec(const DataNode *node);

    /**
     * Execute command in binary mode (function called with a single data node name/id as argument)
     *
     * @param parent Pointer to executable node
     * @param pos_payload Position of payload in req buffer
     */
    int bin_exec(const DataNode *node, unsigned int pos_payload);

    /**
     * Fill the resp buffer with a JSON response status message
     *
     * @param code Status code
     * @returns length of status message in buffer or 0 in case of error
     */
    int txt_response(int code);

    /**
     * Fill the resp buffer with a CBOR response status message
     *
     * @param code Status code
     * @returns length of status message in buffer or 0 in case of error
     */
    int bin_response(uint8_t code);

    /**
     * Serialize a node value into a JSON string
     *
     * @param buf Pointer to the buffer where the JSON value should be stored
     * @param size Size of the buffer, i.e. maximum allowed length of the value
     * @param node Pointer to node which should be serialized
     *
     * @returns Length of data written to buffer or 0 in case of error
     */
    int json_serialize_value(char *buf, size_t size, const DataNode *node);

    /**
     * Serialize node name and value as JSON object
     *
     * same as json_serialize_value, just that the node name is also serialized
     */
    int json_serialize_name_value(char *buf, size_t size, const DataNode *node);

    /**
     * Deserialize a node value from a JSON string
     *
     * @param buf Pointer to the position of the value in a buffer
     * @param len Length of value in the buffer
     * @param type Type of the JSMN token as identified by the parser
     * @param node Pointer to node where the deserialized value should be stored
     *
     * @returns Number of tokens processed (always 1) or 0 in case of error
     */
    int json_deserialize_value(char *buf, size_t len, jsmntype_t type, const DataNode *node);

    /**
     * Array of nodes database provided during initialization
     */
    DataNode *data_nodes;

    /**
     * Number of nodes in the data_nodes array
     */
    size_t num_nodes;

    /**
     * Pointer to request buffer (provided in process function)
     */
    uint8_t *req;

    /**
     * Length of the request
     */
    size_t req_len;

    /**
     * Pointer to response buffer (provided in process function)
     */
    uint8_t *resp;

    /**
     * Size of response buffer (i.e. maximum length)
     */
    size_t resp_size;

    /**
     * Pointer to the start of JSON payload in the request
     */
    char *json_str;

    /**
     * JSON tokes in json_str parsed by JSMN
     */
    jsmntok_t tokens[TS_NUM_JSON_TOKENS];

    /**
     * Number of JSON tokens parsed by JSMN
     */
    int tok_count;

    /**
     * Stores current authentication status (authentication as "normal" user as default)
     */
    uint16_t _auth_flags = TS_USR_MASK;
};

#endif /* THINGSET_H_ */
