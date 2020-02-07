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


#include "twi.h"

#include "core.h"

#include <util/delay.h>

#include <avr/io.h>
#include <avr/interrupt.h>





// *************************** Configuration ***********************************


#define TWI_SL_SEND_ACTIVE 0    ///< @internal
  // Send actively, without using the USI (NOT IMPLEMENTED, may never be implemented).
  //
  // It appears [2019-03-31, ATtiny84] that a transmitting
  // USI may pull the SDA line up to 1 for a short time when sending a '1' via the USIDR instead of just
  // keeping SDA in a high-impedance state as expected according to the i2c standard. This appears to
  // cause problems with long cables and line amplifiers such as the P82B715.
  // If this TWI_SL_SEND_ACTIVE == 1, data (not ACK/NACK) is sent actively in the foreground without
  // using the USI to avoid this problem.
  //
  // With active sending, TwiSlReplyCommit() returns after sending completed,
  // otherwise it returns immediately.
  // With active sending, interrupts may be disabled for the duration of a byte
  // (TBD: true? - allowing interrupts requires to stretch the clock).


#define P_TWI_MA_ON_SCL_STRETCH // TwiSlIterate ()
  // Code to be executed by master while waiting if SCL is kept down by the slave.
  // * The fastest solution is to keep it empty. However, the main loop may hang
  //   for an indefinite time.
  // * 'TwiIterate()' is required if this same device is connected to the same bus
  //   as both a master and slave (e.g. for loopback tests).
  // * The most proper (but perhaps not performant) solution is to have the main
  //   pogramm provide an "IterateAll()" function, which is then called here.

#define P_TWI_MA_TIME_BASE 2.0
  // Ideal transmission time of one bit in mircoseconds. This number is multiplied by
  // the selected "hub_speed" factor.
  // Note: The loop overhead adds ~4 instructions (>= 4µs @ 1MHz) per iteration,
  //       This refers to an implicit increment of 8.0 for the time base in most
  //       delay loops ('WAIT_HALF()').




// *****************************************************************************
// *                                                                           *
// *                          TWI Slave                                        *
// *                                                                           *
// *****************************************************************************



// ***** Request and Reply Buffers *****


/*  Request and reply buffers & Life Cycle
 *  ======================================
 *
 *  For handling slave communication, two actors (logical threads/processes) have to be distinguished:
 *
 *  a) ISR - The slave ISR in interaction with 'TwiSlIterate()', the  functionality implemented in this module.
 *  b) APP - The (application) routines that generate replies on requests. These may be the
 *           application, the hub (below) or the error handler in 'TwiSlIterate()'.
 *
 *
 *  Request & Reply Buffer and their Synchronization
 *  ------------------------------------------------
 *
 *  a) Request Buffer
 *      (state represented by: slReqBuf, slReqBytes, slReqStatus (complete == (reqStatus != brIncomplete), slReqAdr)
 *      - is filled by ISR
 *      - is marked complete by ISR
 *      - is cleared by ISR together with the commited reply (APP has arbitrary time to read it by not comitting fully)
 *      - may be read by APP before completeness (i.e. by hub)
 *
 *    To fill means:  Add bytes to buffer, increase 'slReqBytes',
 *                    turn 'slReqStatus' from 'brIncomplete' (= not complete) to any other state (= complete),
 *                    turn 'slReqAdr' from 0xff (= unknown) to another value (= known)
 *    To clear means: slReqBytes = 0, slReqStatus = brIncomplete, skReqAdr = 0xff
 *
 *  b) Reply Buffer
 *      (state represented by: slRplBuf, slRplBytes, slRplComplete (= commited) )
 *      - is filled by APP
 *      - marked complete (committed) by APP
 *      - may be read by ISR in background (slRplPtr; transfer to master)
 *      - is finally cleared by ISR
 *      - is guaranteed to be cleared by the ISR together with the request buffer
 *
 *    To fill means:  Add bytes to buffer, increase 'slRplBytes',
 *                    turn 'slRplComplete' from 'false' to 'true'
 *    To clear means: slRplBytes = 0, slReqComplete = false
 *
 *
 *  Communication Phases
 *  --------------------
 *
 *  Phase 1: ISR fills request; End indication: request marked complete
 *            - Prefetching by APP is allowed
 *            - Filling reply is not allowed
 *            - This phase is shortcut if a reply fetch without request is received by the TWI interface
 *              (completed with length 0; slReqAdr set to reply address)
 *
 *  Phase 2: APP fills reply; End indication: Commit / reply marked complete
 *            - Prefetching by ISR is allowed (partial commit)
 *            - Request is frozen in "complete" state
 *
 *  Phase 3: ISR transmits reply; End indication: Request *and* reply cleared
 *            - Request is frozen in "complete" state (before completion of this phsse)
 *              => If the APP sees "complete" requests in two subsequent polls, it can
 *                determine whether the second is a new one by checking the "complete"
 *                state of the reply.
 *
 *
 *  TWI Slave Operation
 *  -------------------
 *
 *  Init: Both buffers (request + reply) cleared.
 *
 *  1. On incoming request(s): Append to request buffer
 *     - keep address of the first message (if 'reqBufBytes == 0'); never change it afterwards
 *       (NACK all addressing to other addresses until the request buffer is cleared again)
 *     - effectively ignore second & more requests afterwards (master must know that:
 *       replies must always be fetched before sending a new request, else the reply may
 *       not match the last request)
 *     - track request status in ISR until != brIncomplete, then mark request as complete
 *
 *  2. On incoming reply addressing to any served address:
 *     - mark request as complete (for hub, to start reply fetching)
 *     - if 'slReqAdr' is unset, set it
 *
 *  1./2. Once the address is stored in 'slReqAdr', ignore (NACK) all communication to other addresses.
 *
 *  3. On request complete:
 *     - make sure a reply is generated and will be committed
 *       - own address and valid request => main program, indicated by return value of TwiSlIterate()
 *       - own address, request invalid => commit error reply ( TwiSlIterate() )
 *       - child address => fetch and forward expected number of bytes from child ( TwiHubIterate() ), commit afterwards
 *     - deliver reply buffer as requested
 *     - at buffer end: stall if uncommitted, else send dummy bytes
 *
 *  4. With commit: clear request buffer immediately (important for hub and to avoid double commits by APP)
 *
 *  5. After reply fetched completely (NACK from master): Clear reply buffer, goto 1.
 *
 *  This implies and preserves the monotony properties of the two buffers:
 *  - buffers only fill before commit (by APP)
 *  - "complete" flags  only transition from 0->1 before/with commit
 *  - 'slReqAdr' never changes after the first addressing
 *  - 'slReqAdr' is set before marking request complete
 *  - request buffer is never reset before reply; both buffers are reset together
 *
 *
 *  Hub Operation ( TwiHubIterate() )
 *  ---------------------------------
 *
 *  1. (hsIdle) If a) there are request bytes not forwarded yet (hubReqPtr < reqBytes), b) reply is empty,
 *        and c) slReqAdr is a child address: Start forwarding request to child (-> hsRequestForwarding)
 *
 *        Note: Condition b) is to check if we already forwarded a request in this cycle.
 *
 *  2. (hsRequestForwarding) Forward bytes to child as possible.
 *        If request complete and forwarded completely, stop forwarding and switch to hsIdle
 *
 *  3. (hsIdle) If a) request complete, b) request forwarded completely, c) slReqAdr is known and
 *        a child address (the request may be missing, in this case the conditions are met after
 *        reply addressing):
 *        - Estimate reply length
 *        - Start forwarding reply from child (-> hsForwardingReply)
 *
 *  4. (hsReplyForwarding) Fetch and commit all bytes from child individually. Then goto 1.
 *
 */


// Request buffer ...
/* const */ TBrRequest twiSlRequest;            // User view
#define slReqBuf ((uint8_t *) &twiSlRequest)    // Internal view (byte array)
#define slReqBufSize (sizeof (twiSlRequest))    // Max. capacity
static volatile uint8_t slReqBytes;             // Number of valid bytes (monotonic: 0 < ... < BR_REQUEST_SIZE_MAX)
static volatile EBrStatus slReqStatus;          // Request status (monotonic: 'brIncomplete' < 'brUnchecked' < any other; 'slReqBytes' only changes if 'brIncomplete')
#if WITH_TWIHUB
static volatile uint8_t slReqAdr;               // Adress associated with current transaction
#endif


static inline void SlReqClear () {
  slReqBytes = 0;
  slReqStatus = brIncomplete;
#if WITH_TWIHUB
  slReqAdr = 0xff;
#endif
}


// Reply buffer ...
TBrReply twiSlReply;                            // User view
#define slRplBuf ((uint8_t *) &twiSlReply)      // Internal view (byte array)
#define slRplBufSize (sizeof (twiSlReply))      // Max. capacity
static volatile uint8_t slRplBytes;             // Number of valid bytes
static volatile bool slRplComplete;             // "Complete" flag


static inline void SlRplClear () {
  slRplBytes = 0;
  slRplComplete = false;
}




// ***** TWI Slave State & General Helpers *****


/* Notes on the USI Slave Implementation
 * -------------------------------------
 *
 * This code in this section is based on:
 *
 * a) Microchip (Atmel) Application Note AVR312
 *
 * b) Name    : USI TWI Slave driver - I2C/TWI-EEPROM
 *    FeatureRecord : 1.3  - Stable
 *    autor   : Martin Junghans jtronics@gmx.de
 *    page    : www.jtronics.de,
 *              http://www.jtronics.de/avr-tutorial/avr-library-i2c-twi-slave-usi/
 *    License : GNU General Public License
 *
 *    Created from Atmel source files for Application Note AVR312:
 *    Using the USI Module as an I2C slave like an I2C-EEPROM.
 *
 * c) Additional correction from https://www.mikrocontroller.net/topic/38917 :
 *
 *    --- usiTwiSlave_old.c  2011-10-21 12:17:44.000000000 +0200
 *    +++ usiTwiSlave.c  2014-10-25 17:00:17.794250296 +0200
 *    @@ -255,7 +255,7 @@
 *
 *       while (  ( USI_PIN & ( 1 << USI_SCL_BIT ) ) &&  !( ( USI_PIN & ( 1 << USI_SDA_BIT ) ) ));// SCL his high and SDA is low
 *
 *    -  if ( !( USI_PIN & ( 1 << USI_SDA_BIT ) ) )
 *    +  if ( !( USI_PIN & ( 1 << USI_SCL_BIT ) ) )
 *         {  // A Stop Condition did not occur
 *         USICR =
 *         ( 1 << USISIE ) |                // Keep Start Condition Interrupt enabled to detect RESTART
 *
 *    Without this patch, users noticed problems with addresses >= 64 due
 *    to a race condition, since SDA may have gone up between the "while" loop
 *    and the "if" condition.
 *
 * d) Own development to improve robustness and allow extensions. For example,
 *    a "while" loop inside an ISR with an unbound waiting time is a bad practice
 *    and may cause the system to lock up ...
 */


typedef enum {

  // General...
  slsBusy = 0,          // (Init) Bus is busy, i.e. some other transaction may be happening
  slsIdle,              // Bus is (known to be) idle, i.e. a stop condition has been received, but no start condition
  slsNotifying,         // We are notifying the master (any interrupts should be off now)

  // Addressing ...
  slsStartCond,         // Start condition just received => Wait for SCL=0, then setup overflow interrupt and go to ADRESSING
  slsAddressing,        // An address (own or other) was received - further action required

  // Master write mode (-> request) ...
  slsReceiving,         // Receiving data (a request) from master
  slsReceivingAck,      // Sending [NO]ACK to master

  // Master read mode (-> reply) ...
  slsSending,           // Sending data (a reply) to master
  slsSendingStall,      // We should send, but do not have the reply yet
  slsSendingWaitAck,    // Awaiting [NO]ACK from sender
  slsSendingCheckAck    // Reacting on ACK/NOACK

} ETwiSlState;


// Slave State ...
static volatile ETwiSlState slState;
static volatile uint8_t slRplPtr;               // Next byte to be transferred to master
#if TWI_SL_NOTIFY
bool twiSlNotifyPending;
#endif


static void SlResetCommunication () {
  SlReqClear ();
  SlRplClear ();
  slRplPtr = 0;        // reset pointer in reply buffer
}





// ***** USI helpers *****


#define USICR_DEFAULTS(OVF_HOLD) (                                            \
    ( 1 << USIWM1 ) | ( OVF_HOLD << USIWM0 ) |                                \
        /* keep USI in two-wire mode, no/with USI counter overflow hold */    \
    ( 1 << USICS1 ) | ( 0 << USICS0 ) | ( 0 << USICLK ) |                     \
        /* keep clock source (external + positive edge for USIDR / both edges for counter) */ \
    ( 0 << USITC ) )                                                          \
        /* keep no toggle clock port */


static inline void UsiInit () {
  // In Two Wire mode (USIWM1, USIWM0 = 1X), the slave USI will pull SCL
  // low when a start condition is detected or a counter overflow (only
  // for USIWM1, USIWM0 = 11).  This inserts a wait state. SCL is released
  // by the ISRs (USI_START_vect and USI_OVERFLOW_vect).

#if MCU_TYPE == MCU_TYPE_ATTINY861
  USIPP &= ~(1 << USIPOS);      // Set USIPOS == 0, USI signals to port B
#endif
  USICR = USICR_DEFAULTS(0);    // Activate USI 2-wire mode
  USIDR = 0;                    // Init DR (otherwise, the first addressing fails, 20190616)

  P_DDR_OUT (P_USI_SCL);        // Set SCL as output
  P_OUT_1 (P_USI_SCL);          // Set SCL high

  P_DDR_IN (P_USI_SDA);         // Set SDA as input
  P_OUT_1 (P_USI_SDA);          // Set SDA high
}


static inline void UsiDone () {
  P_DDR_IN(P_USI_SCL | P_USI_SDA);  // Set SCL and SDA as inputs (high impedance)
  P_OUT_0(P_USI_SCL | P_USI_SDA);   // Set SCL and SDA low (to never drive a 1)
  USICR = 0;                    // Clear all control register (ports become normal outputs)
}


static inline void UsiResetToWaitForStartCondition () {
  P_DDR_IN (P_USI_SDA);         // set SDA as input

  USICR = ( 1 << USISIE ) |   // enable start condition interrupt
          ( 0 << USIOIE ) |   // disable overflow interrupt
          USICR_DEFAULTS(0);  // no USI counter overflow hold

  USISR = ( 1 << USISIF ) |   // clear start condition flag
          ( 1 << USIOIF ) |   // clear counter overflow flag
          ( 1 << USIPF ) |    // clear stop detector
          ( 1 << USIDC ) |    // clear data output collision
          ( 0x0 << USICNT0 ); // clear (do not set) counter
}


static inline void UsiSetOnStartCondInterrupt () {
  P_DDR_IN (P_USI_SDA);         // set SDA as input
  USICR = ( 0 << USISIE ) |   // disable start condition interrupt; do NOT clear USISIF to not release SCL before addressing is started!
          ( 0 << USIOIE ) |   // disable overflow interrupt
          USICR_DEFAULTS(0);  // no USI counter overflow hold
  USISR = ( 1 << USIOIF ) |   // clear overflow interrupt flag (just in case)
          ( 1 << USIPF ) |    // clear stop detector
          ( 1 << USIDC ) |    // clear data output collision
          ( 0x0 << USICNT0);  // set USI to sample 8 bits (count 16 external SCL pin toggles)
}


static inline void UsiSetToAdressing () {
  USICR = ( 1 << USISIE ) |   // keep start condition interrupt enabled to re-synchronize on error
          ( 1 << USIOIE ) |   // enable overflow interrupt
          USICR_DEFAULTS(1);  // hold SCL low on USI counter overflow
  USISR = ( 1 << USISIF ) | ( 1 << USIOIF ) |
                              // clear interrupt flags - resetting the Start Condition Flag will release SCL
          ( 0x0 << USICNT0);  // set (keep) USI to sample 8 bits (count 16 external SCL pin toggles)
}


static inline void UsiSetToSendAck () {
  USIDR = 0;                          // prepare ACK (SDA = 0)
  P_DDR_OUT (P_USI_SDA);              // set SDA as output
  USISR = ( 1 << USIOIF ) |           // clear overflow interrupt flag
          ( 0x0E << USICNT0 );        // set USI counter to shift 1 bit
}


static inline void UsiSetToReadAck () {
  USIDR = 0x80;                       // clear USIDR, but set bit 7 to release SDA (important to avoid SDA spikes!)
  P_DDR_IN (P_USI_SDA);               // set SDA as input
  USISR = ( 1 << USIOIF ) |           // clear overflow interrupt flag
          ( 0x0E << USICNT0 );        // set USI counter to shift 1 bit
}


static inline void UsiSetToSendData () {
  P_DDR_OUT (P_USI_SDA);                // set SDA as output

  // Set USICR to exit stall mode...
  USICR = ( 1 << USISIE ) |   // keep start condition interrupt enabled to re-synchronize on error
          ( 1 << USIOIE ) |   // enable overflow interrupt
          USICR_DEFAULTS(1);  // hold SCL low on USI counter overflow

  USISR = ( 1 << USIOIF ) |           // clear overflow interrupt flag
          ( 0x0 << USICNT0 );         // set USI counter to shift 8 bits
}


static inline void UsiSetToSendingStall () {
  USICR = ( 0 << USISIE ) |   // disable start condition interrupt; do NOT clear USISIF to not release SCL before addressing is started!
          ( 0 << USIOIE ) |   // disable overflow interrupt
          USICR_DEFAULTS(1);  // hold SCL low on USI counter overflow
}


static inline void UsiSetToReadData () {
  P_DDR_IN (P_USI_SDA);                 // set SDA as input
  USISR = ( 1 << USIOIF ) |           // clear overflow interrupt flag
          ( 0x0 << USICNT0 );         // set USI to shift in 8 bits
}


static inline void UsiSetToNotify () {
  USICR = ( 0 << USISIE ) |   // disable all interrupts
          ( 0 << USIOIE ) |
          USICR_DEFAULTS(0);  // no USI counter overflow hold
  USISR = ( 1 << USISIF ) |   // clear all flags ...
          ( 1 << USIOIF ) |
          ( 1 << USIPF ) |
          ( 1 << USIDC ) |
          ( 0x0 << USICNT0 ); // clear (do not set) counter
  USIDR = 0;                  // prepare SDA = 0
  P_DDR_OUT (P_USI_SDA);      // set SDA as output
}





// ***** USI Start Condition ISR *****


ISR ( ISR_USI_STARTCOND ) {
  UsiSetOnStartCondInterrupt ();
  slState = slsStartCond;
}





// ***** USI Overflow ISR *****


ISR ( ISR_USI_OVERFLOW ) {
  // Handles all the communication. Only disabled when waiting for a new Start Condition.
  register uint8_t data, adr;

  switch ( slState ) {

    // ***** Addressing ... *****

    case slsAddressing:

      // Check address and send ACK (and next slsSending) if OK, else reset USI...
      data = USIDR;
      adr = data >> 1;
#if !WITH_TWIHUB
      if (adr == brConfigRecord.adr) {    // own address?
#else
      if (slReqAdr == 0xff) {
        if (adr == brConfigRecord.adr ||  // own address? (must be special case, since 'hubMaxAdr' may be < own address!)
            (adr > brConfigRecord.adr && adr <= brConfigRecord.hubMaxAdr))
                                          // child address in the served subnet?
          slReqAdr = adr;
      }
      if (adr == slReqAdr) {
#endif

        // We are addressed ...
        UsiSetToSendAck();    // Acknowledge

        // Master sends a request - we receive ...
        if ((data & 0x01) == 0) {
          slState = slsReceiving;

        // Master fetches a reply - we send ...
        } else {
          if (slReqStatus == brIncomplete) slReqStatus = brRequestCheckError;   // complete request buffer
#if !TWI_SL_SEND_ACTIVE
          slState = slsSending;
#else
#error TBD
          // TBD: 1. Switch off USI
          //      2. Make sure to keep the clock stretched
#endif
        }
      }
      else {

        // We are not addressed: Ignore the communication ...
        UsiResetToWaitForStartCondition ();
        slState = slsBusy;
      }
      break;


    // ***** Receiving a request *****

    case slsReceiving:
      // Set USI to sample data from master ...
      UsiSetToReadData ();
      slState = slsReceivingAck;
      break;

    case slsReceivingAck:
      // Copy data from USIDR and send ACK ...
      data = USIDR;                     // Read data received
      if (slReqStatus == brIncomplete && slReqBytes < slReqBufSize) {  // Store new byte and update status...
        slReqBuf[slReqBytes++] = data;
        if (slReqBytes >= BR_REQUEST_SIZE_MIN)
          if (slReqBytes >= BrRequestSize (twiSlRequest.op)) slReqStatus = brUnchecked;
      }
      UsiSetToSendAck ();
      slState = slsReceiving;
      break;


    // ***** Sending a reply *****

#if !TWI_SL_SEND_ACTIVE
    case slsSendingCheckAck:
      // Check reply and goto slsSending if OK, else reset USI
      if ( USIDR )  {
        // NACK => the master does not want more data
        UsiResetToWaitForStartCondition ();
        SlResetCommunication ();
          // Sending is complete => Clear communication buffers
          // For the case that these lines are missed (not executed), a communication reset is also issued
          // in 'TwiSlIterate()' for state 'slsStartCond'.
        slState = slsBusy;
        break;
      }
      // ACK => From here we just drop straight into slsSending if the master sent an ACK
    case slsSending:
      if (!slRplComplete && slRplPtr >= slRplBytes) {
        // The reply buffer has not yet been committed, but we have no new byte to send available:
        // Stall the bus (SCL is auto-pulled low until the USIOIF flag is cleared) ...
        UsiSetToSendingStall ();
        slState = slsSendingStall;
      }
      else {
        // The reply buffer contains unsent bytes or has been committed: Send answer...
        if (slRplPtr < slRplBytes) USIDR = slRplBuf[slRplPtr++];
        else USIDR = 0xff;  // Buffer underflow: The master requests more bytes than we believe to be appropriate (usually an error)
                            // => send dummy byte to avoid bus lockage (0xff minimizes power consumption)
        UsiSetToSendData ();
        slState = slsSendingWaitAck;
      }
      break;
    case slsSendingWaitAck:
      // Set USI to sample reply from master
      UsiSetToReadAck ();
      slState = slsSendingCheckAck;
      break;
#else   // !TWI_SL_SEND_ACTIVE
    case slsSendingCheckAck:
    case slsSending:
    case slsSendingWaitAck:
      break;
#endif  // !TWI_SL_SEND_ACTIVE


    // ***** Defaults / states to be handled by TwiSlIterate () *****

    default:    // covers: slsIdle, slsBusy, slsStartCond, slsSendingStall
      // Usually, we should never get here!
      //~ P_OUT_1(P_A0);
      UsiResetToWaitForStartCondition ();
      slState = slsBusy;

   }    // switch

}   // ISR (ISR_USI_OVERFLOW)





// *************************** General *****************************************


void TwiSlInit () {
  SlResetCommunication ();    // clear buffers and reset pointers

  INIT (slState, slsBusy);    // be pessimistic about potential ongoing traffic
#if TWI_SL_NOTIFY
  INIT (twiSlNotifyPending, false);
#endif

  UsiInit ();
  UsiResetToWaitForStartCondition ();
}


void TwiSlDone () {
  // This function must be callable without prior 'TwiSlInit()' - important for the resurrection check.
  UsiDone ();
}


EBrStatus TwiSlIterate () {

  // Disable interrupts...
  cli ();

  // Advance TWI slave state ...
  switch (slState) {
    case slsStartCond:
      // Advance from 'slsStartCond' state if a falling clock edge has been received.

      // From app note/previous code:
      //   "Wait for SCL to go low to ensure the start condition has completed (the
      //   start detector will hold SCL low ) - if a stop condition arises then leave
      //   the loop to prevent waiting forever - don't use USISR to test for stop
      //   condition as in application note AVR312 because the stop condition flag is
      //   going to be set from the last TWI sequence."

      if (!P_IN(P_USI_SCL)) {
        // SCL has been pulled down (potentially held by us) => We can continue to await the address ...
        UsiSetToAdressing ();
        slState = slsAddressing;
      }
      else if (P_IN(P_USI_SDA)) {
        // SDA has gone up, but SCL has not been pulled down => A stop condition must have occured ...
        UsiResetToWaitForStartCondition ();
        slState = slsIdle;
      }

      // Reset the communication in case we are still in (completed) phase 3 ...
      //   The regular place to do this is in the ISR for state 'slsSendingCheckAck'.
      //   However, the master might have failed to send a NACK before, so that
      //   this might not have happend yet.
      if (slRplComplete && slRplPtr >= slRplBufSize) SlResetCommunication ();
      break;

    case slsSendingStall:
      // Master is waiting for a reply, but none has been prepared yet.
      // If the message received so far with error, we prepare the reply here.
      // If the message is OK or for a child (hub), the main program must prepare the reply.

      // Handle completed requests, reply to faulty requests to the own host ...
      //   Complete & correct requests must be replied to elsewhere.
      if (slReqStatus == brUnchecked) slReqStatus = BrRequestCheck (&twiSlRequest, slReqBytes);
      if (slReqStatus != brOk && slReqStatus != brIncomplete
#if WITH_TWIHUB
          && slReqAdr == brConfigRecord.adr
#endif
      ) {
        twiSlReply.status = (uint8_t) slReqStatus;
        TwiSlReplyCommit (BR_REPLY_SIZE_STATUS);   // NOTE: This will re-enable interrupts!!
      }
      break;

    case slsBusy:
      // Check for stop condition to switch from 'slsBusy' to 'slsIdle' ...
      //
      // This cannot be done in any state in a "clean up" way. The stop condition flag
      // may go up spuriously during an own transfer.
      // Note: The start condition flag must not be reset here. This causes addressing
      // failures [2019-06-16], probably due to race conditions for a quick start condtion
      // shortly after a stop condition before reaching this code.
      //
      if (USISR & ( 1 << USIPF )) {
        slState = slsIdle;
      }
      break;

#if TWI_SL_NOTIFY
    case slsIdle:
      // Check for a pending notify request and send notification ...
      if (twiSlNotifyPending) {
        slState = slsNotifying;
        UsiSetToNotify ();
        sei ();       // enable interrupts when waiting
        _delay_us (TWI_SL_NOTIFY_US);
        cli ();       // disable interrupts again
        UsiResetToWaitForStartCondition ();
        slState = slsIdle;
        twiSlNotifyPending = false;
      }
      break;
#endif

    default:
      break;
  }

  // Done, enable interrupts again ...
  sei ();
#if WITH_TWIHUB
  if (slReqStatus == brOk && slReqAdr != brConfigRecord.adr) return brIncomplete;
#endif
  return slReqStatus;
}


void TwiSlReplyCommitPartial (uint8_t bytes, bool complete) {
#if !TWI_SL_SEND_ACTIVE
  cli ();
  slRplBytes = bytes;
  slRplComplete = complete;
  if (complete) {      // clear request on final commit, but leave the address intact ...
    slReqBytes = 0;
    slReqStatus = brIncomplete;
  }
  if (slState == slsSendingStall && slRplPtr < slRplBytes) {
    // The master is already waiting for the first / a new byte: Send it now ...
    USIDR = slRplBuf[slRplPtr++];
    UsiSetToSendData ();
    slState = slsSendingWaitAck;
  }
  sei ();
#else
#error "TBD"
#endif
}


void TwiSlReplyCommit (uint8_t bytes) {
  BrReplyPackage (&twiSlReply, bytes);
  TwiSlReplyCommitPartial (bytes, true);
}


void TwiSlReplyFlush () {
  while (slRplBytes) { TwiSlIterate (); }
};





// *****************************************************************************
// *                                                                           *
// *                          TWI Master                                       *
// *                                                                           *
// *****************************************************************************


#if WITH_TWI_MASTER


// ***** Bit-level helpers *****


#define WAIT_HALF(SPEEDDOWN)    do { for (register uint8_t n = SPEEDDOWN; n; n--) _delay_us (P_TWI_MA_TIME_BASE/2.0); } while (0)
#define WAIT_QUARTER(SPEEDDOWN) do { for (register uint8_t n = SPEEDDOWN; n; n--) _delay_us (P_TWI_MA_TIME_BASE/4.0); } while (0)


#define SXX_INIT(SXX) {                                                       \
  P_DDR_IN(SXX);                              /* set port passive         */  \
  if (TWI_MA_INTERNAL_PULLUP) P_OUT_1(SXX);   /* activate internal pullup */  \
  else P_OUT_0(SXX);                          /* set port to 0 forever    */  \
}


#define SCL_UP_AND_LET_STRETCH(SCL) {                                                       \
  P_DDR_IN(SCL);                              /* high impedance                         */  \
  if (TWI_MA_INTERNAL_PULLUP) P_OUT_1(SCL);   /* activate internal pullup */                \
  while (!P_IN(SCL)) P_TWI_MA_ON_SCL_STRETCH; /* wait while SCL low (clock stretching)  */  \
}


#define SCL_DOWN(SCL) {                                               \
  if (TWI_MA_INTERNAL_PULLUP) P_OUT_0(SCL);   /* set value 0      */  \
  P_DDR_OUT(SCL);                             /* drive port       */  \
}


#define SDA_UP(SDA) do {                                              \
  P_DDR_IN(SDA);                              /* high impedance   */  \
  if (TWI_MA_INTERNAL_PULLUP) P_OUT_1(SDA);   /* activate pullup  */  \
} while (0)


#define SDA_DOWN(SDA) do {                                            \
  if (TWI_MA_INTERNAL_PULLUP) P_OUT_0(SDA);   /* set value 0      */  \
  P_DDR_OUT(SDA);                             /* drive port       */  \
} while (0)





// ***** Byte-level sending and receiving *****


/* The following de-facto functions are implemented as macros to allow
 * optimized inlined code and guarantee that the SCL/SDA port parameters
 * are handled as constants for optimized machine code.
 */


#define MA_SEND_START(SCL, SDA, SPEEDDOWN) { \
  /* expects: SCL=x, SDA=x (will wait on SCL=1)                           */  \
  /* leaves:  SCL=0, SDA=0                                                */  \
  SDA_UP                    (SDA);            \
  SCL_UP_AND_LET_STRETCH    (SCL);            \
  WAIT_HALF                 (SPEEDDOWN);                                      \
  SDA_DOWN                  (SDA);            \
  WAIT_HALF                 (SPEEDDOWN);                                      \
  SCL_DOWN                  (SCL);            \
}


#define MA_SEND_STOP(SCL, SDA, SPEEDDOWN) {                                   \
  /* expects: SCL=0, SDA=x (will wait on SCL=1)                           */  \
  /* leaves:  SCL=1, SDA=1                                                */  \
  SDA_DOWN                  (SDA);                                            \
  WAIT_HALF                 (SPEEDDOWN);   /* TBD: Remove? */                 \
  SCL_UP_AND_LET_STRETCH    (SCL);                                            \
  WAIT_HALF                 (SPEEDDOWN);                                      \
  SDA_UP                    (SDA);                                            \
  WAIT_HALF                 (SPEEDDOWN);                                      \
}


#define MA_SEND_BYTE(SCL, SDA, SPEEDDOWN, DATA, RET_ACK) {                    \
  /* expects: SCL=1, SDA=x (is 0 after start, 1 after byte sending)       */  \
  /* leaves:  SCL=0, SDA=1                                                */  \
  uint8_t n;                                                                  \
                                                                              \
  /* Send 8 bits ...                                                      */  \
  for (n = 8; n > 0; n--) {                                                   \
    if (DATA & 0x80) SDA_UP     (SDA);                                        \
    else             SDA_DOWN   (SDA);                                        \
    DATA <<= 1;                                                               \
    WAIT_HALF                   (SPEEDDOWN);                                  \
    SCL_UP_AND_LET_STRETCH      (SCL);                                        \
    WAIT_HALF                   (SPEEDDOWN);                                  \
    SCL_DOWN                    (SCL);                                        \
  }                                                                           \
                                                                              \
  /* Get ACK ...                                                          */  \
  SDA_UP                        (SDA);                                        \
  WAIT_HALF                     (SPEEDDOWN);                                  \
  SCL_UP_AND_LET_STRETCH        (SCL);                                        \
  WAIT_HALF                     (SPEEDDOWN);                                  \
  RET_ACK = P_IN(SDA) ? false : true;                                         \
    /* ACK = line is pulled down, else NACK                               */  \
  SCL_DOWN                      (SCL);                                        \
}



#define MA_RECEIVE_BYTE(SCL, SDA, SPEEDDOWN, ACK, RET_DATA) { \
  /* expects: SCL=x, SDA=x (SDA is 0 after start, 1 after byte sending)   */  \
  /* leaves:  SCL=0, SDA=x!  (stop or repeated start must be sent immediately afterwards!)  */  \
  uint8_t n;                                                                  \
                                                                              \
  /* Receive 8 bits ...                                                   */  \
  SDA_UP                        (SDA);                                        \
  RET_DATA = 0;                                                               \
  for (n = 8; n > 0; n--) {                                                   \
    WAIT_HALF                   (SPEEDDOWN);                                  \
    SCL_UP_AND_LET_STRETCH      (SCL);                                        \
    WAIT_HALF                   (SPEEDDOWN);                                  \
      /* sample just before pulling SCL down again                        */  \
    RET_DATA <<= 1;                                                           \
    if (P_IN(SDA)) RET_DATA |= 1;                                             \
    SCL_DOWN                    (SCL);                                        \
  }                                                                           \
                                                                              \
  /* ACK or NACK ...                                                      */  \
  if (ACK)  SDA_DOWN            (SDA);                                        \
  else      SDA_UP              (SDA);                                        \
  WAIT_HALF                     (SPEEDDOWN);                                  \
  SCL_UP_AND_LET_STRETCH        (SCL);                                        \
  WAIT_HALF                     (SPEEDDOWN);                                  \
  SCL_DOWN                      (SCL);                                        \
}





// *************************** Byte-level Master Functions *********************


#if TWI_MA_PORTS > 1
int8_t twiMaPort = 0;
#else
#define twiMaPort 0
#endif


//~ #define speedDown 4


void TwiMaInit () {
  SXX_INIT (P_TWI_MA_0_SCL);
  SXX_INIT (P_TWI_MA_0_SDA);
}


void TwiMaSendStart () {
  register uint8_t speedDown = brConfigRecord.hubSpeed;

  switch (twiMaPort) {
    case 0:
      MA_SEND_START (P_TWI_MA_0_SCL, P_TWI_MA_0_SDA, speedDown);
      break;
    default:
      break;
  }
}


void TwiMaSendStop () {
  register uint8_t speedDown = brConfigRecord.hubSpeed;

  switch (twiMaPort) {
    case 0:
      MA_SEND_STOP  (P_TWI_MA_0_SCL, P_TWI_MA_0_SDA, speedDown);
      break;
    default:
      break;
  }
}


bool TwiMaSendByte (uint8_t data) {
  register uint8_t speedDown = brConfigRecord.hubSpeed;
  bool ack;

  switch (twiMaPort) {
    case 0:
      MA_SEND_BYTE  (P_TWI_MA_0_SCL, P_TWI_MA_0_SDA, speedDown, data, ack);
      break;
    default:
      ack = false;
      break;
  }

  // Done...
  return ack;
}


uint8_t TwiMaReceiveByte (bool ack) {
  register uint8_t speedDown = brConfigRecord.hubSpeed;
  register uint8_t data;

  switch (twiMaPort) {
    case 0:
      MA_RECEIVE_BYTE (P_TWI_MA_0_SCL, P_TWI_MA_0_SDA, speedDown, ack, data);
      break;
    default:
      data = 0xff;
      break;
  }

  // Done...
  return data;
}


static inline uint8_t TwiMaGetScl () {
  // Return the SDA status of the current port (required for the hub to detect
  // a slave notification).
  switch (twiMaPort) {
    case 0:
      return P_IN(P_TWI_MA_0_SCL);
      break;
    default:
      return 0xff;
      break;
  }
}


static inline uint8_t TwiMaGetSda () {
  // Return the SDA status of the current port (required for the hub to detect
  // a slave notification).
  switch (twiMaPort) {
    case 0:
      return P_IN(P_TWI_MA_0_SDA);
      break;
    default:
      return 0xff;
      break;
  }
}


static inline void TwiMaSetResurrection (bool on) {
  switch (twiMaPort) {
    case 0:
      if (on) {
        SCL_DOWN (P_TWI_MA_0_SCL);
        SDA_DOWN (P_TWI_MA_0_SDA);
      }
      else {
        SDA_UP (P_TWI_MA_0_SDA);
        SCL_UP_AND_LET_STRETCH (P_TWI_MA_0_SCL);
      }
      break;
    default:
      break;
  }
}


#endif // WITH_TWI_MASTER





// *****************************************************************************
// *                                                                           *
// *                          TWI Hub                                          *
// *                                                                           *
// *****************************************************************************


#if WITH_TWIHUB


typedef enum {
  hsIdle = 0,           // Master is idle (0 = reset state)
  hsNotified,           // Master is currently being notified by some child
  hsRequestForwarding,  // Request is being forwarded
  hsReplyForwarding,    // Reply is being fetched and forwarded
  hsResurrection        // Both SCL and SDA are pulled low (requested by BR_CTRL_HUB_RESURRECTION)
} EHubState;


static EHubState hubState;    // current hub state (fully managed by TwiHubIterate()) (!reset to 'hsIdle')
static uint8_t hubReqPtr;     // next request byte to be forwarded to child (!reset to 0)
static uint8_t hubRplPtr;     // next reply byte from child to commit (!reset to 0)
static uint8_t hubRplBytes;   // estimated length of the reply


void TwiHubIterate () {
  register bool isLast;

  // Select TWI master port...
  TwiMaSelectPort (TWIHUB_PORT);

  // Progress TWI master communication ...
  switch (hubState) {

    case hsResurrection:
      if ((RegGet (BR_REG_CTRL) & BR_CTRL_HUB_RESURRECTION) == 0) {
        TwiMaSetResurrection (false);
        hubState = hsIdle;
      }
      // Fall through to 'hsIdle' handling ...
      //   This is important to handle the master bus locking situations that in this
      //   we caused ourselves.
    case hsIdle:

      // Check for resurrection request ...
      if ((RegGet (BR_REG_CTRL) & BR_CTRL_HUB_RESURRECTION) != 0) {
        TwiMaSetResurrection (true);
        hubState = hsResurrection;
      }

      // Check for host notification and handle it ...
      if (!TwiMaGetSda () && TwiMaGetScl ()) {   // SDA low and SCL high?
        ReportChange (BR_CHANGED_CHILD);
        hubState = hsNotified;
        break;
      }

      // Check for possible request or reply forwarding ...
      if (slReqAdr <= brConfigRecord.adr || slReqAdr > brConfigRecord.hubMaxAdr) break;
        // does address match child space?

      // Start request forwarding if possible ...
      if (hubReqPtr < slReqBytes && !slRplBytes) {    // a) unforwarded request bytes, b) we did not start committing yet
        if (!TwiMaGetScl () || !TwiMaGetSda ()) {     // bus locked / down?
          hubReqPtr = slReqBufSize;   // mark request as fully forwarded (same as non-ack'ed address)
          break;
        }
        TwiMaSendStart ();
        if (!TwiMaSendByte ((slReqAdr << 1) | 0)) {    // child address + "write"
          // Address was not ACK'ed: Assume there is no child at the address - Cancel forwarding...
          TwiMaSendStop ();
          hubReqPtr = slReqBufSize;   // mark request as fully forwarded
          break;
        }
        hubState = hsRequestForwarding;
        break;
      }

      // Start reply forwarding if possible ...
      if (hubReqPtr >= slReqBytes && slReqStatus != brIncomplete && slReqStatus != brUnchecked) {
                // Request complete and forwarded completely?
                // An unchecked message may become 'brOk' later: Must wait, not start with an error assumption
                // in order to estimate the reply size correctly.
        if (!TwiMaGetScl () || !TwiMaGetSda ()) {       // bus locked / down?
          // Bus locked: Send 'brNoBus' to master ...
          twiSlReply.status = brNoBus;
          TwiSlReplyCommit (BR_REPLY_SIZE_STATUS);
          hubReqPtr = hubRplPtr = 0;
          break;
        }

        TwiMaSendStart ();
        if (!TwiMaSendByte ((slReqAdr << 1) | 1)) {    // child address + "read"
          // Address not ACK'ed or bus locked: Send 'brNoDevice' to master ...
          TwiMaSendStop ();
          twiSlReply.status = brNoDevice;
          TwiSlReplyCommit (BR_REPLY_SIZE_STATUS);
          hubReqPtr = hubRplPtr = 0;
        }

        hubRplBytes = (slReqStatus == brOk) ? BrReplySize (twiSlRequest.op) : BR_REPLY_SIZE_STATUS;
        hubState = hsReplyForwarding;
      }
      break;

    case hsNotified:

      // Some notification from a slave is going on: Check if it is over ...
      //   If both SDA and SCL are low, we have a locked / switched off bus.
      //   In this case we also leave this state in order to a avoid being locked up here.
      if (TwiMaGetSda () || !TwiMaGetScl ()) hubState = hsIdle;
      break;

    case hsRequestForwarding:

      // Forward a request byte if possible...
      if (hubReqPtr < slReqBytes) {
        if (!TwiMaSendByte (slReqBuf[hubReqPtr++])) {
          // Error (no ACK) ...
          TwiMaSendStop ();
          hubReqPtr = slReqBufSize;   // mark request as fully forwarded
          hubState = hsIdle;
        }
      }
      else {
        if (slReqStatus != brIncomplete) {    // request comlete and completely forwarded?
          TwiMaSendStop ();
          hubState = hsIdle;
        }
      }
      break;

    case hsReplyForwarding:

      // Forward one or more reply byte(s) if possible...
      isLast = (hubRplPtr >= hubRplBytes - 1);
      slRplBuf[hubRplPtr++] = TwiMaReceiveByte (!isLast);  // receive byte
      TwiSlReplyCommitPartial (hubRplPtr, isLast);         // forward to our master
      if (isLast) {
        // Last byte: Finish the transfer...
        TwiMaSendStop ();
        hubReqPtr = hubRplPtr = 0;
        hubState = hsIdle;
      }
      break;

    default:
      break;
  }
}


#endif // WITH_TWIHUB





// *****************************************************************************
// *                                                                           *
// *                          Testing                                          *
// *                                                                           *
// *****************************************************************************


#if 0   // Testing




// ***** Short Loopback Test *****


#define TEST_WRITE 0


int TestLoopbackShort (void) {
  uint8_t adr, data, reply;
  uint8_t errcnt;
  bool ok;

  P_DDR_OUT(P_A0);  // Debug LED...
  P_OUT_0(P_A0);

  //~ while (1);

  // Init...
  TwiSlInit ();
  TwiMaInit ();

  sei ();

  adr = RegGet (BR_REG_CFG_ADR);

  // Main test loop...
  errcnt = 0;
  while (1) {
    data = 0xc3;

    twiSlReply.status = data;
    TwiSlReplyCommit (1);

    TwiMaSendStart ();
#if !TEST_WRITE
    TwiMaSendByte ((adr << 1) | 1);   // Address + "read"
    reply = TwiMaReceiveByte (false);
    //~ TwiMaReceiveByte (false);
    TwiMaSendStop ();
    //~ TwiSlIterate ();    // let slave detect (unexpected) stop condition
    ok = (reply == (uint8_t) data);
#else
    TwiMaSendByte ((adr << 1) | 0);   // Address + "write"
    ok = TwiMaSendByte (data);
    TwiMaSendStop ();
#endif

    // Check result and report error in case ...
    if (errcnt) errcnt--;
    if (!ok) errcnt = 0xff;
    if (!errcnt) PORTA |= 1; else PORTA &= ~1;
    _delay_us (500);
  }
}


#endif  // Testing
