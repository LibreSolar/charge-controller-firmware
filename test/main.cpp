#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "battery.h"            // battery settings
#include "charger.h"            // charger state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "uext.h"               // communication interfaces, displays, etc. in UEXT connector
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "log.h"                // log data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet

#include "tests.h"

dcdc_t dcdc = {};
power_port_t hs_port = {};       // high-side (solar for typical MPPT)
power_port_t ls_port = {};       // low-side (battery for typical MPPT)
power_port_t *bat_port = NULL;
pwm_switch_t pwm_switch = {};   // only necessary for PWM charger
battery_conf_t bat_conf;        // actual (used) battery configuration
battery_conf_t bat_conf_user;   // temporary storage where the user can write to
battery_state_t bat_state;      // battery state information
load_output_t load;
log_data_t log_data;
extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

int main() {
    charger_tests();

    // TODO
    //battery_tests();
    //load_tests();
}