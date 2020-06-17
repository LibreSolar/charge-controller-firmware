/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2020 Martin Jäger / Libre Solar
 */

/*
 * Generated defines (from devicetree_unfixed.h)
 */

#define DT_N_S_adc_inputs_S_temp_bat_EXISTS 1

#define DT_N_S_adc_inputs_S_v_high_P_multiplier 105600
#define DT_N_S_adc_inputs_S_v_high_P_divider 5600
#define DT_N_S_adc_inputs_S_v_low_P_multiplier 105600
#define DT_N_S_adc_inputs_S_v_low_P_divider 5600
#define DT_N_S_adc_inputs_S_v_pwm_P_multiplier 25224   // see pwm_2420_lus.dts
#define DT_N_S_adc_inputs_S_v_pwm_P_divider 984
#define DT_N_S_adc_inputs_S_v_pwm_P_offset 2338

// amp gain: 50, resistor: 4 mOhm
#define DT_N_S_adc_inputs_S_i_load_P_multiplier 1000
#define DT_N_S_adc_inputs_S_i_load_P_divider (4 * 50)
#define DT_N_S_adc_inputs_S_i_dcdc_P_multiplier 1000
#define DT_N_S_adc_inputs_S_i_dcdc_P_divider (4 * 50)
#define DT_N_S_adc_inputs_S_i_pwm_P_multiplier 1000
#define DT_N_S_adc_inputs_S_i_pwm_P_divider (4 * 50)

/*
 * Macros copied from devicetree.h
 */

#define DT_CAT(node_id, prop_suffix) node_id##prop_suffix

#define DT_PROP(node_id, prop) DT_CAT(node_id, _P_##prop)

#define DT_NODE_EXISTS(name) DT_CAT(name, _EXISTS)

#define DT_CHILD(node_id, child) UTIL_CAT(node_id, DT_S_PREFIX(child))

#define DT_CAT(node_id, prop_suffix) node_id##prop_suffix

#define DT_S_PREFIX(name) _S_##name

#define UTIL_CAT(a, ...) UTIL_PRIMITIVE_CAT(a, __VA_ARGS__)
#define UTIL_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__

/*
 * Simplified mocks for certain macros from devicetree.h
 */

#define DT_PATH(node) DT_N_S_##node

#define DT_FOREACH_CHILD(dummy, fn) \
    fn(DT_N_S_adc_inputs_S_v_low) \
    fn(DT_N_S_adc_inputs_S_v_high) \
    fn(DT_N_S_adc_inputs_S_v_pwm) \
    fn(DT_N_S_adc_inputs_S_i_dcdc) \
    fn(DT_N_S_adc_inputs_S_i_load) \
    fn(DT_N_S_adc_inputs_S_i_pwm) \
    fn(DT_N_S_adc_inputs_S_temp_bat) \
    fn(DT_N_S_adc_inputs_S_vref_mcu) \
    fn(DT_N_S_adc_inputs_S_temp_mcu)
