Customization
=============

This firmware is developed to allow easy customization for individual use-cases. Below options
based on the Zephyr devicetree and Kconfig systems try to allow customization without changing
any of the files tracked by git, so that a `git pull` does not lead to conflicts with local
changes.

Hardware-specific changes
-------------------------

In Zephyr, all hardware-specific configuration is described in the
`Devicetree <https://docs.zephyrproject.org/latest/guides/dts/index.html>`_.

The file ``boards/arm/board_name/board_name.dts`` contains the default devicetree specification
(DTS) for a board. It is based on the DTS of the used MCU, which is included from the main Zephyr
repository.

In order to overwrite the default devicetree specification, so-called overlays can be used. An
overlay file can be specified via the west command line. If it is stored as ``board_name.overlay``
in the ``app`` subfolder, it will be recognized automatically when building the firmware for that
board.

Application firmware configuration
----------------------------------

For configuration of the application-specific features, Zephyr uses the
`Kconfig system <https://docs.zephyrproject.org/latest/guides/kconfig/index.html>`_.

The configuration can be changed using ``west build -t menuconfig`` command or manually by changing
the prj.conf file (see ``Kconfig`` file for possible options).

Similar to DTS overlays, Kconfig can also be customized per board. Create a folder ``app/boards``
and  a file ``board_name.conf`` in that folder. The configuration from this file will be merged with
the ``prj.conf`` automatically.

Change the battery type
"""""""""""""""""""""""

By default, the charge controller is configured for maintainance-free VRLA gel battery
(``CONFIG_BAT_TYPE_GEL``). Possible other pre-defined options are ``CONFIG_BAT_TYPE_FLOODED``,
``CONFIG_BAT_TYPE_AGM``, ``CONFIG_BAT_TYPE_LFP``, ``CONFIG_BAT_TYPE_NMC`` and
``CONFIG_BAT_TYPE_NMC_HV``.

The number of cells is automatically selected by Kconfig to get 12V nominal voltage. It can also
be manually specified via ``CONFIG_BAT_NUM_CELLS``.

To compile the firmware with default settings for 12V LiFePO4 batteries, add the following to
``prj.conf`` or the board-specific ``.conf`` file:

.. code-block:: bash

    CONFIG_BAT_TYPE_LFP=y
    CONFIG_BAT_NUM_CELLS=4

Configure serial for ThingSet protocol
""""""""""""""""""""""""""""""""""""""

By default, the charge controller uses the serial interface in the UEXT connector for the
`ThingSet protocol <https://libre.solar/thingset/>`_. This allows to use WiFi modules with ESP32
without any firmware change.

Add the following configuration if you prefer to use the serial of the additional debug RX/TX pins
present on many boards:

.. code-block:: bash

    CONFIG_UEXT_SERIAL_THINGSET=n

To disable regular data publication in one-second interval on ThingSet serial at startup add the
following configuration:

.. code-block:: bash

    CONFIG_THINGSET_SERIAL_PUB_DEFAULT=n


Custom functions (separate C/C++ files)
---------------------------------------

If you want experiment with some completely new functions for the charge controller, new files
should be stored in a subfolder ``src/custom``.

To add the files to the firmware build, create an own CMakeLists.txt in the folder and enable the
Kconfig option ``CONFIG_CUSTOMIZATION``.

If you think the customization could be relevant for others, please consider sending a pull request.
Integrating features into the upstream charge controller firmware guarantees that the feature stays
up-to-date and that it will not accidentally break after changes in the original firmware.
