/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "tests.h"
#include "unity.h"

#include "thingset.h"
#include "cbor.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

extern uint8_t req_buf[];
extern uint8_t resp_buf[];
extern ThingSet ts;

int hex2bin(char *const hex, uint8_t *bin, size_t bin_size)
{
    int len = strlen(hex);
    unsigned int pos = 0;
    for (int i = 0; i < len; i += 3) {
        if (pos < bin_size) {
            bin[pos++] = (char)strtoul(&hex[i], NULL, 16);
        }
        else {
            return 0;
        }
    }
    return pos;
}

void _txt_patch(char const *name, char const *value)
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"%s\":%s}", name, value);
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);
}

int _txt_fetch(char const *name, char *value_read)
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?conf \"%s\"", name);
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);

    int pos_dot = strchr((char *)resp_buf, '.') - (char *)resp_buf + 1;
    char buf[100];
    strncpy(buf, (char *)resp_buf, pos_dot);
    buf[pos_dot] = '\0';
    //printf("buf: %s, resp: %s\n", buf, resp_buf);

    TEST_ASSERT_EQUAL_STRING(":85 Content.", buf);

    return snprintf(value_read, strlen((char *)resp_buf) - pos_dot, "%s", resp_buf + pos_dot + 1);
}

// returns length of read value
int _bin_fetch(uint16_t id, char *value_read)
{
    uint8_t req[] = {
        TS_FETCH,
        0x18, ID_CONF,
        0x19, (uint8_t)(id >> 8), (uint8_t)id
    };

    ts.process(req, sizeof(req), resp_buf, TS_RESP_BUFFER_LEN);
    //printf("TEST: Read request len: %d, response len: %d, resp code:%d\n", req_len, resp_len, resp_buf[0]);
    TEST_ASSERT_EQUAL_UINT8(TS_STATUS_CONTENT, resp_buf[0]);

    int value_len = cbor_size((uint8_t*)resp_buf + 1);
    memcpy(value_read, resp_buf + 1, value_len);
    return value_len;
}

// returns length of read value
void _bin_patch(uint16_t id, char *value)
{
    unsigned int len = cbor_size((uint8_t*)value);

    uint8_t req[100] = {
        TS_PATCH,
        0x18, ID_CONF,
        0xA1,
        0x19, (uint8_t)(id >> 8), (uint8_t)id
    };

    TEST_ASSERT(len + 7 < sizeof(req));
    memcpy(req + 7, value, len);
    ts.process(req, len + 7, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL_HEX8(TS_STATUS_CHANGED, resp_buf[0]);
}

void _json2cbor(char const *name, char const *json_value, uint16_t id, const char *const cbor_value_hex)
{
    char buf[100];  // temporary data storage (JSON or CBOR)

    uint8_t cbor_value[100];
    int len = hex2bin((char *)cbor_value_hex, cbor_value, sizeof(cbor_value));

    //printf("json2cbor(\"%s\", \"%s\", 0x%x, 0x(%s) )\n", name, json_value, id, cbor_value_hex);

    _txt_patch(name, json_value);
    len = _bin_fetch(id, buf);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(cbor_value, buf, len);
}

void _cbor2json(char const *name, char const *json_value, uint16_t id, char const *cbor_value_hex)
{
    char buf[100];  // temporary data storage (JSON or CBOR)

    char cbor_value[100];
    hex2bin((char *)cbor_value_hex, (uint8_t *)cbor_value, sizeof(cbor_value));

    //printf("cbor2json(\"%s\", \"%s\", 0x%x, 0x(%s) )\n", name, json_value, id, cbor_value_hex);

    _bin_patch(id, cbor_value);
    _txt_fetch(name, buf);

    TEST_ASSERT_EQUAL_STRING(json_value, buf);
}

void txt_patch_bin_fetch()
{
    // uint16
    _json2cbor("ui16", "0", 0x6005, "00");
    _json2cbor("ui16", "23", 0x6005, "17");
    _json2cbor("ui16", "24", 0x6005, "18 18");
    _json2cbor("ui16", "255", 0x6005, "18 ff");
    _json2cbor("ui16", "256", 0x6005, "19 01 00");
    _json2cbor("ui16", "65535", 0x6005, "19 FF FF");

    // uint32
    _json2cbor("ui32", "0", 0x6003, "00");
    _json2cbor("ui32", "23", 0x6003, "17");
    _json2cbor("ui32", "24", 0x6003, "18 18");
    _json2cbor("ui32", "255", 0x6003, "18 ff");
    _json2cbor("ui32", "256", 0x6003, "19 01 00");
    _json2cbor("ui32", "65535", 0x6003, "19 FF FF");
    _json2cbor("ui32", "65536", 0x6003, "1A 00 01 00 00");
    _json2cbor("ui32", "4294967295", 0x6003, "1A FF FF FF FF");

    // uint64
    #if TS_64BIT_TYPES_SUPPORT
    _json2cbor("ui64", "4294967295", 0x6001, "1A FF FF FF FF");
    _json2cbor("ui64", "4294967296", 0x6001, "1B 00 00 00 01 00 00 00 00");
    _json2cbor("ui64", "9223372036854775807", 0x6001, "1B 7F FF FF FF FF FF FF FF"); // maximum value for int64
    #endif

    // int16 (positive values)
    _json2cbor("i16", "0", 0x6006, "00");
    _json2cbor("i16", "23", 0x6006, "17");
    _json2cbor("i16", "24", 0x6006, "18 18");
    _json2cbor("i16", "255", 0x6006, "18 ff");
    _json2cbor("i16", "256", 0x6006, "19 01 00");
    _json2cbor("i16", "32767", 0x6006, "19 7F FF");                 // maximum value for int16

    // int32 (positive values)
    _json2cbor("i32", "0", 0x6004, "00");
    _json2cbor("i32", "23", 0x6004, "17");
    _json2cbor("i32", "24", 0x6004, "18 18");
    _json2cbor("i32", "255", 0x6004, "18 ff");
    _json2cbor("i32", "256", 0x6004, "19 01 00");
    _json2cbor("i32", "65535", 0x6004, "19 FF FF");
    _json2cbor("i32", "65536", 0x6004, "1A 00 01 00 00");
    _json2cbor("i32", "2147483647", 0x6004, "1A 7F FF FF FF");      // maximum value for int32

    // int64 (positive values)
    #if TS_64BIT_TYPES_SUPPORT
    _json2cbor("i64", "4294967295", 0x6002, "1A FF FF FF FF");
    _json2cbor("i64", "4294967296", 0x6002, "1B 00 00 00 01 00 00 00 00");
    _json2cbor("i64", "9223372036854775807", 0x6002, "1B 7F FF FF FF FF FF FF FF"); // maximum value for int64
    #endif

    // int16 (negative values)
    _json2cbor("i16", "-0", 0x6006, "00");
    _json2cbor("i16", "-24", 0x6006, "37");
    _json2cbor("i16", "-25", 0x6006, "38 18");
    _json2cbor("i16", "-256", 0x6006, "38 ff");
    _json2cbor("i16", "-257", 0x6006, "39 01 00");
    _json2cbor("i16", "-32768", 0x6006, "39 7F FF");

    // int32 (negative values)
    _json2cbor("i32", "-0", 0x6004, "00");
    _json2cbor("i32", "-24", 0x6004, "37");
    _json2cbor("i32", "-25", 0x6004, "38 18");
    _json2cbor("i32", "-256", 0x6004, "38 ff");
    _json2cbor("i32", "-257", 0x6004, "39 01 00");
    _json2cbor("i32", "-65536", 0x6004, "39 FF FF");
    _json2cbor("i32", "-65537", 0x6004, "3A 00 01 00 00");
    _json2cbor("i32", "-2147483648", 0x6004, "3A 7F FF FF FF");      // maximum value for int32

    // int64 (negative values)
    #if TS_64BIT_TYPES_SUPPORT
    _json2cbor("i64", "-4294967296", 0x6002, "3A FF FF FF FF");
    _json2cbor("i64", "-4294967297", 0x6002, "3B 00 00 00 01 00 00 00 00");
    _json2cbor("i64", "-9223372036854775808", 0x6002, "3B 7F FF FF FF FF FF FF FF"); // maximum value for int64
    #endif

    // float
    _json2cbor("f32", "12.340",  0x6007, "fa 41 45 70 a4");
    _json2cbor("f32", "-12.340", 0x6007, "fa c1 45 70 a4");
    _json2cbor("f32", "12.345",  0x6007, "fa 41 45 85 1f");

    // bool
    _json2cbor("bool", "true",  0x6008, "f5");
    _json2cbor("bool", "false",  0x6008, "f4");

    // string
    _json2cbor("strbuf", "\"Test\"",  0x6009, "64 54 65 73 74");
    _json2cbor("strbuf", "\"Hello World!\"",  0x6009, "6c 48 65 6c 6c 6f 20 57 6f 72 6c 64 21");
}


void bin_patch_txt_fetch()
{
    // uint16
    _cbor2json("ui16", "0", 0x6005, "00");
    _cbor2json("ui16", "23", 0x6005, "17");
    _cbor2json("ui16", "23", 0x6005, "18 17");       // less compact format
    _cbor2json("ui16", "24", 0x6005, "18 18");
    _cbor2json("ui16", "255", 0x6005, "18 ff");
    _cbor2json("ui16", "255", 0x6005, "19 00 ff");   // less compact format
    _cbor2json("ui16", "256", 0x6005, "19 01 00");
    _cbor2json("ui16", "65535", 0x6005, "19 FF FF");

    // uint32
    _cbor2json("ui32", "0", 0x6003, "00");
    _cbor2json("ui32", "23", 0x6003, "17");
    _cbor2json("ui32", "23", 0x6003, "18 17");       // less compact format
    _cbor2json("ui32", "24", 0x6003, "18 18");
    _cbor2json("ui32", "255", 0x6003, "18 ff");
    _cbor2json("ui32", "255", 0x6003, "19 00 ff");   // less compact format
    _cbor2json("ui32", "256", 0x6003, "19 01 00");
    _cbor2json("ui32", "65535", 0x6003, "19 FF FF");
    _cbor2json("ui32", "65535", 0x6003, "1A 00 00 FF FF");  // less compact format
    _cbor2json("ui32", "65536", 0x6003, "1A 00 01 00 00");
    _json2cbor("ui32", "4294967295", 0x6003, "1A FF FF FF FF");

    // uint64
    #if TS_64BIT_TYPES_SUPPORT
    _cbor2json("ui64", "4294967295", 0x6001, "1A FF FF FF FF");
    _cbor2json("ui64", "4294967295", 0x6001, "1B 00 00 00 00 FF FF FF FF"); // less compact format
    _cbor2json("ui64", "4294967296", 0x6001, "1B 00 00 00 01 00 00 00 00");
    _cbor2json("ui64", "18446744073709551615", 0x6001, "1B FF FF FF FF FF FF FF FF");
    #endif

    // int32 (positive values)
    _cbor2json("i32", "23", 0x6004, "17");
    _cbor2json("i32", "23", 0x6004, "18 17");       // less compact format
    _cbor2json("i32", "24", 0x6004, "18 18");
    _cbor2json("i32", "255", 0x6004, "18 ff");
    _cbor2json("i32", "255", 0x6004, "19 00 ff");   // less compact format
    _cbor2json("i32", "256", 0x6004, "19 01 00");
    _cbor2json("i32", "65535", 0x6004, "19 FF FF");
    _cbor2json("i32", "65535", 0x6004, "1A 00 00 FF FF");  // less compact format
    _cbor2json("i32", "65536", 0x6004, "1A 00 01 00 00");
    _cbor2json("i32", "2147483647", 0x6004, "1A 7F FF FF FF");      // maximum value for int32

    // int64 (positive values)
    #if TS_64BIT_TYPES_SUPPORT
    _cbor2json("i64", "4294967295", 0x6002, "1A FF FF FF FF");
    _cbor2json("i64", "4294967296", 0x6002, "1B 00 00 00 01 00 00 00 00");
    _cbor2json("i64", "9223372036854775807", 0x6002, "1B 7F FF FF FF FF FF FF FF"); // maximum value for int64
    #endif

    // int16 (negative values)
    _cbor2json("i16", "-24", 0x6006, "37");
    _cbor2json("i16", "-24", 0x6006, "38 17");      // less compact format
    _cbor2json("i16", "-25", 0x6006, "38 18");
    _cbor2json("i16", "-256", 0x6006, "38 ff");
    _cbor2json("i16", "-257", 0x6006, "39 01 00");
    _cbor2json("i16", "-32768", 0x6006, "39 7F FF");

    // int32 (negative values)
    _cbor2json("i32", "-24", 0x6004, "37");
    _cbor2json("i32", "-24", 0x6004, "38 17");      // less compact format
    _cbor2json("i32", "-25", 0x6004, "38 18");
    _cbor2json("i32", "-256", 0x6004, "38 ff");
    _cbor2json("i32", "-257", 0x6004, "39 01 00");
    _cbor2json("i32", "-65536", 0x6004, "39 FF FF");
    _cbor2json("i32", "-65537", 0x6004, "3A 00 01 00 00");
    _cbor2json("i32", "-2147483648", 0x6004, "3A 7F FF FF FF");      // maximum value for int32

    // int64 (negative values)
    #if TS_64BIT_TYPES_SUPPORT
    _cbor2json("i64", "-4294967296", 0x6002, "3A FF FF FF FF");
    _cbor2json("i64", "-4294967297", 0x6002, "3B 00 00 00 01 00 00 00 00");
    _cbor2json("i64", "-9223372036854775808", 0x6002, "3B 7F FF FF FF FF FF FF FF"); // maximum value for int64
    #endif

    // float
    _cbor2json("f32", "12.34",  0x6007, "fa 41 45 70 a4");
    _cbor2json("f32", "-12.34", 0x6007, "fa c1 45 70 a4");
    _cbor2json("f32", "12.34",  0x6007, "fa 41 45 81 06");      // 12.344
    _cbor2json("f32", "12.35",  0x6007, "fa 41 45 85 1f");      // 12.345 (should be rounded to 12.35)

    // bool
    _cbor2json("bool", "true",  0x6008, "f5");
    _cbor2json("bool", "false",  0x6008, "f4");

    // string
    _cbor2json("strbuf", "\"Test\"",  0x6009, "64 54 65 73 74");
    _cbor2json("strbuf", "\"Hello World!\"",  0x6009, "6c 48 65 6c 6c 6f 20 57 6f 72 6c 64 21");
}

void tests_common()
{
    UNITY_BEGIN();

    // data conversion tests
    RUN_TEST(txt_patch_bin_fetch);
    RUN_TEST(bin_patch_txt_fetch);

    UNITY_END();
}
