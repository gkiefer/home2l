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
#include "twi.h"

#include "gpio.h"
#include "matrix.h"
#include "adc.h"
#include "uart.h"
#include "temperature.h"
#include "shades.h"

#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/boot.h>
#include <util/delay.h>


#if BROWNIE_BASE != 0x0000 \
    && (BROWNIE_BASE != BR_FLASH_BASE_MAINTENANCE || !IS_MAINTENANCE) \
    && (BROWNIE_BASE != BR_FLASH_BASE_OPERATIONAL || IS_MAINTENANCE)
#warning "BROWNIE_BASE is neither 0x0000 nor matching the IS_MAINTENANCE/BR_FLASH_BASE_* settings"
#endif





// **** Scratch area *****


#define FOR_EACH_MODULE(PRE, POST) do {   \
  PRE Core##POST;                         \
  PRE TwiHub##POST;                       \
  PRE Gpio##POST;                         \
  PRE Matrix##POST;                       \
  PRE Adc##POST;                          \
  PRE Uart##POST;                         \
  PRE Temperature##POST;                  \
  PRE Shades##POST;                       \
} while(false)





// *************************** Interrupts **************************************


/* This section defines common interrupt handlers for all pin change interrupts (PCIs).
 * Modules using PCIs must provide two macros here:
 *   PCI_<module>_PIN - the pin mask naming one or multiple pins to be observed.
 *   PCU_<module>_ISR - the ISR called if the PCI is asserted.
 *
 * Note, that spurious interrupts may occur, since it is not possible to identify
 * a pin that causes an interrupt.
 *
 * Note: PCINT* interrupts are always cleared after the global ISR.
 *       This is essential for the UART module and good for the temperature/ZACwire module,
 *       but may cause other pin change interrupts to get lost.
 */





// ***** Pin change interrupt (PCI) declarations for all modules *****

/* Note: ATtiny84 and ATtiny have ISRs for 8-bit groups (PCINT0, PCINT1),
 *       ATtiny861 has a single vector for all pin change interrupts (PCINT).
 */


#if defined(ISR_PCINT0) && (LO(PCINT_ALL_PINS))
ISR(ISR_PCINT0) {
  //~ P_OUT_1 (P_A7);
  PCINT_CALL_SUBISRS (PCINT_ALL_PINS & 0x00ff)
  if (LO(PCINT_ALL_PINS)) GIFR |= (1 << PCIF0);    // Clear interrupt flag
  //~ P_OUT_0 (P_A7);
}
#endif


#if defined(ISR_PCINT1) && (HI(PCINT_ALL_PINS))
ISR(ISR_PCINT1) {
  //~ P_OUT_1 (P_A7);
  PCINT_CALL_SUBISRS (PCINT_ALL_PINS & 0xff00)
  if (HI(PCINT_ALL_PINS)) GIFR |= (1 << PCIF1);    // Clear interrupt flag
  //~ P_OUT_0 (P_A7);
}
#endif


#if defined(ISR_PCINT) && (PCINT_ALL_PINS)
ISR(ISR_PCINT) {
  //~ P_OUT_1 (P_A7);
  PCINT_CALL_SUBISRS (PCINT_ALL_PINS)
  if (PCINT_ALL_PINS) GIFR |= (1 << PCIF);    // Clear interrupt flag
  //~ P_OUT_0 (P_A7);
}
#endif


static inline void InitInterrupts () {
#if MCU_TYPE == BR_MCU_ATTINY85
  PCMSK = PCINT_ALL_PINS;
  GIMSK = PCINT_ALL_PINS ? (1 << PCIE) : 0;
#elif (MCU_TYPE == BR_MCU_ATTINY84)
  PCMSK0 = LO(PCINT_ALL_PINS);
  PCMSK1 = HI(PCINT_ALL_PINS);
  GIMSK = (LO(PCINT_ALL_PINS) ? (1 << PCIE0) : 0) | (HI(PCINT_ALL_PINS) ? (1 << PCIE1) : 0);
#elif (MCU_TYPE == BR_MCU_ATTINY861)
  PCMSK0 = LO(PCINT_ALL_PINS);
  PCMSK1 = HI(PCINT_ALL_PINS);
  GIMSK = ((PCINT_ALL_PINS & 0x0f00) ? (1 << PCIE0) : 0) | ((PCINT_ALL_PINS & 0xf0ff) ? (1 << PCIE1) : 0);
#else
#error "Unsupported MCU type"
#endif
}





// *************************** Bootloader **************************************


static inline void Reboot () {
  TwiSlDone ();   // avoid a blocked TWI bus in case of a crash after reboot
  cli ();
  ((void (*)()) 0x0000) ();
}


static inline void RebootInto (uint8_t fwStartPage) {
  const __flash uint16_t *table = (uint16_t *) (BR_FLASH_PAGESIZE * ((uint16_t) fwStartPage));
  uint16_t delta = ((uint16_t) fwStartPage) * (BR_FLASH_PAGESIZE >> 1);
  uint8_t n;

  // Disable interrupts...
  TwiSlDone ();   // avoid a blocked TWI bus in case of a crash after reboot
  cli ();
  eeprom_busy_wait();

  // Write new reset/interrupt vector table...
  for (n = 0; n < SPM_PAGESIZE; n += 2) {
    boot_spm_busy_wait();
    boot_page_fill (n, table[n >> 1] + delta);
  }
  boot_spm_busy_wait();
  boot_page_erase (0);
  boot_spm_busy_wait();
  boot_page_write (0);
  boot_spm_busy_wait ();    // wait for completion

  // Reset...
  ((void (*)()) 0x0000) ();
}





// *************************** Register read/write *****************************


static inline void HandleRegRead () {
  uint8_t reg = twiSlRequest.op & 0x3f;

  // Call module hooks ...
  FOR_EACH_MODULE(, OnRegRead (reg));

  // Assemble reply...
  twiSlReply.status = brOk;
  twiSlReply.regRead.val = RegGet (reg);
}


static inline void HandleRegWrite () {
  uint8_t reg = twiSlRequest.op & 0x3f;
  uint8_t val = twiSlRequest.regWrite.val;

  switch (reg) {
    case BR_REG_CTRL:      // System control register
      switch (val) {
        case BR_CTRL_REBOOT:
        case BR_CTRL_REBOOT_NEWFW:

          // Reply now, since we will reboot before returning...
          twiSlReply.status = brOk;
          TwiSlReplyCommit (1);
          TwiSlReplyFlush ();

          // Eventually reprogram reset & interrupt table and reboot...
          if (val == BR_CTRL_REBOOT_NEWFW) RebootInto (RegGet (BR_REG_FWBASE));
          else Reboot ();

        default:

          // No special value: Write register...
          RegSet (reg, val);
          break;
      }
      break;

    case BR_REG_FWBASE:
      RegSet (reg, val);
      break;

    default:

      // Call module hooks ...
      FOR_EACH_MODULE(, OnRegWrite (reg, val));
  }
}





// *************************** Memory read/write *******************************


static inline void HandleMemRead () {
  uint8_t space = twiSlRequest.op & 0x0f;
  uint16_t adr = ((uint16_t) twiSlRequest.memRead.adr) << BR_MEM_BLOCKSIZE_SHIFT;
  void *dst = (void *) twiSlReply.memRead.data;

  if (space == BR_MEM_PAGE_SRAM) {         // Read SRAM ...
    memcpy (dst, (uint8_t *) adr, BR_MEM_BLOCKSIZE);
  }
  else if (space == BR_MEM_PAGE_EEPROM) {  // Read EEPROM ...
    eeprom_read_block (dst, (uint8_t *) adr, BR_MEM_BLOCKSIZE);
  }
  else if (space == BR_MEM_PAGE_VROM) {    // Read VROM ...
    memcpy_P (dst, ((const __flash uint8_t *) &brFeatureRecord) + adr, BR_MEM_BLOCKSIZE);
  }
  else if (space >= BR_MEM_PAGE_FLASH) {   // Read PGMEM ...
    adr |= (((uint16_t) (space - BR_MEM_PAGE_FLASH)) << (BR_MEM_BLOCKSIZE_SHIFT + 8));
    memcpy_P (dst, (const __flash uint8_t *) adr, BR_MEM_BLOCKSIZE);
  }
  else memset (dst, 0xff, BR_MEM_BLOCKSIZE);   // Invalid page: Return all-1s
}


#define MSG_SPM_MASK ((SPM_PAGESIZE - 1) ^ (BR_MEM_BLOCKSIZE - 1))
  // mask with 1s for address bits pointing to the same SPM page, but different MEM block


static inline void HandleMemWrite () {
  uint16_t adr, ofs;
  uint8_t *src = twiSlRequest.memWrite.data;
  uint8_t n;

  // Analyse address ...
  adr = ((((uint16_t) (twiSlRequest.op & 0x0f)) << 8) | twiSlRequest.memRead.adr) << BR_MEM_BLOCKSIZE_SHIFT;
  ofs = BR_MEM_OFS (adr);

  // Check permissions ...
  if ( !(RegGet (BR_REG_CTRL) & (BR_MEM_ADR_IS_EEPROM(adr) ? BR_CTRL_UNLOCK_EEPROM : BR_CTRL_UNLOCK_FLASH)) ) {
    twiSlReply.status = brForbidden;
    return;
  }

  // Write SRAM ...
  if (BR_MEM_ADR_IS_SRAM (adr)) {
    memcpy ((uint8_t *) ofs, src, BR_MEM_BLOCKSIZE);
  }

  // Write EEPROM ...
  else if (BR_MEM_ADR_IS_EEPROM (adr)) {
    eeprom_write_block (src, (uint8_t *) ofs, BR_MEM_BLOCKSIZE);
  }

  // Write PGMEM (flash) ...
  else if (BR_MEM_ADR_IS_FLASH (adr)) {

    // Make extra check that the code does not overwrite itself or the reset/interrupt vectors...
    if (ofs < BR_FLASH_BASE_MAINTENANCE
        || (IS_MAINTENANCE && ofs < BR_FLASH_BASE_OPERATIONAL) || (!IS_MAINTENANCE && ofs >= BR_FLASH_BASE_OPERATIONAL)) {
      twiSlReply.status = brForbidden;
      return;
    }

    // Do the flash programming...
    cli ();
    eeprom_busy_wait();
    for (n = 0; n < BR_MEM_BLOCKSIZE; n += 2) {
      boot_spm_busy_wait();
      boot_page_fill (ofs + n, * (uint16_t *) (src + n));
    }
    if ((ofs & MSG_SPM_MASK) == MSG_SPM_MASK) {
      // Last block of page: Write the whole page...
      boot_spm_busy_wait();
      boot_page_erase (ofs);
      boot_spm_busy_wait();
      boot_page_write (ofs);
    }
    boot_spm_busy_wait ();    // wait for completion
    sei ();
  }
}





// *************************** Main ********************************************


int /* __attribute__((OS_main)) */ main (void) {
  register uint8_t op;

  //~ P_DDR_OUT(P_B1);  // Debugging ...
  //~ P_OUT_1(P_B1);
  //~ while (1) {}

  // Configure I/O pins ...
  INIT_PINS ();

  // Resurrection check ...
  //   If SCL and SDA are both low for at least 250ms, we boot into the maintenance
  //   system. This check happens as early as possible and before enabling interrupts
  //   to maximize the chance that we get here, even if the present firmware is
  //   messed up.
#if !IS_MAINTENANCE && BROWNIE_BASE == BR_FLASH_BASE_OPERATIONAL
  int i;
  for (i = 250; (P_IN(TWI_SL_SCL) | P_IN(TWI_SL_SDA)) == 0; i--) {
    if (!i) RebootInto (BR_FLASH_BASE_MAINTENANCE / SPM_PAGESIZE);
    _delay_ms (1);
  }
#endif

  // Init ...
  TwiSlInit ();
  FOR_EACH_MODULE (, Init ());
  InitInterrupts ();

  // Enable interrupts ...
  sei ();

  // Main loop...
  while (1) {

    // Iterate TWI slave and handle a request received ...
    if (TwiSlIterate () == brOk) {
      twiSlReply.status = (uint8_t) brOk;    // preset reply status

      // Execute the operation ...
      op = twiSlRequest.op;
      if      (BR_OP_IS_REG_READ (op))   HandleRegRead ();
      else if (BR_OP_IS_REG_WRITE (op))  HandleRegWrite ();
      else if (BR_OP_IS_MEM_READ (op))   HandleMemRead ();
      else if (BR_OP_IS_MEM_WRITE (op))  HandleMemWrite ();
      else {      // undefined operation...
        twiSlReply.status = (uint8_t) brIllegalOperation;
      }

      // Commit reply...
      TwiSlReplyCommit (twiSlReply.status == (uint8_t) brOk ?
                        BrReplySize (twiSlRequest.op) : 1);

      // Received messages have priority over other modules:
      // Skip iterating application modules...
      continue;
    }

    // Iterate application modules...
    FOR_EACH_MODULE (, Iterate ());
  }
}
