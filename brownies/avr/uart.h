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


#ifndef _UART_
#define _UART_

#include "core.h"

#if !WITH_UART
EMPTY_MODULE(Uart)
#else  // WITH_UART


#include <avr/interrupt.h>



// **** Line helpers *****


#if UART_TX_INV
#define UART_TX_OUT_0() P_OUT_1(P_UART_TX)
#define UART_TX_OUT_1() P_OUT_0(P_UART_TX)
#else
#define UART_TX_OUT_0() P_OUT_0(P_UART_TX)
#define UART_TX_OUT_1() P_OUT_1(P_UART_TX)
#endif

#if UART_RX_INV
#define UART_RX_IS_1() (P_IN(P_UART_RX) == 0)
#else
#define UART_RX_IS_1() (P_IN(P_UART_RX) != 0)
#endif



// ***** Buffers *****


extern volatile uint8_t uartRxBuf[UART_RX_BUFSIZE];
extern volatile uint8_t uartRxBufIn, uartRxBufOut;
extern volatile bool uartFlagOverflow, uartFlagError;

extern volatile uint8_t uartTxBuf[UART_TX_BUFSIZE];
extern volatile uint8_t uartTxBufIn, uartTxBufOut;

#define UART_BUF_BYTES(BUF) ((uart##BUF##BufIn - uart##BUF##BufOut) & (sizeof (uart##BUF##Buf) - 1))
#define UART_BUF_BYTES_FREE(BUF) ((uart##BUF##BufOut - uart##BUF##BufIn - 1) & (sizeof (uart##BUF##Buf) - 1))

#define UART_BUF_IS_FULL(BUF) (UART_BUF_BYTES_FREE (BUF) == 0)
#define UART_BUF_IS_EMPTY(BUF) (UART_BUF_BYTES (BUF) == 0)

#define UART_BUF_PUT(BUF, VAL) do { uart##BUF##Buf[uart##BUF##BufIn] = (VAL); uart##BUF##BufIn = (uart##BUF##BufIn + 1) & (sizeof (uart##BUF##Buf) - 1); } while (0)
#define UART_BUF_GET(BUF, RET) do { RET = uart##BUF##Buf[uart##BUF##BufOut]; uart##BUF##BufOut = (uart##BUF##BufOut + 1) & (sizeof (uart##BUF##Buf) - 1); } while (0)


extern volatile uint8_t uartTLastRx;





// ***** Timing constants *****


#define UART_CLKS_PER_BIT (BR_CPU_FREQ / UART_BAUDRATE)
#define UART_CLKS_ISR_DELAY (90 * (int) (1000000 / BR_CPU_FREQ))
  // estimated number of clock cycles between falling start bit edge and first call of 'MinitimerReset()' in the ISR
  //
  // [2020-11-15] Testing with 9600 baud + logic analyzer
  //   - ISR_USI_OVERFLOW causes delays of typically 30-40us (without ISR entry/exit code)
  //   - Value of 60 would be almost perfekt without USI interrupts.
  //
  //   -> Selected a larger value to be more tolerant towards USI interrupts.


#if UART_CLKS_PER_BIT < 30      // Baud rates > 19200 probably do not work
#warning "UART baud rates >19200 probably do not work with 1 MHz CPU clock frequency."
#endif

#if UART_CLKS_PER_BIT < 60        // this applies to baud rates >= 9600
#define UART_CLOCK_SCALE MINI_CLOCK_SCALE_1
#define UART_MINITICKS_PER_BIT ((int) (UART_CLKS_PER_BIT + 0.5))
#define UART_MINITICKS_FIRST_BIT ((UART_MINITICKS_PER_BIT * 3 / 2) - UART_CLKS_ISR_DELAY)

#elif UART_CLKS_PER_BIT < 480     // this applies to baud rates >= 1200 and < 9600
#define UART_CLOCK_SCALE MINI_CLOCK_SCALE_8
#define UART_MINITICKS_PER_BIT ((int) ((UART_CLKS_PER_BIT / 8) + 0.5))
#define UART_MINITICKS_FIRST_BIT ((UART_MINITICKS_PER_BIT * 3 / 2) - (UART_CLKS_ISR_DELAY/8))

#elif UART_CLKS_PER_BIT < 3840    // this applies to baud rates >= 150 and < 1200
#define UART_CLOCK_SCALE MINI_CLOCK_SCALE_64
#define UART_MINITICKS_PER_BIT ((int) ((UART_CLKS_PER_BIT / 64) + 0.5))
#define UART_MINITICKS_FIRST_BIT ((UART_MINITICKS_PER_BIT * 3 / 2) - (UART_CLKS_ISR_DELAY/64))

#elif UART_CLKS_PER_BIT < 15360   // this applies to baud rates >= 50 and < 150
#define UART_CLOCK_SCALE MINI_CLOCK_SCALE_256
#define UART_MINITICKS_PER_BIT ((int) ((UART_CLKS_PER_BIT / 256) + 0.5))
#define UART_MINITICKS_FIRST_BIT ((UART_MINITICKS_PER_BIT * 3 / 2) - (UART_CLKS_ISR_DELAY/256))

#else
#error "UART baud rate to low!"
#endif



static inline void UartInit () {
  INIT (uartRxBufIn = uartRxBufOut, 0);
  INIT (uartTxBufIn = uartTxBufOut, 0);
#if UART_TX_LISTEN > 0
  INIT (uartTLastRx, BR_TICKS_NEVER);
#endif
  INIT (uartFlagOverflow, false);
  INIT (uartFlagError, false);

  MinitimerStart (UART_CLOCK_SCALE);
  MinitimerReset ();
}


static inline void UartIterate () {
  uint8_t tNext, data, i;

  // Disable interrupts ...
  cli ();

  // Check if we are within a listen period ...
#if UART_TX_LISTEN > 0
  if (uartTLastRx != BR_TICKS_NEVER) {
    if (TimerNow () - uartTLastRx < BR_TICKS_OF_MS (UART_TX_LISTEN)) goto done;   // cannot send yet
    else uartTLastRx = BR_TICKS_NEVER;
  }
#endif

  // Send a byte if there is one to transmit ...
  if (!UART_BUF_IS_EMPTY (Tx)) {
    UART_BUF_GET (Tx, data);
    ReportChange (BR_CHANGED_UART);

    //~ P_OUT_1 (P_A7);

    // Start minitimer and transmission ...
    MinitimerStart (UART_CLOCK_SCALE);
    MinitimerReset ();
    UART_TX_OUT_1();                // idle level
    P_OUT_1 (P_UART_DRIVE);
    tNext = UART_MINITICKS_PER_BIT;     // drive one period of idle level
    while (MinitimerNow () < tNext);

    // Start bit ...
    UART_TX_OUT_0();
    tNext += UART_MINITICKS_PER_BIT;
    while ((uint8_t) (tNext - MinitimerNow ()) < 192);

    //~ P_OUT_0 (P_A7);

    // Data bits ...
    for (i = 8; i; i--) {
      if (data & 1) UART_TX_OUT_1();
      else UART_TX_OUT_0();
      data >>= 1;
      tNext += UART_MINITICKS_PER_BIT;
      while ((uint8_t) (tNext - MinitimerNow ()) < 192);
    }

    // Send parity (optional) ...
#if UART_PARITY != 0
#error "UART parity is not implemented yet!"
#endif

    // Stop bits ...
    UART_TX_OUT_1();
    for (i = 0; i < UART_STOPBITS; i++) {
      tNext += UART_MINITICKS_PER_BIT;
      while ((uint8_t) (tNext - MinitimerNow ()) < 192);
    }
    P_OUT_0 (P_UART_DRIVE);   // switch off transmission
    UART_TX_OUT_0();          // switch TX line low to save power with MAX485 + opto coupler circuits,
                              // as in circuit "relais_rs485"

    // Stop minitimer ...
    MinitimerStop ();
  }

  // Enable interrupts and finish ...
done:
  sei ();
}


static inline void UartISR () {
  // This MUST be inlined, and it must be made sure that no real function calls
  // occur inside the ISR to avoid timing problems. If (non-inline) function calls
  // exist, AVR-GCC generates much more complex entry code (e.g. which saves all
  // processor registers) than without function calls.
  register uint8_t tNext, data, i;

  //~ P_OUT_1 (P_A7);    // (debug) for calibrating the sample timing (UART_CLKS_ISR_DELAY)

  // Abort on not a start bit ...
  if (UART_RX_IS_1()) {
    //~ P_OUT_0 (P_A7);    // (debug) for calibrating the sample timing (UART_CLKS_ISR_DELAY)
    return;  // not a start bit
  }

  // Start minitimer ...
  MinitimerStart (UART_CLOCK_SCALE);
  MinitimerReset ();
  tNext = (UART_MINITICKS_FIRST_BIT < 0) ? 0 : UART_MINITICKS_FIRST_BIT;

  // Multi-byte main loop (to be quit with an explicit "break") ...
  while (1) {

    // Sample data ...
    data = 0;
    for (i = 8; i; i--) {
      //~ P_OUT_0 (P_A7);    // (debug) for calibrating the sample timing (UART_CLKS_ISR_DELAY)
      while ((uint8_t) (tNext - MinitimerNow ()) < 192);
        // UART_MINITICKS_PER_BIT is always < 128, UART_MINITICKS_FIRST_BIT no more than half of this.
        // Hence, if the time of the next sampling (tNext (mod 256)) has not yet been reached,
        // the difference 'tNext - MinitimerNow ()' must always be <192. In other words,
        // we interpret all numbers <192 as positive, and all >=192 as negative two's complement
        // numbers (-64 .. -1).
      //~ P_OUT_1 (P_A7);    // (debug) for calibrating the sample timing (UART_CLKS_ISR_DELAY)
      data >>= 1;
      if (UART_RX_IS_1()) data |= 0x80;
      tNext += UART_MINITICKS_PER_BIT;
    }

    // Sample and check parity (optional) ...
#if UART_PARITY != 0
#error "UART parity is not implemented yet!"
#endif

    // Enqueue  ...
    if (!UART_BUF_IS_FULL (Rx)) {
      UART_BUF_PUT (Rx, data);
      ReportChange (BR_CHANGED_UART);
    }
    else {   // buffer overflow ...
      if (!uartFlagOverflow) {
        uartFlagOverflow = true;
        ReportChange (BR_CHANGED_UART);
      }
    }

    // Wait for stop bit ...
    //   We are now potentially still in the period of the last data (or parity) bit.
    //   If we leave the ISR too early, the current bit may be interpreted as a start
    //   bit and the ISR be invoked again too early.
    while ( (!UART_RX_IS_1()) && ((uint8_t) (tNext - MinitimerNow ()) < 192) );

    // Exit the multi-byte loop or prepare next iteration ...
    if (!UART_MULTI_BYTE_ISR || UART_BUF_IS_FULL (Rx)) break;  // stop if buffer is full
    else {
      // Check if there is a start bit within the next 3 bit cycles (2 stop bits + extra time) ...
      tNext = MinitimerNow () + 3 * UART_MINITICKS_PER_BIT;
      while (UART_RX_IS_1())
        if ((uint8_t) (tNext - MinitimerNow ()) >= 192) goto break_byte_loop;  // time-out => done
      // Have a new byte: set sample time first bit ...
      tNext = MinitimerNow () + 3 * UART_MINITICKS_PER_BIT / 2;
    }

  } // multi-byte main loop
  break_byte_loop:

  // Done ...
  MinitimerStop ();
#if UART_TX_LISTEN > 0
  uartTLastRx = TimerNow ();
#endif
  //~ P_OUT_0 (P_A7);    // (debug) for calibrating the sample timing (UART_CLKS_ISR_DELAY)
}


static inline void UartOnRegRead (uint8_t reg) {
  uint8_t val, n;

  switch (reg) {

    case BR_REG_DEBUG_0:
      RegSet (reg, UART_CLOCK_SCALE);
      break;
    case BR_REG_DEBUG_1:
      RegSet (reg, MinitimerNow ());
      break;
    case BR_REG_DEBUG_2:
      RegSet (reg, UART_MINITICKS_PER_BIT);
      break;

    case BR_REG_UART_STATUS:

      // Get number of bytes received (with interrupts disabled) ...
      cli ();
      n = UART_BUF_BYTES (Rx);
      sei ();
      if (n > 7) n = 7;
      val = (n << BR_UART_STATUS_RX_SHIFT);

      // Get number of bytes that can be transmitted ...
      n = UART_BUF_BYTES_FREE (Tx);
      if (n > 7) n = 7;
      val |= (n << BR_UART_STATUS_TX_SHIFT);

      // Get flags ...
      if (uartFlagOverflow) val |= BR_UART_STATUS_OVERFLOW;
      if (uartFlagError) val |= BR_UART_STATUS_ERROR;

      // Write result ...
      RegSet (BR_REG_UART_STATUS, val);
      break;

    case BR_REG_UART_RX:
      sei ();
      if (!UART_BUF_IS_EMPTY (Rx)) {
        UART_BUF_GET (Rx, val);
        ReportChange (BR_CHANGED_UART);
        RegSet (BR_REG_UART_RX, val);
      }
      cli ();
      break;

    default: break;
  }
}


static inline void UartOnRegWrite (uint8_t reg, uint8_t val) {
  switch (reg) {

    case BR_REG_UART_CTRL:
      RegSet (BR_REG_UART_CTRL, val);
      if (val & BR_UART_CTRL_RESET_RX) {
        cli ();
        uartRxBufIn = uartRxBufOut = 0;
        sei ();
      }
      if (val & BR_UART_CTRL_RESET_TX)
        uartTxBufIn = uartTxBufOut = 0;
      if (val & BR_UART_CTRL_RESET_FLAGS)
        uartFlagOverflow = uartFlagError = false;
      break;

    case BR_REG_UART_TX:
      RegSet (BR_REG_UART_TX, val);
      if (!UART_BUF_IS_FULL (Tx)) {
        UART_BUF_PUT (Tx, val);
        ReportChange (BR_CHANGED_UART);
      }
      break;

    default: break;
  }
}


#endif


#endif // _UART_
