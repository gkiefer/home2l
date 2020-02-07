/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2019-2020 Gundolf Kiefer
 *
 *  Home2L is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Home2L is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Home2L. If not, see <https://www.gnu.org/licenses/>.
 *
 */


#ifndef _TEMPERATURE_
#define _TEMPERATURE_

#include "core.h"


/* NOTE: The pin change ISR blocks for approx. 2.0 - 2.5 ms to read in a complete data word each 100 ms.
 */


#if !WITH_TEMP_ZACWIRE
EMPTY_MODULE(Temperature)
#else  // WITH_TEMP_ZACWIRE


#include <avr/io.h>
#include <avr/interrupt.h>


extern volatile uint16_t temperatureValue;
extern uint16_t temperatureTimeUpdated;


static inline void TemperatureInit () {
  MinitimerStart ();
}


static inline void TemperatureIterate () {
  register uint16_t val;

  // Check for new value from ISR ...
  cli ();
  val = temperatureValue;
  if (val) {
    temperatureValue = 0;           // clear value from ISR
    sei ();
    temperatureTimeUpdated = TimerNow ();
    if (LO(val) != RegGet (BR_REG_TEMP_LO) || HI(val) != RegGet (BR_REG_TEMP_HI)) {   // real change?
      RegSet (BR_REG_TEMP_LO, LO(val));  // update value in registers
      RegSet (BR_REG_TEMP_HI, HI(val));
      ReportChange (BR_CHANGED_TEMP);
    }
  }
  else {
    sei ();

    // Check for time out and delete temperature value in case
    if (temperatureTimeUpdated) if (TimerNow () - temperatureTimeUpdated > BR_TICKS_OF_MS (2000)) {   // time-out is 2000ms
      RegSet (BR_REG_TEMP_LO, 0);        // clear/invalidate values in registers
      RegSet (BR_REG_TEMP_HI, 0);
      if (TEMP_NOTIFY) ReportChange (BR_CHANGED_TEMP);
      temperatureTimeUpdated = 0;     // clear timer to accelerate future checks
    }
  }
}


static inline void TemperatureOnRegRead (uint8_t reg) {}


static inline void TemperatureOnRegWrite (uint8_t reg, uint8_t val) {}


static inline void TemperatureRead ()  {
  register uint16_t value;
  register uint8_t n, parity, bit, halfPeriod;

  // One mini-timer tick is 8 µs; one ZACwire bit is 125 µs ~= 16 ticks.
  if (P_IN(P_TEMP_ZACWIRE)) return;     // exit if pin is high (idle state)

  // We start sampling with the falling edge of the start bit: Wait for the rising edge ...
  MinitimerReset ();
  while (!P_IN(P_TEMP_ZACWIRE))
    if (MinitimerNow () > MINITICKS_OF_US(70)) return;   // time-out (low phase should not exceed 62.5µs)

  // Wait for first falling edge (end of start bit) and determine the half period time ...
  MinitimerReset ();
  while (P_IN(P_TEMP_ZACWIRE))
    if (MinitimerNow () > MINITICKS_OF_US(70)) return;   // time-out (high phase should not exceed 62.5µs)
  halfPeriod = MinitimerNow ();
  MinitimerReset ();      // Reset timer to mark falling edge/start of bit

  // Try to sample two words ...
  value = 0;
  parity = 0;
  for (n = 19; n; n--) {

    // Sample a bit ...
    while (MinitimerNow () < halfPeriod);
    bit = P_IN(P_TEMP_ZACWIRE);

    // Process bit ...
    switch (n) {
      case 11:  // parity high byte
      case 1:   // parity low byte
        if (bit) parity ^= 1;
        if (parity) return;     // wrong parity
        break;
      case 10:  // start bit low byte: just ignore
        break;
      default:  // 19..12 = data bits high byte
                // 9..2 = data bits low byte
        value <<= 1;
        if (bit) {
          value |= 1;
          parity ^= 1;
        }
    }

    // Wait until signal is 1 again ...
    while (!P_IN(P_TEMP_ZACWIRE))
      if (MinitimerNow () > MINITICKS_OF_US(150)) return;   // time-out (worst legal case: stop bit = 125us)

    // Wait for falling edge = start of next bit ...
    if (n > 1) {
      MinitimerReset ();
      while (P_IN(P_TEMP_ZACWIRE))
        if (MinitimerNow () > MINITICKS_OF_US(150)) return;   // time-out (worst legal case: stop bit = 125us)
      MinitimerReset ();      // Reset timer to mark falling edge/start of bit
    }
  }

  // Success: Store value ...
  temperatureValue = (value << 1) | 1;
}


static inline void TemperatureISR () {
  TemperatureRead ();
}


#endif // WITH_TEMP_ZACWIRE


#endif // _TEMPERATURE_
