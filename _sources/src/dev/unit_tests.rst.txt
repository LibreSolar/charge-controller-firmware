Unit Tests
==========

Core functions of the firmware are covered by unit-tests (located in ``tests`` directory). Build
and run the tests with the following command:

.. code-block:: bash

    cd tests
    west build -p -b native_posix -t run


Conformance testing
-------------------

We are using Continuous Integration to perform several checks for each commit or pull-request. In
order to run the same tests locally (recommended before pushing to the server) use the script
``check.sh`` or ``check.bat`` in the ``scripts`` folder:

.. code-block:: bash

    ./scripts/check.sh       # use check.bat on Windows
