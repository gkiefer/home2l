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


#ifndef _BASE_
#define _BASE_


/** @file
 *
 * This file contains basic definitions for the *Brownie* firmware.
 *
 * @addtogroup brownies_firmware
 *
 * This module describes some internal interfaces of the *Brownie* firmware.
 * They are relevant for extending the firmware by, for example, adding new
 * feature modules.
 *
 * To add a new feature module *foobar*, the following steps have to be done
 * (replace *foobar* with the name of your module):
 *
 * 1. In source directory 'brownies/avr', add a new file *foobar.h* with the following
 *    content:
 *    @code
 *      #ifndef _FOOBAR_
 *      #define _FOOBAR_
 *
 *      #include "core.h"
 *
 *      #if !WITH_FOOBAR
 *      EMPTY_MODULE(Foobar)
 *      #else  // WITH_FOOBAR
 *
 *      void FoobarInit ();
 *      void FoobarIterate ();
 *      void FoobarOnRegRead (uint8_t reg);
 *      void FoobarOnRegWrite (uint8_t reg, uint8_t val);
 *
 *      #endif // WITH_FOOBAR
 *
 *      #endif // _FOOBAR_
 *    @endcode
 *
 * 2. In the same directory, add a new file *foobar.c* with the following content:
 *    @code
 *      #if WITH_FOOBAR
 *
 *      #include "foobar.h"
 *
 *      ... put your code here ...
 *
 *      void FoobarInit () {
 *        ... put your code here ...
 *      }
 *
 *      void FoobarIterate () {
 *        ... put your code here ...
 *      }
 *
 *      void FoobarOnRegRead (uint8_t reg) {
 *        ... put your code here ...
 *      }
 *
 *      void FoobarOnRegWrite (uint8_t reg, uint8_t val) {
 *        ... put your code here ...
 *      }
 *
 *
 *      #endif // WITH_FOOBAR
 *    @endcode
 *
 *    The documentation of CoreInit() and friends describe what the interface
 *    functions must do.
 *
 * 3. Add the source file *foobar.c* to the list of source files'SRC' in 'brownies/avr/Makefile'.
 *
 * 4. In file @ref configure.h, add a new section for the compile-time parameters of your module.
 *    At least one parameter, 'WITH_FOOBAR' must be defined. Other parameters must be prefixed
 *    with 'FOOBAR_'.
 *
 *    In the same file, please also extend the sections "Features: Auto-Completion",
 *    "Ports: Auto-Completion", "Ports: Checks", and "MCU: Reset Pin Configuration".
 *
 * 5. If the module *foobar* requires new MCU pins: Add them in section "MCU Port Assignments"
 *    in file @ref configure.h.
 *
 * 6. If the module *foobar* requires new registers: Go to section "Register Map" in
 *    file @ref interface.h (starting with @ref BR_REG_CHANGED), identify a resonable unused
 *    register number and add the definition of your register(s) there. Name the registers
 *    'BR_REG_FOOBAR_...', and name constants (e.g. bit masks) related to your registers
 *    like 'BR_FOOBAR_...'.
 *
 * @{
 */


#include <avr/io.h>

#include "interface.h"





// *************************** Basic Helpers ***********************************


/// @name Basic Helpers ...
/// @{

#define INIT(NAME,VAL) if ((VAL) != 0) NAME = (VAL);
  ///< @brief Variable assignment for '*Init()' functions which are only called once on startup.
  ///
  /// This is to wrap normal variable assignemts, which are optimized out if the
  /// value is zero (BSS initialization has done the job already).


#define LO(X) ((X) & 0xff)      ///< Get low byte of a 16-bit word-
#define HI(X) ((X) >> 8)        ///< Get high byte of a 16-bit word.
#define HILO(H,L) ((((uint16_t) (H)) << 8) | (L))
                                ///< Compose a 16-bit word from high and low byte.


#define SHIFT_OF_MASK(X) (((X) & 0x01) ? 0 : ((X) & 0x02) ? 1 : ((X) & 0x04) ? 2 : ((X) & 0x08) ? 3 \
                        : ((X) & 0x10) ? 4 : ((X) & 0x20) ? 5 : ((X) & 0x40) ? 6 : ((X) & 0x80) ? 7 : 8)
  ///< @brief Get the number of shifts to obtain a certain byte mask (inverse of '1 << N').





// ********************* MCU Port Access Macros ********************************


/// @}
/// @name MCU Port Access Macros ...
/// @{

#define P_A0 0x0001
#define P_A1 0x0002
#define P_A2 0x0004
#define P_A3 0x0008
#define P_A4 0x0010
#define P_A5 0x0020
#define P_A6 0x0040
#define P_A7 0x0080

#define P_B0 0x0100
#define P_B1 0x0200
#define P_B2 0x0400
#define P_B3 0x0800
#define P_B4 0x1000
#define P_B5 0x2000
#define P_B6 0x4000
#define P_B7 0x8000


#if (defined(PINA) && defined(PORTA) && defined(DDRA)) || DOXYGEN


#define P_IN(P) (LO(P) ? (PINA & LO(P)) : HI(P) ? (PINB & HI(P)) : 0)
  ///< @brief Read a _single_ pin; result is either 0 or non-zero, depending on whether the pin is set.
#define P_IN_MULTI(MASK) ((LO(MASK) ? (PINA & LO(MASK)) : 0) | (HI(MASK) ? ((uint16_t) (PINB & HI(MASK))) << 8 : 0))
  ///< @brief Read multiple pins, selected by PMASK; result is 16-bit vector.

#define P_OUT_0(P) do { if (LO(P)) PORTA &= ~LO(P); if (HI(P)) PORTB &= ~HI(P); } while(0)
#define P_OUT_1(P) do { if (LO(P)) PORTA |=  LO(P); if (HI(P)) PORTB |=  HI(P); } while(0)
  ///< @brief Set/reset pin(s).
  /// Usually, only a single bit should be passed here to make the compiler generate 'sbi' and 'cbi' instructions.
#define P_OUT_MULTI(MASK, P) do {                                   \
      if (LO(MASK)) PORTA = (PORTA & ~LO(MASK)) | (LO(P) & LO(MASK)); \
      if (HI(MASK)) PORTB = (PORTB & ~HI(MASK)) | (HI(P) & HI(MASK)); \
    } while(0)
  ///< @brief Set multiple pins, selected by 'MASK', to 'P'.

#define P_DDR_IN(P)   do { if (LO(P)) DDRA &= ~LO(P); if (HI(P)) DDRB &= ~HI(P); } while(0)
  ///< @brief Set port(s) as input.
#define P_DDR_OUT(P)  do { if (LO(P)) DDRA |=  LO(P); if (HI(P)) DDRB |=  HI(P); } while(0)
  ///< @brief Set port(s) as output.


#else   // defined(PINA) && defined(PORTA) && defined(DDRA)


#define P_IN(P) (HI(P) ? (PINB & HI(P)) : 0)
#define P_IN_MULTI(MASK) ((HI(MASK) ? ((uint16_t) (PINB & HI(MASK))) << 8 : 0))

#define P_OUT_0(P) do { if (HI(P)) PORTB &= ~HI(P); } while(0)
#define P_OUT_1(P) do { if (HI(P)) PORTB |=  HI(P); } while(0)
#define P_OUT_MULTI(MASK, P) do { if (HI(MASK)) PORTB = (PORTB & ~HI(MASK)) | (HI(P) & HI(MASK)); } while(0)

#define P_DDR_IN(P)   do { if (HI(P)) DDRB &= ~HI(P); } while(0)
#define P_DDR_OUT(P)  do { if (HI(P)) DDRB |=  HI(P); } while(0)


#endif  // defined(PINA) && defined(PORTA) && defined(DDRA)





// *************************** MCU Type ****************************************


/// @}
/// @name MCU Types ...
/// @{

#if DOXYGEN
#define MCU_TYPE    ///< MCU type compiled for; is set to any 'BR_MCU_*' value (see @ref BR_MCU_NONE and friends).
#endif


#if defined(__AVR_ATtiny85__)
#define MCU_TYPE BR_MCU_ATTINY85
#endif

#if defined(__AVR_ATtiny84__) || defined(__AVR_ATtiny84A__)
#define MCU_TYPE BR_MCU_ATTINY84
#endif

#if defined(__AVR_ATtiny861__) || defined(__AVR_ATtiny861A__)
#define MCU_TYPE BR_MCU_ATTINY861
#endif

#ifndef MCU_TYPE
#error "Unsupported MCU type!"
#endif


/// @}    // name

/// @}    // addtogroup brownies_firmware


#endif // _BASE_
