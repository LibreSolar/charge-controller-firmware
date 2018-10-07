
#include "thingset.h"
#include "cbor.h"
#include "test_data.h"
#include "unity.h"
#include <inttypes.h>


void _write_json(char const *name, char const *value)
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write \"%s\":%s", name, value);
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success.", resp.data);
}

int _read_json(char const *name, char *value_read)
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!read \"%s\"", name);
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);

    char buf[100];
    strncpy(buf, resp.data, 11);
    buf[11] = '\0';
    //printf("buf: %s, resp: %s\n", buf, resp.data);

    TEST_ASSERT_EQUAL_STRING(":0 Success.", buf);

    return snprintf(value_read, strlen(resp.data) - 11, "%s", resp.data + 12);
}

// returns length of read value
int _read_cbor(uint16_t id, char *value_read)
{
    str_buffer_t req, resp;

    // generate read request
    req.data[0] = TS_FUNCTION_READ;
    req.data[1] = id >> 8;
    req.data[2] = (uint8_t)id;
    req.pos = 3;
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL_UINT8(TS_STATUS_SUCCESS, resp.data[0] - 0x80);
    //printf("TEST: Read request len: %d, response len: %d\n", req.pos, resp.pos);

    int value_len = cbor_size((uint8_t*)resp.data + 1);
    memcpy(value_read, resp.data + 1, value_len);
    return value_len;
}

// returns length of read value
void _write_cbor(uint16_t id, char *value)
{
    str_buffer_t req, resp;

    int len = cbor_size((uint8_t*)value);

    // generate write request
    req.data[0] = TS_FUNCTION_WRITE;
    req.data[1] = id >> 8;
    req.data[2] = (uint8_t)id;
    memcpy(req.data + 3, value, len);
    req.pos = len + 3;
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL_UINT8(TS_STATUS_SUCCESS, resp.data[0] - 0x80);
}

void _json2cbor(char const *name, char const *json_value, uint16_t id, char const *cbor_value_hex)
{
    char buf[100];  // temporary data storage (JSON or CBOR)

    // extract binary CBOR data
    char cbor_value[100];
    int len = strlen(cbor_value_hex);
    int pos = 0;
    for (int i = 0; i < len; i += 3) {
        cbor_value[pos++] = (char)strtoul(&cbor_value_hex[i], NULL, 16);
    }

    //printf("\n");
    //printf ("json2cbor(\"%s\", \"%s\", 0x%x, 0x(%s) )\n", name, json_value, id, cbor_value_hex);

    //printf("before write: i64 = 0x%" PRIi64 " = %" PRIx64 "\n", i64, i64);
    //printf("before write: i64 = %16.llX = %lli\n", i64, i64);
    _write_json(name, json_value);
    //printf("after write:  i64 = %16.llX = %lli\n", i64, i64);
    //printf("after write: i64 = %" PRIi64 " = %llx\n", i64, i64);
    len = _read_cbor(id, buf);

    TEST_ASSERT_EQUAL_HEX8_ARRAY(cbor_value, buf, len);
}

void _cbor2json(char const *name, char const *json_value, uint16_t id, char const *cbor_value_hex)
{
    char buf[100];  // temporary data storage (JSON or CBOR)

    // extract binary CBOR data
    char cbor_value[100];
    int len = strlen(cbor_value_hex);
    int pos = 0;
    for (int i = 0; i < len; i += 3) {
        cbor_value[pos++] = (char)strtoul(&cbor_value_hex[i], NULL, 16);
    }

    _write_cbor(id, cbor_value);
    len = _read_json(name, buf);

    TEST_ASSERT_EQUAL_STRING(json_value, buf);
}

void write_json_read_cbor()
{
    // positive integers
    _json2cbor("i32", "23", 0x6004, "17");
    _json2cbor("i32", "24", 0x6004, "18 18");
    _json2cbor("i32", "255", 0x6004, "18 ff");
    _json2cbor("i32", "256", 0x6004, "19 01 00");
    _json2cbor("i32", "65535", 0x6004, "19 FF FF");
    _json2cbor("i32", "65536", 0x6004, "1A 00 01 00 00");
    _json2cbor("i32", "2147483647", 0x6004, "1A 7F FF FF FF");      // maximum value for int32

    // only for int64 or uint32
    _json2cbor("i64", "4294967295", 0x6002, "1A FF FF FF FF");
    _json2cbor("i64", "4294967296", 0x6002, "1B 00 00 00 01 00 00 00 00");
    _json2cbor("i64", "9223372036854775807", 0x6002, "1B 7F FF FF FF FF FF FF FF"); // maximum value for int64

    // negative integers
    _json2cbor("i32", "-0", 0x6004, "00");
    _json2cbor("i32", "-24", 0x6004, "37");
    _json2cbor("i32", "-25", 0x6004, "38 18");
    _json2cbor("i32", "-256", 0x6004, "38 ff");
    _json2cbor("i32", "-257", 0x6004, "39 01 00");
    _json2cbor("i32", "-65536", 0x6004, "39 FF FF");
    _json2cbor("i32", "-65537", 0x6004, "3A 00 01 00 00");
    _json2cbor("i32", "-2147483648", 0x6004, "3A 7F FF FF FF");      // maximum value for int32

    _json2cbor("i64", "-4294967296", 0x6002, "3A FF FF FF FF"); 
    _json2cbor("i64", "-4294967297", 0x6002, "3B 00 00 00 01 00 00 00 00"); 
    _json2cbor("i64", "-9223372036854775808", 0x6002, "3B 7F FF FF FF FF FF FF FF"); // maximum value for int64

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


void write_cbor_read_json()
{
    // positive integers
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

    _cbor2json("i64", "4294967295", 0x6002, "1A FF FF FF FF");
    _cbor2json("i64", "4294967296", 0x6002, "1B 00 00 00 01 00 00 00 00");
    _cbor2json("i64", "9223372036854775807", 0x6002, "1B 7F FF FF FF FF FF FF FF"); // maximum value for int64

    // negative integers
    _cbor2json("i32", "-24", 0x6004, "37");
    _cbor2json("i32", "-24", 0x6004, "38 17");      // less compact format
    _cbor2json("i32", "-25", 0x6004, "38 18");
    _cbor2json("i32", "-256", 0x6004, "38 ff");
    _cbor2json("i32", "-257", 0x6004, "39 01 00");
    _cbor2json("i32", "-65536", 0x6004, "39 FF FF");
    _cbor2json("i32", "-65537", 0x6004, "3A 00 01 00 00");
    _cbor2json("i32", "-2147483648", 0x6004, "3A 7F FF FF FF");      // maximum value for int32

    _cbor2json("i64", "-4294967296", 0x6002, "3A FF FF FF FF");
    _cbor2json("i64", "-4294967297", 0x6002, "3B 00 00 00 01 00 00 00 00");
    _cbor2json("i64", "-9223372036854775808", 0x6002, "3B 7F FF FF FF FF FF FF FF"); // maximum value for int64

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