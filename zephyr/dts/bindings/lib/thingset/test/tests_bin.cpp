/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "tests.h"
#include "unity.h"

#include "thingset.h"
#include "cbor.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern ThingSet ts;

extern float f32;
extern int32_t i32;
extern ArrayInfo int32_array;
extern ArrayInfo float32_array;

extern ArrayInfo pub_serial_array;

void test_bin_get_output_ids()
{
    uint8_t req[] = { TS_GET, 0x18, ID_OUTPUT, 0xF7 };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    char resp_hex[] =
        "85 83 "     // successful response: array with 3 elements
        "18 71 "
        "18 72 "
        "18 73 ";

    uint8_t resp_expected[100];
    int len = hex2bin(resp_hex, resp_expected, sizeof(resp_expected));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(resp_expected, resp, len);
}

void test_bin_get_output_names()
{
    uint8_t req[] = { TS_GET, 0x18, ID_OUTPUT, 0x80 };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    char resp_hex[] =
        "85 83 "     // successful response: array with 3 elements
        "65 42 61 74 5F 56 "
        "65 42 61 74 5F 41 "
        "6C 41 6D 62 69 65 6E 74 5F 64 65 67 43";

    uint8_t resp_expected[100];
    int len = hex2bin(resp_hex, resp_expected, sizeof(resp_expected));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(resp_expected, resp, len);
}

void test_bin_get_output_names_values()
{
    uint8_t req[] = { TS_GET, 0x18, ID_OUTPUT, 0xA0 };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    char resp_hex[] =
        "85 A3 "     // successful response: map with 3 elements
        "65 42 61 74 5F 56 "
        "FA 41 61 99 9A "        // 14.1
        "65 42 61 74 5F 41 "
        "FA 40 A4 28 F6 "        // 5.13
        "6C 41 6D 62 69 65 6E 74 5F 64 65 67 43 "
        "16";

    uint8_t resp_expected[100];
    int len = hex2bin(resp_hex, resp_expected, sizeof(resp_expected));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(resp_expected, resp, len);
}

void test_bin_patch_multiple_nodes()
{
    char req_hex[] =
        "07 18 30 "
        #if TS_64BIT_TYPES_SUPPORT
        "A9 "      // write map with 9 elements
        "19 60 01 01 "                  // value 1
        "19 60 02 02 "
        #else
        "A7 "      // write map with 7 elements
        #endif
        "19 60 03 03 "
        "19 60 04 04 "
        "19 60 05 05 "
        "19 60 06 06 "
        "19 60 07 fa 40 fc 7a e1 "      // float32 7.89
        "19 60 08 f5 "                  // true
        "19 60 09 64 74 65 73 74 ";        // string "test"

    uint8_t req_bin[100];
    int len = hex2bin(req_hex, req_bin, sizeof(req_bin));

    uint8_t resp[100];
    ts.process(req_bin, len, resp, sizeof(resp));
    TEST_ASSERT_EQUAL_HEX8(TS_STATUS_CHANGED, resp[0]);
}

void test_bin_fetch_multiple_nodes()
{
    f32 = 7.89;

    char req_hex[] =
        "05 18 30 "
        #if TS_64BIT_TYPES_SUPPORT
        "89 "      // read array with 9 elements
        "19 60 01 "
        "19 60 02 "
        #else
        "87 "      // read array with 7 elements
        #endif
        "19 60 03 "
        "19 60 04 "
        "19 60 05 "
        "19 60 06 "
        "19 60 07 "
        "19 60 08 "
        "19 60 09 ";

    uint8_t req_bin[100];
    int len = hex2bin(req_hex, req_bin, sizeof(req_bin));

    uint8_t resp[100];
    ts.process(req_bin, len, resp, sizeof(resp));

    char resp_hex[] =
        #if TS_64BIT_TYPES_SUPPORT
        "85 89 "     // successful response: array with 9 elements
        "01 "                  // value 1
        "02 "
        #else
        "85 87 "     // successful response: array with 7 elements
        #endif
        "03 "
        "04 "
        "05 "
        "06 "
        "fa 40 fc 7a e1 "      // float32 7.89
        "f5 "                  // true
        "64 74 65 73 74 ";        // string "test"

    uint8_t resp_expected[100];
    len = hex2bin(resp_hex, resp_expected, sizeof(resp_expected));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(resp_expected, resp, len);
}

void test_bin_patch_float_array()
{
    float *arr = (float *)float32_array.ptr;
    arr[0] = 0;
    arr[1] = 0;

    uint8_t req[] = {
        TS_PATCH,
        0x18, ID_CONF,
        0xA1,
            0x19, 0x70, 0x04,
            0x82,
                0xFA, 0x40, 0x11, 0x47, 0xAE,       // 2.27
                0xFA, 0x40, 0x5C, 0x28, 0xF6        // 3.44
    };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    TEST_ASSERT_EQUAL_HEX8(TS_STATUS_CHANGED, resp[0]);
    TEST_ASSERT_EQUAL_FLOAT(2.27, arr[0]);
    TEST_ASSERT_EQUAL_FLOAT(3.44, arr[1]);
}

void test_bin_fetch_float_array()
{
    float *arr = (float *)float32_array.ptr;
    arr[0] = 2.27;
    arr[1] = 3.44;

    uint8_t req[] = {
        TS_FETCH,
        0x18, ID_CONF,
        0x19, 0x70, 0x04
    };

    uint8_t resp_expected[] = {
        TS_STATUS_CONTENT,
        0x82,
        0xFA, 0x40, 0x11, 0x47, 0xAE,
        0xFA, 0x40, 0x5C, 0x28, 0xF6
    };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(resp_expected, resp, sizeof(resp_expected));
}

void test_bin_fetch_rounded_float()
{
    f32 = 8.4;

    uint8_t req[] = {
        TS_FETCH,
        0x18, ID_CONF,
        0x19, 0x60, 0x0A
    };

    uint8_t resp_expected[] = {
        TS_STATUS_CONTENT,
        0x08
    };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(resp_expected, resp, sizeof(resp_expected));
}

void test_bin_patch_rounded_float()
{
    f32 = 0;

    uint8_t req[] = {
        TS_PATCH,
        0x18, ID_CONF,
        0xA1,
            0x19, 0x60, 0x0A,
            0x05
    };

    uint8_t resp[1];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    TEST_ASSERT_EQUAL_HEX8(TS_STATUS_CHANGED, resp[0]);
    TEST_ASSERT_EQUAL_FLOAT(5.0, f32);
}

void test_bin_pub()
{
    uint8_t bin[100];
    ts.bin_pub(bin, sizeof(bin), PUB_SER);

    TEST_ASSERT_EQUAL_UINT8(TS_PUBMSG, bin[0]);

    char hex_expected[] =
        "1F A4 "     // map with 4 elements
        "18 1A 1A 00 BC 61 4E "     // int 12345678
        "18 71 FA 41 61 99 9a "     // float 14.10
        "18 72 FA 40 a4 28 f6 "     // float 5.13
        "18 73 16 ";                // int 22

    uint8_t bin_expected[100];
    int len = hex2bin(hex_expected, bin_expected, sizeof(bin_expected));

    TEST_ASSERT_EQUAL_HEX8_ARRAY(bin_expected, bin, len);
}

void test_bin_pub_can()
{
    int start_pos = 0;
    uint32_t msg_id;
    uint8_t can_data[8];

    uint8_t Bat_V_hex[] = { 0xFA, 0x41, 0x61, 0x99, 0x9a };
    uint8_t Bat_A_hex[] = { 0xFA, 0x40, 0xa4, 0x28, 0xf6 };

    // first call (should return Bat_V)
    int len = ts.bin_pub_can(start_pos, PUB_CAN, 123, msg_id, can_data);
    TEST_ASSERT_NOT_EQUAL(-1, len);
    TEST_ASSERT_EQUAL_HEX(0x71, (msg_id & 0x00FFFF00) >> 8);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(Bat_V_hex, can_data, 5);

    // second call (should return Bat_A)
    len = ts.bin_pub_can(start_pos, PUB_CAN, 123, msg_id, can_data);
    TEST_ASSERT_NOT_EQUAL(-1, len);
    TEST_ASSERT_EQUAL_HEX(0x72, (msg_id & 0x00FFFF00) >> 8);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(Bat_A_hex, can_data, 5);

    // third call (should not find further nodes)
    len = ts.bin_pub_can(start_pos, PUB_CAN, 123, msg_id, can_data);
    TEST_ASSERT_EQUAL(-1, len);
}

void test_bin_sub()
{
    char msg_hex[] =
        "1F A2 "     // map with 4 elements
        "18 31 FA 41 61 99 9a "     // float 14.10
        "18 32 FA 40 a4 28 f6 ";    // float 5.13

    uint8_t msg_bin[100];
    int len = hex2bin(msg_hex, msg_bin, sizeof(msg_bin));

    int ret = ts.bin_sub(msg_bin, len, TS_WRITE_MASK, PUB_SER);

    TEST_ASSERT_EQUAL_HEX8(TS_STATUS_CHANGED, ret);
}

extern bool dummy_called_flag;

void test_bin_exec()
{
    dummy_called_flag = 0;

    uint8_t req[] = {
        TS_POST,
        0x19, 0x50, 0x01,       // node ID as endpoint
        0x80                    // empty array (no parameters)
    };

    uint8_t resp[100];
    ts.process(req, sizeof(req), resp, sizeof(resp));

    TEST_ASSERT_EQUAL_HEX8(TS_STATUS_VALID, resp[0]);
    TEST_ASSERT_EQUAL(1, dummy_called_flag);
}

void test_bin_num_elem()
{
    uint8_t req[] = { 0xB9, 0xF0, 0x00 };

    uint16_t num_elements;
    cbor_num_elements(req, &num_elements);
    TEST_ASSERT_EQUAL(0xF000, num_elements);
}

void test_bin_serialize_long_string()
{
    char str[300];
    uint8_t buf[302];

    for (unsigned int i = 0; i < sizeof(str); i++) {
        str[i] = 'T';
    }
    str[299] = '\0';

    int len_total = cbor_serialize_string(buf, str, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT(302, len_total);
    TEST_ASSERT_EQUAL_UINT(0x79, buf[0]);
    TEST_ASSERT_EQUAL_UINT(0x01, buf[1]);   // 0x01 << 8 + 0x2C = 299
    TEST_ASSERT_EQUAL_UINT(0x2B, buf[2]);
}

void tests_binary_mode()
{
    UNITY_BEGIN();

    // GET request
    RUN_TEST(test_bin_get_output_ids);
    RUN_TEST(test_bin_get_output_names);
    RUN_TEST(test_bin_get_output_names_values);

    // PATCH request
    RUN_TEST(test_bin_patch_multiple_nodes);
    RUN_TEST(test_bin_patch_float_array);
    RUN_TEST(test_bin_patch_rounded_float);     // writes an integer to float

    // FETCH request
    RUN_TEST(test_bin_fetch_multiple_nodes);
    RUN_TEST(test_bin_fetch_float_array);
    RUN_TEST(test_bin_fetch_rounded_float);

    // POST request
    RUN_TEST(test_bin_exec);

    // pub/sub messages
    RUN_TEST(test_bin_pub);
    RUN_TEST(test_bin_pub_can);
    RUN_TEST(test_bin_sub);

    // general tests
    RUN_TEST(test_bin_num_elem);
    RUN_TEST(test_bin_serialize_long_string);

    UNITY_END();
}
