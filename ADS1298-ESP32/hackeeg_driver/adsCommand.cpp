/**
 * send and receive commands from TI ADS129x chips.
 *
 * Copyright (c) 2013 by Adam Feuer <adam@adamfeuer.com>
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

#include "Arduino.h"
#include "adsCommand.h"
#include "ads129x.h"
#include "SpiDma.h"

/*void wait_for_drdy(int interval)
{
	int i = 0;
	while (digitalRead(IPIN_DRDY) == HIGH) {
		if (i < interval) {
			continue;
		}
		i = 0;
	}
}*/

void adcSendCommand(uint8_t cmd) {
    digitalWrite(PIN_CS, LOW);
    spiSend(cmd);
    delayMicroseconds(1);
    digitalWrite(PIN_CS, HIGH);
}

void adcSendCommandLeaveCsActive(uint8_t cmd) {
    digitalWrite(PIN_CS, LOW);
    spiSend(cmd);
}

void adcWreg(uint8_t reg, uint8_t val) {
    //see pages 40,43 of datasheet -
    digitalWrite(PIN_CS, LOW);
    spiSend(ADS129x::WREG | reg);
    delayMicroseconds(2);
    spiSend(0);    // number of registers to be read/written – 1
    delayMicroseconds(2);
    spiSend(val);
    delayMicroseconds(1);
    digitalWrite(PIN_CS, HIGH);
}

uint8_t adcRreg(uint8_t reg) {
    uint8_t out = 0;
    digitalWrite(PIN_CS, LOW);
    spiSend(ADS129x::RREG | reg);
    delayMicroseconds(2);
    spiSend(0);    // number of registers to be read/written – 1
    delayMicroseconds(2);
    out = spiRec();
    delayMicroseconds(1);
    digitalWrite(PIN_CS, HIGH);
    return out;
}
