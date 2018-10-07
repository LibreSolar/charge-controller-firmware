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

#ifndef __TS_CONFIG_H_
#define __TS_CONFIG_H_


/* Define endian-ness of CPU (little endian for AVR, x86 and most ARM, e.g. STM32)
 * (if not already defined by your framework)
 */
#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
#define LITTLE_ENDIAN
//#define BIG_ENDIAN
#endif

/* Maximum number of expected JSON tokens (i.e. arrays, map keys, values,
 * primitives, etc.)
 * 
 * Thingset throws an error if maximum number of tokens is reached in a 
 * request or response.
 */
#define TS_NUM_JSON_TOKENS 50

/* Suggested buffer length for thingset requests and responses
 */
#define TS_REQ_BUFFER_LEN 200
#define TS_RESP_BUFFER_LEN 200

/* If verbose status messages are switched on, a response in text-based mode
 * contains not only the status code, but also a message.
 */
#define TS_VERBOSE_STATUS_MESSAGES

/* Switch on support for 64 bit variable types (uint64_t, int64_t, double)
 *
 * This should be commented out for most 8-bit microcontrollers to increase
 * performance
 */
//#define TS_64BIT_TYPES_SUPPORT

/* Macros for conversion between host and network byte order
 */
#if defined(__htonl) && defined(__ntohl)
    #define htons  __htons
    #define htonl  __htonl
    #define ntohs  __ntohs
    #define ntohl  __ntohl
#elif defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)
    #define htons(A) (A)
    #define htonl(A) (A)
    #define ntohs(A) (A)
    #define ntohl(A) (A)
#elif defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
    #define htons(A) ((((uint16_t)(A) & 0xff00) >> 8) | \
                      (((uint16_t)(A) & 0x00ff) << 8))
    #define htonl(A) ((((uint32_t)(A) & 0xff000000) >> 24) | \
                      (((uint32_t)(A) & 0x00ff0000) >> 8)  | \
                      (((uint32_t)(A) & 0x0000ff00) << 8)  | \
                      (((uint32_t)(A) & 0x000000ff) << 24))
    #define ntohs  htons
    #define ntohl  htonl
#else
    #error "Must define one of BIG_ENDIAN or LITTLE_ENDIAN"
#endif


#endif /* __TS_CONFIG_H_ */