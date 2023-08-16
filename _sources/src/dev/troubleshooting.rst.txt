Troubleshoting
==============

This page contains a collection of often encountered issues and solutions.

Errors with STM32L072 MCU using OpenOCD
---------------------------------------

The standard OpenOCD settings sometimes fail to flash firmware to this MCU, which is used in the
MPPT 1210 HUS and the PWM charge controller.

Try one of these workarounds:

1. Change OpenOCD settings to ``adapter_khz 500`` in ``openocd-path/scripts/target/stm32l0.cfg``.

2. Use other tools or debug probes such as `STM32CubeProgrammer`_ or `Segger J-Link`_.

3. Change OpenOCD setting from ``reset_config srst_only`` to ``reset_config srst_open_drain`` or ``reset_config none`` in ``boards/arm/<yourboard>/support/openocd.cfg``.

.. _STM32CubeProgrammer: https://www.st.com/en/development-tools/stm32cubeprog.html
.. _Segger J-Link: https://www.segger.com/products/debug-probes/j-link/
