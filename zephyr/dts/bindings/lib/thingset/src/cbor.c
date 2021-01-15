/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#include "cbor.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_serialize_uint(uint8_t *data, uint64_t value, size_t max_len)
#else
int cbor_serialize_uint(uint8_t *data, uint32_t value, size_t max_len)
#endif
{
    if (max_len < 1) {
        return 0;   // error
    }
    else if (value < 24) {
        data[0] = CBOR_UINT | (uint8_t)value;
        //printf("serialize: value = %.2X < 24, data: %.2X\n", (uint32_t)value, data[0]);
        return 1;
    }
    else if (value <= 0xFF && max_len >= 2) {
        data[0] = CBOR_UINT | CBOR_UINT8_FOLLOWS;
        data[1] = value;
        //printf("serialize: value = 0x%.2X < 0xFF, data: %.2X %.2X\n", (uint32_t)value, data[0], data[1]);
        return 2;
    }
    else if (value <= 0xFFFF && max_len >= 3) {
        data[0] = CBOR_UINT | CBOR_UINT16_FOLLOWS;
        data[1] = value >> 8;
        data[2] = value;
        //printf("serialize: value = 0x%.4X <= 0xFFFF, data: %.2X %.2X %.2X\n", (uint32_t)value, data[0], data[1], data[2]);
        return 3;
    }
    else if (value <= 0xFFFFFFFF && max_len >= 5) {
        data[0] = CBOR_UINT | CBOR_UINT32_FOLLOWS;
        data[1] = value >> 24;
        data[2] = value >> 16;
        data[3] = value >> 8;
        data[4] = value;
        //printf("serialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n", (uint32_t)value, data[0], data[1], data[2], data[3], data[4]);
        return 5;
    }
#ifdef TS_64BIT_TYPES_SUPPORT
    else if (max_len >= 9) {
        data[0] = CBOR_UINT | CBOR_UINT64_FOLLOWS;
        data[1] = (value >> 32) >> 24;
        data[2] = (value >> 32) >> 16;
        data[3] = (value >> 32) >> 8;
        data[4] = (value >> 32);
        data[5] = value >> 24;
        data[6] = value >> 16;
        data[7] = value >> 8;
        data[8] = value;
        return 9;
    }
#endif
    else {
        return 0;
    }
}

#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_serialize_int(uint8_t *data, int64_t value, size_t max_len)
#else
int cbor_serialize_int(uint8_t *data, int32_t value, size_t max_len)
#endif
{
    if (max_len < 1) {
        return 0;
    }

    if (value >= 0) {
        return cbor_serialize_uint(data, value, max_len);
    } else {
        int size = cbor_serialize_uint(data, -1 - value, max_len);
        data[0] |= CBOR_NEGINT;    // set major type 1 for negative integer
        return size;
    }
}

int cbor_serialize_float(uint8_t *data, float value, size_t max_len)
{
    if (max_len < 5)
        return 0;

    data[0] = CBOR_FLOAT32;

    union { float f; uint32_t ui; } f2ui;
    f2ui.f = value;
    data[1] = f2ui.ui >> 24;
    data[2] = f2ui.ui >> 16;
    data[3] = f2ui.ui >> 8;
    data[4] = f2ui.ui;

    return 5;
}

int cbor_serialize_bool(uint8_t *data, bool value, size_t max_len)
{
    if (max_len < 1)
        return 0;

    data[0] = value ? CBOR_TRUE : CBOR_FALSE;
    return 1;
}

int cbor_serialize_string(uint8_t *data, const char *value, size_t max_len)
{
    unsigned int len = strlen(value);

    //printf("serialize string: \"%s\", len = %d, max_len = %d\n", value, len, max_len);

    if (len < 24 && len + 1 <= max_len) {
        data[0] = CBOR_TEXT | (uint8_t)len;
        strcpy((char*)&data[1], value);
        return len + 1;
    }
    else if (len < 0xFF && len + 2 <= max_len) {
        data[0] = CBOR_TEXT | CBOR_UINT8_FOLLOWS;
        data[1] = (uint8_t)len;
        strcpy((char*)&data[2], value);
        return len + 2;
    }
    else if (len < 0xFFFF && len + 3 <= max_len) {
        data[0] = CBOR_TEXT | CBOR_UINT16_FOLLOWS;
        data[1] = (uint16_t)len >> 8;
        data[2] = (uint16_t)len;
        strcpy((char*)&data[3], value);
        return len + 3;
    }
    else {    // string too long (more than 65535 characters)
        return 0;
    }
}

int _serialize_num_elements(uint8_t *data, size_t num_elements, size_t max_len)
{
    if (num_elements < 24 && max_len > 0) {
        data[0] |= (uint8_t)num_elements;
        return 1;
    }
    else if (num_elements < 0xFF && max_len > 1) {
        data[0] |= CBOR_UINT8_FOLLOWS;
        data[1] = (uint8_t)num_elements;
        return 2;
    }
    else if (num_elements < 0xFFFF && max_len > 1) {
        data[0] |= CBOR_UINT16_FOLLOWS;
        data[1] = (uint16_t)num_elements >> 8;
        data[2] = (uint16_t)num_elements;
        return 3;
    }
    else {    // too many elements (more than 65535)
        return 0;
    }
}

int cbor_serialize_map(uint8_t *data, size_t num_elements, size_t max_len)
{
    data[0] = CBOR_MAP;
    return _serialize_num_elements(data, num_elements, max_len);
}

int cbor_serialize_array(uint8_t *data, size_t num_elements, size_t max_len)
{
    data[0] = CBOR_ARRAY;
    return _serialize_num_elements(data, num_elements, max_len);
}

#ifdef TS_64BIT_TYPES_SUPPORT
int _cbor_uint_data(uint8_t *data, uint64_t *bytes)
#else
int _cbor_uint_data(uint8_t *data, uint32_t *bytes)
#endif
{
    uint8_t info = data[0] & CBOR_INFO_MASK;

    //printf("uint_data: %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X %.2X\n", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);

    if (info < 24) {
        *bytes = info;
        return 1;
    }
    else if (info == CBOR_UINT8_FOLLOWS) {
        *bytes = data[1];
        return 2;
    }
    else if (info == CBOR_UINT16_FOLLOWS) {
        *bytes = data[1] << 8 | data[2];
        return 3;
    }
#ifdef TS_64BIT_TYPES_SUPPORT
    else if (info == CBOR_UINT32_FOLLOWS) {
        *(uint64_t*)bytes = ((uint64_t)data[1] << 24) | ((uint64_t)data[2] << 16) |
            ((uint64_t)data[3] << 8) | ((uint64_t)data[4]);
        return 5;
    }
    else if (info == CBOR_UINT64_FOLLOWS) {
        *(uint64_t*)bytes = ((uint64_t)data[1] << 56) | ((uint64_t)data[2] << 48) |
            ((uint64_t)data[3] << 40) | ((uint64_t)data[4] << 32) | ((uint64_t)data[5] << 24) |
            ((uint64_t)data[6] << 16) | ((uint64_t)data[7] << 8) | ((uint64_t)data[8]);
        return 9;
    }
#else
    else if (info == CBOR_UINT32_FOLLOWS) {
        *bytes = data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4];
        return 5;
    }
#endif
    else {
        return 0;
    }
}

#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_deserialize_uint64(uint8_t *data, uint64_t *value)
{
    uint64_t tmp;
    int size;
    uint8_t type = data[0] & CBOR_TYPE_MASK;

    if (!value || type != CBOR_UINT)
        return 0;

    size = _cbor_uint_data(data, &tmp);
    if (size > 0 && tmp <= UINT64_MAX) {
        *value = tmp;
        return size;
    }
    return 0;
}

int cbor_deserialize_int64(uint8_t *data, int64_t *value)
{
    uint64_t tmp;
    int size;
    uint8_t type = data[0] & CBOR_TYPE_MASK;

    if (!value || (type != CBOR_UINT && type != CBOR_NEGINT))
        return 0;

    size = _cbor_uint_data(data, &tmp);
    if (size > 0) {
        if (type == CBOR_UINT) {
            if (tmp <= INT64_MAX) {
                *value = (int64_t)tmp;
                //printf("deserialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n",
                //    (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
                return size;
            }
        }
        else if (type == CBOR_NEGINT) {
            // check if CBOR negint fits into C int
            // -1 - tmp >= -INT32_MAX - 1         | x (-1)
            // 1 + tmp <= INT32_MAX + 1
            if (tmp <= INT64_MAX) {
                *value = -1 - (uint64_t)tmp;
                //printf("deserialize: value = %.8X, tmp = %.8X, data: %.2X %.2X %.2X %.2X %.2X\n",
                //  *value, (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
                return size;
            }
        }
    }

    return 0;
}
#endif


int cbor_deserialize_uint32(uint8_t *data, uint32_t *value)
{
#ifdef TS_64BIT_TYPES_SUPPORT
    uint64_t tmp;
#else
    uint32_t tmp;
#endif
    int size;
    uint8_t type = data[0] & CBOR_TYPE_MASK;

    //printf("deserialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n",
    //  (uint32_t)value, data[0], data[1], data[2], data[3], data[4]);

    if (!value || type != CBOR_UINT)
        return 0;

    size = _cbor_uint_data(data, &tmp);
    if (size > 0 && tmp <= INT32_MAX) {
        *value = tmp;
        return size;
    }
    return 0;
}

int cbor_deserialize_int32(uint8_t *data, int32_t *value)
{
#ifdef TS_64BIT_TYPES_SUPPORT
    uint64_t tmp;
#else
    uint32_t tmp;
#endif
    int size;
    uint8_t type = data[0] & CBOR_TYPE_MASK;

    if (!value || (type != CBOR_UINT && type != CBOR_NEGINT))
        return 0;

    size = _cbor_uint_data(data, &tmp);
    if (size > 0) {
        if (type == CBOR_UINT) {
            if (tmp <= INT32_MAX) {
                *value = (int32_t)tmp;
                //printf("deserialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n",
                //  (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
                return size;
            }
        }
        else if (type == CBOR_NEGINT) {
            // check if CBOR negint fits into C int
            // -1 - tmp >= -INT32_MAX - 1         | x (-1)
            // 1 + tmp <= INT32_MAX + 1
            if (tmp <= INT32_MAX) {
                *value = -1 - (uint32_t)tmp;
                //printf("deserialize: value = %.8X, tmp = %.8X, data: %.2X %.2X %.2X %.2X %.2X\n",
                //  *value, (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
                return size;
            }
        }
    }

    return 0;
}

int cbor_deserialize_uint16(uint8_t *data, uint16_t *value)
{
    uint32_t tmp;
    int size = cbor_deserialize_uint32(data, &tmp); // also checks value for null-pointer

    if (size > 0 && tmp <= UINT16_MAX) {
        *value = tmp;
        return size;
    }
    return 0;
}

int cbor_deserialize_int16(uint8_t *data, int16_t *value)
{
    int32_t tmp;
    int size = cbor_deserialize_int32(data, &tmp); // also checks value for null-pointer

    if (size > 0 && tmp <= INT16_MAX && tmp >= INT16_MIN) {
        *value = tmp;
        return size;
    }
    return 0;
}

int cbor_deserialize_decimal_fraction(uint8_t *data, int32_t *mantissa, int32_t exponent)
{
    // ToDo: implementation
    return 0;
}

int cbor_deserialize_float(uint8_t *data, float *value)
{
    if (!value) {
        return 0;
    }

    uint8_t type = data[0] & CBOR_TYPE_MASK;
    if (type == CBOR_UINT) {
#ifdef TS_64BIT_TYPES_SUPPORT
        uint64_t tmp;
        int len = cbor_deserialize_uint64(data, &tmp);
        *value = (float)tmp;
        return len;
#else
        uint32_t tmp;
        int len = cbor_deserialize_uint32(data, &tmp);
        *value = (float)tmp;
        return len;
#endif
    }
    else if (type == CBOR_NEGINT) {
#ifdef TS_64BIT_TYPES_SUPPORT
        int64_t tmp;
        int len = cbor_deserialize_int64(data, &tmp);
        *value = (float)tmp;
        return len;
#else
        int32_t tmp;
        int len = cbor_deserialize_int32(data, &tmp);
        *value = (float)tmp;
        return len;
#endif
    }
    else if (data[0] == CBOR_FLOAT32) {
        union { float f; uint32_t ui; } f2ui;
        f2ui.ui = data[1] << 24 | data[2] << 16 | data[3] << 8 | data[4];
        *value = f2ui.f;
        return 5;
    }
    return 0;
}

int cbor_deserialize_bool(uint8_t *data, bool *value)
{
    if (!value || !data)
        return 0;

    if (data[0] == CBOR_TRUE) {
        *value = true;
        return 1;
    }
    else if (data[0] == CBOR_FALSE) {
        *value = false;
        return 1;
    }
    return 0;
}

int cbor_deserialize_string(uint8_t *data, char *value, uint16_t buf_size)
{
    uint8_t type = data[0] & CBOR_TYPE_MASK;
    uint8_t info = data[0] & CBOR_INFO_MASK;
    uint16_t len;

    //printf("deserialize string: \"%s\", len = %d, max_len = %d\n", (char*)&data[1], len, buf_size);

    if (!value || type != CBOR_TEXT)
        return 0;

    if (info < 24) {
        len = info;
        if (len < buf_size) {
            strncpy(value, (char*)&data[1], len);
            value[len] = '\0';
            //printf("deserialize string: \"%s\", len = %d, max_len = %d\n", (char*)&data[1], len, buf_size);
            return len + 1;
        }
    }
    else if (info == CBOR_UINT8_FOLLOWS) {
        len = data[1];
        if (len < buf_size) {
            strncpy(value, (char*)&data[2], len);
            value[len] = '\0';
            return len + 1;
        }
    }
    else if (info == CBOR_UINT16_FOLLOWS) {
        len = data[1] << 8 | data[2];
        if (len < buf_size) {
            strncpy(value, (char*)&data[3], len);
            value[len] = '\0';
            return len + 1;
        }
    }
    return 0;   // longer string not supported
}

// stores size of map or array in num_elements
int cbor_num_elements(uint8_t *data, uint16_t *num_elements)
{
    uint8_t type = data[0] & CBOR_TYPE_MASK;
    uint8_t info = data[0] & CBOR_INFO_MASK;

    //printf("type: %x, info: %x\n", type, info);

    if (!num_elements)
        return 0;

    // normal type (single data element)
    if (type != CBOR_MAP && type != CBOR_ARRAY) {
        *num_elements = 1;
        return 0;
    }

    if (info < 24) {
        *num_elements = info;
        return 1;
    }
    else if (info == CBOR_UINT8_FOLLOWS) {
        *num_elements = data[1];
        return 2;
    }
    else if (info == CBOR_UINT16_FOLLOWS) {
        *num_elements = data[1] << 8 | data[2];
        return 3;
    }
    return 0;   // more map/array elements not supported
}


// determines the size of a cbor data item starting at given pointer
int cbor_size(uint8_t *data)
{
    uint8_t type = data[0] & CBOR_TYPE_MASK;
    uint8_t info = data[0] & CBOR_INFO_MASK;

    if (type == CBOR_UINT || type == CBOR_NEGINT) {
        if (info < 24)
            return 1;
        switch (info) {
        case CBOR_UINT8_FOLLOWS:
            return 2;
        case CBOR_UINT16_FOLLOWS:
            return 3;
        case CBOR_UINT32_FOLLOWS:
            return 5;
        case CBOR_UINT64_FOLLOWS:
            return 9;
        }
    }
    else if (type == CBOR_BYTES || type == CBOR_TEXT) {
        if (info < 24) {
            return info + 1;
        }
        else {
            if (info == CBOR_UINT8_FOLLOWS)
                return 1 + data[1];
            else if (info == CBOR_UINT16_FOLLOWS)
                return 1 + (data[1] << 8 | data[2]);
            else
                return 0;   // longer string / byte array not supported
        }
    }
    else if (type == CBOR_7) {
        switch (data[0]) {
        case CBOR_FALSE:
        case CBOR_TRUE:
            return 1;
            break;
        case CBOR_FLOAT32:
            return 5;
            break;
        case CBOR_FLOAT64:
            return 9;
            break;
        }
    }

    return 0;   // float16, arrays, maps, tagged types, etc. curently not supported
}
