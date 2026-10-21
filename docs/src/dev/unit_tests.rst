Unit Tests
==========

Core functions of the firmware are covered by unit tests based on the Zephyr ztest framework. The
tests are located in the ``tests`` directory:

- ``tests/app``: Unit tests of the application (charger, DC/DC control, load output, etc.)
- ``tests/drivers``: Tests of the drivers in this repository

All tests are run on ``native_sim`` with emulated drivers using twister:

.. code-block:: bash

    west twister -T tests --integration

The driver tests are additionally built (but not run) for some of the boards to ensure that the
drivers compile for all supported MCUs.

Hardware tests
--------------

The half-bridge drivers configure timers which drive the MOSFETs of the power stage. A wrong
configuration (e.g. missing dead time) could destroy the hardware. So the drivers are tested on a
Nucleo-G474RE board instead of an actual charge controller. The outputs of the half bridge are
wired to input capture channels of another timer (loop-back), which measures the frequency, duty
cycle, dead time and overlap of the signals at the pins.

The test is located in ``tests/drivers/half_bridge_loopback`` and covers the drivers for
advanced-control timers (TIM1), general-purpose timers (TIM3) and the HRTIM. The required wiring
is described in ``boards/nucleo_g474re.overlay`` of the test.

Twister only runs the test on boards providing the ``half_bridge_loopback`` fixture, which has to
be specified in a hardware map file (e.g. ``map.yaml``) together with the ID of the ST-LINK probe:

.. code-block:: yaml

    - connected: true
      id: 00180012544B500420343637
      platform: nucleo_g474re
      product: STLINK-V3
      runner: pyocd
      serial: /dev/ttyACM0
      fixtures:
        - half_bridge_loopback

A hardware map with the IDs of the connected probes can be generated with
``west twister --generate-hardware-map map.yaml``. Afterwards, run the tests with:

.. code-block:: bash

    west twister -T tests/drivers/half_bridge_loopback --device-testing --hardware-map map.yaml

Without a hardware map (e.g. in CI), the test is only built.

Firmware builds
---------------

The firmware can be built for all boards and board revisions in the same way as the tests:

.. code-block:: bash

    west twister -T app --integration


Conformance testing
-------------------

We are using Continuous Integration to perform several checks for each commit or pull-request. In
order to run the same tests locally (recommended before pushing to the server) use the script
``check.sh`` or ``check.bat`` in the ``scripts`` folder:

.. code-block:: bash

    ./scripts/check.sh       # use check.bat on Windows
