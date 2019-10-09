
#include "tests.h"

#include "dcdc.h"
#include "dc_bus.h"
#include "log.h"

#include <time.h>
#include <stdio.h>
#include <math.h>

extern Dcdc dcdc;
extern DcBus hv_bus;
extern DcBus lv_bus;
extern BatConf bat_conf;

static void init_structs_buck()
{
    dc_bus_init_solar(&hv_bus, 10);
    hv_bus.voltage = 20;
    hv_bus.dis_voltage_start = 18;

    battery_conf_init(&bat_conf, BAT_TYPE_GEL, 6, 100);
    battery_init_dc_bus(&lv_bus, &bat_conf, 1);
    lv_bus.voltage = 14;

    dcdc_init(&dcdc);
}

static void init_structs_boost()
{
    dc_bus_init_solar(&lv_bus, 10);
    lv_bus.voltage = 20;
    lv_bus.dis_voltage_start = 18;

    battery_conf_init(&bat_conf, BAT_TYPE_NMC, 10, 9);
    battery_init_dc_bus(&hv_bus, &bat_conf, 1);
    hv_bus.voltage = 3.7 * 10;

    dcdc_init(&dcdc);
}

void start_valid_mppt_buck()
{
    init_structs_buck();
    TEST_ASSERT_EQUAL(1, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void start_valid_mppt_boost()
{
    init_structs_boost();
    TEST_ASSERT_EQUAL(-1, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_start_before_restart_delay()
{
    init_structs_buck();
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval + 1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
    dcdc.off_timestamp = time(NULL) - dcdc.restart_interval;
    TEST_ASSERT_EQUAL(1, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_start_if_dcdc_disabled()
{
    init_structs_buck();
    dcdc.enabled = false;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_start_if_dcdc_lv_voltage_low()
{
    init_structs_buck();
    lv_bus.voltage = dcdc.ls_voltage_min - 0.5;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

// buck

void no_buck_start_if_bat_voltage_low()
{
    init_structs_buck();
    lv_bus.voltage = bat_conf.voltage_absolute_min - 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_buck_start_if_bat_voltage_high()
{
    init_structs_buck();
    lv_bus.voltage = bat_conf.voltage_topping + 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_buck_start_if_bat_chg_not_allowed()
{
    init_structs_buck();
    lv_bus.chg_current_max = 0;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_buck_start_if_solar_voltage_high()
{
    init_structs_buck();
    hv_bus.voltage = dcdc.hs_voltage_max + 1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_buck_start_if_solar_voltage_low()
{
    init_structs_buck();
    hv_bus.voltage = 17;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

// boost

void no_boost_start_if_bat_voltage_low()
{
    init_structs_boost();
    hv_bus.voltage = bat_conf.voltage_absolute_min - 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_boost_start_if_bat_voltage_high()
{
    init_structs_boost();
    hv_bus.voltage = bat_conf.voltage_topping + 0.1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_boost_start_if_bat_chg_not_allowed()
{
    init_structs_boost();
    hv_bus.chg_current_max = 0;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_boost_start_if_solar_voltage_high()
{
    init_structs_boost();
    lv_bus.voltage = dcdc.ls_voltage_max + 1;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void no_boost_start_if_solar_voltage_low()
{
    init_structs_boost();
    lv_bus.voltage = 17;
    TEST_ASSERT_EQUAL(0, dcdc_check_start_conditions(&dcdc, &hv_bus, &lv_bus));
}

void dcdc_tests()
{
    UNITY_BEGIN();

    // 1. Check of general (re)start conditions

    // check if initialization of test is correct and we start at all
    RUN_TEST(start_valid_mppt_buck);
    RUN_TEST(start_valid_mppt_boost);

    RUN_TEST(no_start_before_restart_delay);
    RUN_TEST(no_start_if_dcdc_disabled);
    RUN_TEST(no_start_if_dcdc_lv_voltage_low);

    // 2. Check startup for MPPT buck converter scenario

    RUN_TEST(no_buck_start_if_bat_voltage_low);
    RUN_TEST(no_buck_start_if_bat_voltage_high);
    RUN_TEST(no_buck_start_if_bat_chg_not_allowed);
    RUN_TEST(no_buck_start_if_solar_voltage_high);
    RUN_TEST(no_buck_start_if_solar_voltage_low);

    // 3. Check startup for MPPT boost converter scenario

    RUN_TEST(no_boost_start_if_bat_voltage_low);
    RUN_TEST(no_boost_start_if_bat_voltage_high);
    RUN_TEST(no_boost_start_if_bat_chg_not_allowed);
    RUN_TEST(no_boost_start_if_solar_voltage_high);
    RUN_TEST(no_boost_start_if_solar_voltage_low);

    // 4. Check startup for nanogrid scenario

    // TODO

    // 5. Check DC/DC control after being started

    // TODO

    UNITY_END();
}