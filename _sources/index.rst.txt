====================================================
Libre Solar Charge Controller Firmware Documentation
====================================================

This documentation describes the firmware based on `Zephyr RTOS`_ for different Libre Solar MPPT/PWM
Charge Controllers.

The firmware is under ongoing development. Most recent version can be found on
`GitHub <https://github.com/LibreSolar/charge-controller-firmware>`_.

This documentation is licensed under the Creative Commons Attribution-ShareAlike 4.0 International
(CC BY-SA 4.0) License.

.. image:: static/images/cc-by-sa-centered.png

The full license text is available at `<https://creativecommons.org/licenses/by-sa/4.0/>`_.

.. _Zephyr RTOS: https://zephyrproject.org

.. toctree::
    :caption: Overview
    :hidden:

    src/overview/features
    src/overview/supported_hardware
    src/overview/charger_concepts

.. toctree::
    :caption: Development
    :hidden:

    src/dev/workspace_setup
    src/dev/building_flashing
    src/dev/customization
    src/dev/unit_tests
    src/dev/troubleshooting

.. toctree::
    :caption: API Reference
    :hidden:

    src/api/bat_charger
    src/api/daq
    src/api/data_objects
    src/api/dcdc
    src/api/device_status
    src/api/half_bridge
    src/api/load
    src/api/power_port
    src/api/pwm_switch
    src/api/misc
