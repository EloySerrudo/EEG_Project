/*
 * adsCommand.h
 *
 * Copyright (c) 2013-2019 by Adam Feuer <adam@adamfeuer.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _ADS_COMMAND_H
#define _ADS_COMMAND_H

#include "Arduino.h"

// constants define pins on Arduino 

// NodeMCU ESP32
// HackEEG Shield v1.5.0
const uint8_t IPIN_PWDN = 21;
// const uint8_t PIN_CLKSEL = 48; // Está a DVDD en OpenBCI
const uint8_t IPIN_RESET = 22;

const uint8_t PIN_START = 17;
const uint8_t IPIN_DRDY = 16;   // board 0: JP1, pos. 1
//const int IPIN_DRDY = 25; // board 1: JP1, pos. 2
//const int IPIN_DRDY = 26; // board 2: JP1, pos. 3
//const int IPIN_DRDY = 27; // board 3: JP1, pos. 4

const uint8_t PIN_CS = 5;   // board 0: JP2, pos. 3
//const int PIN_CS = 52; // board 1: JP2, pos. 4
//const int PIN_CS = 10; // board 2: JP2, pos. 5
//const int PIN_CS = 4;  // board 3: JP2, pos. 6

//const int PIN_DOUT = 11;  //SPI out
//const int PIN_DIN = 12;   //SPI in
//const int PIN_SCLK = 13;  //SPI clock

void adcWreg(uint8_t reg, uint8_t val);
void adcSendCommand(uint8_t cmd);
void adcSendCommandLeaveCsActive(uint8_t cmd);
uint8_t adcRreg(uint8_t reg);

#endif // _ADS_COMMAND_H
