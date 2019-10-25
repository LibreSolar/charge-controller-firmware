/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
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

#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

// Debug printing macros
// see https://stackoverflow.com/questions/1644868/define-macro-for-debug-printing-in-c

// Set the bits in DEBUG_PRINT_FLAGS via platformio.ini to enable specific debug print levels

// Bit 0: Serious errors
#define print_error(fmt, ...) \
    do { if (DEBUG_PRINT_FLAGS & (1 << 0)) printf(fmt, ##__VA_ARGS__); } while (0)

// Bit 1: Warnings
#define print_warning(fmt, ...) \
    do { if (DEBUG_PRINT_FLAGS & (1 << 1)) printf(fmt, ##__VA_ARGS__); } while (0)

// Bit 2: Information only
#define print_info(fmt, ...) \
    do { if (DEBUG_PRINT_FLAGS & (1 << 2)) printf(fmt, ##__VA_ARGS__); } while (0)

// Bit 3: Unit test information
#define print_test(fmt, ...) \
    do { if (DEBUG_PRINT_FLAGS & (1 << 3)) printf(fmt, ##__VA_ARGS__); } while (0)

// Bit 4: Communication module special flag
#define print_comms(fmt, ...) \
    do { if (DEBUG_PRINT_FLAGS & (1 << 4)) printf(fmt, ##__VA_ARGS__); } while (0)

#endif /* DEBUG_H */
