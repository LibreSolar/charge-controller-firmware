# Configuration of supported boards

## Zephyr board definition

The definitions found in `boards/arm/my-board/` folders are used directly by Zephyr.

More detiails can be found in Zephyr
[porting guide](https://docs.zephyrproject.org/latest/guides/porting/board_porting.html) and
[Kconfig description](https://docs.zephyrproject.org/latest/guides/kconfig/setting.html#the-initial-configuration).

- `board.cmake`, `CMakeLists.txt`: CMake definitions for board, normally don't need to be touched.
- `Kconfig.board`: Defines the name of the board as used in DTS config.
- `Kconfig.defconfig`: Configuration of invisible Kconfig symbols, i.e. symbols that should not be adjustable by the user via menuconfig.
- `<BOARD>_defconfig`: Default configuration of the board (can be overwritten by prj.conf)
- `<BOARD>.dts`: Main board device tree specification.
- `<BOARD>.yaml`:
- `pinmux.c`: Defines which pins should be used for peripherals like SPI1, SPI2, SPI3 if selected by the user.

## PlatformIO configuration

PlatformIO needs an additional definition file for each board. It is stored as `boards/my-board.json`
