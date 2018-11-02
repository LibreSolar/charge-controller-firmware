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

#include "config.h"

#ifdef GSM_ENABLED     // otherwise don't compile code to reduce firmware size

#include "pcb.h"
#include "interface.h"
#include "data_objects.h"
#include "GSM_MQTT.h"

Serial serial_uext(PIN_UEXT_TX, PIN_UEXT_RX, 9600);
GSM_MQTT MQTT(serial_uext, 120);

extern Serial serial;  // tx, rx for USB Debug on PC

int last_call = 0;
const int interval = 60*5;  // 5 minutes

void gsm_init(void)
{
    MQTT.begin();
}

void gsm_process(void)
{
    if (time(NULL) > last_call + interval) {
        last_call = time(NULL);
        if (MQTT.available())
        {
            serial.printf("Publishing MQTT test message\n");
            MQTT.publish("MyTopic", "Hello");
        }
        MQTT.processing();
    }
}

#endif /* GSM_ENABLED */