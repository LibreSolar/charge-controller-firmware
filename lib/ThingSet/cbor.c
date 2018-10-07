/* ThingSet protocol library
 * Copyright (c) 2017-2018 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>  // for definition of endianness

#include "ts_config.h"
#include "cbor.h"


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
        //printf("value = %.2X < 24, data: %.2X\n", (uint32_t)value, data[0]);
        return 1;
    } 
    else if (value <= 0xFF && max_len >= 2) {
        data[0] = CBOR_UINT | CBOR_UINT8_FOLLOWS;
        data[1] = (uint8_t)value;
        //printf("value = 0x%.2X < 0xFF, data: %.2X %.2X\n", (uint32_t)value, data[0], data[1]);
        return 2;
    } 
    else if (value <= 0xFFFF && max_len >= 3) {
        data[0] = CBOR_UINT | CBOR_UINT16_FOLLOWS;
        *((uint16_t*)&data[1]) = htons((uint16_t)value);
        //printf("serialize: value = 0x%.4X <= 0xFFFF, data: %.2X %.2X %.2X\n", (uint32_t)value, data[0], data[1], data[2]);
        return 3;
    } 
    else if (value <= 0xFFFFFFFF && max_len >= 5) {
        data[0] = CBOR_UINT | CBOR_UINT32_FOLLOWS;
        *((uint32_t*)&data[1]) = htonl((uint32_t)value);
        //printf("serialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n", (uint32_t)value, data[0], data[1], data[2], data[3], data[4]);
        return 5;
    }
#ifdef TS_64BIT_TYPES_SUPPORT
    else if (max_len >= 9) {
        data[0] = CBOR_UINT | CBOR_UINT64_FOLLOWS;
        *((uint32_t*)&data[1]) = htonl((uint32_t)(value >> 32));
        *((uint32_t*)&data[5]) = htonl((uint32_t)value);
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
    *((uint32_t*)&data[1]) = htonl(*((uint32_t*)&value));
    return 5;
}

int cbor_serialize_bool(uint8_t *data, bool value, size_t max_len)
{
    if (max_len < 1)
        return 0;
    
    data[0] = value ? CBOR_TRUE : CBOR_FALSE;
    return 1;
}

int cbor_serialize_string(uint8_t *data, char *value, size_t max_len)
{
    unsigned int len = strlen(value);

    //printf("serialize string: \"%s\", len = %d, max_len = %d\n", value, len, max_len);

    if (len < 24 && len + 1 < max_len) {
        data[0] = CBOR_TEXT | (uint8_t)len;
        strcpy((char*)&data[1], value);
        return len + 1;
    }
    else if (len < 0xFF && len + 2 < max_len) {
        data[0] = CBOR_TEXT | CBOR_UINT8_FOLLOWS;
        data[1] = (uint8_t)len;
        strcpy((char*)&data[2], value);
        return len + 2;
    }
    else if (len < 0xFFFF && len + 3 < max_len) {
        data[0] = CBOR_TEXT | CBOR_UINT16_FOLLOWS;
        *((uint16_t*)&data[1]) = (uint16_t)len;
        strcpy((char*)&data[3], value);
        return len + 3;
    }
    else {    // string too long (more than 65535 characters)
        return 0;
    }
}

#ifdef TS_64BIT_TYPES_SUPPORT
int _cbor_uint_data(uint8_t *data, uint64_t *bytes)
#else
int _cbor_uint_data(uint8_t *data, uint32_t *bytes)
#endif
{
    uint8_t info = data[0] & CBOR_INFO_MASK;

    //printf("uint_data: %.2X %.2X %.2X %.2X %.2X\n", data[0], data[1], data[2], data[3], data[4]);

    if (info < 24) {
        *bytes = info;
        return 1;
    }
    else if (info == CBOR_UINT8_FOLLOWS) {
        *bytes = data[1];
        return 2;
    }
    else if (info == CBOR_UINT16_FOLLOWS) {
        *bytes = ntohs(*((uint16_t*)&data[1]));
        return 3;
    }
    else if (info == CBOR_UINT32_FOLLOWS) {
        *bytes = ntohl(*((uint32_t*)&data[1]));
        return 5;
    }
#ifdef TS_64BIT_TYPES_SUPPORT
    else if (info == CBOR_UINT64_FOLLOWS) {
        *bytes = ((uint64_t)ntohl(*((uint32_t*)&data[1])) << 32)
               + ntohl(*((uint32_t*)&data[5]));
        return 9;
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
    if (size > 0 && tmp <= INT64_MAX) {
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
                //printf("deserialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n", (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
                return size;
            }
        }
        else if (type == CBOR_NEGINT) {
            // check if CBOR negint fits into C int
            // -1 - tmp >= -INT32_MAX - 1         | x (-1)
            // 1 + tmp <= INT32_MAX + 1
            if (tmp <= INT64_MAX) {
                *value = -1 - (uint64_t)tmp;
                //printf("deserialize: value = %.8X, tmp = %.8X, data: %.2X %.2X %.2X %.2X %.2X\n", *value, (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
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

    //printf("deserialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n", (uint32_t)value, data[0], data[1], data[2], data[3], data[4]);

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
                //printf("deserialize: value = 0x%.8X <= 0xFFFFFFFF, data: %.2X %.2X %.2X %.2X %.2X\n", (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
                return size;
            }
        }
        else if (type == CBOR_NEGINT) {
            // check if CBOR negint fits into C int
            // -1 - tmp >= -INT32_MAX - 1         | x (-1)
            // 1 + tmp <= INT32_MAX + 1
            if (tmp <= INT32_MAX) {
                *value = -1 - (uint32_t)tmp;
                //printf("deserialize: value = %.8X, tmp = %.8X, data: %.2X %.2X %.2X %.2X %.2X\n", *value, (uint32_t)tmp, data[0], data[1], data[2], data[3], data[4]);
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

int cbor_deserialize_float(uint8_t *data, float *value)
{
    if (data[0] != CBOR_FLOAT32 || !value)
        return 0;
    
    uint32_t bytes = ntohl(*((uint32_t*)&data[1]));
    *value = *((float*)&bytes);
    return 5;
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

// returns strlen (excluding null-termination character)
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
            return len;
        }
    }
    else if (info == CBOR_UINT8_FOLLOWS) {
        len = data[1];
        if (len < buf_size) {
            strncpy(value, (char*)&data[2], len);
            value[len] = '\0';
            return len;
        }
    }
    else if (info == CBOR_UINT16_FOLLOWS) {
        len = ntohs(*((uint16_t*)&data[1]));
        if (len < buf_size) {
            strncpy(value, (char*)&data[3], len);
            value[len] = '\0';
            return len;
        }
    }
    else {
        return 0;   // longer string not supported
    }
}

// determines the size of the cbor data item starting at given pointer
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
                return 1 + ntohl(*((uint16_t*)&data[1]));
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
