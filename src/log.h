
#ifndef __LOG_H_
#define __LOG_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Log Data
 *
 * Saves maximum ever measured values to EEPROM
 */
typedef struct {
    float battery_voltage_max;
    float solar_voltage_max;
    float dcdc_current_max;
    float load_current_max;
    float temp_int;             // °C (internal MCU temperature sensor)
    float temp_int_max;         // °C
    float temp_mosfets_max;
    int day_counter;
} log_data_t;


#ifdef __cplusplus
}
#endif

#endif // __LOG_H_
