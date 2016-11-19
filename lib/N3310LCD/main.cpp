/*
* N3310LCD. A program to interface mbed with the nuelectronics
* Nokia 3310 LCD shield from www.nuelectronics.com. Ported from
* the nuelectronics Arduino code.
*
* Copyright (C) <2009> Petras Saduikis <petras@petras.co.uk>
*
* This file is part of N3310LCD.
*
* N3310LCD is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* 
* N3310LCD is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with N3310LCD.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "N3310SPIConfig.h"
#include "N3310LCD.h"
#include "Joystick.h"
#include "mbed_bmp.h"

// demo for nuelectronics Nokia 3310 LCD shield (www.nuelectronics.com)
// 

// menu starting points
#define MENU_X    10        // 0-83
#define MENU_Y    1        // 0-5

#define DEMO_ITEMS 4

// menu definition
char menu_items[DEMO_ITEMS][12] =
{
    "TEMPERATURE",
    "CHAR MAP",
    "BITMAP",
    "ABOUT"    
};

void temperature(N3310LCD* lcd)
{
    lcd->writeStringBig(5, 1, "+21.12", NORMAL);
    lcd->writeString(73, 2, "C", NORMAL);
}

void charmap(N3310LCD* lcd)
{
    for(int i = 0; i < 5; i++)
    {
        for(int j = 0; j < 14; j++)
        {
          lcd->locate(j*6, i);
          lcd->writeChar(i*14 + j + 32, NORMAL);
        }
    }
}

void bitmap(N3310LCD* lcd)
{
    lcd->drawBitmap(20, 1, mbed_bmp, 48, 24);
}

void about(N3310LCD* lcd)
{
    lcd->writeString(0, 1, "Nokia 3310 LCD", NORMAL);
    lcd->writeString(15, 2, "driven by", NORMAL);
    lcd->writeString(30, 3, "mbed", NORMAL);
}

void (*menu_funcs[DEMO_ITEMS])(N3310LCD*) = 
{
    temperature,
    charmap,
    bitmap,
    about
};

void initMenu(N3310LCD* lcd)
{
    lcd->writeString(MENU_X, MENU_Y, menu_items[0], HIGHLIGHT );
    
    for (int i = 1; i < DEMO_ITEMS; i++)
    {
        lcd->writeString(MENU_X, MENU_Y + i, menu_items[i], NORMAL);
    }
}

void waitforOKKey(N3310LCD* lcd, Joystick* jstick)
{
    lcd->writeString(38, 5, "OK", HIGHLIGHT );

    int key = 0xFF;
    while (key != CENTER_KEY)
    {
        for (int i = 0; i < NUM_KEYS; i++)
        {
            if (jstick->getKeyState(i) !=0)
            {
                jstick->resetKeyState(i);  // reset
                if (CENTER_KEY == i) key = CENTER_KEY;
            }
        }
    }
}

void autoDemo(N3310LCD* lcd)
{
    while (true)
    {
        for (int i = 0; i < DEMO_ITEMS; i++)
        {
            lcd->cls();
            lcd->backlight(ON);
            wait(1);
                
            (*menu_funcs[i])(lcd);
        
            wait(3);
        
            lcd->backlight(OFF);
            wait(3);
        }
    }
}

int main() 
{
    Joystick jstick(N3310SPIPort::AD0);
    N3310LCD lcd(N3310SPIPort::MOSI, N3310SPIPort::MISO, N3310SPIPort::SCK,
                 N3310SPIPort::CE, N3310SPIPort::DAT_CMD, N3310SPIPort::LCD_RST,
                 N3310SPIPort::BL_ON);
    lcd.init();
    lcd.cls();
    lcd.backlight(ON);
    
    // demo stuff
    // autoDemo(&lcd);
    
    initMenu(&lcd);
    int currentMenuItem = 0;
    Ticker jstickPoll;
    jstickPoll.attach(&jstick, &Joystick::updateADCKey, 0.01);    // check ever 10ms
    
    
    while (true)
    {
    for (int i = 0; i < NUM_KEYS; i++)
    {
        if (jstick.getKeyState(i) != 0)
        {    
            jstick.resetKeyState(i);  // reset button flag
            switch(i)
            {
                case UP_KEY:
                    // current item to normal display
                    lcd.writeString(MENU_X, MENU_Y + currentMenuItem, menu_items[currentMenuItem], NORMAL);
                    currentMenuItem -=1;
                    if (currentMenuItem <0)  currentMenuItem = DEMO_ITEMS -1;
                    // next item to highlight display
                    lcd.writeString(MENU_X, MENU_Y + currentMenuItem, menu_items[currentMenuItem], HIGHLIGHT);
                    break;
                    
                case DOWN_KEY:
                    // current item to normal display
                    lcd.writeString(MENU_X, MENU_Y + currentMenuItem, menu_items[currentMenuItem], NORMAL);
                    currentMenuItem +=1;
                    if(currentMenuItem >(DEMO_ITEMS - 1))  currentMenuItem = 0;
                    // next item to highlight display
                    lcd.writeString(MENU_X, MENU_Y + currentMenuItem, menu_items[currentMenuItem], HIGHLIGHT);
                    break;
                    
                case LEFT_KEY:
                    initMenu(&lcd);
                    currentMenuItem = 0;
                    break;
                    
                case RIGHT_KEY:
                    lcd.cls();
                    (*menu_funcs[currentMenuItem])(&lcd);
                    waitforOKKey(&lcd, &jstick);
                    lcd.cls();
                    initMenu(&lcd);
                    currentMenuItem = 0;
                    break;    
            }        
        }
    }
    }    
         
    return EXIT_SUCCESS;
}
