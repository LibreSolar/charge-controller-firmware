# MPPT Charger Software

Software based on ARM mbed framework for the LibreSolar MPPT solar charge controllers

**Remark:** See also development branch for most recent updates.

## Supported devices

The software is configurable to support different charge controller PCBs with either STM32F072 (including CAN support) or low-power STM32L072/3 MCUs.

- [Libre Solar MPPT 20A (v0.10)](https://github.com/LibreSolar/MPPT-Charger_20A)
- [Libre Solar MPPT 12A (v0.5)](https://github.com/LibreSolar/MPPT-Charger_20A/tree/legacy-12A-version)
- CloudSolar (not yet published)

## Firmware documentation (see docs folder)

- [MPPT charger firmware structure](docs/firmware.md)
- [Charger state machine](docs/charger.md)
