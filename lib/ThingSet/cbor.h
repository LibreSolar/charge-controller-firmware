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

#ifndef __CBOR_H_
#define __CBOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ts_config.h"

#define CBOR_TYPE_MASK          0xE0    /* top 3 bits */
#define CBOR_INFO_MASK          0x1F    /* low 5 bits */

#define CBOR_BYTE_FOLLOWS       24      /* indicator that the next byte is part of this item */

/* Jump Table for Initial Byte (cf. table 5) */
#define CBOR_UINT       0x00            /* type 0 */
#define CBOR_NEGINT     0x20            /* type 1 */
#define CBOR_BYTES      0x40            /* type 2 */
#define CBOR_TEXT       0x60            /* type 3 */
#define CBOR_ARRAY      0x80            /* type 4 */
#define CBOR_MAP        0xA0            /* type 5 */
#define CBOR_TAG        0xC0            /* type 6 */
#define CBOR_7          0xE0            /* type 7 (float and other types) */

/* Major types (cf. section 2.1) */
/* Major type 0: Unsigned integers */
#define CBOR_UINT8_FOLLOWS      24      /* 0x18 */
#define CBOR_UINT16_FOLLOWS     25      /* 0x19 */
#define CBOR_UINT32_FOLLOWS     26      /* 0x1a */
#define CBOR_UINT64_FOLLOWS     27      /* 0x1b */

/* Indefinite Lengths for Some Major types (cf. section 2.2) */
#define CBOR_VAR_FOLLOWS        31      /* 0x1f */

/* Major type 6: Semantic tagging */
#define CBOR_DATETIME_STRING_FOLLOWS        0
#define CBOR_DATETIME_EPOCH_FOLLOWS         1

/* Major type 7: Float and other types */
#define CBOR_FALSE      (CBOR_7 | 20)
#define CBOR_TRUE       (CBOR_7 | 21)
#define CBOR_NULL       (CBOR_7 | 22)
#define CBOR_UNDEFINED  (CBOR_7 | 23)
/* CBOR_BYTE_FOLLOWS == 24 */
#define CBOR_FLOAT16    (CBOR_7 | 25)
#define CBOR_FLOAT32    (CBOR_7 | 26)
#define CBOR_FLOAT64    (CBOR_7 | 27)
#define CBOR_BREAK      (CBOR_7 | 31)


/* Serialization (C values to CBOR data)
 */

#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_serialize_uint(uint8_t *data, uint64_t value, size_t max_len);
#else
int cbor_serialize_uint(uint8_t *data, uint32_t value, size_t max_len);
#endif

#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_serialize_int(uint8_t *data, int64_t value, size_t max_len);
#else
int cbor_serialize_int(uint8_t *data, int32_t value, size_t max_len);
#endif

int cbor_serialize_float(uint8_t *data, float value, size_t max_len);

int cbor_serialize_bool(uint8_t *data, bool value, size_t max_len);

int cbor_serialize_string(uint8_t *data, char *value, size_t max_len);

/* Deserialization (CBOR data to C values)
 */

#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_deserialize_uint64(uint8_t *data, uint64_t *value);
int cbor_deserialize_int64(uint8_t *data, int64_t *value);
#endif

int cbor_deserialize_uint32(uint8_t *data, uint32_t *value);
int cbor_deserialize_int32(uint8_t *data, int32_t *value);

int cbor_deserialize_uint16(uint8_t *data, uint16_t *value);
int cbor_deserialize_int16(uint8_t *data, int16_t *value);

int cbor_deserialize_float(uint8_t *data, float *value);

int cbor_deserialize_bool(uint8_t *data, bool *value);

int cbor_deserialize_string(uint8_t *data, char *value, uint16_t buf_size);

/* Determine the size of the cbor data item starting at given pointer
 */
int cbor_size(uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* __CBOR_H_ */