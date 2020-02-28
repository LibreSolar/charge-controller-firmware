# Firmware modules

## Data Acquisition

Files: daq*.h/cpp

Handles current and voltage measurement through the ADC with direct memory access (DMA). As a reference for current measurement, a digital-analog-converter (DAC) is used and also configured in this module.

The measurements are filtered and afterwards stored in the different sub-component objects.

## DC/DC converter

Files: dcdc.h/cpp

High-level functions to control the DC/DC converter. This module does not directly access any hardware. The PWM signal for the half bridge is generated in a separate module (see below).

## Data Objects for ThingSet protocol

Files: data_objects.h/cpp

The charge controller uses the [ThingSet protocol](https://thingset.github.io/) for communication with external devices. Internal data that should be exposed via the protocol is defined in the Data Objects module.

## Device status

Files: device_status.h/cpp

Stores global error flags and system-level measurements that are not specific to one sub-component.

## EEPROM

Files: eeprom.h/cpp

Stores data in ThingSet protocol format in the internal or external EEPROM (depending on the board).

## Half bridge

Files: half_bridge.h/cpp

Generates the synchronous PWM signal for the half bridge in the DC/DC converter. Depending on the MCU, either the advanced timer TIM1 or the basic timer TIM3 is used.

## LEDs

Files: leds.h/cpp

Provides higher-level access to different LEDs (e.g. blinking and flickering). Internally, charlieplexing is used to optimize power consumption and needed pin numbers.

## Load Output

Files: load*.h/cpp

Load output and USB charging output functions. Hardware-access is defined in load_drv.cpp file.

## Power Port and DC bus

Files: power_port.h/cpp

Contains DcBus and PowerPort classes. See [charger concepts](charger-concepts.md) for further details.

## PWM Switch

Files: pwm_switch.h/cpp

Only used for PWM solar charge controllers. This module is currently not yet separated into hardware-level and high-level functions.

## Timer configuration in mbed firmware

- mbed us_ticker (cannot be changed)
    - TIM2 for STM32F0
    - TIM21 for STM32L0
- PWM generation for DC/DC half bridge (50-70 kHz)
    - TIM1 for STM32F0 (advanced timer)
    - TIM3 for STM32L0 (standard timer)
- ADC and DMA (1 kHz)
    - TIM15 for STM32F0
    - TIM6 for STM32L0
- DC/DC control function (10 Hz)
    - TIM16 for STM32F0
    - TIM7 for STM32L0
- LED light control
    - TIM17 for STM32F0
    - TIM22 for STM32L0
