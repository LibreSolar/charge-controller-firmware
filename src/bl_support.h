/*
 * Copyright (c) 2019 Scene Connect Ltd.
 *
 * This code is based on the bootloader developed by Okra Solar.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BL_SUPPORT_H
#define BL_SUPPORT_H

#ifdef BOOTLOADER_ENABLED

#include <cstdint>

/** Length of the bootloader name string (including \0 termination) */
const uint8_t BOOTLADER_NAME_LENGTH = 18;

/**
 * Flash address for the bootloader status structure.
 * This address must be aligned to a flash page. Each time a new application
 * is detected by the bootloader (BootloaderState::newApp),
 * the full page is erased, and the updated struct is written to the address
 */
const uint32_t BOOTLOADER_STATUS_STRUCT_ADDR = 0x0802FF80; // The last flash page

/**
 * Bootloader state enumeration. This state needs to be set to "newApp"
 * by the application after an update, and to "stableApp" after the
 * first successful boot
 */
enum BootloaderState {
    NO_STATE = 0,     ///< State not properly initialized
    NEW_APP,          ///< Set By App after download of the binary
    ATTEMPT_NEW_APP,  ///< Set by Bootloader when first booting the new binary
    STABLE_APP,       ///< Set by App after sucessful boot of the new app
};

/**
 * Bootloader status struct. This struct's status field should be updated
 * in flash by the app after first boot (stableApp) or after an update of the
 * other firmware binary (newApp)
 */
struct BootloaderStatus {
    char bootloader_name[BOOTLADER_NAME_LENGTH];
    uint32_t bootloader_version;
    uint32_t status;   ///< Update this field and write to flash in your app!
    uint32_t live_app_select;
    uint32_t retry_count;
};

/**
 * Write the bootloader status to the flash address 'BOOTLOADER_STATUS_STRUCT_ADDR'.
 *
 * @param status: The struct containing the bootloader status information.
 */
void write_status_reg(BootloaderStatus &status);

/**
 * Unlock the FLASH control register access and the program memory access.
 */
void flash_unlock(void);

/**
 * Program a word at a specified address in the flash memory.
 * (1) Perform the data write (32-bit word) at the desired address
 * (2) Wait until the BSY bit is reset in the FLASH_SR register
 * (3) Check the EOP flag in the FLASH_SR register
 * (4) clear it by software by writing it at 1
 *
 * @param address The flash address to write to.
 * @param data The data to write to the flash.
 */
void flash_program(uint32_t address, uint32_t data);


/**
 *  Called from main. This implements the bootloader status update from the application side.
 */
void check_bootloader(void);

#endif /* BOOTLOADER_ENABLED */

#endif /* BL_SUPPORT_H */
