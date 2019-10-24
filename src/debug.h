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

void black_hole(const char *str, ...);

// serious errors
#if DEBUG_PRINT_FLAGS & (1 << 0)
#define print_error printf
#else
#define print_error black_hole
#endif

// warnings
#if DEBUG_PRINT_FLAGS & (1 << 1)
#define print_warning printf
#else
#define print_warning black_hole
#endif

// information only
#if DEBUG_PRINT_FLAGS & (1 << 2)
#define print_info printf
#else
#define print_info black_hole
#endif

// unit test information
#if DEBUG_PRINT_FLAGS & (1 << 3)
#define print_test printf
#else
#define print_test black_hole
#endif

// communication module special flag
#if DEBUG_PRINT_FLAGS & (1 << 4)
#define print_comms printf
#else
#define print_comms black_hole
#endif

#endif /* DEBUG_H */
