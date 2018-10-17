# MPPT charger firmware structure

## Important remarks

- Do **not** use mbed Ticker class, as it disturbs the timer settings and might cause the charge controller to crash. In addition to that, it contains a lot of overhead and is not very efficient. Timers should be programmed bare metal. Currently, TIM1 is used for PWM generation and TIM15 to trigger ADC readings.

- The PWM signal generation for the DC/DC controller inside the charge controller is done in the file dcdc.cpp. Most of the dangerous functions which could turn your MOSFETs into magic smoke are implemented here, so don't touch it unless you know what you're doing.

## Toolchain and flashing instructions

See the Libre Solar website for a detailed instruction how to [develop software](http://libre.solar/docs/toolchain/) and [flash new firmware](http://libre.solar/docs/flashing/).

Please note that you have to create the file **config.cpp** by using / changing one of the provided templates to your needs before the software compiles successfully. The file config.cpp is **not** tracked in git since it represents a specific configuration. Please do not add it in a pull request, change the templates if necessary.  

## Timer configuration

- PWM generation for DC/DC half bridge (50-70 kHz)
    - TIM1 for STM32F0 (advanced timer)
    - TIM3 for STM32L0 (standard timer)
- ADC and DMA (1 kHz)
    - TIM15 for STM32F0
    - TIM6 for STM32L0
- DC/DC control function (100 Hz)
    - TIM16 for STM32F0
    - TIM7 for STM32L0
