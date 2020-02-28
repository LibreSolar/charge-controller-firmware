# Zephyr cheat sheet

## West commands

Initial board selection

        west build -b <board-name>

Flash with specific debug probe (runner), e.g. J-Link

        west flash -r jlink

Report of used memory (RAM and flash)

        west build -t rom_report
        west build -t ram_report

User configuration using menuconfig

        west build -t menuconfig
