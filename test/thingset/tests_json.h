
#include "thingset.h"
#include "test_data.h"
#include "unity.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void json_wrong_command()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!abcd \"f32\"");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":31 Unknown function.", resp.data);
}

void json_write_wrong_data_structure()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write [\"f32\"]");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":33 Wrong format.", resp.data);

    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write [\"f32\":54.3");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":33 Wrong format.", resp.data);

    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write[\"f32\":54.3]");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":33 Wrong format.", resp.data);
}

void json_write_float()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write \"f32\" : 54.3");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success.", resp.data);
    TEST_ASSERT_EQUAL_FLOAT(54.3, f32);
}

void json_write_int()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write {\"i32\":61}");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success.", resp.data);
    TEST_ASSERT_EQUAL(61, i32);
}

void json_write_array()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write {    \"f32\" : 52,\"i32\":50.6}");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success.", resp.data);
    TEST_ASSERT_EQUAL_FLOAT(52.0, f32);
    TEST_ASSERT_EQUAL(50, i32);
}

void json_write_readonly()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write \"i32_output\" : 52");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":36 Unauthorized.", resp.data);
}

void json_write_unknown()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!write \"i3\" : 52");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":32 Data object not found.", resp.data);
}

void json_read_float()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!read \"f32\"");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success. 54.30", resp.data);
}

void json_read_array()
{
    str_buffer_t req, resp;
    //                                                      float        bool         int        
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!read [\"f32\", \"bool\", \"i32\"]");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success. [54.30, false, 61]", resp.data);
}

void json_list_input()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!list \"input\"");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":0 Success. [\"loadEnTarget\", \"usbEnTarget\"]", resp.data);

}

void json_list_all()
{
    str_buffer_t req, resp;
    req.pos = snprintf(req.data, TS_REQ_BUFFER_LEN, "!list ");
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL(strlen(resp.data), resp.pos);
    TEST_ASSERT_EQUAL_STRING(":38 Response too long.", resp.data);
}