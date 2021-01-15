/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2017 Martin Jäger / Libre Solar
 */

#ifndef CBOR_H_
#define CBOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "ts_config.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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

/**
 * Serialize unsigned integer value
 *
 * @param data Buffer where CBOR data shall be stored
 * @param value Variable containing value to be serialized
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_serialize_uint(uint8_t *data, uint64_t value, size_t max_len);
#else
int cbor_serialize_uint(uint8_t *data, uint32_t value, size_t max_len);
#endif

/**
 * Serialize signed integer value
 *
 * @param data Buffer where CBOR data shall be stored
 * @param value Variable containing value to be serialized
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
#ifdef TS_64BIT_TYPES_SUPPORT
int cbor_serialize_int(uint8_t *data, int64_t value, size_t max_len);
#else
int cbor_serialize_int(uint8_t *data, int32_t value, size_t max_len);
#endif

/**
 * Serialize decimal fraction (e.g. 1234 * 10^3)
 *
 * @param data Buffer where CBOR data shall be stored
 * @param mantissa Mantissa of the value to be serialized
 * @param exponent Exponent of the value to be serialized
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
int cbor_serialize_decimal_fraction(uint8_t *data, int32_t mantissa, int32_t exponent,
                                                                                size_t max_len);

/**
 * Serialize 32-bit float
 *
 * @param data Buffer where CBOR data shall be stored
 * @param value Variable containing value to be serialized
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
int cbor_serialize_float(uint8_t *data, float value, size_t max_len);


/**
 * Serialize boolean
 *
 * @param data Buffer where CBOR data shall be stored
 * @param value Variable containing value to be serialized
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
int cbor_serialize_bool(uint8_t *data, bool value, size_t max_len);

/**
 * Serialize string
 *
 * @param data Buffer where CBOR data shall be stored
 * @param value Pointer to string that should be be serialized
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
int cbor_serialize_string(uint8_t *data, const char *value, size_t max_len);

/**
 * Serialize the header (length field) of an array
 *
 * Actual elements of the array have to be serialized afterwards
 *
 * @param data Buffer where CBOR data shall be stored
 * @param num_elements Number of elements in the array
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns Number of bytes added to buffer or 0 in case of error
 */
int cbor_serialize_array(uint8_t *data, size_t num_elements, size_t max_len);

/**
 * Serialize the header (length field) of a map (equivalent to JSON object)
 *
 * Actual elements of the map have to be serialized afterwards
 *
 * @param data Buffer where CBOR data shall be stored
 * @param num_elements Number of elements in the array
 * @param max_len Maximum remaining space in buffer (i.e. max length of serialized data)
 *
 * @returns number of bytes added to buffer or 0 in case of error
 */
int cbor_serialize_map(uint8_t *data, size_t num_elements, size_t max_len);

/**
 * Deserialization (CBOR data to C values)
 */

#ifdef TS_64BIT_TYPES_SUPPORT
/**
 * Deserialize 64-bit unsigned integer
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_uint64(uint8_t *data, uint64_t *value);

/**
 * Deserialize 64-bit signed integer
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_int64(uint8_t *data, int64_t *value);
#endif

/**
 * Deserialize 32-bit unsigned integer
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_uint32(uint8_t *data, uint32_t *value);

/**
 * Deserialize 32-bit signed integer
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_int32(uint8_t *data, int32_t *value);

/**
 * Deserialize 16-bit unsigned integer
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_uint16(uint8_t *data, uint16_t *value);

/**
 * Deserialize 16-bit signed integer
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_int16(uint8_t *data, int16_t *value);

/**
 * Deserialize decimal fraction type
 *
 * The exponent is fixed, so the mantissa is multiplied to match the exponent
 *
 * @param data Buffer containing CBOR data with matching type
 * @param mantissa Pointer to the variable where the mantissa should be stored
 * @param exponent Exponent of internally used variable in C
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_decimal_fraction(uint8_t *data, int32_t *mantissa, int32_t exponent);

/**
 * Deserialize 32-bit float
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_float(uint8_t *data, float *value);

/**
 * Deserialize bool
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_bool(uint8_t *data, bool *value);

/**
 * Deserialize string
 *
 * @param data Buffer containing CBOR data with matching type
 * @param value Pointer to the variable where the value should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_deserialize_string(uint8_t *data, char *value, uint16_t buf_size);

/**
 * Determine the number of elements in a map or an array
 *
 * @param data Buffer containing CBOR data with matching type
 * @param num_elements Pointer to the variable where the result should be stored
 *
 * @returns Number of bytes read from buffer or 0 in case of error
 */
int cbor_num_elements(uint8_t *data, uint16_t *num_elements);

/**
 * Determine the size of the cbor data item
 *
 * @param data Pointer for starting point of data item
 *
 * @returns Size in bytes
 */
int cbor_size(uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* CBOR_H_ */
