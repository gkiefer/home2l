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


#ifndef _ENOCEAN_
#define _ENOCEAN_

#include "env.H"


#define ENO_MAX_DATA_BYTES 32


enum EEnoStatus {
  enoOk = 0,
  enoIncomplete,      ///< a telegram is still incomplete
  enoNoLink,          ///< no link
  enoNoSync,          ///< no leading 0x55 or corrupted data
  enoCrcErrorHeader,  ///< wrong CRC for the header
  enoCrcErrorData,    ///< wrong CRC for data
  enoWrongPacketType, ///< wrong packet type (only ERP1 is presently supported)
  enoInterrupted      ///< the operation has been interrupted
};


const char *EnoStatusStr (EEnoStatus status);
  ///< @brief Return a readable string for a status code.


class CEnoTelegram {
  public:
    CEnoTelegram () { isValid = false; }

    // Accessing fields ...
    uint32_t DeviceId () { return deviceId; }
    uint8_t Rorg () { return rorg; }
    int DataBytes () { return dataBytes; }
    uint8_t *Data () { return data; }
    uint8_t Status () { return teleStatus; }
    uint8_t SignalStrength () { return signalStrength; }

    // Service ...
    const char *ToStr (CString *ret);

    // Telegram parsing ...
    EEnoStatus Parse (uint8_t *buf, int bufBytes, int *retBytes = NULL);
      ///< @brief Parse a buffer for a correct telegram.
      /// If 'enoOk' is returned, '*this' is set accordingly.

  protected:
    bool isValid;

    // Data fields ...
    uint32_t deviceId;      // device ID (TXID)
    uint8_t rorg;           // RORG value
    int dataBytes;
    uint8_t data[ENO_MAX_DATA_BYTES];
    uint8_t teleStatus;     // status value of telegram

    // Optional data fields ...
    uint8_t signalStrength; // signal strength (best RSSI value of all received subtelegrams)
};


void EnoInit ();
void EnoDone ();

const char *EnoLinkDevice ();
  ///< Get Linux device of the EnOcean interface.

EEnoStatus EnoReceive (CEnoTelegram *telegram, TTicksMonotonic maxTime = -1);
  ///< @brief Receive pending data from the EnOcean link.
  ///
  /// The function returns when a complete telegram is received, after EnoInterrupt() was called,
  /// or after a maximum time waiting time.
  /// If the link is device is not accessible, it is tried to be reconnected.
  /// If incorrect data is received, the function returns immediately with the appropriate return
  /// code.
  /// There is no guarantee that the maximum waiting time is actually waited, the function may also
  /// return any earlier. However, unless maxTime == 0, it is legal to call this in a loop. This will
  /// not result in an active waiting loop.
  ///
  /// @param telegram is the telegram filled in if @ref enoOk is returned.
  /// @param maxTime is the maximum waiting time. If set to -1, the function may block forever. If set to 0, the function does not block.
  /// @return status of the operation

void EnoInterrupt ();
  ///< @brief Interrupt a running EnoReceive() instance.
  /// **Note:** All other functions and methods in this module must be called from the same thread.
  /// This is the only function allowed to be called from any thread.


#endif // _ENOCEAN_
