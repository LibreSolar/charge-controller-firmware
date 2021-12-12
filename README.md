# Libre Solar Charge Controller Firmware

![build badge](https://travis-ci.com/LibreSolar/charge-controller-firmware.svg?branch=main)

This repository contains the firmware for the different Libre Solar Charge Controllers based on [Zephyr RTOS](https://www.zephyrproject.org/).

Coding style is described [here](https://github.com/LibreSolar/coding-style).

## Development and release model

The `main` branch is used for ongoing development of the firmware.

Releases are created from `main` after significant updates have been introduced to the firmware. Each release has to pass tests with multiple boards.

A release is tagged with a version number consisting of the release year and a release count for that year (starting at zero). For back-porting of bug-fixes, a branch named after the release followed by `-branch` is created, e.g. `v21.0-branch`.

## Supported devices

The software is configurable to support different charge controller PCBs with STM32F072, low-power STM32L072 or most recent STM32G431 MCUs:

| Board                                               | Default revision | Older revisions |
|-----------------------------------------------------|------------------|-----------------|
| [Libre Solar MPPT 2420 HC](https://github.com/LibreSolar/mppt-2420-hc)      | 0.2  | 0.1 |
| [Libre Solar MPPT 2420 LC](https://github.com/LibreSolar/mppt-2420-lc)      | 0.10 |     |
| [Libre Solar MPPT 1210 HUS](https://github.com/LibreSolar/mppt-1210-hus)    | 0.7  | 0.4 |
| [Libre Solar PWM 2420 LUS](https://github.com/LibreSolar/pwm-2420-lus)      | 0.3  | 0.2 |

## Building and flashing the firmware

This repository contains git submodules, so you need to clone (download) it by calling:

```
git clone --recursive https://github.com/LibreSolar/charge-controller-firmware
```

Unfortunately, the green GitHub "Clone or download" button does not include submodules. If you cloned the repository already and want to pull the submodules, run `git submodule update --init --recursive`.

As the `main` branch may contain unstable code, make sure to select the desired release branch (see GitHub for a list of releases and branches):
```
git switch <your-release>-branch
```

Building the firmware requires the native Zephyr build system.

This guide assumes you have already installed the Zephyr SDK and the `west` tool according to the [Zephyr documentation](https://docs.zephyrproject.org/latest/getting_started/).

The repository root directory is the `west` workspace. The `app` subfolder contains all application source files, the CMake entry point and the `west` manifest, so we have to move into this folder:

        cd app

The following command initializes the west workspace:

        west init -l .

This command will create a `.west/config` file in the workspace root and set up the repository as specified in the `west.yml` file. Afterwards the following command pulls the Zephyr source and all necessary modules, which might take a while:

        west update

Initial board selection (see `boards` subfolder for correct names):

        west build -b <board-name>@<revision>

The appended `@<revision>` specifies the board version according to the above table. It can be omitted if only a single board revision is available or if the default (most recent) version should be used. See also [here](https://docs.zephyrproject.org/latest/application/index.html#application-board-version) for more details regarding board revision handling in Zephyr.

Flash with specific debug probe (runner), e.g. J-Link:

        west flash -r jlink

User configuration using menuconfig:

        west build -t menuconfig

Report of used memory (RAM and flash):

        west build -t rom_report
        west build -t ram_report

## Firmware customization

This firmware is developed to allow easy customization for individual use-cases. Below options based on the Zephyr devicetree and Kconfig systems try to allow customization without changing any of the files tracked by git, so that a `git pull` does not lead to conflicts with local changes.

### Hardware-specific changes

In Zephyr, all hardware-specific configuration is described in the [devicetree](https://docs.zephyrproject.org/latest/guides/dts/index.html).

The file `boards/arm/board_name/board_name.dts` contains the default devicetree specification (DTS) for a board. It is based on the DTS of the used MCU, which is included from the main Zephyr repository.

In order to overwrite the default devicetree specification, so-called overlays can be used. An overlay file can be specified via the west command line. If it is stored as `board_name.overlay` in the `zephyr` subfolder, it will be recognized automatically when building the firmware for that board (also with PlatformIO).

### Application firmware configuration

For configuration of the application-specific features, Zephyr uses the [Kconfig system](https://docs.zephyrproject.org/latest/guides/kconfig/index.html).

The configuration can be changed using `west build -t menuconfig` command or manually by changing the prj.conf file (see `Kconfig` file for possible options).

Similar to DTS overlays, Kconfig can also be customized per board. Create a folder `zephyr/boards` and a file `board_name.conf` in that folder. The configuration from this file will be merged with the `prj.conf` automatically.

#### Change the battery type

By default, the charge controller is configured for maintainance-free VRLA gel battery (`CONFIG_BAT_TYPE_GEL`). Possible other pre-defined options are `CONFIG_BAT_TYPE_FLOODED`, `CONFIG_BAT_TYPE_AGM`, `CONFIG_BAT_TYPE_LFP`, `CONFIG_BAT_TYPE_NMC` and `CONFIG_BAT_TYPE_NMC_HV`.

The number of cells is automatically selected by Kconfig to get 12V nominal voltage. It can also be manually specified via `CONFIG_BAT_NUM_CELLS`.

To compile the firmware with default settings for 12V LiFePO4 batteries, add the following to `prj.conf` or the board-specific `.conf` file:

```
CONFIG_BAT_TYPE_LFP=y
CONFIG_BAT_NUM_CELLS=4
```

#### Configure serial for ThingSet protocol

By default, the charge controller uses the serial interface in the UEXT connector for the [ThingSet protocol](https://libre.solar/thingset/). This allows to use WiFi modules with ESP32 without any firmware change.

Add the following configuration if you prefer to use the serial of the LS.one port or the additional debug RX/TX pins present on most boards:

```
CONFIG_UEXT_SERIAL_THINGSET=n
```

To disable regular data publication in one-second interval on ThingSet serial at startup add the following configuration:

```
CONFIG_THINGSET_SERIAL_PUB_DEFAULT=n
```

### Custom functions (separate C/C++ files)

If you want experiment with some completely new functions for the charge controller, new files should be stored in a subfolder `src/custom`.

To add the files to the firmware build, create an own CMakeLists.txt in the folder and enable the Kconfig option `CONFIG_CUSTOMIZATION`.

If you think the customization could be relevant for others, please consider sending a pull request. Integrating features into the upstream charge controller firmware guarantees that the feature stays up-to-date and that it will not accidentally break after changes in the original firmware.

## API documentation

The documentation auto-generated by Doxygen can be found [here](https://libre.solar/charge-controller-firmware/).

## Conformance testing

We are using Travis CI to perform several checks for each commit or pull-request. In order to run
the same tests locally (recommended before pushing to the server) use the script `check.sh` or ` check.bat` in the `scripts` folder:

    bash scripts/check.sh       # Linux
    cmd scripts\check.bat       # Windows

### Unit-tests

Core functions of the firmware are covered by unit-tests (located in `tests` directory). Build and run the tests with the following command:

    cd tests
    west build -p -b native_posix -t run

## Additional firmware documentation (docs folder)

- [Charge controller basic concepts](docs/charger-concepts.md)

- [Firmware module overview](docs/module-overview.md)

## Troubleshooting

### Errors with STM32L072 MCU using OpenOCD

The standard OpenOCD settings sometimes fail to flash firmware to this MCU, which is used in the MPPT 1210 HUS and the PWM charge controller.

Try one of these workarounds:

1. Change OpenOCD settings to `adapter_khz 500` in `openocd-path/scripts/target/stm32l0.cfg`.

2. Use other tools or debug probes such as [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) or [Segger J-Link](https://www.segger.com/products/debug-probes/j-link/).
