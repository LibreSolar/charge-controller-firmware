/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
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

#ifndef _PCB_H_
#define _PCB_H_

/* This file includes the PCB specific settings depending on the PCB selected
 * as a compiler define flag in platformio.h
 */

// generic settings (for all charge controller boards)
#define LOW_SIDE_VOLTAGE_MAX    32
#define HIGH_SIDE_VOLTAGE_MAX   55

#if defined(PCB_LS_005)
    #include "pcb_ls_005.h"
#elif defined(PCB_LS_010)
    #include "pcb_ls_010.h"
#elif defined(PCB_CS_02)
    #include "pcb_cs_02.h"
#elif defined(PCB_CS_04)
    #include "pcb_cs_04.h"
#else
    #error "PCB has to be specified!"
#endif

#endif /* _PCB_H_ */
