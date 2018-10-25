# MPPT Charger Software

Software based on ARM mbed framework for the LibreSolar MPPT solar charge controllers

**Remark:** See also development branch for most recent updates.

## Supported devices

The software is configurable to support different charge controller PCBs with either STM32F072 (including CAN support) or low-power STM32L072/3 MCUs.

- [Libre Solar MPPT 20A (v0.10)](https://github.com/LibreSolar/MPPT-Charger_20A)
- [Libre Solar MPPT 12A (v0.5)](https://github.com/LibreSolar/MPPT-Charger_20A/tree/legacy-12A-version)
- CloudSolar (not yet published)

## Toolchain and flashing instructions

See the Libre Solar website for a detailed instruction how to [develop software](http://libre.solar/docs/toolchain/) and [flash new firmware](http://libre.solar/docs/flashing/).


## Initial software setup (IMPORTANT!)

1. Select the correct board in `platformio.ini` by removing the comment before the board name under [platformio]
2. Copy `config.h_template` to `config.h` and adjust basic settings (`config.h` is ignored by git, so your changes are kept after software updates using `git pull`)
3. (optional) To perform more advanced battery settings, copy `config.cpp_template` to `config.cpp` and change according to your needs. (also `config.cpp` is excluded from git versioning)

## Additional firmware documentation (docs folder)

- [MPPT charger firmware details](docs/firmware.md)
- [Charger state machine](docs/charger.md)
