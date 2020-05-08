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


#include "config.h"

#if WITH_MATRIX

#include "matrix.h"
#include "core.h"

#include <avr/io.h>





// *************************** Event Buffer ************************************


#if (MATRIX_BUFSIZE & (MATRIX_BUFSIZE-1)) != 0
#error "MATRIX_BUFSIZE must be a power of 2!"
#endif


static uint8_t buf[MATRIX_BUFSIZE], bufCycles[MATRIX_BUFSIZE];
static uint8_t bufIn, bufOut;
static bool bufOverflow;


static inline void BufInit () {
  INIT (bufIn, 0);
  INIT (bufOut, 0);
  INIT (bufOverflow, true);     // Init in overflow state (~= event queue disabled)
}


static inline void BufClear () {
  bufIn = bufOut = 0;
  bufOverflow = false;
}


static inline void BufPut (uint8_t val, uint8_t cycle) {
  uint8_t nextIn;

  if (bufOverflow) return;
  nextIn = (bufIn + 1) & (MATRIX_BUFSIZE-1);
  if (nextIn == bufOut) bufOverflow = true;
  else {
    buf[bufIn] = val;
    bufCycles[bufIn] = cycle;
    bufIn = nextIn;
  }
}


static inline uint8_t BufGet () {
  register uint8_t ret;

  if (bufIn == bufOut)
    return bufOverflow ? BR_MATRIX_EV_OVERFLOW : BR_MATRIX_EV_EMPTY;
  ret = buf[bufOut];
  bufOut = (bufOut + 1) & (MATRIX_BUFSIZE-1);
  return ret;
}


static inline uint8_t BufGetNextCylce () {
  // Get cycle value for the next event returned by 'BufGet()'.
  // The return value is only valid, if the return value of 'BufGet()' is.
  // It is not allowed to put to or clear the buffer between the two calls.
  return bufCycles[bufOut];
}





// *************************** Matrix ******************************************


#if MATRIX_T_SAMPLE >= MATRIX_T_PERIOD
#error "MATRIX_T_SAMPLE must be less than MATRIX_T_PERIOD!"
#endif


#define matrix ((uint8_t *) &_regFile[BR_REG_MATRIX_0])
                                          // current matrix values, directly mapped to regfile
static uint8_t matrixLast[MATRIX_ROWS];   // last read values for debouncing;
                                          // only if the currently read value is equal to the last read one, it is processed further
#if MATRIX_ROWS > 1
static uint8_t matrixRow;   // currently sampled row
#else
#define matrixRow 0         // some optimization
#endif
static uint8_t matrixCycle; // cycle counter (incremented whenever 'matrixRow' resets)

static uint16_t tSample;    // Time when sampling has started (BR_TICKS_NEVER = not sampling)
static uint16_t tPeriod;    // Time to start next sampling





// *************************** Top-Level ***************************************


#define AFTER(T, TREF) ((int16_t) (T - TREF) >= 0)


void MatrixInit () {
  BufInit ();
#if MATRIX_ROWS > 1
  INIT (matrixRow, 0);
#endif
  INIT (tSample, 0);
  tPeriod = TimerNow ();
}


void MatrixIterate () {
  uint16_t tNow;
  uint8_t i, rowVal, rowVal1, rowVal2, mask, stableMask, changedMask;

  tNow = TimerNow ();
  if (tSample != BR_TICKS_NEVER) {

    // We are sampling ...
    if (AFTER (tNow, tSample)) {

      // Read sample ...
      rowVal = GPIO_FROM_PMASK (P_IN_MULTI (GPIO_TO_PMASK (MATRIX_COLS_GMASK))) >> MATRIX_COLS_GSHIFT;

      // Process sample ...
      rowVal1 = matrix[matrixRow];      // 'rowVal' at time step -1
      rowVal2 = matrixLast[matrixRow];  // 'rowVal' at time step -2
      stableMask = ~(rowVal ^ rowVal1); // bits not changed between steps -1 and 0
      changedMask = (rowVal1 ^ rowVal2) & stableMask;
                                        // bits changed between steps -2 and -1 and stable now
                                        //   -> These will be reported.
      matrixLast[matrixRow] = rowVal1;  // shift row values ... next at step -2 is current at step -1
      matrix[matrixRow] = rowVal;       // ... next at step -1 is current at step 0

      // Report changes ...
      if (changedMask) {                // some bit(s) in the row changed?

        // Submit events ...
        for (i = 0, mask = 0x01; mask && !bufOverflow; i++, mask <<= 1) if (changedMask & mask) {
          BufPut (
              (i << BR_MATRIX_EV_COL_SHIFT) |
              (matrixRow << BR_MATRIX_EV_ROW_SHIFT) |
              ((rowVal & mask) ? (1 << BR_MATRIX_EV_VAL_SHIFT) : 0),
              matrixCycle
          );
        }

        // Report change ...
        ReportChange (BR_CHANGED_MATRIX);
      }

      // Stop row stimulation ...
      P_OUT_MULTI (GPIO_TO_PMASK (MATRIX_ROWS_GMASK), 0);
      tSample = BR_TICKS_NEVER;

      // Select next row...
#if MATRIX_ROWS > 1
      matrixRow++;
      if (matrixRow >= MATRIX_ROWS) {
        matrixRow = 0;
        matrixCycle++;
      }
#endif

      // Set next period time ...
      do {
        tPeriod += MATRIX_T_PERIOD;
      } while (AFTER (tNow, tPeriod));
    }
  }
  else {

    // We are waiting for the next period ...
    if (AFTER (tNow, tPeriod)) {

      // Start row stimulation ...
      P_OUT_MULTI (GPIO_TO_PMASK (MATRIX_ROWS_GMASK),
                   GPIO_TO_PMASK ((1 << MATRIX_ROWS_GSHIFT) << matrixRow));

      // Set time for sampling ...
      tSample = TimerNow () + MATRIX_T_SAMPLE;
        // read time again to ensure that minimum sampling time is held
        // (it may be shortened if an interrupt occurs between the initial 'tNow' reading and the row stimulation)
    }
  }
}


void MatrixOnRegRead (uint8_t reg) {
  uint8_t cycle, event;

  if (reg == BR_REG_MATRIX_EVENT) {
    cycle = BufGetNextCylce ();
    event = BufGet ();        // get and consume next event
    RegSet (BR_REG_MATRIX_EVENT, event);
    RegSet (BR_REG_MATRIX_ECYCLE, event < 0x80 ? cycle : 0);
  }
}


void MatrixOnRegWrite (uint8_t reg, uint8_t val) {
  if (reg == BR_REG_MATRIX_EVENT) {
    if (val == BR_MATRIX_EV_EMPTY) BufClear (); // Reset buffer
  }
}


#endif // WITH_MATRIX
