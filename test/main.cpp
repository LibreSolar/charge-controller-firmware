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

DcBus hv_bus;
PowerPort hv_terminal(&hv_bus);         // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_port_hv(&hv_bus);        // internal high voltage side port of DC/DC converter

DcBus lv_bus;
PowerPort dcdc_port_lv(&lv_bus);        // internal low voltage side of DC/DC converter
PowerPort lv_terminal(&lv_bus);         // low voltage terminal (battery for typical MPPT)
PowerPort load_terminal(&lv_bus);       // load terminal (also connected to lv_bus)

PowerPort &bat_terminal = lv_terminal;
PowerPort &solar_terminal = hv_terminal;

Dcdc dcdc(&dcdc_port_hv, &dcdc_port_lv, DCDC_MODE_INIT);

PwmSwitch pwm_switch = {};      // only necessary for PWM charger

Charger charger(&lv_terminal);
LoadOutput load(&load_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

LogData log_data;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

int main()
{
    adc_tests();
    bat_charger_tests();
    dc_bus_tests();
    half_brigde_tests();
    dcdc_tests();
    log_tests();

    // TODO
    //load_tests();
}