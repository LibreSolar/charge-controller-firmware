#ifndef OUTPUT_H
#define OUTPUT_H

#include "structs.h"

void output_serial(dcdc_t *dcdc, battery_t *bat, load_output_t *load);
void output_oled(dcdc_t *dcdc, dcdc_port_t *solar_port,  dcdc_port_t *bat_port, battery_t *bat, load_output_t *load);

#endif
