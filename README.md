# Libre Solar Charge Controller Firmware

![build badge](https://github.com/LibreSolar/charge-controller-firmware/actions/workflows/zephyr.yml/badge.svg)

This repository contains the firmware for the different Libre Solar Charge Controllers based on [Zephyr RTOS](https://www.zephyrproject.org/).

Coding style is described [here](https://github.com/LibreSolar/coding-style).

## Development and release model

The `main` branch is used for ongoing development of the firmware.

Releases are created from `main` after significant updates have been introduced to the firmware. Each release has to pass tests with multiple boards.

A release is tagged with a version number consisting of the release year and a release count for that year (starting at zero). For back-porting of bug-fixes, a branch named after the release followed by `-branch` is created, e.g. `v21.0-branch`.

## Documentation

The firmware documentation including build instructions and API reference can be found under [libre.solar/charge-controller-firmware](https://libre.solar/charge-controller-firmware/).

In order to build the documentation locally you need to install Doxygen, Sphinx and Breathe and run `make html` in the `docs` folder.

## License

This firmware is released under the [Apache-2.0 License](LICENSE).
