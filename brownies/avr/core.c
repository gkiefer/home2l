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


#include "core.h"

#include "version.h"
#include "twi.h"

#include <string.h>

#include <avr/pgmspace.h>
#include <avr/eeprom.h>


#if BR_FLASH_PAGESIZE < SPM_PAGESIZE
#error "BR_FLASH_PAGESIZE must not be smaller than SPM_PAGESIZE"
#endif





// *************************** Memory and Registers ****************************



// ***** VROM *****


const __flash TBrFeatureRecord brFeatureRecord = {
  .versionMajor     = VERSION_MAJOR,
  .versionMinor     = VERSION_MINOR,
  .versionRevision  = VERSION_REVISION,

  .features =
      BR_FEATURE_MAINTENANCE * IS_MAINTENANCE +
      BR_FEATURE_TIMER    * WITH_TIMER +
      BR_FEATURE_NOTIFY   * TWI_SL_NOTIFY +
      BR_FEATURE_TWIHUB   * WITH_TWIHUB +
      BR_FEATURE_ADC_0    * WITH_ADC +
      BR_FEATURE_ADC_1    * ((WITH_ADC && ADC_PORTS >= 2) ? 1 : 0) +
      BR_FEATURE_UART     * WITH_UART +
      BR_FEATURE_TEMP     * WITH_TEMP_ZACWIRE +
      BR_FEATURE_SHADES_0 * WITH_SHADES +
      BR_FEATURE_SHADES_1 * ((WITH_SHADES && SHADES_PORTS >= 2) ? 1 : 0),

  .gpiPresence  = GPIO_IN_PRESENCE,
  .gpiPullup    = GPIO_IN_PULLUP & GPIO_IN_PRESENCE,
  .gpoPresence  = GPIO_OUT_PRESENCE,
  .gpoPreset    = GPIO_OUT_PRESET & GPIO_OUT_PRESENCE,

  .matDim       = (MATRIX_ROWS << 4) | (MATRIX_COLS),

  .fwName       = BROWNIE_FWNAME,

  .mcuType      = MCU_TYPE,

  .magic        = BR_MAGIC
};



// ***** EEPROM and Configuration *****


#define BR_INIT_ADR 7


struct SBrEeprom brEeprom EEMEM = {
  .id = "new",
  .cfg = {
      .adr              = BR_INIT_ADR,
      .magic            = BR_MAGIC,

      .oscCal           = 0xff,         // 0xff = factory default

      .hubMaxAdr        = 0,            // no child devices
      .hubSpeed         = 0,            // = maximum speed

      .shadesDelayUp    = { 0, 0 },           // Default: no delay ...
      .shadesDelayDown  = { 0, 0 },
      .shadesSpeedUp    = { 0xff, 0xff },     // Default: fastest possible speed (minimize engine times) ...
      .shadesSpeedDown  = { 0xff, 0xff },
    },
  .shadesPos = { 0xff, 0xff }
};

TBrConfigRecord _brConfigRecord;



// ***** Fuses Preset *****


#if IS_MAINTENANCE || BROWNIE_BASE == 0x0000    // Add fuse bits for maintenance and test systems only

FUSES = {
  .low      = LFUSE_DEFAULT,
  .high     = HFUSE_DEFAULT & FUSE_EESAVE,
  .extended = EFUSE_DEFAULT & FUSE_SELFPRGEN,
};

#endif



// ***** Registers *****


uint8_t _regFile[BR_REGISTERS];





// *************************** Change Reporting ********************************


#if !IS_MAINTENANCE


uint8_t chgShadow;      // shadow register copied and reset on register read


void ReportChangeAndNotify (uint8_t mask) {
  if (mask & ~chgShadow) {      // change is new? (we do not notify twice for a similar change)
    chgShadow |= mask;
    TwiSlNotify ();
  }
}


#endif





// *************************** Timer *******************************************


#if WITH_TIMER


static inline void TimerInit () {

  // Inititialize the 16-bit timer ...
#if MCU_TYPE == BR_MCU_ATTINY85 || MCU_TYPE == BR_MCU_ATTINY84

  //    On ATtiny85 and ATtiny84, the Timer/Counter1 is used.
  TIMSK1 = 0;   // Disable all interrupt sources
  TCCR1A = 0;   // Normal port operation (OC1A/OC1B disconnected); No waveform generation
  TCCR1B = 5;   // Clock selection: clk_io / 1024
  GTCCR = 1;    // Reset prescaler
  TCNT1 = 0;    // Reset timer register

#elif MCU_TYPE == BR_MCU_ATTINY861

  //    On ATtiny861, the Timer/Counter0 is the appropriate 16 bit counter.
  TIMSK = 0;                  // Disable all interrupt sources
  TCCR0A = (1 << TCW0);       // Normal, 16-bit mode
  TCCR0B = (1 << PSR0) | 5;   // Clock selection: clk_io / 1024; reset prescaler (PSR0)
  TCNT0H = 0;                 // Reset timer register ...
  TCNT0L = 0;                 // Reset timer register ...

#else
#error "Unsupported MCU."
#endif
}


uint16_t TimerNow () {
#if MCU_TYPE == BR_MCU_ATTINY85 || MCU_TYPE == BR_MCU_ATTINY84
  register uint16_t t = TCNT1;
#elif MCU_TYPE == BR_MCU_ATTINY861
  register uint16_t t = TCNT0L;
  t |= ((uint16_t) TCNT0H << 8);
#else
#error "Unsupported MCU."
#endif
  return t == BR_TICKS_NEVER ? (BR_TICKS_NEVER + 1) : t;
}


#endif // WITH_TIMER





// *************************** Main functions **********************************


void CoreInit () {

  // Copy EEPROM config to local config memory ...
  eeprom_read_block (&_brConfigRecord, &brEeprom.cfg, BR_EEPROM_CFG_SIZE);

  // Read out or write OSCCAL register ...
  if (_brConfigRecord.oscCal == 0xff)
    eeprom_write_byte (&brEeprom.cfg.oscCal, _brConfigRecord.oscCal = OSCCAL);
      // No OSCCAL value configured: read back factory default
  else
    OSCCAL = _brConfigRecord.oscCal;      // Acticate configured OSCCAL value

  // Init registers ...
  //   Note: RAM contents (.bss) are zero'ed by startup code
  _regFile[BR_REG_MAGIC] = BR_MAGIC;
  _regFile[BR_REG_FWBASE] = BROWNIE_BASE / BR_FLASH_PAGESIZE;

  // Init submodules ...
#if WITH_TIMER
  TimerInit ();
#endif
}


void CoreOnRegRead (uint8_t reg) {
  register uint16_t t;

  switch (reg) {
#if !IS_MAINTENANCE
    case BR_REG_CHANGED:
      _regFile[BR_REG_CHANGED] = chgShadow;
      chgShadow = 0;
      break;
#endif
    case BR_REG_TICKS_LO:
      t = TimerNow ();
      _regFile[BR_REG_TICKS_LO] = LO(t);
      _regFile[BR_REG_TICKS_HI] = HI(t);
      break;
  }
}


void CoreOnRegWrite (uint8_t reg, uint8_t val) {
  switch (reg) {
    case BR_REG_CHANGED:
      _regFile[BR_REG_CHANGED] = val;
      break;
  }
}
