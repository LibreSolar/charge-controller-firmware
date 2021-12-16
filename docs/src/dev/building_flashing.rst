Building and Flashing
=====================

As the ``main`` branch may contain unstable code, make sure to select the desired release branch
(see GitHub for a list of releases and branches):

.. code-block:: bash

    git switch <your-release>-branch
    west update

The ``app`` subfolder contains all application source files and the CMake entry point to build the
firmware, so we go into that directory first.

.. code-block:: bash

    cd app

Initial board selection (see ``boards`` subfolder for correct names):

.. code-block:: bash

    west build -b <board-name>@<revision>

The appended ``@<revision>`` specifies the board version according to the above table. It can be
omitted if only a single board revision is available or if the default (most recent) version should
be used. See also
`here <https://docs.zephyrproject.org/latest/application/index.html#application-board-version>`_
for more details regarding board revision handling in Zephyr.

Flash with specific debug probe (runner), e.g. J-Link:

.. code-block:: bash

    west flash -r jlink
