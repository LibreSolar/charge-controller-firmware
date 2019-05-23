#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "uext.h"               // communication interfaces, displays, etc. in UEXT connector
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "log.h"                // log data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet

#include "tests.h"

dcdc_t dcdc = {};
dc_bus_t hs_bus = {};       // high-side (solar for typical MPPT)
dc_bus_t ls_bus = {};       // low-side (battery for typical MPPT)
dc_bus_t *bat_port = NULL;
dc_bus_t load_bus = {};         // load terminal
pwm_switch_t pwm_switch = {};   // only necessary for PWM charger
battery_conf_t bat_conf;        // actual (used) battery configuration
battery_conf_t bat_conf_user;   // temporary storage where the user can write to
charger_t charger;              // battery state information
load_output_t load;
log_data_t log_data;
extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

int main()
{
    adc_tests();
    bat_charger_tests();
    dc_bus_tests();

    // TODO
    //battery_tests();
    //load_tests();
}