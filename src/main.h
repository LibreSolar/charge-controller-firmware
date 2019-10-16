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

#include "config.h"

#include <stdio.h>

#include "bat_charger.h"
#include "dc_bus.h"
#include "log.h"
#include "pwm_switch.h"
#include "half_bridge.h"
#include "load.h"

extern LogData log_data;
extern Charger charger;
extern BatConf bat_conf;
extern BatConf bat_conf_user;
extern Dcdc dcdc;
extern LoadOutput load;
extern DcBus hv_terminal;
extern DcBus lv_bus_int;
extern DcBus lv_terminal;
extern DcBus load_terminal;
extern PwmSwitch pwm_switch;
