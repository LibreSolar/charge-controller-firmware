/* LibreSolar MPPT charge controller firmware
 * Copyright (c) 2016-2018 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UNIT_TEST

#include "config.h"
#include "data_objects.h"

#include "half_bridge.h"

#include "Adafruit_SSD1306.h"
#include "display.h"

#include "output.h"

// See here: https://github.com/ARMmbed/sd-driver  (should be included into main mbed branch >5.10)
//#include "SDBlockDevice.h"
//SDBlockDevice bd(PIN_UEXT_MOSI, PIN_UEXT_MISO, PIN_UEXT_SCK, PIN_UEXT_SSEL);
//uint8_t block[512] = "Hello World!\n";

//#include "FATFileSystem.h"
//FATFileSystem fs("fs");

//Serial serial_uext(PIN_UEXT_TX, PIN_UEXT_RX, "serial_uext");

extern I2C i2c;
Adafruit_SSD1306_I2c oled(i2c, PIN_UEXT_SSEL, 0x78, 64, 128);

extern Serial serial;

void output_sdcard()
//int output_sdcard(dcdc_t *dcdc, dcdc_port_t *solar_port,  dcdc_port_t *bat_port, battery_t *bat, load_output_t *load)
{
/*
    // Try to mount the filesystem
    printf("Mounting the filesystem... ");
    fflush(stdout);
    int err = fs.mount(&bd);
    printf("%s\n", (err ? "Fail :(" : "OK"));
    if (err) {
        // Reformat if we can't mount the filesystem
        // this should only happen on the first boot
        printf("No filesystem found, formatting... ");
        fflush(stdout);
        err = fs.reformat(&bd);
        printf("%s\n", (err ? "Fail :(" : "OK"));
        if (err) {
            error("error: %s (%d)\n", strerror(-err), err);
        }
    }

    // Open the numbers file
    printf("Opening \"/fs/numbers.txt\"... ");
    fflush(stdout);
    FILE *f = fopen("/fs/numbers.txt", "r+");
    printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        // Create the numbers file if it doesn't exist
        printf("No file found, creating a new file... ");
        fflush(stdout);
        f = fopen("/fs/numbers.txt", "w+");
        printf("%s\n", (!f ? "Fail :(" : "OK"));
        if (!f) {
            error("error: %s (%d)\n", strerror(errno), -errno);
        }

        for (int i = 0; i < 10; i++) {
            printf("\rWriting numbers (%d/%d)... ", i, 10);
            fflush(stdout);
            err = fprintf(f, "    %d\n", i);
            if (err < 0) {
                printf("Fail :(\n");
                error("error: %s (%d)\n", strerror(errno), -errno);
            }
        }
        printf("\rWriting numbers (%d/%d)... OK\n", 10, 10);

        printf("Seeking file... ");
        fflush(stdout);
        err = fseek(f, 0, SEEK_SET);
        printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
        if (err < 0) {
            error("error: %s (%d)\n", strerror(errno), -errno);
        }
    }

    // Go through and increment the numbers
    for (int i = 0; i < 10; i++) {
        printf("\rIncrementing numbers (%d/%d)... ", i, 10);
        fflush(stdout);

        // Get current stream position
        long pos = ftell(f);

        // Parse out the number and increment
        int32_t number;
        fscanf(f, "%d", &number);
        number += 1;

        // Seek to beginning of number
        fseek(f, pos, SEEK_SET);
    
        // Store number
        fprintf(f, "    %d\n", number);

        // Flush between write and read on same file
        fflush(f);
    }
    printf("\rIncrementing numbers (%d/%d)... OK\n", 10, 10);

    // Close the file which also flushes any cached writes
    printf("Closing \"/fs/numbers.txt\"... ");
    fflush(stdout);
    err = fclose(f);
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }
    
    // Display the root directory
    printf("Opening the root directory... ");
    fflush(stdout);
    DIR *d = opendir("/fs/");
    printf("%s\n", (!d ? "Fail :(" : "OK"));
    if (!d) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    printf("root directory:\n");
    while (true) {
        struct dirent *e = readdir(d);
        if (!e) {
            break;
        }

        printf("    %s\n", e->d_name);
    }

    printf("Closing the root directory... ");
    fflush(stdout);
    err = closedir(d);
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    // Display the numbers file
    printf("Opening \"/fs/numbers.txt\"... ");
    fflush(stdout);
    f = fopen("/fs/numbers.txt", "r");
    printf("%s\n", (!f ? "Fail :(" : "OK"));
    if (!f) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    printf("numbers:\n");
    while (!feof(f)) {
        int c = fgetc(f);
        printf("%c", c);
    }

    printf("\rClosing \"/fs/numbers.txt\"... ");
    fflush(stdout);
    err = fclose(f);
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(errno), -errno);
    }

    // Tidy up
    printf("Unmounting... ");
    fflush(stdout);
    err = fs.unmount();
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(-err), err);
    }

    printf("Mbed OS filesystem example done!\n");
    */
}


void output_serial(dcdc_t *dcdc, battery_t *bat, load_output_t *load)
//void output_serial(dcdc_t *dcdc, dcdc_port_t *solar_port,  dcdc_port_t *bat_port, battery_t *bat, load_output_t *load)
{
    /*
    #ifdef SDCARD_ENABLED
        //sd.mount();
        FILE *fp = fopen("/sd/solar_log.csv", "a");
    #else
        //FILE *fp = fopen("/serial_uext", "a");
        FILE *fp = fopen("/serial", "a");
    #endif

    if(fp == NULL) {
        printf("Could not open file for write\n");
    }
    else {
        char buffer[32];
        time_t seconds = time(NULL);
        strftime(buffer, 32, "%F %T", localtime(&seconds));
        //printf("Time as a custom formatted string = %s", buffer);
        //fprintf(fp, "%s;", buffer);
        fprintf(fp, "%d ", (int)time(NULL));
//        fprintf(fp, "%d;", seconds_day);
//        fprintf(fp, "%d;", day_counter);
        fprintf(fp, "%.2f %.1f ", dcdc->hs_voltage, dcdc->ls_voltage * meas->dcdc_current);
        fprintf(fp, "%.2f %.1f ", meas->battery_voltage, meas->dcdc_current);
        fprintf(fp, "%.1f %.2f ", meas->load_current * meas->battery_voltage, meas->load_current);
        fprintf(fp, "%d ", meas->load_enabled);
        fprintf(fp, "%.0f %.0f ", meas->input_Wh_day, fabs(meas->output_Wh_day));
        fprintf(fp, "%.1f %.1f ", meas->input_Wh_total / 1000.0, fabs(meas->output_Wh_total / 1000.0));
        switch (bat->state) {
            case CHG_IDLE:
                fprintf(fp, "Standby");
                break;
            case CHG_CC:
                fprintf(fp, "CC");
                break;
            case CHG_CV:
                fprintf(fp, "CV");
                break;
            case CHG_TRICKLE:
                fprintf(fp, "Trickle");
                break;
            default:
                fprintf(fp, "Error");
                break;
        }
        fprintf(fp, " %.1f ", half_bridge_get_duty_cycle() * 100.0);
        fprintf(fp, "%d ", half_bridge_enabled());
        fprintf(fp, "\n");
    }

    fclose(fp);

    #ifdef SDCARD_ENABLED
    sd.unmount();
    #endif
    */
}

void output_oled(dcdc_t *dcdc, dcdc_port_t *solar_port,  dcdc_port_t *bat_port, battery_t *bat, load_output_t *load)
{
    float tmp;

    oled.clearDisplay();

    oled.drawBitmap(6, 0, bmp_pv_panel, 16, 16, 1);
    oled.drawBitmap(104, 0, bmp_load, 16, 16, 1);

    if (half_bridge_enabled() == false) {
        oled.drawBitmap(27, 3, bmp_disconnected, 32, 8, 1);
    }
    else {
        oled.drawBitmap(34, 3, bmp_arrow_right, 5, 7, 1);
    }

    if (load->enabled) {
        oled.drawBitmap(84, 3, bmp_arrow_right, 5, 7, 1);
    }
    else {
        oled.drawBitmap(81, 3, bmp_disconnected, 17, 7, 1);
    }

    oled.drawRect(52, 2, 18, 9, 1);     // battery shape
    oled.drawRect(69, 3, 3, 7, 1);      // battery terminal
    oled.drawRect(54, 4, 2, 5, 1);      // bar 1
    oled.drawRect(57, 4, 2, 5, 1);      // bar 2
    oled.drawRect(60, 4, 2, 5, 1);      // bar 3
    oled.drawRect(63, 4, 2, 5, 1);      // bar 4
    oled.drawRect(66, 4, 2, 5, 1);      // bar 5

    // solar panel data
    if (half_bridge_enabled()) {
        tmp = solar_port->voltage * solar_port->current;
        oled.setTextCursor(0, 18);
        oled.printf("%4.0fW", (abs(tmp) < 1) ? 0 : tmp);     // remove negative zeros
    }
    else {
        oled.setTextCursor(8, 18);
        oled.printf("n/a");
    }
    if (solar_port->voltage > bat_port->voltage) {
        oled.setTextCursor(0, 26);
        oled.printf("%4.1fV", solar_port->voltage);
    }

    // battey data
    tmp = bat_port->voltage * (bat_port->current - load->current);
    oled.setTextCursor(42, 18);
    oled.printf("%5.1fW", (abs(tmp) < 0.1) ? 0 : tmp);    // remove negative zeros
    oled.setTextCursor(42, 26);
    oled.printf("%5.1fV", bat_port->voltage);

    // load data
    tmp = bat_port->voltage * load->current;
    oled.setTextCursor(90, 18);
    oled.printf("%5.1fW", (abs(tmp) < 0.1) ? 0 : tmp);    // remove negative zeros
    oled.setTextCursor(90, 26);
    oled.printf("%5.1fA\n", (abs(load->current) < 0.1) ? 0 : load->current);

    oled.setTextCursor(0, 36);
    oled.printf("Day +%5.0fWh -%5.0fWh", bat->input_Wh_day, fabs(bat->output_Wh_day));
    oled.printf("Tot +%4.1fkWh -%4.1fkWh", bat->input_Wh_total / 1000.0, fabs(bat->output_Wh_total) / 1000.0);

    oled.setTextCursor(0, 56);
    oled.printf("T %.0fC PWM %.0f%% SOC %d%%", dcdc->temp_mosfets, half_bridge_get_duty_cycle() * 100.0, bat->soc);

    oled.display();
}

#endif /* UNIT_TEST */