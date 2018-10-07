
#include "thingset.h"
#include "cbor.h"
#include "test_data.h"
#include "unity.h"

void cbor_write_float()
{
    str_buffer_t req, resp;

    union float2bytes { float f; char b[4]; } f2b;
    f2b.f = 54.0;

    char req2[] = { TS_FUNCTION_WRITE, 101, 0, TS_T_FLOAT32,  // Obj ID 101
        f2b.b[0], f2b.b[1], f2b.b[2], f2b.b[3] };

    memcpy(req.data, req2, sizeof(req2));
    req.pos = sizeof(req2);
    thingset_process(&req, &resp, &data);

    TEST_ASSERT_EQUAL(0x80 + TS_STATUS_SUCCESS, resp.data[0]);
    TEST_ASSERT_EQUAL(1, resp.pos);
}

// returns length of read value
int _cbor_write_read_test(uint16_t id, char *value_write, char *value_read)
{
    str_buffer_t req, resp;

    int value_len = cbor_size((uint8_t*)value_write);

    // generate write request
    req.data[0] = TS_FUNCTION_WRITE;
    req.data[1] = id >> 8;
    req.data[2] = (uint8_t)id;
    memcpy(req.data + 3, value_write, value_len);
    req.pos = value_len + 3;
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL_UINT8(TS_STATUS_SUCCESS, resp.data[0] - 0x80);
    //printf("TEST: Write request len: %d, response len: %d\n", req.pos, resp.pos);

    // generate read request
    req.data[0] = TS_FUNCTION_READ;
    req.pos = 3; // data object ID same as above
    thingset_process(&req, &resp, &data);
    TEST_ASSERT_EQUAL_UINT8(TS_STATUS_SUCCESS, resp.data[0] - 0x80);
    //printf("TEST: Read request len: %d, response len: %d\n", req.pos, resp.pos);

    value_len = cbor_size((uint8_t*)resp.data + 1);
    memcpy(value_read, resp.data + 1, value_len);
    return value_len;
}

/*
void cbor_read_int()
{
    char req[3] = { TS_READ, 102, 0 };       // Obj ID 102
    char resp[100];
    int len = thingset_request(req, 3, resp, 100);

    TEST_ASSERT_EQUAL(TS_READ + 128, resp[0]);
    TEST_ASSERT_EQUAL(TS_T_DECIMAL_FRAC, resp[1]);
    TEST_ASSERT_EQUAL(0, resp[2]);              // exponent
    TEST_ASSERT_EQUAL(7, len);

    union { int32_t i; uint8_t b[4]; } i2b;
    i2b.b[0] = resp[3];
    i2b.b[1] = resp[4];
    i2b.b[2] = resp[5];
    i2b.b[3] = resp[6];
    TEST_ASSERT_EQUAL_INT32(61, i2b.i);
}

void cbor_get_data_object_name()
{
    char req[3] = { TS_OBJ_NAME, 0x01, 0x00 };   // Obj ID 1
    char resp[100];
    int len = thingset_request(req, 3, resp, 100);

    TEST_ASSERT_EQUAL(TS_OBJ_NAME + 128, resp[0]);
    TEST_ASSERT_EQUAL(T_STRING, resp[1]);
    TEST_ASSERT_EQUAL(sizeof("vSolar")-1, len - 2);     // protocol without nullbyte
    TEST_ASSERT_EQUAL_STRING_LEN("vSolar", &(resp[2]), len - 2);
}
*/
/*
void test_list()
{
    uint8_t req[2] = { TS_LIST, TS_C_CAL };        // category: calibration settings
    uint8_t resp[100];
    int len = thingset_request(req, 2, resp, 100);

    TEST_ASSERT_EQUAL(TS_LIST + 128, resp[0]);
    TEST_ASSERT_EQUAL(TS_T_UINT16, resp[1]);
    TEST_ASSERT_EQUAL(2 + 6*2, len);

    int i = 2;
    TEST_ASSERT_EQUAL_UINT16(100, (resp[i++] + ((uint16_t)resp[i++] << 8)));
    TEST_ASSERT_EQUAL_UINT16(101, (resp[i++] + ((uint16_t)resp[i++] << 8)));
    TEST_ASSERT_EQUAL_UINT16(102, (resp[i++] + ((uint16_t)resp[i++] << 8)));
    TEST_ASSERT_EQUAL_UINT16(103, (resp[i++] + ((uint16_t)resp[i++] << 8)));
    TEST_ASSERT_EQUAL_UINT16(104, (resp[i++] + ((uint16_t)resp[i++] << 8)));
    TEST_ASSERT_EQUAL_UINT16(105, (resp[i++] + ((uint16_t)resp[i++] << 8)));
}

void test_list_small_buffer()
{
    uint8_t req[2] = { TS_LIST, 0 };        // category 0
    uint8_t resp[10];
    int len = req.data(req, 2, resp, 10);

    TEST_ASSERT_EQUAL(2, len);
    TEST_ASSERT_EQUAL(TS_LIST + 128, resp[0]);
    TEST_ASSERT_EQUAL(TS_STATUS_RESPONSE_TOO_LONG, resp[1]);
}
*/