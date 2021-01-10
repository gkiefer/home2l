/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2021 Gundolf Kiefer
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


#include "configure.h"

#if WITH_SHADES

#include "shades.h"
#include "core.h"

#include <avr/eeprom.h>




// *************************** Configuration ***********************************

// These values may be moved to 'config.h' some day.


#define SHADES_OVERDRIVE 10
  // Units by which shades moved into an end position (0 or 100) are overdriven
  // a) for calibration purposes and b) to let the relay switch after the internal
  // end switch of the actuator has already stopped the engine
  // (=> increased lifetime of the relay by avoiding arcs and sparks)
#define SHADES_DEBOUNCE_TIME  50
  // Time in ms for which a button state is held (for debouncing)





// ********************** Actuator / Button State ******************************


// 'actBtnState' bits...
#define AS_OFF          0
#define AS_UP           BR_SHADES_0_ACT_UP
#define AS_DN           BR_SHADES_0_ACT_DN
#define AS_REVERSE_WAIT (AS_UP | AS_DN)

#define BS_UP           BR_SHADES_0_BTN_UP
#define BS_DN           BR_SHADES_0_BTN_DN

#define AS_MASK         (AS_UP | AS_DN)
#define BS_MASK         (BS_UP | BS_DN)



typedef struct {
  uint8_t actBtnState;  // Bits reported via 'BR_REG_SHADES_STATUS'

  bool    calibrating;  // Indicates whether we are in a calibration run, and 'rawPos' contains a
                        // valid (!= SHADES_POS_NONE) value, which is however virtual and may be far from reality.

    /* If the shades are in an unknown position (rawPos == SHADES_POS_NONE), the following is done
     * to get it calibrated:
     *
     * 1. Nothing happens unless a request is issued.
     *
     * 2. If the first request is issued, this flag is set and the request is modified to 0% or 100%
     *    (+ overdrive), whatever is closer to the issued request. The position 'rawPos' is set to the
     *    opposite value, which results in minimum engine run time assuming a worst-case real position
     *    of the shades.
     *
     * 3. After the actuators stopped due to having reached 'rawPos', this flag is cleared.
     *
     * 4. If the effective request changes to 0xff, the shades are stopped, 'rawPos' is set to SHADES_POS_NONE
     *    again, and this flag is clear. (Effectively, the calibration is cancelled and a new one will
     *    start with the next request.)
     *
     * 5. If the effective request changes to some defined value, nothing special is done.
     *    (This new request will apply after the calibration has finished).
     *
     * As a future extension, this flag may be used to do re-calibrations. I.e., if the shades have been
     * moved to positions neither 0% nor 100%, a intermediate motion to 0% or 100% can be inserted.
     */

  int16_t rawPos;       // raw position (may be <0 or >100 while moving; SHADES_POS_NONE(0x8000) == unknown/uncalibrated)
  int16_t rawReq;       // effective raw request (after evaluation of RINT, REXT and overdrive; may be <0 or >100)

  uint16_t tuPos;       // time unit referring to the current position
  uint16_t tuActStart;  // time unit at which the actor has been started (for up/down delays)
  uint16_t tuActStop;   // time unit at which the actor has been stopped (for the reverse delay)

  uint16_t tBtnChange;  // time (in ticks) of last button event (any button, any direction) for debouncing

} TShade;


TShade shades[SHADES_PORTS];




// *************************** Physical access *********************************


static inline uint8_t ReadButtons (uint8_t shIdx) {
  register uint8_t bs = 0;
  if (shIdx == 0 || SHADES_PORTS <= 1) {
    if (!P_IN (P_SHADES_0_BTN_UP)) bs |= BS_UP;
    if (!P_IN (P_SHADES_0_BTN_DN)) bs |= BS_DN;
  }
  else {
    if (!P_IN (P_SHADES_1_BTN_UP)) bs |= BS_UP;
    if (!P_IN (P_SHADES_1_BTN_DN)) bs |= BS_DN;
  }
  return bs;
}


static inline void WriteActuators (uint8_t shIdx, uint8_t state) {
#if SHADES_PORTS > 2
#error "SHADES_PORTS > 2 not supported here."
#endif
  if (shIdx == 0 || SHADES_PORTS <= 1) {
    switch (state) {
      case AS_UP:
        P_OUT_0 (P_SHADES_0_ACT_DN);    // power off for "down"
        P_OUT_1 (P_SHADES_0_ACT_UP);    // power on for "up"
        break;
      case AS_DN:
        P_OUT_0 (P_SHADES_0_ACT_UP);    // power off for "up"
        P_OUT_1 (P_SHADES_0_ACT_DN);    // power on for "down"
        break;
      default:
        P_OUT_0 (P_SHADES_0_ACT_UP);    // power off in both directions ...
        P_OUT_0 (P_SHADES_0_ACT_DN);
    }
  }
  else {      // shIdx == 1
    switch (state) {
      case AS_UP:
        P_OUT_0 (P_SHADES_1_ACT_DN);    // power off for "down"
        P_OUT_1 (P_SHADES_1_ACT_UP);    // power on for "up"
        break;
      case AS_DN:
        P_OUT_0 (P_SHADES_1_ACT_UP);    // power off for "up"
        P_OUT_1 (P_SHADES_1_ACT_DN);    // power on for "down"
        break;
      default:
        P_OUT_0 (P_SHADES_1_ACT_UP);    // power off in both directions ...
        P_OUT_0 (P_SHADES_1_ACT_DN);
    }
  }
}






// *************************** Managing single shades **************************


static void OnRequestChanged (uint8_t shIdx) {
#if SHADES_PORTS > 2
#error "SHADES_PORTS > 2 not supported."
#endif
  TShade *sh;
  uint8_t reqInt, reqExt;

  // Get arguments ...
  if (shIdx == 0 || SHADES_PORTS == 1) {      // 'SHADES_PORTS == 1': let compiler optimize out the "else" branch
    sh = &shades[0];
    reqInt = RegGet (BR_REG_SHADES_0_RINT);
    reqExt = RegGet (BR_REG_SHADES_0_REXT);
  }
  else {
    sh = &shades[1];
    reqInt = RegGet (BR_REG_SHADES_1_RINT);
    reqExt = RegGet (BR_REG_SHADES_1_REXT);
  }

  // Determine effective request value and fold it into 'reqInt' ...
  if (reqExt <= 100) reqInt = reqExt;  // propagate 'reqExt' to 'reqInt'
  if (reqInt > 100) reqInt = 0xff;     // map any invalid value to "none"

  // Start / stop calibration cycle (-> 'rawPos'; if calibrating -> 'rawReq') ...
  if (!sh->calibrating) {   // Normal case ...
    if (reqInt <= 100 && sh->rawPos == SHADES_POS_NONE) {

      // We are uncalibrated and have to move => insert calibration cycle ...
      if (reqInt <= 50) {
        sh->rawReq =  - (SHADES_OVERDRIVE << SHADES_POS_SHIFT);        // move to 0% surely ...
        sh->rawPos = ((100 + SHADES_OVERDRIVE) << SHADES_POS_SHIFT);
      }
      else {
        sh->rawReq = ((100 + SHADES_OVERDRIVE) << SHADES_POS_SHIFT);   // move to 100% surely  ...
        sh->rawPos =  - (SHADES_OVERDRIVE << SHADES_POS_SHIFT);
      }
      sh->calibrating = true;
    }
  }
  else {

    // We are currently in a calibration run...
    if (reqInt > 100) {               // no request or request removed => stop calibration ...
      sh->rawPos = SHADES_POS_NONE;   // no calibration result => will have to start again next time
      sh->calibrating = false;
    }
  }

  // Calculate effective request value (only if not calibrating -> 'sh->rawReq') ...
  if (!sh->calibrating) {
    if (reqInt > 100) sh->rawReq = SHADES_POS_NONE;
    else if (reqInt == 0) sh->rawReq = - (SHADES_OVERDRIVE << SHADES_POS_SHIFT);
    else if (reqInt == 100) sh->rawReq = ((100 + SHADES_OVERDRIVE) << SHADES_POS_SHIFT);
    else sh->rawReq = (reqInt << SHADES_POS_SHIFT);
  }
}


static void IterateSingle (uint8_t shIdx, TShade *sh) {
#if SHADES_PORTS > 2
#error "SHADES_PORTS > 2 not supported here."
#endif
  uint16_t tuNow;
  uint8_t asStart, tuDelay;
  uint8_t bsCur, bsNew, bsPushed, regReqInt, regPos, pos;
  bool movingUp;

  // Read & Handle buttons ...
  bsCur = (sh->actBtnState & BS_MASK);
  bsNew = ReadButtons (shIdx);
  if (sh->tBtnChange) {
    if (TimerNow () - sh->tBtnChange >= BR_TICKS_OF_MS (SHADES_DEBOUNCE_TIME))
      sh->tBtnChange = BR_TICKS_NEVER;        // '0' == "waited long enough"
  }
  if (bsNew != bsCur) if (!sh->tBtnChange) {  // accepted button change (after debouncing)...

    // Set RINT based on button down events ...
    bsPushed = bsNew & ~bsCur;
    if (bsPushed) {
      regReqInt = (shIdx == 0 || SHADES_PORTS <= 1) ? BR_REG_SHADES_0_RINT : BR_REG_SHADES_1_RINT;
      if (sh->actBtnState & AS_MASK)  {                           // Actuator active or "reverse waiting" => stop
        regPos = (shIdx == 0 || SHADES_PORTS <= 1) ? BR_REG_SHADES_0_POS : BR_REG_SHADES_1_POS;
        RegSet (regReqInt, RegGet (regPos));
      }
      else if (bsPushed == BS_UP)     RegSet (regReqInt, 0);      // Up
      else if (bsPushed == BS_DN)     RegSet (regReqInt, 100);    // Down
      OnRequestChanged (shIdx);
    }

    // Write back button state and set time stamp ...
    sh->actBtnState = (sh->actBtnState & ~BS_MASK) | bsNew;
    sh->tBtnChange = TimerNow ();
  }

  // Update actuator state and 'rawPos', start/stop engines as adequate ...
  movingUp = false;
  switch (sh->actBtnState & AS_MASK) {

    case AS_OFF:
      // Actuators are off: Check, if we should start them ...
      if (sh->rawReq != SHADES_POS_NONE) {    // have defined request?
        asStart = 0;
        tuDelay = 0;
        if (sh->rawPos > 0)     // not fully up? -> consider moving up...
          if (((sh->rawPos - sh->rawReq) >> SHADES_POS_SHIFT) > SHADES_TOLERANCE) {
            asStart = AS_UP;
            tuDelay = brConfigRecord.shadesDelayUp[shIdx];
          }
        if (sh->rawPos < (100 << SHADES_POS_SHIFT))     // not fully down? -> consider moving down...
          if (((sh->rawReq - sh->rawPos) >> SHADES_POS_SHIFT) > SHADES_TOLERANCE) {
            asStart = AS_DN;
            tuDelay = brConfigRecord.shadesDelayDown[shIdx];
          }

        // Start moving if required ...
        if (asStart) {
          if (SHADES_PERSISTENCE)   // persistent position is "unknown" while moving
            eeprom_write_byte (&brEeprom.shadesPos[shIdx], 0xff);
          WriteActuators (shIdx, asStart);        // start engine!
          tuNow = TimerNow () >> SHADES_TU_SHIFT;
          sh->tuActStart = tuNow;
          sh->tuPos = tuNow + tuDelay;   // extrapolate position time to the time the shades will first move
          sh->actBtnState |= asStart;
        }
      }
      break;

    case AS_REVERSE_WAIT:
      if (TimerNow () - (sh->tuActStop << SHADES_TU_SHIFT) >= BR_TICKS_OF_MS(SHADES_REVERSE_DELAY))
        sh->actBtnState &= ~AS_MASK;
      break;

    case AS_UP:   // Moving up ...
      movingUp = true;
      // Fall through ('movingUp' has been preset to 'false' above for the down case)...
    case AS_DN:   // Moving up or down ...
      // Update position and time values ...
      tuNow = TimerNow () >> SHADES_TU_SHIFT;
      while (((int16_t) (tuNow - sh->tuPos) << SHADES_TU_SHIFT) > 0) {
        /* Note on the shifting and arithmetic above:
         *   The ticks timer wraps around after 2^16 ticks (approx. 60 seconds) or 2^(16-SHADES_TU_SHIFT) time units.
         *   The 'tuPos' value does not! By subtracting and shifting back to ticks units, only the difference
         *   between the last and current time must be below approx. 60 seconds; The same limitation holds
         *   for the delay time, but *not* for the total shades run time, which may now last up to 60 << SHADES_TU_SHIFT
         *   seconds.
         *
         * Note on the loop at all:
         *   Actually, we do a multiplication here ("sh->rawPos += (tuNow - sh->tuPos) * brConfigRecord.shadesSpeedUp[shIdx]").
         *   It can be expected that this loop only has very few iterations (0 or 1; 2 and above only in rare cases),
         *   so that this loop is considered more efficient than multiplying.
         */
        if (movingUp) sh->rawPos -= brConfigRecord.shadesSpeedUp[shIdx];
        else          sh->rawPos += brConfigRecord.shadesSpeedDown[shIdx];
        sh->tuPos++;
      }
      // Stop if necessary ...
      if (sh->rawReq == SHADES_POS_NONE
          || (movingUp && sh->rawPos < (sh->rawReq + (1 << (SHADES_POS_SHIFT-1))))
          || (!movingUp && sh->rawPos >= sh->rawReq - (1 << (SHADES_POS_SHIFT-1)))) {
        WriteActuators (shIdx, AS_OFF);         // stop engine
        sh->actBtnState |= AS_REVERSE_WAIT;
        sh->tuActStop = tuNow;                  // record time
        if (movingUp) {
          if (sh->rawPos != SHADES_POS_NONE && sh->rawPos < 0) {
            sh->rawPos = 0;   // adjust (truncate) position
            sh->calibrating = false;
          }
        }
        else {
          if (sh->rawPos != SHADES_POS_NONE && sh->rawPos > (100 << SHADES_POS_SHIFT)) {
            sh->rawPos = (100 << SHADES_POS_SHIFT);     // adjust (truncate) position
            sh->calibrating = false;
          }
        }
        OnRequestChanged (shIdx);     // If we just stopped a calibration phase, we must update the request now!
      }
      // Write back pos register ...
      pos = (sh->rawPos == SHADES_POS_NONE || sh->calibrating)  ? 0xff :
            (sh->rawPos < 0)                                    ?    0 :
            (sh->rawPos >= (100 << SHADES_POS_SHIFT))           ?  100 :
                ((sh->rawPos + (1 << (SHADES_POS_SHIFT-1))) >> SHADES_POS_SHIFT);   // round to nearest integer (important!)
      regPos = (shIdx == 0 || SHADES_PORTS <= 1) ? BR_REG_SHADES_0_POS : BR_REG_SHADES_1_POS;
      RegSet (regPos, pos);
      if (SHADES_PERSISTENCE)   // set persistent position when stopping
        if ((sh->actBtnState & AS_MASK) == AS_REVERSE_WAIT) eeprom_write_byte (&brEeprom.shadesPos[shIdx], pos);
      break;

  } // switch (sh->actBtnState & AS_MASK)
}





// *************************** Main entry points *******************************


static uint16_t tLastStatusRead;      // for emergency watchdog


static void ResetRequests () {
  RegSet (BR_REG_SHADES_0_REXT, 0xff);
  RegSet (BR_REG_SHADES_0_RINT, SHADES_0_RINT_FAILSAFE);
  RegSet (BR_REG_SHADES_1_REXT, 0xff);
  RegSet (BR_REG_SHADES_1_RINT, SHADES_1_RINT_FAILSAFE);
}


static inline uint16_t PosToRaw (uint8_t pos) {
  if (pos > 100) return SHADES_POS_NONE;
  else return ((int16_t) pos) << SHADES_POS_SHIFT;
}


void ShadesInit () {
  uint8_t n;

  // Init registers ...
  RegSet (BR_REG_SHADES_STATUS, 0);
#if SHADES_PERSISTENCE
  RegSet (BR_REG_SHADES_0_POS, eeprom_read_byte (&brEeprom.shadesPos[0]));
  shades[0].rawPos = PosToRaw (RegGet (BR_REG_SHADES_0_POS));
  RegSet (BR_REG_SHADES_1_POS, SHADES_PORTS > 1 ? eeprom_read_byte (&brEeprom.shadesPos[1]) : 0xff);
  if (SHADES_PORTS > 1) shades[1].rawPos = PosToRaw (RegGet (BR_REG_SHADES_1_POS));
#else
  RegSet (BR_REG_SHADES_0_POS, 0xff);
  RegSet (BR_REG_SHADES_1_POS, 0xff);
#endif
  ResetRequests ();

  // Init data structures ...
  INIT (tLastStatusRead, 0);
  for (n = 0; n < SHADES_PORTS; n++) {
    INIT (shades[n].actBtnState, 0);
    INIT (shades[n].calibrating, false);
    if (!SHADES_PERSISTENCE) INIT (shades[n].rawPos, SHADES_POS_NONE);
    INIT (shades[n].tBtnChange, BR_TICKS_NEVER);
    OnRequestChanged (n);
  }
}


void ShadesIterate () {
#if SHADES_PORTS > 2
#error "SHADES_PORTS > 2 not supported here."
#endif
  uint8_t statCur, statNew;

  // Handle safety cases ...
  if (SHADES_TIMEOUT) if (tLastStatusRead) if (TimerNow () - tLastStatusRead > BR_TICKS_OF_MS(SHADES_TIMEOUT)) {
    // Time-out happened: Go into a failsafe state...
    tLastStatusRead = 0;
    ResetRequests ();
    OnRequestChanged (0);
    if (SHADES_PORTS > 1) OnRequestChanged (1);
  }

  // Iterate over shades ...
  IterateSingle (0, &shades[0]);
  if (SHADES_PORTS > 1) IterateSingle (1, &shades[1]);

  // Update status register ...
  statCur = RegGet (BR_REG_SHADES_STATUS);
#if SHADES_PORTS == 1
  statNew = shades[0].actBtnState;
#elif SHADES_PORTS == 2
  statNew = shades[0].actBtnState | (shades[1].actBtnState << 4);
#endif
  RegSet (BR_REG_SHADES_STATUS, statNew);
  if (statNew != statCur) ReportChangeAndNotify (BR_CHANGED_SHADES);
}


void ShadesOnRegRead (uint8_t reg) {
  if (reg == BR_REG_SHADES_STATUS || reg == BR_REG_CHANGED) tLastStatusRead = TimerNow ();
}


void ShadesOnRegWrite (uint8_t reg, uint8_t val) {
  switch (reg) {
    case BR_REG_SHADES_0_RINT:
    case BR_REG_SHADES_0_REXT:
      RegSet (reg, val);
      tLastStatusRead = TimerNow ();
      OnRequestChanged (0);
      break;
    case BR_REG_SHADES_0_POS:
      RegSet (reg, val);
      shades[0].rawPos = PosToRaw (val);
      if (SHADES_PERSISTENCE) eeprom_write_byte (&brEeprom.shadesPos[0], val);
      break;

#if SHADES_PORTS > 1
    case BR_REG_SHADES_1_RINT:
    case BR_REG_SHADES_1_REXT:
      RegSet (reg, val);
      tLastStatusRead = TimerNow ();
      OnRequestChanged (1);
      break;
    case BR_REG_SHADES_1_POS:
      RegSet (reg, val);
      shades[1].rawPos = PosToRaw (val);
      if (SHADES_PERSISTENCE) eeprom_write_byte (&brEeprom.shadesPos[1], val);
      break;
#endif

    default: break;
  }
}


#endif // WITH_SHADES
