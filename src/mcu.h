
#ifndef MCU_H_
#define MCU_H_

// mbed defines STM32Fxx variables automatically, so we define them also for Zephyr

#ifdef CONFIG_SOC_STM32L073XX
#define STM32L0
#define STM32L073
#endif

#ifdef CONFIG_SOC_STM32F072XX
#define STM32F0
#define STM32F072
#endif

#ifdef STM32L073
#include "stm32l073xx.h"
#endif

#ifdef STM32F072
#include "stm32f072xx.h"
#endif

#endif // MCU_H_
