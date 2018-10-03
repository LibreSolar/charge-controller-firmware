# MPPT Charger Software

Software based on ARM mbed library for the LibreSolar 20 A MPPT solar charge controller

## Important remarks

- Do **not** use mbed Ticker class, as it disturbs the timer settings and might cause the charge controller to crash. In addition to that, it contains a lot of overhead and is not very efficient. Timers should be programmed bare metal. Currently, TIM1 is used for PWM generation and TIM15 to trigger ADC readings.

- The PWM signal generation for the DC/DC controller inside the charge controller is done in the file dcdc.cpp. Most of the dangerous functions which could turn your MOSFETs into magic smoke are implemented here, so don't touch it unless you know what you're doing.

## State machine of the charge controller

The charge management is implemented using a state machine with the following states:

### Standby
Initial state of the charge controller. If the solar voltage is high enough and the battery is not full, charging in CC mode is started.

### CC / bulk charging
The battery is charged with maximum possible current (MPPT algorithm is active) until the CV voltage limit is reached.

### CV / absorption charging
Lead-acid batteries are charged for some time using a slightly higher charge voltage. After a current cutoff limit or a time limit is reached, the charger goes into trickle or equalization mode for lead-acid batteries or back into Standby for Li-ion batteries.

### Trickle charging
This mode is kept forever for a lead-acid battery and keeps the battery at full state of charge. If too much power is drawn from the battery, the charger switches back into CC / bulk charging mode.

### Equalization charging
This mode is only used for lead-acid batteries after several deep-discharge cycles or a very long period of time with no equalization. Voltage is increased to 15V or above, so care must be taken for the other system components attached to the battery. (currently, no equalization charging is enabled in the software)

![Text](img/MPPT_flow_chart.png)

## Toolchain and flashing instructions

See the Libre Solar website for a detailed instruction how to [develop software](http://libre.solar/docs/toolchain/) and [flash new firmware](http://libre.solar/docs/flashing/).

Please note that you have to create the file **config.cpp** by using / changing one of the provided templates to your needs before the software compiles successfully. The file config.cpp is **not** tracked in git since it represents a specific configuration. Please do not add it in a pull request, change the templates if necessary.  
