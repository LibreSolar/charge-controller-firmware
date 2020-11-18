# Libre Solar Charge Controller Firmware

![build badge](https://travis-ci.com/LibreSolar/charge-controller-firmware.svg?branch=master)

This repository contains the firmware for the different Libre Solar Charge Controllers based on [Zephyr RTOS](https://www.zephyrproject.org/).

Coding style is described [here](https://github.com/LibreSolar/coding-style).

## Development and release model

The `master` branch contains the latest release of the firmware plus some cherry-picked bug-fixes. So you can always pull this branch to get a stable and working firmware for your charge controller.

New features are developed in the `develop` branch and merged into master after passing tests with multiple boards. The `develop` branch is changed frequently and may even be rebased to fix previous commits. Use this branch only if you want to try out most recent features and be aware that something might be broken.

Releases are generated from master after significant changes have been made. The year is used as the major version number. The minor version number starts at zero and is increased for each release in that year.

## Supported devices

The software is configurable to support different charge controller PCBs with STM32F072, low-power STM32L072 or most recent STM32G431 MCUs:

- [Libre Solar MPPT 2420 HC](https://github.com/LibreSolar/mppt-2420-hc)
- [Libre Solar MPPT 2420 LC](https://github.com/LibreSolar/mppt-2420-lc)
- [Libre Solar MPPT 1210 HUS](https://github.com/LibreSolar/mppt-1210-hus)
- [Libre Solar PWM 2420 LUS](https://github.com/LibreSolar/pwm-2420-lus)

## Building and flashing the firmware

This repository contains git submodules, so you need to clone (download) it by calling:

```
git clone --recursive https://github.com/LibreSolar/charge-controller-firmware
```

Unfortunately, the green GitHub "Clone or download" button does not include submodules. If you cloned the repository already and want to pull the submodules, run `git submodule update --init --recursive`.

### PlatformIO

It is suggested to use Visual Studio Code and PlatformIO for firmware development, as it simplifies compiling and uploading the code a lot:

1. Install Visual Studio Code and [PlatformIO](https://platformio.org/platformio-ide) to build the firmware.

2. Adjust configuration in `zephyr/prj.conf` if necessary.

3. Select the correct board in `platformio.ini` by removing the comment before the board name under `[platformio]` or create a file `custom.ini` with your personal settings.

4. Connect the board via a programmer. See the Libre Solar website for [further project-agnostic instructions](http://libre.solar/docs/flashing).

5. Press the upload button at the bottom left corner in VS Code.

### Native Zephyr environment

You can also use the Zephyr build system directly for advanced configuration with `menuconfig` or if you need more recently added features.

This guide assumes you have already installed the Zephyr SDK and the west tool according to the [Zephyr documentation](https://docs.zephyrproject.org/latest/getting_started/). Also make sure that `west` is at least at version `0.8.0`.

Now after you cloned the repository (see above), go into the root firmware directory and initialize a west workspace:

        west init -l zephyr

This command will create a `.west/config` file and set up the repository as specified in `zephyr/west.yml` file. Afterwards the following command pulls the Zephyr source and all necessary modules, which might take a while:

        west update

The CMake entry point is in the `zephyr` subfolder, so you need to run `west build` and `west flash` command in that directory.

        cd zephyr

Initial board selection (see `boards subfolder for correct names):

        west build -b <board-name>

Flash with specific debug probe (runner), e.g. J-Link:

        west flash -r jlink

User configuration using menuconfig:

        west build -t menuconfig

Report of used memory (RAM and flash):

        west build -t rom_report
        west build -t ram_report

### Troubleshooting

#### Errors with STM32L072 MCU using OpenOCD

The standard OpenOCD settings sometimes fail to flash firmware to this MCU, which is used in the MPPT 1210 HUS and the PWM charge controller.

Try one of these workarounds:

1. Change OpenOCD settings to `adapter_khz 500` in line 70 in `~/.platformio/packages/tool-openocd/scripts/target/stm32l0.cfg`.

2. Use other tools or debug probes such as [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) or [Segger J-Link](https://www.segger.com/products/debug-probes/j-link/).

#### Connection failures

If flashing fails like this in PlatformIO:

```
Error: init mode failed (unable to connect to the target)
in procedure 'program'
** OpenOCD init failed **
shutdown command invoked
```

or with ST-Link:

```bash
$ st-flash write .pio/build/mppt-1210-hus-v0.4/firmware.bin 0x08000000
st-flash 1.5.1
2019-06-21T18:13:03 INFO common.c: Loading device parameters....
2019-06-21T18:13:03 WARN common.c: unknown chip id! 0x5fa0004
```

check the connection between the programmer (for example the ST-Link of the Nucleo board) and the charge controller.

## Firmware customization

This firmware is developed to allow easy customization for individual use-cases. Below options based on the Zephyr devicetree and Kconfig systems try to allow customization without changing any of the files tracked by git, so that a `git pull` does not lead to conflicts with local changes.

### Hardware-specific changes

In Zephyr, all hardware-specific configuration is described in the [devicetree](https://docs.zephyrproject.org/latest/guides/dts/index.html).

The file `boards/arm/board_name/board_name.dts` contains the default devicetree specification (DTS) for a board. It is based on the DTS of the used MCU, which is included from the main Zephyr repository.

In order to overwrite the default configuration, so-called overlays can be used. An overlay file can be specified via the west command line. If it is stored as `board_name.overlay` in the `zephyr` subfolder, it will be recognized automatically when building the firmware for that board (also with PlatformIO).

In order to support older board versions (e.g. with changed pins), some overlays with appended version numbers are already existing as a template. Remove the version number (e.g. `_0v4`) to "activate" them.

### Application firmware configuration

For configuration of the application-specific features, Zephyr uses the [Kconfig system](https://docs.zephyrproject.org/latest/guides/kconfig/index.html).

The configuration can be changed using `west build -t menuconfig` command or manually by changing the prj.conf file.

Similar to DTS overlays, the Kconfig can also be customized for each board. Copy the default `prj.conf` to a file `prj_board_name.conf` to use this file instead of `prj.conf`. Please not that, in contrast to overlays, the board-specific Kconfig files are not **added** to the default `prj.conf`, but they **replace** the default configuration instead.

### Custom functions (separate C/C++ files)

If you want experiment with some completely new functions for the charge controller, new files should be stored in a subfolder `src/custom`.

To add the files to the firmware build, create an own CMakeLists.txt in the folder and enable the Kconfig option `CONFIG_CUSTOMIZATION`.

If you think the customization could be relevant for others, please consider sending a pull request. Integrating features into the upstream charge controller firmware guarantees that the feature stays up-to-date and that it will not accidentally break after changes in the original firmware.

## API documentation

The documentation auto-generated by Doxygen can be found [here](https://libre.solar/charge-controller-firmware/).

## Conformance testing

We are using Travis CI to perform several checks for each commit or pull-request. In order to run
the same tests locally (recommended before pushing to the server) use the script `check.sh` or ` check.bat`:

    bash check.sh       # Linux
    cmd check.bat       # Windows

### Unit-tests

Core functions of the firmware are covered by unit-tests (located in test directory). Run the tests with the following command:

    platformio test -e unit_test

### Static code analysis

PlatformIO integrates cppcheck and clangtidy. The following command is an example to run the checks for MPPT 1210 HUS charge controller code:

    platformio check -e mppt_1210_hus

## Additional firmware documentation (docs folder)

- [Charge controller basic concepts](docs/charger-concepts.md)

- [Firmware module overview](docs/module-overview.md)
