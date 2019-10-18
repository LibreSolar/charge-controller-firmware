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

#ifndef HARDWARE_H
#define HARDWARE_H

/** @file
 *
 * @brief
 * Hardware-specific functions like timers, watchdog, bootloader
 */

/** Initialization of IWDG
 *
 * @param timeout Timeout in seconds
 */
void init_watchdog(float timeout);

/** Feeds/kicks the watchdog
 */
void feed_the_dog();

/** Timer for system control (main DC/DC or PWM control loop) is started
 *
 * @param freq_Hz Frequency in Hz (10 Hz suggested)
 */
void control_timer_start(int freq_Hz);

/** DC/DC or PWM control loop (implemented in main.cpp)
 */
void system_control();

/** Reset device and start STM32 internal bootloader
 */
void start_stm32_bootloader();

#endif /* HARDWARE_H */