# MPPT charger firmware structure

## Important remarks

- Do **not** use mbed Ticker class, as it disturbs the timer settings and might cause the charge controller to crash. In addition to that, it contains a lot of overhead and is not very efficient. Timers are programmed bare metal.

- The PWM signal generation for the DC/DC controller inside the charge controller is done in the file half_bridge_xxx.cpp and dcdc.cpp. Most of the dangerous functions which could turn your MOSFETs into magic smoke are implemented here, so don't touch it unless you know what you're doing.

## Toolchain and flashing instructions

See the Libre Solar website for a detailed instruction how to [develop software](http://libre.solar/docs/toolchain) and [flash new firmware](http://libre.solar/docs/flashing).


## Timer configuration
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
