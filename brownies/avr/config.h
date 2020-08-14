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


/* This file is arranged in the following sections:
 *
 * 1. Features: User Parameters
 *    - any user parameters settable in 'Family.mk' are predefined and documented here.
 *
 * 2. Features: Auto-Completion
 *    - auto-completion of feature-related parameters ('WITH_*' macros)
 *    - auto-enable "timer" and "notify" features
 *
 * 3. MCU: Pin Assignments
 *    - pin assignments of supported MCU types
 *    -> Edit this section to add a new MCU!
 *
 * 4. Ports: Auto-Completion
 *    - auto-generate pin-related macros (P_*, RESET_DDR_IN_*, RESET_DDR_OUT_*, RESET_DDR_STATE_*)
 *
 * 5. Ports: Checks
 *    - sanity checks
 *
 * 6. MCU: Reset Pin Configuration
 *    - define INIT_PINS() macro
 */


#ifndef _BROWNIE_CONFIG_
#define _BROWNIE_CONFIG_


/** @file
 *
 * @addtogroup brownies_features
 *
 * This module contains all changable feature settings for *Brownie* firmwares.
 * New customized firmwares can be added to the *Brownie family* by changing
 * the file [brownies/avr/Family.mk](../brownies/avr/Family.mk).
 *
 * @{
 */


#include "base.h"





// *************************** Features: User Parameters ***********************


// This section lists and documents the user-definable feature settings and
// sets defaults.



/// @name General ...
/// @{

#ifndef IS_MAINTENANCE
#define IS_MAINTENANCE 0          ///< @brief This firmware is a maintenance system?
#endif

#ifndef TWI_SL_NOTIFY
#define TWI_SL_NOTIFY (!IS_MAINTENANCE)
  ///< @brief Device may perform notifications via its slave interface?
  ///
  /// If the device must comply to the i2c standard, this option must be switched off
  /// (e.g. if the master is the Linux host).
#endif

#ifndef TWI_SL_NOTIFY_US
#define TWI_SL_NOTIFY_US 10000.0
  ///< @brief Duration of a notification in microseconds
  ///
  /// This time must be at least the transmission time of approx. 2 bytes, so that in case
  /// of a collision the observed address becomes all-0.
  /// 10 ms is sufficient for an effective bit rate of 2 kbit/s including processing times and clock
  /// stretching (20 bits in 10 ms).
#endif



/// @}
/// @name GPIO ...
/// @{

#ifndef WITH_TIMER
#define WITH_TIMER 0              ///< @brief The timer is enabled if this is 1 *or* any other feature requires the timer.
#endif



/// @}
/// @name GPIO ...
/// @{

#ifndef GPIO_IN_PRESENCE
#define GPIO_IN_PRESENCE  0       ///< Pins to be used as GP inputs
#endif

#ifndef GPIO_IN_PULLUP
#define GPIO_IN_PULLUP    0       ///< Inputs with activated internal pullups
#endif

#ifndef GPIO_OUT_PRESENCE
#define GPIO_OUT_PRESENCE 0       ///< Pins to be used as GP outputs
#endif

#ifndef GPIO_OUT_PRESET
#define GPIO_OUT_PRESET   0       ///< Output default state (will be set before Z-state is left)
#endif



/// @}
/// @name TWI Master / Hub ...
///
/// **Note:** Presently, only a single TWI master is supported, which then acts as hub,
/// i.e. all requests/replies to other nodes of the same subnet are forwarded
/// via the master port currently set (#0 at present).
/// In the future, this may change: There may be multiple master ports, and
/// master ports may be used independently of the hub functionality - for
/// example, to work with i2c devices locally.
///
/// @{

#ifndef WITH_TWIHUB
#define WITH_TWIHUB 0             ///< Enable TWI hub functionality over master port #0.
#endif
#ifndef TWIHUB_PORT
#define TWIHUB_PORT 0             ///< TWI master port to use for the hub (must be 0 presently).
#endif

#ifndef TWI_MA_PORTS
#define TWI_MA_PORTS (WITH_TWIHUB ? 1 : 0)  ///< Number of TWI master ports.
#endif
#ifndef TWI_MA_INTERNAL_PULLUP
#define TWI_MA_INTERNAL_PULLUP 0  ///< Activate internal pullups for the master SCL/SDA lines (DEPRECATED).
#endif



/// @}
/// @name Matrix ...
///
/// The diode switch matrix uses the pins avaliable as GPIO for the MCU, which
/// may then not be used as GPIOs.
///
/// If MATRIX_ROWS > 1, the last MATRIX_ROWS ports are the stimulation ports.
/// The MATRIX_COLS ports before those (or the last ports at all, if no stimulating
/// ports exist) are the sensing ports.
///
/// In the special case of MATRIX_ROWS == 1, no stimulation ports are assigned,
/// and no active stimulation is performed by the software module. In the circuitry,
/// the row lines of the switches should be connected to VCC (i.e. constantly be
/// pulled high via some resistor) instead of a stimulation port.
///
/// @{

#ifndef MATRIX_ROWS
#define MATRIX_ROWS       0       ///< Number of stimulating lines (rows) (max. 8).
#endif

#ifndef MATRIX_COLS
#define MATRIX_COLS       0       ///< Number of sensing lines (columns) (max. 8).
#endif

#ifndef MATRIX_ROWS_GSHIFT
#if MATRIX_ROWS >= 2
#define MATRIX_ROWS_GSHIFT (GPIO_PINS_MAX - MATRIX_ROWS)
  ///< @brief Index of the first GPIO pin to be assigned to row lines.
  ///
  /// By default, the last MATRIX_ROWS GPIO pins are the stimulation (row) lines.
  /// If MATRIX_ROWS == 1, no row lines are used. But this setting may have an effect
  /// on the default MATRIX_COLS_GSHIFT setting.
#else
#define MATRIX_ROWS_GSHIFT GPIO_PINS_MAX    // no stimulating lines at all ...
#endif
#endif

#ifndef MATRIX_COLS_GSHIFT
#define MATRIX_COLS_GSHIFT (MATRIX_ROWS_GSHIFT - MATRIX_COLS)
  ///< @brief Index of the first GPIO pin to be assigned to column lines.
  ///
  /// By default, the last MATRIX_COLS pins just before the row lines are the sensing (column) lines.
#endif

#ifndef MATRIX_T_SAMPLE
#define MATRIX_T_SAMPLE   4       ///< Time (ticks) a row is driven to 1 to sample.
#endif

#ifndef MATRIX_T_PERIOD
#define MATRIX_T_PERIOD  16       ///< Time (ticks) before switching to the next row.
#endif

#ifndef MATRIX_BUFSIZE
#define MATRIX_BUFSIZE    8       ///< Event buffer size.
#endif



/// @}
/// @name Temperature ...
/// @{

#ifndef WITH_TEMP_ZACWIRE
#define WITH_TEMP_ZACWIRE   0     ///< Enable ZACwire temperature interface.
#endif

#ifndef TEMP_NOTIFY
#define TEMP_NOTIFY         0     ///< Enable bus notification for temperature changes (does not affect @ref BR_REG_CHANGED).
#endif



/// @}
/// @name ADCs ...
/// @{

#ifndef ADC_PORTS
#define ADC_PORTS      0     ///< Number of ADC input ports (max. 2, depending on MCUs) (NOT IMPLEMENTED YET)
#endif



/// @}
/// @name Shades ...
///
/// The 'shades' module allows to control up to two window shades or actuators
/// in a wider sense (e.g. including actuators to open/close windows or gates).
/// Each actuator is associated with two output pins to active its engine in
/// the "up" or "down" direction and two input pins connected to two push buttons
/// for the two directions.
///
/// Note on the shades ports:
///
/// 1. For the buttons ('P_SHADES_n_BTN_*'), the internal pullup is activated,
///    and the pin must be pulled to GND (logical 0) by the button.
///
/// 2. The actor pins ('P_SHADES_n_ACT_*') are driven high (+VDD) if the engine
///    must be started and low (GND) to stop the engine.
///
/// @{

#ifndef SHADES_PORTS
#define SHADES_PORTS        0     ///< Number of (shades) actuators (max. 2)
#endif

#ifndef SHADES_TIMEOUT
#define SHADES_TIMEOUT  30000
  ///< @brief Number of milliseconds without connection before the brownie assumes that the master is offline.
  ///
  /// After the timeout,
  /// - all external requests (REXT) are reset to 0xff,
  /// - all internal requests (RINT) are set to BR_SHADES_<n>_RINT_FAILSAFE.
  ///
  /// A value of 0 means "no timeout".
#endif

#ifndef SHADES_REVERSE_DELAY
#define SHADES_REVERSE_DELAY 1000
  ///< @brief Minimum time (in ms) the actor is kept off before it is switched on again (either in the same or other direction).
#endif

#ifndef SHADES_0_RINT_FAILSAFE
#define SHADES_0_RINT_FAILSAFE 0xff
#endif
#ifndef SHADES_1_RINT_FAILSAFE
#define SHADES_1_RINT_FAILSAFE 0xff
  ///< @brief Failsafe internal request value(s) (RINT) for the case the brownie has lost
  /// its connection to the master (-1 = RINT is not changed on time-out).
  ///
  /// Typically used values are:
  /// -  -1: For normal shades: Shades eventually stop and become controllable by local buttons.
  /// -   0: For window openers: The window is closed and afterwards becomes controllable by local buttons.
  ///
  /// The failsafe request is activated (and REXT set to -1) in the following situations:
  /// - after reset,
  /// - after the first successful read or write access to any of the shades registers or the
  ///   @ref BR_REG_CHANGED register: if no further such access happens within the time period specified
  ///   by @ref SHADES_TIMEOUT.
#endif

#ifndef SHADES_PERSISTENCE
#define SHADES_PERSISTENCE    0
  ///< @brief If set, the position is stored in EEPROM to minimize calibrations.
#endif

#ifndef SHADES_TOLERANCE
#define SHADES_TOLERANCE      2
  ///< @brief Tolerated deviation between the real and requested position in %.
  /// If the difference between real and requested state is no more than this, shades are not started to move.
#endif


/// @}      // name
/// @}      // addtogroup brownies_features








// *************************** Features: Auto-Completion ***********************


#if !DOXYGEN


// GPIO ...
#define WITH_GPIO ((GPIO_IN_PRESENCE) || (GPIO_OUT_PRESENCE))


// TWI Master...
#define WITH_TWI_MASTER ((TWI_MA_PORTS) > 0)


// Matrix...
#define WITH_MATRIX ((MATRIX_ROWS) * (MATRIX_COLS) != 0)


// Analog ...
#define WITH_ADC ((ADC_PORTS) > 0)


// Shades ...
#define WITH_SHADES ((SHADES_PORTS) > 0)


// Timer ...
#if WITH_MATRIX || WITH_SHADES || WITH_TEMP_ZACWIRE
  // The above features require the timer: Auto-enable it
#undef WITH_TIMER
#define WITH_TIMER 1
#endif


// Notifications ...
#if !WITH_GPIO && !WITH_TWIHUB && !WITH_MATRIX && !WITH_SHADES
  // Without any of these features, notifications make no sense:
  // We disable notification at all.
#undef TWI_SL_NOTIFY
#define TWI_SL_NOTIFY 0
#endif


#endif // !DOXYGEN





// ********************** MCU: Pin Assignments *********************************


#if DOXYGEN


// Note: All Doxygen documentation for the MCU port assignemts is done here at
//       the beginning of this section to deal with the alternative sections
//       defining the constants of the same names.


/** @addtogroup brownies_pins
 *
 * The following constants must (or may) be defined in an MCU-specific way
 * to identify the pin(s) of the respective signal. For each MCU type, the
 * configuration file contains a section defining these.
 *
 * Some are optional, depending on the MCU and features supported.
 *
 * New MCUs or features may be added, or the pin assignment of some MCU may be
 * changed by editing source code following this comment.
 * @{
 */

#define ISR_PCINT0          ///< Interrupt vector: pin change 0
#define ISR_PCINT1          ///< Interrupt vector: pin change 1
#define ISR_USI_STARTCOND   ///< Interrupt vector: USI start condition
#define ISR_USI_OVERFLOW    ///< Interrupt vector: USI overflow

// USI ...
#define P_USI_SCL           ///< TWI slave SCL ('twi_sl_scl')
#define P_USI_SDA           ///< TWI slave SDA ('twi_sl_sda')

// GPIOs ...
#define GPIO_PINS_MAX       ///< GPIO: Maximum number of pins usable as GPIOs
#define GPIO_TO_PMASK(GMASK)
  ///< @brief GPIO: Map a logical GPIO mask to MCU port A/B mask (see also @ref GPIO_FROM_PMASK).
#define GPIO_FROM_PMASK(PMASK)
  ///< @brief GPIO: Map MCU port mask to a logical GPIO mask.
  ///
  /// These two macros define the mapping betweeen logical GPIO pins to port A and port B bits
  /// for all GPIOs enabled by @ref GPIO_PINS_MAX.
  /// Unused bits must be masked out, so that GPIO_TO_PMASK(0xffff) identifies
  /// all ports used as GPIOs and GPIO_FROM_MASK(0xffff) identifies all logically
  /// usable GPIOs, respectively.

// TWI Master ...
#define P_TWI_MA_0_SCL      ///< TWI master: SCL pin
#define P_TWI_MA_0_SDA      ///< TWI master: SDA pin

// Temperature ...
#define P_TEMP_ZACWIRE      ///< Temperature: ZACwire data pin

// Shades ...
#define P_SHADES_0_BTN_UP   ///< Shades (0): button "up" pin
#define P_SHADES_0_BTN_DN   ///< Shades (0): button "down" pin
#define P_SHADES_0_ACT_UP   ///< Shades (0): actuator "up" pin
#define P_SHADES_0_ACT_DN   ///< Shades (0): actuator "down" pin
#define P_SHADES_1_BTN_UP   ///< Shades 1: button "up" pin
#define P_SHADES_1_BTN_DN   ///< Shades 1: button "down" pin
#define P_SHADES_1_ACT_UP   ///< Shades 1: actuator "up" pin
#define P_SHADES_1_ACT_DN   ///< Shades 1: actuator "down" pin

// ADC(s) ...
#define P_ADC_0             ///< ADC (0) pin
#define P_ADC_1             ///< ADC 1 pin

  /// @}      // addtogroup brownies_pins
#else // DOXYGEN





// ***** ATtiny85 *****


#if MCU_TYPE == MCU_TYPE_ATTINY85


// Interrupts ...
//   Interrupt vector names vary with the MCU model. A translation list can be found at:
//   https://www.nongnu.org/avr-libc/user-manual/group__avr__interrupts.html
#define ISR_PCINT0          PCINT0_vect
#define ISR_USI_STARTCOND   USI_START_vect
#define ISR_USI_OVERFLOW    USI_OVF_vect

// USI ...
#define P_USI_SCL           P_B2
#define P_USI_SDA           P_B0

// GPIOs ...
#define GPIO_PINS_MAX 3

#define GPIO_TO_PMASK(GMASK) ((((GMASK) & 0x06) << 10) | (((GMASK) & 0x01) << 9))
#define GPIO_FROM_PMASK(PMASK) ((((PMASK) & 0x18) >> 10) | (((PMASK) & 0x02) >> 9))
  // Macros to map logical GPIO pins to port A and port B bits.
  // Unused bits should be masked out, so that 'GPIO_TO_PMASK(0xffff)' identifies
  // all ports usable as GPIOs and GPIO_FROM_MASK(0xffff) identifies all logically
  // usable GPIOs, respectively.

// TWI Master ...
#define P_TWI_MA_0_SCL      P_B3
#define P_TWI_MA_0_SDA      P_B4





// ***** ATtiny84 *****


#elif MCU_TYPE == MCU_TYPE_ATTINY84


// Interrupts ...
//   Interrupt vector names vary with the MCU model. A translation list can be found at:
//   https://www.nongnu.org/avr-libc/user-manual/group__avr__interrupts.html
#define ISR_PCINT0          PCINT0_vect
#define ISR_PCINT1          PCINT1_vect
#define ISR_USI_STARTCOND   USI_STR_vect
#define ISR_USI_OVERFLOW    USI_OVF_vect

// USI ...
#define P_USI_SCL           P_A4
#define P_USI_SDA           P_A6

// GPIOs ...
#define GPIO_PINS_MAX 8

#define GPIO_TO_PMASK(GMASK) (((GMASK) & 0x8f) | (((GMASK) & 0x70) << 4))
#define GPIO_FROM_PMASK(PMASK) (((PMASK) & 0x8f) | ((((PMASK) >> 4)) & 0x70))
  // Macros to map logical GPIO pins to port A and port B bits.
  // Unused bits should be masked out, so that 'GPIO_TO_PMASK(0xffff)' identifies
  // all ports usable as GPIOs and GPIO_FROM_MASK(0xffff) identifies all logically
  // usable GPIOs, respectively.

// TWI Master ...
#define P_TWI_MA_0_SCL      P_B0
#define P_TWI_MA_0_SDA      P_B1

// Temperature ...
#define P_TEMP_ZACWIRE      P_A0

// Shades ...
#define P_SHADES_0_BTN_UP   P_A2
#define P_SHADES_0_BTN_DN   P_A1
#define P_SHADES_0_ACT_UP   P_B1
#define P_SHADES_0_ACT_DN   P_B0

#define P_SHADES_1_BTN_UP   P_A3
#define P_SHADES_1_BTN_DN   P_A5
#define P_SHADES_1_ACT_UP   P_B2
#define P_SHADES_1_ACT_DN   P_A7

// ADC(s) ...
#define P_ADC_0             P_A5
#define P_ADC_1             P_A7





// ***** ATtiny861 *****


#elif MCU_TYPE == MCU_TYPE_ATTINY861


// Interrupts ...
//   Interrupt vector names vary with the MCU model. A translation list can be found at:
//   https://www.nongnu.org/avr-libc/user-manual/group__avr__interrupts.html
#define ISR_PCINT         PCINT_vect      // one vector for PCINT0 and PCINT1 (different from t84/t85)
#define ISR_USI_STARTCOND USI_START_vect
#define ISR_USI_OVERFLOW  USI_OVF_vect

// USI ...
#define P_USI_SCL         P_B2
#define P_USI_SDA         P_B0

// GPIOs ...
#define GPIO_PINS_MAX 12

#define GPIO_TO_PMASK(GMASK) (((GMASK) & 0x0ff) | (((GMASK) & 0xf00) << 3))
#define GPIO_FROM_PMASK(PMASK) (((PMASK) & 0x0ff) | ((((PMASK) >> 3)) & 0xf00))
  // Macros to map logical GPIO pins to port A and port B bits.
  // Unused bits should be masked out, so that 'GPIO_TO_PMASK(0xffff)' identifies
  // all ports usable as GPIOs and GPIO_FROM_MASK(0xffff) identifies all logically
  // usable GPIOs, respectively.

// Temperature ...
#define P_TEMP_ZACWIRE    P_B1



#else   // MCU_TYPE == ...
#error "Unknown MCU"
#endif  // MCU_TYPE == ...


#endif // !DOXYGEN





// *************************** Ports: Auto-Completion **************************


#if !DOXYGEN


// TWI Slave ...
//   The TWI slave is always present and always uses the USI.
#define TWI_SL_SCL P_USI_SCL
#define TWI_SL_SDA P_USI_SDA

#define RESET_DDR_IN_TWI_SL   ((TWI_SL_SCL) | (TWI_SL_SDA))
#define RESET_DDR_OUT_TWI_SL  0
#define RESET_STATE_TWI_SL    0


// GPIOs ...
#define GPIO_HAVE_UPPER (((GPIO_IN_PRESENCE) >= 0x100 || (GPIO_OUT_PRESENCE) >= 0x100))

#define RESET_DDR_IN_GPIO   GPIO_TO_PMASK (GPIO_IN_PRESENCE)
#define RESET_DDR_OUT_GPIO  GPIO_TO_PMASK (GPIO_OUT_PRESENCE)
#define RESET_STATE_GPIO    GPIO_TO_PMASK ((GPIO_IN_PULLUP & GPIO_IN_PRESENCE) | (GPIO_OUT_PRESET & GPIO_OUT_PRESENCE))


// TWI Master ...
#if !WITH_TWI_MASTER || TWI_MA_PORTS < 1 || !defined(P_TWI_MA_0_SCL) || !defined(P_TWI_MA_0_SDA)
#undef P_TWI_MA_0_SCL
#undef P_TWI_MA_0_SDA
#define P_TWI_MA_0_SCL        0
#define P_TWI_MA_0_SDA        0
#endif

#if !WITH_TWI_MASTER || TWI_MA_PORTS < 2 || !defined(P_TWI_MA_1_SCL) || !defined(P_TWI_MA_1_SDA)
#undef P_TWI_MA_1_SCL
#undef P_TWI_MA_1_SDA
#define P_TWI_MA_1_SCL        0
#define P_TWI_MA_1_SDA        0
#endif

#if !WITH_TWI_MASTER || TWI_MA_PORTS < 3 || !defined(P_TWI_MA_2_SCL) || !defined(P_TWI_MA_2_SDA)
#undef P_TWI_MA_2_SCL
#undef P_TWI_MA_2_SDA
#define P_TWI_MA_2_SCL        0
#define P_TWI_MA_2_SDA        0
#endif

#if !WITH_TWI_MASTER || TWI_MA_PORTS < 4 || !defined(P_TWI_MA_3_SCL) || !defined(P_TWI_MA_3_SDA)
#undef P_TWI_MA_3_SCL
#undef P_TWI_MA_3_SDA
#define P_TWI_MA_3_SCL        0
#define P_TWI_MA_3_SDA        0
#endif

#define RESET_DDR_IN_TWI_MA ((P_TWI_MA_0_SCL) | (P_TWI_MA_0_SDA) | (P_TWI_MA_1_SCL) | (P_TWI_MA_1_SDA) \
                           | (P_TWI_MA_2_SCL) | (P_TWI_MA_2_SDA) | (P_TWI_MA_3_SCL) | (P_TWI_MA_3_SDA))
#define RESET_DDR_OUT_TWI_MA  0
#define RESET_STATE_TWI_MA    0


// Matrix...
#if MATRIX_ROWS > 1
#define MATRIX_ROWS_GMASK (((1 << MATRIX_ROWS) - 1) << MATRIX_ROWS_GSHIFT) // GPIO mask of the stimulating lines
#else
#define MATRIX_ROWS_GMASK  0    // no stimulating lines at all ...
#endif
#define MATRIX_COLS_GMASK (((1 << MATRIX_COLS) - 1) << MATRIX_COLS_GSHIFT) // GPIO mask of the sensing lines

#define RESET_DDR_IN_MATRIX   GPIO_TO_PMASK (MATRIX_COLS_GMASK)
#define RESET_DDR_OUT_MATRIX  GPIO_TO_PMASK (MATRIX_ROWS_GMASK)
#define RESET_STATE_MATRIX    0     // drive '0' on row lines, keep column lines at high-impedance state


// Temperature ...
#if !WITH_TEMP_ZACWIRE || !defined(P_TEMP_ZACWIRE)
#undef P_TEMP_ZACWIRE
#define P_TEMP_ZACWIRE      0
#endif

#define RESET_DDR_IN_TEMP   P_TEMP_ZACWIRE
#define RESET_DDR_OUT_TEMP  0
#define RESET_STATE_TEMP    0


// ADC(s) ...
#if !WITH_ADC || ADC_PORTS < 1 || !defined(P_ADC_0)
#undef P_ADC_0
#define P_ADC_0             0
#endif

#if !WITH_ADC || ADC_PORTS < 2 || !defined(P_ADC_1)
#undef P_ADC_1
#define P_ADC_1             0
#endif

#define RESET_DDR_IN_ADC    (P_ADC_0 | P_ADC_1)
#define RESET_DDR_OUT_ADC   0
#define RESET_STATE_ADC     0


// Shades ...
#if !WITH_SHADES || SHADES_PORTS < 1 || \
    !defined(P_SHADES_0_BTN_UP) || !defined(P_SHADES_0_BTN_DN) || !defined(P_SHADES_0_ACT_UP) || !defined(P_SHADES_0_ACT_DN)
#undef P_SHADES_0_BTN_UP
#undef P_SHADES_0_BTN_DN
#undef P_SHADES_0_ACT_UP
#undef P_SHADES_0_ACT_DN
#define P_SHADES_0_BTN_UP     0
#define P_SHADES_0_BTN_DN     0
#define P_SHADES_0_ACT_UP     0
#define P_SHADES_0_ACT_DN     0
#endif

#if !WITH_SHADES || SHADES_PORTS < 2 || \
    !defined(P_SHADES_1_BTN_UP) || !defined(P_SHADES_1_BTN_DN) || !defined(P_SHADES_1_ACT_UP) || !defined(P_SHADES_1_ACT_DN)
#undef P_SHADES_1_BTN_UP
#undef P_SHADES_1_BTN_DN
#undef P_SHADES_1_ACT_UP
#undef P_SHADES_1_ACT_DN
#define P_SHADES_1_BTN_UP     0
#define P_SHADES_1_BTN_DN     0
#define P_SHADES_1_ACT_UP     0
#define P_SHADES_1_ACT_DN     0
#endif

#define RESET_DDR_IN_SHADES    (P_SHADES_0_BTN_UP | P_SHADES_0_BTN_DN | P_SHADES_1_BTN_UP | P_SHADES_1_BTN_DN)
#define RESET_DDR_OUT_SHADES   (P_SHADES_0_ACT_UP | P_SHADES_0_ACT_DN | P_SHADES_1_ACT_UP | P_SHADES_1_ACT_DN)
#define RESET_STATE_SHADES     RESET_DDR_IN_SHADES    // activate internal pullups for shades buttons


#endif // !DOXYGEN





// *************************** Ports: Checks ***********************************


#if !DOXYGEN    // The doxygen preprocessor may have some problems with the following macros.


// Check for pin conflicts ...
#define USEMASK_TWI_MA  (RESET_DDR_IN_TWI_MA | RESET_DDR_OUT_TWI_MA)
#define USEMASK_GPIO    (RESET_DDR_IN_GPIO | RESET_DDR_OUT_GPIO)
#define USEMASK_MATRIX  (RESET_DDR_IN_MATRIX | RESET_DDR_OUT_MATRIX)
#define USEMASK_TEMP    (RESET_DDR_IN_TEMP | RESET_DDR_OUT_TEMP)
#define USEMASK_ADC     (RESET_DDR_IN_ADC | RESET_DDR_OUT_ADC)
#define USEMASK_SHADES  (RESET_DDR_IN_SHADES | RESET_DDR_OUT_SHADES)

#if (TWI_SL_SCL | TWI_SL_SDA) & (USEMASK_TWI_MA | USEMASK_GPIO | USEMASK_MATRIX | USEMASK_TEMP | USEMASK_ADC | USEMASK_SHADES)
#error "TWI slave pins conflict with others!"
#endif

#if USEMASK_TWI_MA & (USEMASK_GPIO | USEMASK_MATRIX | USEMASK_TEMP | USEMASK_ADC | USEMASK_SHADES)
#error "TWI master pins conflict with defined GPIOs or other pins!"
#endif

#if USEMASK_MATRIX & (USEMASK_GPIO | USEMASK_TWI_MA | USEMASK_TEMP | USEMASK_ADC | USEMASK_SHADES)
#error "Matrix pins conflict with defined GPIOs or other pins!"
#endif

#if USEMASK_TEMP & (USEMASK_GPIO | USEMASK_TWI_MA | USEMASK_MATRIX | USEMASK_ADC | USEMASK_SHADES)
//~ #define STR(x) XSTR(x)
//~ #define XSTR(x) #x
//~ #pragma message(STR(USEMASK_TEMP) "; " STR(USEMASK_ADC))
#error "Temperature pin conflicts with defined GPIOs or other pins!"
#endif

#if USEMASK_ADC & (USEMASK_GPIO | USEMASK_TWI_MA | USEMASK_MATRIX | USEMASK_TEMP | USEMASK_SHADES)
#error "ADC pin(s) conflict with defined GPIOs or other pins!"
#endif

#if USEMASK_SHADES & (USEMASK_GPIO | USEMASK_TWI_MA | USEMASK_MATRIX | USEMASK_TEMP | USEMASK_ADC)
#error "Shades pin(s) conflict with defined GPIOs or other pins!"
#endif


// GPIO ...
#if (GPIO_IN_PRESENCE & GPIO_OUT_PRESENCE) \
    || (GPIO_IN_PRESENCE) >= (1 << GPIO_PINS_MAX) \
    || (GPIO_OUT_PRESENCE) >= (1 << GPIO_PINS_MAX)
#error "GPIOs misconfigured: Too many or concliction GPIOs defined!"
#endif


// TWI  Hub...
#if TWIHUB_PORT > TWI_MA_PORTS
#error "TWI hub: non-existing master port selected"
#endif


// TWI Master...
#if TWI_MA_PORTS > 0 && (!P_TWI_MA_0_SCL || !P_TWI_MA_0_SDA)
#error "TWI master port #0 enabled by configuration, but no MCU pins available!"
#endif
#if TWI_MA_PORTS > 1 && (!P_TWI_MA_1_SCL || !P_TWI_MA_1_SDA)
#error "TWI master port #1 enabled by configuration, but no MCU pins available!"
#endif
#if TWI_MA_PORTS > 2 && (!P_TWI_MA_2_SCL || !P_TWI_MA_2_SDA)
#error "TWI master port #2 enabled by configuration, but no MCU pins available!"
#endif
#if TWI_MA_PORTS > 3 && (!P_TWI_MA_3_SCL || !P_TWI_MA_3_SDA)
#error "TWI master port #3 enabled by configuration, but no MCU pins available!"
#endif
#if TWI_MA_PORTS > 4
#error "At most 4 TWI master ports supported!"
#endif


// Matrix...
#if !WITH_MATRIX && (MATRIX_ROWS || MATRIX_COLS)
#warning "Matrix appears to be partly configured - disabled."
#endif
#if MATRIX_ROWS > 8
#error "Matrix: too many rows!"
#endif
#if MATRIX_COLS > 8
#error "Matrix: too many columns!"
#endif
#if (MATRIX_ROWS > 1) && ((MATRIX_ROWS_GSHIFT < 0) || ((MATRIX_ROWS_GSHIFT + MATRIX_ROWS) > GPIO_PINS_MAX))
#error "Matrix: some row lines assigned to non-existing GPIO pins!"
#endif
#if (MATRIX_COLS_GSHIFT < 0) || ((MATRIX_COLS_GSHIFT + MATRIX_COLS) > GPIO_PINS_MAX)
#error "Matrix: some column lines assigned to non-existing GPIO pins!"
#endif
#if (MATRIX_ROWS_GMASK & MATRIX_COLS_GMASK) != 0
#error "Matrix: row and column pins overlap!"
#endif

// Temperature ...


// ADCs ...
#if ADC_PORTS > 0 && !P_ADC_0
#error "ADC #0 enabled by configuration, but no MCU pin available!"
#endif
#if ADC_PORTS > 1 && !P_ADC_1
#error "ADC #1 enabled by configuration, but no MCU pin available!"
#endif
#if ADC_PORTS > 2
#error "At most 2 ADC ports supported!"
#endif


// Shades ...
#if SHADES_PORTS > 0 && (!P_SHADES_0_BTN_DN || !P_SHADES_0_BTN_UP || !P_SHADES_0_ACT_DN || !P_SHADES_0_ACT_UP)
#error "Shades #0 enabled by configuration, but no MCU pins available!"
#endif
#if SHADES_PORTS > 1 && (!P_SHADES_1_BTN_DN || !P_SHADES_1_BTN_UP || !P_SHADES_1_ACT_DN || !P_SHADES_1_ACT_UP)
#error "Shades #1 enabled by configuration, but no MCU pins available!"
#endif
#if SHADES_PORTS > 2
#error "At most 2 shades supported!"
#endif


#endif // !DOXYGEN





// *************************** MCU: Reset Pin Configuration ********************


// Reset configuration for all used pins ...
#define RESET_DDR_IN_USED (RESET_DDR_IN_TWI_SL | RESET_DDR_IN_GPIO | RESET_DDR_IN_TWI_MA | RESET_DDR_IN_MATRIX | RESET_DDR_IN_TEMP | RESET_DDR_IN_ADC | RESET_DDR_IN_SHADES)
#define RESET_DDR_OUT_USED (RESET_DDR_OUT_TWI_SL | RESET_DDR_OUT_GPIO | RESET_DDR_OUT_TWI_MA | RESET_DDR_OUT_MATRIX | RESET_DDR_OUT_TEMP | RESET_DDR_OUT_ADC | RESET_DDR_OUT_SHADES)
#define RESET_STATE_USED (RESET_STATE_TWI_SL | RESET_STATE_GPIO | RESET_STATE_TWI_MA | RESET_STATE_MATRIX | RESET_STATE_TEMP | RESET_STATE_ADC | RESET_STATE_SHADES)

// Explicitly define the mask for all unused pins ...
//    The TWI slave pins, Reset, VCC and GND are excluded here.
//    We assume that all other pins are generally usable as GPIOs.
#define UNUSED_PINS (GPIO_TO_PMASK(0xffff) & ~(RESET_DDR_IN_USED | RESET_DDR_OUT_USED))

// Reset configuration for all pins ...
#define RESET_DDR_IN  (RESET_DDR_IN_USED | UNUSED_PINS)
#define RESET_DDR_OUT RESET_DDR_OUT_USED

#if !IS_MAINTENANCE
#define RESET_STATE   (RESET_STATE_USED | UNUSED_PINS)
  // Following the Atmel/Microchip recommendations, unused pins are configured as
  // inputs with their internal pullups activated in operational mode.
  // This allows to omit external circuitry for them.
#else
#define RESET_STATE   (RESET_STATE_USED)
  // In maintenance mode, all ports are set into a high-impedance state. This may cause inputs to float.
  // However, we avoid to pull up outputs (in operational mode) in a way that undesired behavior results.
#endif



// Initialization macro for the main program ...
#define INIT_PINS() do {                                      \
  P_DDR_IN(RESET_DDR_IN | RESET_DDR_OUT);                     \
  P_OUT_0((RESET_DDR_IN | RESET_DDR_OUT) & ~RESET_STATE);     \
  P_OUT_1((RESET_DDR_IN | RESET_DDR_OUT) & RESET_STATE);      \
  P_DDR_OUT(RESET_DDR_OUT);                                   \
} while(false);





#endif // _BROWNIE_CONFIG_
