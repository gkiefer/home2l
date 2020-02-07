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


#ifndef _SHADES_
#define _SHADES_





// *************************** Constants ***************************************


/* Scratch calculations for timings/countings
 *
 *   Typical duration full up/down (100%): 15..50 sec.
 *   => 1m in approx. 20 secs. -> 0.05 m/sec
 *      -> 2% .. 7% per sec. = 0.002% ... 0.007% per tick
 *
 *   with 8 fractional bits: 0.5 .. 1.8 units
 *   with timer scaled by 64: 32 .. 115 units per macro-tick (64 ms / ~ 16 per second)
 *
 * => fastest possible shades: speed = 255 -> ~100 steps -> 64ms * 100 = 6.4 secs total time -> OK
 *    slowest possible shades: speed = 1 -> 25600 steps -> 64ms * 25600 = 1638 secs total time (27 minutes)
 *
 * Resolution is always <= 1%.
 *
 * Calculation of calibration values:
 *
 *    delayUp/delayDown: given in macro-ticks (units of 1/16 sec.) -> max. is 16 secs.
 *    speedUp/speedDown: given in 1/256% per macro-ticks (1/16s) = 1/16% per second
 */


#define SHADES_TU_SHIFT   6     // ticks by which the tick timer is shifted to obtain shades time units
                                // (6 = one time unit has ~64ms)

#define SHADES_POS_SHIFT  8     // Fractional bits in 16 bit position values
                                // => 'int16_t' can represent values from -127% .. +127% (-128% == 0x8000 = SHADES_POS_NONE)
#define SHADES_POS_NONE 0x8000  // Value representing no or undefined value





// *************************** AVR headers *************************************


#if AVR


#if !WITH_SHADES
EMPTY_MODULE(Shades)
#else  // WITH_SHADES


void ShadesInit ();
void ShadesIterate ();
void ShadesOnRegRead (uint8_t reg);
void ShadesOnRegWrite (uint8_t reg, uint8_t val);


#endif  // WITH_SHADES


#endif // AVR





// *************************** Exported Linux helpers **************************


#if !AVR


#include <math.h>


static inline float ShadesDelayFromByte (uint8_t byte) {
  return byte * ((float) (1 << SHADES_TU_SHIFT) / BR_TICKS_PER_SECOND);
}


static inline float ShadesSpeedFromByte (uint8_t byte) {
  return (100 << SHADES_POS_SHIFT) * ((float) (1 << SHADES_TU_SHIFT) / BR_TICKS_PER_SECOND) / (float) byte;
}


static inline bool ShadesDelayToByte (float delay, uint8_t *byte) {
  float byteVal;

  byteVal = delay * (BR_TICKS_PER_SECOND / (float) (1 << SHADES_TU_SHIFT));
  if (byteVal < 0.0 || byteVal > 255.4) return false;
  *byte = round (byteVal);
  return true;
}


static inline bool ShadesSpeedToByte (float speed, uint8_t *byte) {
  float byteVal;

  if (speed == 0.0) return false;
  byteVal = (100 << SHADES_POS_SHIFT) * ((float) (1 << SHADES_TU_SHIFT) / BR_TICKS_PER_SECOND) / speed;
  if (byteVal < 0.6 || byteVal > 255.4) return false;
  *byte = round (byteVal);
  return true;
}


#endif // !AVR



#endif
