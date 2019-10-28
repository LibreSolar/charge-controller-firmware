/*
 * Scene Connect Bootloader
 * Copyright (C) 2019 Scene Connect 
 * https://www.scene.community/
 *
 * This code is developed based on the Okra Bootloader from Okra Solar.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "bl_support.h"
#ifdef BOOTLOADER_ENABLED
#include "stm32l0xx.h"
#endif
#include "debug.h"

/**
 * Unlock the FLASH control register access and the program memory access.
 */
void flash_unlock(void)
{
    if (HAL_IS_BIT_SET(FLASH->PECR, FLASH_PECR_PRGLOCK)) {
        /* Unlocking FLASH_PECR register access*/
        if (HAL_IS_BIT_SET(FLASH->PECR, FLASH_PECR_PELOCK)) {
            WRITE_REG(FLASH->PEKEYR, FLASH_PEKEY1);
            WRITE_REG(FLASH->PEKEYR, FLASH_PEKEY2);
        }        
        /* Unlocking the program memory access */
        WRITE_REG(FLASH->PRGKEYR, FLASH_PRGKEY1);
        WRITE_REG(FLASH->PRGKEYR, FLASH_PRGKEY2);  
    }
    else {
        // Error handler
    }
}

/**
 * Program a word at a specified address in the flash memory.
 * (1) Perform the data write (32-bit word) at the desired address
 * (2) Wait until the BSY bit is reset in the FLASH_SR register
 * (3) Check the EOP flag in the FLASH_SR register
 * (4) clear it by software by writing it at 1 
 */
void flash_program(uint32_t address, uint32_t data)
{
    /* Program word (32-bit) at a specified address.*/
    *(__IO uint32_t *)address = data;

    while ((FLASH->SR & FLASH_SR_BSY) != 0) { /* (2) */
        /* For robust implementation, add here time-out management */
    }
    if ((FLASH->SR & FLASH_SR_EOP) != 0) { /* (3) */
        FLASH->SR = FLASH_SR_EOP; /* (4) */
    }
    else {
        /* Manage the error cases */
    }
}

/**
 * Function to write the bootloader status to the flash address 'BOOTLOADER_STATUS_STRUCT_ADDR'.
 * @param status: The struct containing the bootloader status information.
 */
void write_status_reg(BootloaderStatus& status)
{
    // Make sure the status reg is halfword aligned
    // Check if this is really required.
    static_assert(sizeof(status) % 2 == 0);
    
    // Unlock the FLASH_PECR register access.
    // Then unlock the program memory access.
    flash_unlock();     

    /* 
    * Erase a page in the Flash at BOOTLOADER_STATUS_STRUCT_ADDR - For STM32L073RZ
    * (1) Set the ERASE and PROG bits in the FLASH_PECR register to enable page erasing 
    * (2) Write a 32-bit word value in an address of the selected page to start the erase sequence 
    * (3) Wait until the BSY bit is reset in the FLASH_SR register 
    * (4) Check the EOP flag in the FLASH_SR register 
    * (5) Clear EOP flag by software by writing EOP at 1 
    * (6) Reset the ERASE and PROG bits in the FLASH_PECR register to disable the page erase 
    * Program and erase control register (FLASH_PECR) - This register can only be written after a 
    * good write sequence done in FLASH_PEKEYR, resetting the PELOCK bit.
    */
    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_PROG;

    *(__IO uint32_t *)BOOTLOADER_STATUS_STRUCT_ADDR = (uint32_t)0;
  
    while ((FLASH->SR & FLASH_SR_BSY) != 0) {
        /* For robust implementation, add here time-out management */
    }

    if ((FLASH->SR & FLASH_SR_EOP) != 0) { 
        FLASH->SR = FLASH_SR_EOP;
    }
    else {
        /* Manage the error cases */
    }
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);

    /*
    * Write status struct to the flash
    * (1) Set the PROG and FPRG bits in the FLASH_PECR register to enable a half page programming
    * (2) Perform the data write (half-word) at the desired address
    * (3) Wait until the BSY bit is reset in the FLASH_SR register
    * (4) Check the EOP flag in the FLASH_SR register
    * (5) clear it by software by writing it at 1
    * (6) Reset the PROG and FPRG bits to disable programming
    */
    uint32_t *data = (uint32_t *)&status;
    uint32_t address = BOOTLOADER_STATUS_STRUCT_ADDR;    

    for (unsigned int i = 0; i < sizeof(status) / sizeof(uint32_t); i++) {
        flash_program(address, *data); 
        data++;
        address += 4;
    }  

    // Locking FLASH_PECR register again
    // Set the PRGLOCK Bit to lock the FLASH Registers access 
    SET_BIT(FLASH->PECR, FLASH_PECR_PRGLOCK);    
}

// Called from main
void check_bootloader(void)
{
    #ifdef BOOTLOADER_ENABLED

    //print_info("App1...\r\n");

    // Read the bootloader status from the flash memory location
    BootloaderStatus status_reg = *((BootloaderStatus *)BOOTLOADER_STATUS_STRUCT_ADDR);

    // If the Bootloader Status is ATTEMPT_NEW_APP, change it to STABLE_APP
    if (status_reg.status == BootloaderState::ATTEMPT_NEW_APP) {
        status_reg.status = BootloaderState::STABLE_APP;
        write_status_reg(status_reg); // Write the status to flash
        //print_info("Switched to stable app...\r\n");
    }
    #endif
}