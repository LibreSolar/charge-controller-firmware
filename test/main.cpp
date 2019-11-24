#include "thingset.h"           // handles access to internal data via communication interfaces
#include "pcb.h"                // hardware-specific settings
#include "config.h"             // user-specific configuration

#include "half_bridge.h"        // PWM generation for DC/DC converter
#include "hardware.h"           // hardware-related functions like load switch, LED control, watchdog, etc.
#include "dcdc.h"               // DC/DC converter control (hardware independent)
#include "pwm_switch.h"         // PWM charge controller
#include "bat_charger.h"        // battery settings and charger state machine
#include "adc_dma.h"            // ADC using DMA and conversion to measurement values
#include "eeprom.h"             // external I2C EEPROM
#include "load.h"               // load and USB output management
#include "leds.h"               // LED switching using charlieplexing
#include "device_status.h"      // device-level data (error memory, min/max measurements, etc.)
#include "data_objects.h"       // for access to internal data via ThingSet

#include "tests.h"

PowerPort lv_terminal;          // low voltage terminal (battery for typical MPPT)

#if FEATURE_DCDC_CONVERTER
PowerPort hv_terminal;          // high voltage terminal (solar for typical MPPT)
PowerPort dcdc_lv_port;         // internal low voltage side of DC/DC converter
Dcdc dcdc(&hv_terminal, &dcdc_lv_port, DCDC_MODE_INIT);
#endif

#if FEATURE_PWM_SWITCH
PowerPort pwm_terminal;         // external terminal of PWM switch port (normally solar)
PowerPort pwm_port_int;         // internal side of PWM switch
PwmSwitch pwm_switch(&pwm_terminal, &pwm_port_int);
PowerPort &solar_terminal = pwm_terminal;
#else
PowerPort &solar_terminal = SOLAR_TERMINAL;     // defined in config.h
#endif

#if FEATURE_LOAD_OUTPUT
PowerPort load_terminal;        // load terminal (also connected to lv_bus)
LoadOutput load(&load_terminal);
#endif

PowerPort &bat_terminal = BATTERY_TERMINAL;     // defined in config.h
#ifdef GRID_TERMINAL
PowerPort &grid_terminal = GRID_TERMINAL;
#endif

Charger charger(&lv_terminal);

BatConf bat_conf;               // actual (used) battery configuration
BatConf bat_conf_user;          // temporary storage where the user can write to

DeviceStatus dev_stat;

extern ThingSet ts;             // defined in data_objects.cpp

time_t timestamp;    // current unix timestamp (independent of time(NULL), as it is user-configurable)

int main()
{
    adc_tests();
    bat_charger_tests();
    power_port_tests();
    half_brigde_tests();
    dcdc_tests();
    device_status_tests();
    load_tests();
}
