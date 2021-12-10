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


#ifndef _CORE_
#define _CORE_

#include "configure.h"


/** @file
 *
 * This file contains core functionality of the *Brownie* firmware.
 */

/** @addtogroup brownies_firmware
 *
 * @{
 */


#define EMPTY_MODULE(NAME)                                          \
  static inline void NAME##Init () {}                               \
  static inline void NAME##Iterate () {}                            \
  static inline void NAME##OnRegRead (uint8_t reg) {}               \
  static inline void NAME##OnRegWrite (uint8_t reg, uint8_t val) {} \
  static inline void NAME##ISR () {}
  ///< @brief Helper to declare an empty / deactivated module (see @ref brownies_firmware).




// *************************** Memory and Registers ****************************


/// @name Memory and Registers ...
/// @{



// ***** VROM *****

extern const __flash TBrFeatureRecord brFeatureRecord;
  ///< @brief The *Brownie* feature recourd.



// ***** Configuration (SRAM-backed) *****

extern TBrConfigRecord _brConfigRecord;       ///< @private
#define brConfigRecord ((const TBrConfigRecord) _brConfigRecord)
  ///< @brief The *Brownie* config recourd.




// ***** Registers *****

extern uint8_t _regFile[BR_REGISTERS];        ///< @private

#define RegGet(REG) (_regFile[REG])
  ///< @brief Get a register value.
#define RegSet(REG, VAL) do { _regFile[REG] = (VAL); } while (0)
  ///< @brief Set a register to a new value.





// ***************** Persistent Storage (EEPROM) *******************************


/// @}
/// @name Persistent Storage (EEPROM) ...
/// @{


/** @brief Structure to describe the complete EEPROM content.
 *
 * It is important to have predictable addresses for the ID and config records.
 */
struct SBrEeprom {
  TBrIdRecord id;         ///< @brief *Brownie* ID record
  TBrConfigRecord cfg;    ///< @brief *Brownie* config record (persistent EEPROM copy)

  // Module-related data for persistency ...
  uint8_t shadesPos[2];   ///< @brief (shades) Position
};


extern struct SBrEeprom brEeprom;
  ///< @brief Complete EEPROM contents





// *************************** Change Reporting ********************************


/// @}
/// @name Change Reporting ...
/// @{


#if !IS_MAINTENANCE || DOXYGEN


extern uint8_t chgShadow;      ///< @internal


static inline void ReportChange (uint8_t mask) { chgShadow |= mask; }
  ///< @brief Set (a) bit(s) in the @ref BR_REG_CHANGED register.
void ReportChangeAndNotify (uint8_t mask);
  ///< @brief Set (a) bit(s) in the @ref BR_REG_CHANGED register and issue a TWI host notification.


#endif // IS_MAINTENANCE





// *************************** Timer *******************************************


/// @}
/// @name Timer ...
/// @{


#if WITH_TIMER || DOXYGEN


// Constants are defined in 'interface.h' (BR_TICKS_*).


uint16_t TimerNow ();
  ///< @brief Get current time in ticks.
  ///
  /// One tick is approx. 1ms (1024µs for a calibrated clock).
  /// => Counter overflow is in approx. one minute (~65 seconds).
  /// This function never returns 0 (= BR_TICKS_NEVER) (if necessary by rounding away from that value).


#else   // WITH_TIMER
static inline uint16_t TimerNow () { return 0; }
#endif  // WITH_TIMER





// *************************** Mini-Timer **************************************


/// @}
/// @name Mini-Timer ...
///
/// 8-bit counter with a cycle time of 8µs to be used locally in modules.
/// Presently, the mini-timer is used by the ISRs of the UART and the 'temperature' modules.
/// Minitimers must be used with interrupts disabled.
///
/// **Note:** MinitimerReset() and MinitimerNow() are implemented as macros to ensure
///    that they are inlined. Otherwise, ISRs may get a very long prologue saving
///    many registers. This causes problems in 'TemperatureISR()' on the ATtiny861.
///
/// @{


#define MINITICKS_OF_US(X) (X * (BR_CPU_FREQ / 1000000) / 8)
  ///< @brief Number of 8-bit timer ticks of a microsecond value for a clock selection of clk_io/8.


#if DOXYGEN

#define MINI_CLOCK_SCALE_1            ///< @brief Minitimer clock period of 1/BR_CPU_FREQ
#define MINI_CLOCK_SCALE_8            ///< @brief Minitimer clock period of 8 * 1/BR_CPU_FREQ
#define MINI_CLOCK_SCALE_64           ///< @brief Minitimer clock period of 64 * 1/BR_CPU_FREQ
#define MINI_CLOCK_SCALE_256          ///< @brief Minitimer clock period of 256 * 1/BR_CPU_FREQ
#define MINI_CLOCK_SCALE_1024         ///< @brief Minitimer clock period of 1024 * 1/BR_CPU_FREQ

void MinitimerStart (int clockScale); ///< @brief Start the minitimer (pass any MINI_CLOCK_SCALE_* constants)
void MinitimerStop ();                ///< @brief Stop the minitimer
void MinitimerReset();                ///< @brief Reset the minitimer
uint8_t MinitimerNow();               ///< @brief Get the current minitimer value


#elif MCU_TYPE == BR_MCU_ATTINY85 || MCU_TYPE == BR_MCU_ATTINY84


#define MINI_CLOCK_SCALE_1    1
#define MINI_CLOCK_SCALE_8    2
#define MINI_CLOCK_SCALE_64   3
#define MINI_CLOCK_SCALE_256  4
#define MINI_CLOCK_SCALE_1024 5

static inline void MinitimerStart (int CLOCK_SCALE) {
  TCCR0A = 0;   // Normal port operation (OC1A/OC1B disconnected); No waveform generation
  TCCR0B = CLOCK_SCALE;   // enable 8-bit timer with clk_io / 8; one tick is 8 µs; one ZACwire bit is 125 µs ~= 16 clock units
}

static inline void MinitimerStop () {
  TCCR0B = 0;
}

#define MinitimerReset() do { TCNT0 = 0; } while (0)

#define MinitimerNow() ((uint8_t) TCNT0)


#elif MCU_TYPE == BR_MCU_ATTINY861


#define MINI_CLOCK_SCALE_1    1
#define MINI_CLOCK_SCALE_8    4
#define MINI_CLOCK_SCALE_64   7
#define MINI_CLOCK_SCALE_256  9
#define MINI_CLOCK_SCALE_1024 11

static inline void MinitimerStart (int CLOCK_SCALE) {
  TCCR1A = 0;   // Normal port operation (OC1A/OC1B disconnected); No waveform generation
  TCCR1B = (1 << PSR1) | CLOCK_SCALE;
  TC1H = 0;     // Set high byte to 0 (affects TCNT1, OCR1C and any other 10 bit registers)
  OCR1C = 0xff; // "top" value for counter
  PLLCSR = 0;   // disable PLL, ensure synchronous mode
}

static inline void MinitimerStop () {
  TCCR1B = 0;
}

#define MinitimerReset() do { TCNT1 = 0; TCCR1B |= (1 << PSR1); } while (0)
  // Note: According to the manual, "Due to synchronization of the CPU, Timer/Counter1
  //       data written into Timer/Counter1 is delayed by one and half CPU clock cycles
  //       in synchronous mode". Hence, we add a delay here before reading TCNT1.

#define MinitimerNow() ((uint8_t) TCNT1)


#endif // MCU_TYPE == BR_MCU_ATTINY861





// ***************** Interface Functions of the "core" Module ******************


/// @}
/// @name Interface Functions of the "core" Module ...
///
/// The feature modules (e.g. gpio.[hc], shades.[hc]) have a common set of
/// interface functions. The following functions are the interface for the
/// pseudo-module "core", which implements some core functionality.
///
/// **If you are developing a feature module: Never call these functions directly.
/// They are only documented here in order to document the general module interface
/// functions.**
///
/// @{


void CoreInit ();
  ///< @brief Initialize the module.
  ///
  /// In this case, the timers and the "changed" register logic are initialized.
  ///
static inline void CoreIterate () {}
  ///< @brief Iterate the module.
  ///
  /// This function is called regularly from the main event loop and may do some
  /// regular housekeeping.
  ///
void CoreOnRegRead (uint8_t reg);
  ///< @brief Update a register when it is read.
  ///
  /// The ...OnRegRegRead() functions are called whenever the *Brownie* has received
  /// a "register read" request just before it is answered. The function must check
  /// if 'reg' references a register maintained by this module and may then update the
  /// register content. For example, the GPIO module reads the GPIO inputs then.
  ///
  /// This function handles reads from the @ref BR_REG_CHANGED register (i.e.
  /// auto-resets it on read) and reads from the timer registers (i.e. latches
  /// the 16-bit time value on access to @ref BR_REG_TICKS_LO).
  ///
void CoreOnRegWrite (uint8_t reg, uint8_t val);
  ///< @brief Write a register.
  ///
  /// The ...OnRegRegWrite() functions are called whenever the *Brownie* has received
  /// a "register write" request. The function must check, if 'reg' references a
  /// register maintained by this module and perform the repective action.
  /// Also, the passed value must be written into the respective register.
  ///
  /// This function handles writes to the @ref BR_REG_CHANGED register.
  ///


/// @}    // name

/// @}    // addtogroup brownies_firmware



#endif // _CORE_
