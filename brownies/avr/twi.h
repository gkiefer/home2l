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


#ifndef _TWI_
#define _TWI_


/** @file
 *
 * This file is the interface to the TWI slave, master and hub functionality.
 *
 * @addtogroup brownies_firmware
 *
 * @{
 */


#include "core.h"




// *************************** TWI Slave ***************************************


/// @name TWI Slave (Internal) ...
///
/// These functions are used by the main program (avr/main.c).
/// **None of these functions should be called by feature modules.**
///
/// @{



// ***** General *****

void TwiSlInit ();
  ///< @brief @private Init the slave interface
void TwiSlDone ();
  ///< @brief @private Shutdown the slave interface; can be called without prior TwiSlInit() (important for resurrection check).

EBrStatus TwiSlIterate ();
  ///< @brief @private Progress the slave interface.
  /// @return #brOk if a complete and valid request is available, #brIncomplete if it is still to come,
  /// or any other code if an error has been detected.
  ///
  /// If #brOk is received (and only then),
  /// - a reply must be formulated and commited,
  /// - the reply buffer may be arbitrarily overwritten for this.
  /// If no #brOk is received, it is not allowed to write to the reply buffer.
  ///
  /// TwiSlIterate() takes care of replying to faulty or incomplete messages.



// ***** BrRequests and replies *****

extern /* const */ TBrRequest twiSlRequest;
  ///< @brief @private Request as received from master. Should never be written to.
  /// May contain invalid data. The status returned by TwiSlGetRequestStatus() reports the validity.
extern TBrReply twiSlReply;
  ///< @brief @private Reply to be sent on commit.
  /// Checksum(s) does not need to be set (will be done during commiting).

void TwiSlReplyCommit (uint8_t bytes);
  ///< @brief @private To be called to enqueue a reply for a master message.
  /// @param bytes must be the total size of the reply.
  /// The reply is packaged. This will also clear any incoming request message.
  /// Writing to the reply buffer is no longer allowed after this.
  ///
void TwiSlReplyCommitPartial (uint8_t bytes, bool complete);
  ///< @brief @private Commit a partial reply message. The reply is NOT packaged.
  /// @param bytes is the number of leading bytes in the buffer ready to be sent.
  /// @param complete = 'true' indicates that this is the last invocation for the reply
  ///    and that 'bytes' is the final length of the reply.
  /// This function may be called repeatedly (with 'complete == true' with and only
  /// with the last call). Unlike TwiSlReplyCommit(), the reply is not packaged in
  /// any of the calls. The caller must take care of this!
void TwiSlReplyFlush ();
  ///< @brief @private Wait until the last reply has been sent out. The reply must be commited before.



// ***** Host notification *****


#if TWI_SL_NOTIFY || DOXYGEN

extern bool twiSlNotifyPending;   ///< @private

static inline void TwiSlNotify () { twiSlNotifyPending = true; }
  ///< @brief @private Issue a host notification signal, which will be sent at next occasion.

#else

static inline void TwiSlNotify () {}

#endif





// *************************** TWI Master **************************************


/// @}
/// @name TWI Master ...
///
/// **Note:** These functions may only be called if the hub functionality is disabled.
///
/// @{


#if WITH_TWI_MASTER || DOXYGEN


void TwiMaInit ();                    ///< @brief Init all TWI master ports

#if TWI_MA_PORTS > 1 || DOXYGEN
extern int8_t twiMaPort;              ///< @private
static inline void TwiMaSelectPort (int8_t port) { twiMaPort = port; }
                                      ///< @brief Select the TWI master port for the next operations.
#else
static inline void TwiMaSelectPort (int8_t port) {}
#endif

void TwiMaSendStart ();               ///< @brief Send a start condition.
void TwiMaSendStop ();                ///< @brief Send a stop condition.
bool TwiMaSendByte (uint8_t data);    ///< @brief Send a byte (returns the ACK bit).
uint8_t TwiMaReceiveByte (bool ack);  ///< @brief Receive a byte ('ack' to be set to 1 if more bytes are expected).


#endif // WITH_TWI_MASTER





// ************************** TWI Hub (Uses Master) ****************************


/// @}
/// @name TWI Hub (Uses Master)...
/// @{


#if WITH_TWIHUB || DOXYGEN


static inline void TwiHubInit () { TwiMaInit (); }
  ///< @brief @private Init the hub functionality.

void TwiHubIterate ();
  ///< @brief @private Iterate and react on host notifications from any slave.

static inline void TwiHubOnRegRead (uint8_t reg) {}
  ///< @brief @private Empty module interface function (see CoreOnRegRead() ).
static inline void TwiHubOnRegWrite (uint8_t reg, uint8_t val) {}
  ///< @brief @private Empty module interface function (see CoreOnRegWrite() ).


#else   // WITH_TWIHUB


static inline void TwiHubInit () {}
static inline void TwiHubIterate () {}
static inline void TwiHubOnRegRead (uint8_t reg) {}
static inline void TwiHubOnRegWrite (uint8_t reg, uint8_t val) {}


#endif  // WITH_TWIHUB


/// @}    // name

/// @}    // addtogroup brownies_firmware


#endif // _TWI_
