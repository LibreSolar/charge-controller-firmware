# Libre Solar Charge Controller Firmware

![build badge](https://travis-ci.com/LibreSolar/charge-controller-firmware.svg?branch=master)

This repository contains the firmware for the different Libre Solar Charge Controllers. Originally, the firmware was based on [ARM Mbed OS](https://www.mbed.com). The current version also supports using [Zephyr RTOS](https://www.zephyrproject.org/) for some boards.

Coding style is described [here](https://libre.solar/docs/coding_style/).

**Warning:** This firmware is under active development. Even though we try our best not to break any features that worked before, not every commit is fully tested on every board before including it to the master branch. For stable and tested versions consider using the latest [release](https://github.com/LibreSolar/charge-controller-firmware/releases).

## Supported devices

The software is configurable to support different charge controller PCBs with either STM32F072 (including CAN support) or low-power STM32L072/3 MCUs.

- [Libre Solar MPPT 12/24V 20A with CAN](https://github.com/LibreSolar/MPPT-2420-LC)
- [Libre Solar MPPT 12V 10A with USB](https://github.com/LibreSolar/MPPT-1210-HUS)
- [Libre Solar PWM 12/24V 20A](https://github.com/LibreSolar/PWM-2420-LUS)

## Building and flashing the firmware

### ARM Mbed OS

It is suggested to use Visual Studio Code and PlatformIO for firmware development, as it simplifies compiling and uploading the code a lot:

1. Install Visual Studio Code and [PlatformIO](https://platformio.org/platformio-ide) to build the firmware.

2. Copy `src/config.h_template` to `src/config.h`, and adjust basic settings. `config.h` is ignored by git, so your changes are kept after software updates using `git pull`.

3. Select the correct board in `platformio.ini` by removing the comment before the board name under `[platformio]` or create a file `custom.ini` with your personal settings.

4. Connect the board via a programmer. See the Libre Solar website for [further project-agnostic instructions](http://libre.solar/docs/flashing).

5. Press the upload button at the bottom left corner in VS Code.

### Zephyr

Support for Zephyr was recently added to PlatformIO. You may need to run `pio upgrade` and `pio update` to get the most recent version.

After that, you should be able to compile and flash Zephyr for supported boards (-zephyr suffix in platformio.ini) in the same way as explained above for Mbed.

As the build system in PlatformIO is not the same as the native Zephyr build system, there might still be some issues. Generally, also `west build` and `west flash` should work if called from within the zephyr subdirectory.

### Troubleshooting

#### Errors with STM32L072 MCU using OpenOCD

The standard OpenOCD settings sometimes fail to flash firmware to this MCU, which is used in the MPPT 1210 HUS and the PWM charge controller.

Try one of these workarounds:

1. Change OpenOCD settings to `set WORKAREASIZE 0x1000` in the file `~/.platformio/packages/tool-openocd/scripts/board/st_nucleo_l073rz.cfg`.

2. Use ST-Link tools. For Windows there is a GUI tool. Under Linux, uncomment the `upload_command` line in `platformio.ini`.

3. Use other debuggers and tools, e.g. Segger J-Link.

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

## Bootloader Support (STM32L07x only)

The custom linker script file STM32L073XZ.ld.link_script.ld needs to be updated before generating the application firmware binary. For each application, the flash start address and the maximum code size need to be updated in this file. Currently, the locations 0x08001000 and 0x08018000 are used for applications 1 & 2 respectively.

## API documentation

The documentation auto-generated by Doxygen can be found [here](https://libre.solar/charge-controller-firmware/).

## Conformance testing

We are using Travis CI to perform several checks for each commit or pull-request. In order to run
the same tests locally (recommended before each pull-request) use the script `check.sh`:

    bash check.sh

### Unit-tests

In order to run the unit tests, you need a PlatformIO Plus account. Run tests with the following command:

    platformio test -e unit-test-native

### Static code analysis

PlatformIO integrates cppcheck and clangtidy. The following command is an example to run the checks for MPPT 1210 HUS charge controller code:

    platformio check -e mppt-1210-hus-v0.7

## Additional firmware documentation (docs folder)

- [MPPT charger firmware details](docs/firmware.md)
- [Charger state machine](docs/charger.md)
