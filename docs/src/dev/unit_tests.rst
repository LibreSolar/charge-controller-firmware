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

The firmware can be built for all boards and board revisions in the same way:

.. code-block:: bash

    west twister -T app --integration


Conformance testing
-------------------

We are using Continuous Integration to perform several checks for each commit or pull-request. In
order to run the same tests locally (recommended before pushing to the server) use the script
``check.sh`` or ``check.bat`` in the ``scripts`` folder:

.. code-block:: bash

    ./scripts/check.sh       # use check.bat on Windows
