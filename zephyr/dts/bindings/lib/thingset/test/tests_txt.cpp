/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "tests.h"
#include "unity.h"

#include "thingset.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern uint8_t req_buf[];
extern uint8_t resp_buf[];
extern ThingSet ts;

extern float f32;
extern int32_t i32;
extern ArrayInfo int32_array;
extern ArrayInfo float32_array;
extern bool b;

extern bool pub_serial_enable;
extern ArrayInfo pub_serial_array;

void test_txt_get_output_names()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?output/");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. [\"Bat_V\",\"Bat_A\",\"Ambient_degC\"]", resp_buf);
}

void test_txt_get_output_names_values()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?output");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. {\"Bat_V\":14.10,\"Bat_A\":5.13,\"Ambient_degC\":22}", resp_buf);
}

void test_txt_fetch_array()
{
    f32 = 52.80;
    b = false;
    i32 = 50;

    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?conf [\"f32\",\"bool\",\"i32\"]");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. [52.80,false,50]", resp_buf);
}

void test_txt_fetch_rounded()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?conf \"f32_rounded\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. 53", resp_buf);
}

void test_txt_fetch_int32_array()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?conf [\"arrayi32\"]");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. [[4,2,8,4]]", resp_buf);
}

void test_txt_fetch_float_array()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?conf [\"arrayfloat\"]");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. [[2.27,3.44]]", resp_buf);
}

void test_txt_patch_wrong_data_structure()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!conf [\"f32\":54.3");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A0 Bad Request.", resp_buf);

    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!conf{\"f32\":54.3}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A4 Not Found.", resp_buf);
}

void test_txt_patch_array()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {    \"f32\" : 52.8,\"i32\":50.6}");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);
    TEST_ASSERT_EQUAL_FLOAT(52.8, f32);
    TEST_ASSERT_EQUAL(50, i32);
}

void test_txt_patch_readonly()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=test {\"i32_readonly\" : 52}");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A3 Forbidden.", resp_buf);
}

void test_txt_patch_wrong_path()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=info {\"i32\" : 52}");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A4 Not Found.", resp_buf);
}

void test_txt_patch_unknown_node()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"i3\" : 52}");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A4 Not Found.", resp_buf);
}

bool conf_callback_called;

void conf_callback(void)        // implement function as defined in test_data.h
{
    conf_callback_called = 1;
}

void test_txt_conf_callback()
{
    conf_callback_called = 0;
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"i32\":52}");

    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);
    TEST_ASSERT_EQUAL(1, conf_callback_called);
}

bool dummy_called_flag;

void dummy(void)
{
    dummy_called_flag = 1;
}

void test_txt_exec()
{
    dummy_called_flag = 0;

    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!exec/dummy");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":83 Valid.", resp_buf);
    TEST_ASSERT_EQUAL(1, dummy_called_flag);
}

void test_txt_pub_msg()
{
    int resp_len = ts.txt_pub((char *)resp_buf, TS_RESP_BUFFER_LEN, PUB_SER);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(
        "# {\"Timestamp_s\":12345678,\"Bat_V\":14.10,\"Bat_A\":5.13,\"Ambient_degC\":22}",
        resp_buf);
}

void test_txt_pub_list_channels()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?pub/");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":85 Content. [\"serial\",\"can\"]", resp_buf);
}

void test_txt_pub_enable()
{
    pub_serial_enable = false;
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=pub/serial {\"Enable\":true}");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);
    TEST_ASSERT_EQUAL(pub_serial_enable, true);
}

void test_txt_pub_delete_append_node()
{
    // before change
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?pub/serial/IDs");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(
        ":85 Content. [\"Timestamp_s\",\"Bat_V\",\"Bat_A\",\"Ambient_degC\"]", resp_buf);

    // delete "Ambient_degC"
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "-pub/serial/IDs \"Ambient_degC\"");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":82 Deleted.", resp_buf);

    // check if it was deleted
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?pub/serial/IDs");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(
        ":85 Content. [\"Timestamp_s\",\"Bat_V\",\"Bat_A\"]", resp_buf);

    // append "Ambient_degC" again
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "+pub/serial/IDs \"Ambient_degC\"");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":81 Created.", resp_buf);

    // check if it was appended
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "?pub/serial/IDs");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(
        ":85 Content. [\"Timestamp_s\",\"Bat_V\",\"Bat_A\",\"Ambient_degC\"]", resp_buf);
}

void test_txt_auth_user()
{
    // authorize as expert user
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!auth \"expert123\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":83 Valid.", resp_buf);

    // write expert user data
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"secret_expert\" : 10}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);

    // attempt to write maker data
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"secret_maker\" : 10}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A1 Unauthorized.", resp_buf);
}

void test_txt_auth_root()
{
    // authorize as maker
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!auth \"maker456\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":83 Valid.", resp_buf);

    // write expert user data
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"secret_expert\" : 10}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);

    // write maker data
    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"secret_maker\" : 10}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":84 Changed.", resp_buf);
}

void test_txt_auth_long_password()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!auth \"012345678901234567890123456789\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":AF Unsupported Content-Format.", resp_buf);
}

void test_txt_auth_failure()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!auth \"abc\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":83 Valid.", resp_buf);

    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"secret_expert\" : 10}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A1 Unauthorized.", resp_buf);
}

void test_txt_auth_reset()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!auth \"expert123\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":83 Valid.", resp_buf);

    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!auth \"wrong\"");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":83 Valid.", resp_buf);

    req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "=conf {\"secret_expert\" : 10}");
    resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A1 Unauthorized.", resp_buf);
}

void test_txt_wrong_command()
{
    size_t req_len = snprintf((char *)req_buf, TS_REQ_BUFFER_LEN, "!abcd \"f32\"");
    int resp_len = ts.process(req_buf, req_len, resp_buf, TS_RESP_BUFFER_LEN);
    TEST_ASSERT_EQUAL(strlen((char *)resp_buf), resp_len);
    TEST_ASSERT_EQUAL_STRING(":A4 Not Found.", resp_buf);
}

void test_txt_get_endpoint()
{
    const DataNode *node;

    node = ts.get_endpoint("conf", strlen("conf"));
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(node->id, ID_CONF);

    node = ts.get_endpoint("conf/", strlen("conf/"));
    TEST_ASSERT_NOT_NULL(node);
    TEST_ASSERT_EQUAL(node->id, ID_CONF);
}

void tests_text_mode()
{
    UNITY_BEGIN();

    // GET request
    RUN_TEST(test_txt_get_output_names);
    RUN_TEST(test_txt_get_output_names_values);

    // FETCH request
    RUN_TEST(test_txt_fetch_array);
    RUN_TEST(test_txt_fetch_rounded);
    RUN_TEST(test_txt_fetch_int32_array);
    RUN_TEST(test_txt_fetch_float_array);

    // PATCH request
    RUN_TEST(test_txt_patch_wrong_data_structure);
    RUN_TEST(test_txt_patch_array);
    RUN_TEST(test_txt_patch_readonly);
    RUN_TEST(test_txt_patch_wrong_path);
    RUN_TEST(test_txt_patch_unknown_node);
    RUN_TEST(test_txt_conf_callback);

    // POST request
    RUN_TEST(test_txt_exec);

    // pub/sub messages
    RUN_TEST(test_txt_pub_msg);
    RUN_TEST(test_txt_pub_list_channels);
    RUN_TEST(test_txt_pub_enable);
    RUN_TEST(test_txt_pub_delete_append_node);

    // authentication
    RUN_TEST(test_txt_auth_user);
    RUN_TEST(test_txt_auth_root);
    RUN_TEST(test_txt_auth_long_password);
    RUN_TEST(test_txt_auth_failure);
    RUN_TEST(test_txt_auth_reset);

    // general tests
    RUN_TEST(test_txt_wrong_command);
    RUN_TEST(test_txt_get_endpoint);

    UNITY_END();
}
