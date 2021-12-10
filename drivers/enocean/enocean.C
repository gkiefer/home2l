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


#include "enocean.H"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>





// *************************** Environment Parameters **************************


ENV_PARA_STRING ("enocean.link", envEnoLinkDev, "/dev/enocean");
  /* Linux device file of the \textit{EnOcean USB 300} gateway
   */





// *************************** EEnoStatus **************************************


const char *EnoStatusStr (EEnoStatus status) {
  static const char *table[] = {
    "Ok",
    "Incomplete",
    "No link",
    "No sync byte or corrupt data",
    "CRC error in header",
    "CRC error in data",
    "Unsupported packet type",
    "Interrupted"
  };

  ASSERT ((int) status >= 0 && (int) status < sizeof (table) / sizeof (table[0]));
  return table[(int) status];
}





// *************************** CEnoTelegram ************************************


static uint8_t enoCrcTable[256] = {
  0x00, 0x07, 0x0e, 0x09, 0x1c, 0x1b, 0x12, 0x15, 0x38, 0x3f, 0x36, 0x31, 0x24, 0x23, 0x2a, 0x2d,
  0x70, 0x77, 0x7e, 0x79, 0x6c, 0x6b, 0x62, 0x65, 0x48, 0x4f, 0x46, 0x41, 0x54, 0x53, 0x5a, 0x5d,
  0xe0, 0xe7, 0xee, 0xe9, 0xfc, 0xfb, 0xf2, 0xf5, 0xd8, 0xdf, 0xd6, 0xd1, 0xc4, 0xc3, 0xca, 0xcd,
  0x90, 0x97, 0x9e, 0x99, 0x8c, 0x8b, 0x82, 0x85, 0xa8, 0xaf, 0xa6, 0xa1, 0xb4, 0xb3, 0xba, 0xbd,
  0xc7, 0xc0, 0xc9, 0xce, 0xdb, 0xdc, 0xd5, 0xd2, 0xff, 0xf8, 0xf1, 0xf6, 0xe3, 0xe4, 0xed, 0xea,
  0xb7, 0xb0, 0xb9, 0xbe, 0xab, 0xac, 0xa5, 0xa2, 0x8f, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9d, 0x9a,
  0x27, 0x20, 0x29, 0x2e, 0x3b, 0x3c, 0x35, 0x32, 0x1f, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0d, 0x0a,
  0x57, 0x50, 0x59, 0x5e, 0x4b, 0x4c, 0x45, 0x42, 0x6f, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7d, 0x7a,
  0x89, 0x8e, 0x87, 0x80, 0x95, 0x92, 0x9b, 0x9c, 0xb1, 0xb6, 0xbf, 0xb8, 0xad, 0xaa, 0xa3, 0xa4,
  0xf9, 0xfe, 0xf7, 0xf0, 0xe5, 0xe2, 0xeb, 0xec, 0xc1, 0xc6, 0xcf, 0xc8, 0xdd, 0xda, 0xd3, 0xd4,
  0x69, 0x6e, 0x67, 0x60, 0x75, 0x72, 0x7b, 0x7c, 0x51, 0x56, 0x5f, 0x58, 0x4d, 0x4a, 0x43, 0x44,
  0x19, 0x1e, 0x17, 0x10, 0x05, 0x02, 0x0b, 0x0c, 0x21, 0x26, 0x2f, 0x28, 0x3d, 0x3a, 0x33, 0x34,
  0x4e, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5c, 0x5b, 0x76, 0x71, 0x78, 0x7f, 0x6a, 0x6d, 0x64, 0x63,
  0x3e, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2c, 0x2b, 0x06, 0x01, 0x08, 0x0f, 0x1a, 0x1d, 0x14, 0x13,
  0xae, 0xa9, 0xa0, 0xa7, 0xb2, 0xb5, 0xbc, 0xbb, 0x96, 0x91, 0x98, 0x9f, 0x8a, 0x8d, 0x84, 0x83,
  0xde, 0xd9, 0xd0, 0xd7, 0xc2, 0xc5, 0xcc, 0xcb, 0xe6, 0xe1, 0xe8, 0xef, 0xfa, 0xfd, 0xf4, 0xf3
};


static inline uint8_t EnoCrc (uint8_t *data, int bytes) {
  uint8_t crc = 0;
  int i;

  for (i = 0; i < bytes; i++) crc = enoCrcTable[crc ^ data[i]];

  //~ static unsigned faultCount = 0;
  //~ if ((faultCount++) % 7 == 0) return crc ^ 1;

  return crc;
}


const char *CEnoTelegram::ToStr (CString *ret) {
  CString sData;
  int i;

  if (dataBytes > 0) sData.SetF ("%02x", data[0]);
  for (i = 1; i < dataBytes; i++) sData.AppendF (":%02x", (int) data[i]);
  ret->SetF ("DevID=%08x RORG=%02x Data=%s Status=%02x dBm=%i",
             deviceId, rorg, sData.Get (), teleStatus, signalStrength);
  return ret->Get ();
}


EEnoStatus CEnoTelegram::Parse (uint8_t *buf, int bufBytes, int *retBytes) {
  EEnoStatus status;
  int i, dataLen, optLen;

  // Sanity ...
  if (bufBytes == 0) return enoIncomplete;
  if (retBytes) *retBytes = 0;

  // Check header ...
  status = enoOk;
  if (buf[0] != 0x55) status = enoNoSync;
  else if (bufBytes < 6) status = enoIncomplete;
  else if (EnoCrc (buf + 1, 4) != buf[5]) status = enoCrcErrorHeader;
  else if (buf[4] != 1) status = enoWrongPacketType;
  else {
    dataLen = (((int) buf[1]) << 8) + (int) buf[2];
    optLen = buf[3];
    if (bufBytes < 6 + dataLen + optLen + 1) status = enoIncomplete;
    else if (EnoCrc (buf + 6, dataLen + optLen) != buf[6 + dataLen + optLen]) status = enoCrcErrorData;
  }

  // Read data if present ...
  if (status == enoOk) {

    // Data record (= RORG + data + device ID + status) ...
    rorg = buf[6];
    dataBytes = dataLen - 6;    // 'dataLen' refers to complete record; 'dataBytes' to user data
    for (i = 0; i < dataBytes; i++) data[i] = buf[7 + i];
    deviceId = * (uint32_t *) &buf[7 + dataBytes];
    deviceId = (deviceId << 24) | ((deviceId << 8) & 0x00ff0000) | ((deviceId >> 8) & 0x0000ff00) | (deviceId >> 24);   // swap endianess
    teleStatus = buf[7 + dataBytes + 4];

    // Optional data record (only the dBm value is picked out) ...
    if (optLen >= 6) signalStrength = buf[(12 + dataBytes) + 5];
    else signalStrength = 0;

    // Return bytes read ...
    if (retBytes) *retBytes = 6 + dataLen + optLen + 1;
  }

  // Return bytes to skip in error cases ...
  if (status != enoIncomplete && retBytes) {
    for (i = 1; i < bufBytes && buf[i] != 0x55; i++);
    *retBytes = i;
  }

  // Done ...
  return status;
}





// *************************** Top-Level ***************************************


#define ENO_LINK_RETRY_TIME ((TTicks) TICKS_FROM_SECONDS(5))


static int enoFd = -1;
static CSleeper enoSleeper;
static int enoSleeperInterruptCommand = 17;
static TTicks tLastRetry;    // last failed try to open the link
static uint8_t rcvBuf[256];
static int rcvBytes;


void EnoClose () {
  if (enoFd >= 0) {
    close (enoFd);
    //~ if (close (enoFd) < 0) DEBUGF (1, ("close(): %s", strerror (errno)));
    enoFd = -1;
  }
}


static void EnoOpen () {
  struct termios ts;
  TTicks tNow;
  bool ok, isRetry;

  // Sanity ...
  if (enoFd >= 0) return;
  isRetry = (tLastRetry != 0);
  tNow = TicksNowMonotonic ();
  if (isRetry && (tNow - tLastRetry < ENO_LINK_RETRY_TIME)) return;
  tLastRetry = tNow;

  // Open device file ...
  enoFd = open (envEnoLinkDev, O_RDONLY);
  //~ INFOF (("# open (envEnoLinkDev, O_RDONLY) -> %i", enoFd));
  if (enoFd < 0) {
    if (!isRetry) WARNINGF (("Failed to open EnOcean link '%s': %s", envEnoLinkDev, strerror (errno)));
    return;
  }

  // Check for serial interface and set parameters ...
  ok = (tcgetattr (enoFd, &ts) == 0);
  if (ok) {

    // Set "raw" mode (see cfmakeraw(), which is a non-standard function) ...
    ts.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    ts.c_oflag &= ~OPOST;
    ts.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    ts.c_cflag &= ~(CSIZE | PARENB);
    ts.c_cflag |= CS8;

    // Set timeout to 0.5 seconds and prevent read() from returning 0 in normal cases ...
    //   NOTE [2020-08-30]:
    //     If the USB device is removed, read() returns 0, not <0 to indicate an error.
    //     For this reason it VMIN is set to 1, so that read() returning 0 is reserved
    //     as an error condition.
    //     VTIME should normally not have a relevant effect, since any read() is done after
    //     waiting for readability with select().
    ts.c_cc[VMIN] = 1;
    ts.c_cc[VTIME] = 5;

    // Set baudrate ...
    if (cfsetispeed (&ts, B57600) < 0) ok = false;
    if (cfsetospeed (&ts, B57600) < 0) ok = false;

    // Apply parameters ...
    if (ok) if (tcsetattr (enoFd, TCSANOW, &ts) < 0) ok = false;
  }
  if (!ok) {
    WARNINGF (("%s: No EnOcean interface (no TTY).", envEnoLinkDev));
    EnoClose ();
    return;
  }

  // Flush input and output buffers of the serial device ...
  tcflush (enoFd, TCIOFLUSH);

  // Success ...
  if (isRetry) INFOF (("EnOcean link '%s' opened successfully.", envEnoLinkDev));
  else DEBUGF (1, ("EnOcean link '%s' opened successfully.", envEnoLinkDev));
  tLastRetry = 0;
}


void EnoInit () {
  rcvBytes = 0;
  enoSleeper.EnableCmds (sizeof (int));
}


void EnoDone () {
  EnoClose ();
}


const char *EnoLinkDevice () {
  return envEnoLinkDev;
}


static inline EEnoStatus EnoReadFromLink (TTicks maxTime) {
  TTicks dRetry;
  int i, bytes, cmd;

  // Ensure link is open ...
  //~ INFOF (("# EnoReceive: Open... (enoFd = %i)", enoFd));
  EnoOpen ();
  if (enoFd < 0) {
    dRetry = tLastRetry + ENO_LINK_RETRY_TIME - TicksNowMonotonic ();
    ASSERT (dRetry > 0);
    if (maxTime < 0) maxTime = dRetry;
    else maxTime = MIN (maxTime, dRetry);
  }

  // Sleep and handle interrupt ...
  enoSleeper.Prepare ();
  enoSleeper.AddReadable (enoFd);
  enoSleeper.Sleep (maxTime);
  if (enoSleeper.GetCmd (&cmd)) {
    ASSERT (cmd == enoSleeperInterruptCommand);
    return enoInterrupted;
  }

  // Try to open again ...
  //~ INFOF (("# EnoReceive: Open again ... (enoFd = %i)", enoFd));
  EnoOpen ();
  if (enoFd < 0) return enoNoLink;

  // Read something ...
  //~ INFOF (("# EnoReceive: Read (%i) ... (enoFd = %i)", MIN (5, sizeof (rcvBuf) - rcvBytes), enoFd));
  bytes = read (enoFd, rcvBuf + rcvBytes, sizeof (rcvBuf) - rcvBytes);
  if (bytes <= 0) {
    WARNINGF (("Failed to read from EnOcean link '%s': %s", envEnoLinkDev, bytes < 0 ? strerror (errno) : "No more data"));
    EnoClose ();
    return enoNoLink;
  }
  if (envDebug >= 2) {
    CString s;
    s.SetC ("Received:");
    for (i = 0; i < bytes; i++) s.AppendF (" %02x", rcvBuf[rcvBytes + i]);
    DEBUG (2, s.Get ());
  }
  rcvBytes += bytes;

  //~ if (bytes > 0) {
    //~ CString s;
    //~ s.SetF ("### Received %i bytes: buf[%i] =", bytes, rcvBytes);
    //~ for (i = 0; i < rcvBytes; i++) s.AppendF (" %02x", rcvBuf[i]);
    //~ INFO (s.Get ());
  //~ }

  return enoOk;
}


EEnoStatus EnoReceive (CEnoTelegram *telegram, TTicks maxTime) {
  EEnoStatus status;
  int i, bytes;

  // Check for already buffered message ...
  status = telegram->Parse (rcvBuf, rcvBytes, &bytes);

  // If message incomplete: Read from link and check again ...
  if (status == enoIncomplete) {
    status = EnoReadFromLink (maxTime);
    if (status != enoOk) return status;   // interrupt or failure
    status = telegram->Parse (rcvBuf, rcvBytes, &bytes);
  }

  // Try to interpret and complete ...
  //~ INFOF (("# EnoReceive: Parse ... (enoFd = %i)", enoFd));
  if (bytes) {    // remove consumed bytes ...
    //~ INFOF (("### Consumed %i bytes: %s", bytes, EnoStatusStr (status)));
    if (status != enoOk) WARNINGF(("EnOcean: Skipping %i unmatched bytes: %s", bytes, EnoStatusStr (status)));
    rcvBytes -= bytes;
    for (i = 0; i < rcvBytes; i++) rcvBuf[i] = rcvBuf[bytes + i];
  }

  // Done ...
  return status;
}


void EnoInterrupt () {
  enoSleeper.PutCmd (&enoSleeperInterruptCommand);
}
