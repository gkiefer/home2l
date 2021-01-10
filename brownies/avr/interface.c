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


#include "interface.h"

#if AVR


#endif // AVR





// *************************** CRC Helpers *************************************


/* The CRC polynomials follow the recommendations given in [1]. Special care is
 * taken of potential all-1 messages that can happen with TWI/i2c if a slave stop
 * responding at all. The implementations process the most significant bits first,
 * so that bits are processed in the same order as transferred.
 *
 * The correctness of the implementation can be validated by running:
 *
 * > gcc -g -o test_crc -DTEST_CRC interface.c; ./test_crc
 *
 * [1] Philip Koopman, Tridib Chakravarty: "Cyclic Redundancy Code (CRC) Polynomial Selection For Embedded Networks",
 *     International Conference on Dependable Systems and Networks, 2004,
 *     DOI: 10.1109/DSN.2004.1311885
 */

#ifdef TEST_CRC

#include <stdio.h>
#include <string.h>
#include <assert.h>

#endif // TEST_CRC


#define CRC8_POLY 0xa6  // HD=4 for up to 15 message bits (reg read, mem read), HD=3 for up to 247 bits (any), recommended for >119 message bits [1]
#define CRC8_SEED 0xbb  // no accepted all-1 message <= 32 bytes

#define CRC4_POLY 0x9   // HD=3 for up to 11 message bits (reg write; reg read has effectively 12 bits), else HD=2 (any); equivalent to CCITT-4
#define CRC4_SEED 0x1   // no accepted all-1 message <= 32 bytes


static uint8_t CalcCRC (const void *data, int8_t bytes, uint8_t seed, uint8_t poly) {
  uint8_t crc, i;
  const uint8_t *p;
#ifdef TEST_CRC
  int period = (poly & 0xf) ? 255 : 15;
  bool use[256];
  bzero (use, sizeof(use));
#endif

  p = (uint8_t *) data;
  crc = seed;
  while (bytes--) {
    crc ^= *(p++);
    for (i = 8; i; i--) {
      if ((crc & 0x80) != 0) crc = (crc << 1) ^ poly;
      else crc <<= 1;
#ifdef TEST_CRC
      //~ printf ("%3i. crc = %02x\n", (int) period, (int) crc);
      if (!p[-1] && period >= 1) {
        if (period == 1) assert (crc == seed);  // Check if the seed repeats correctly
        else assert (!use[crc]);                // Check if the seed repeats too early
        use[crc] = true;
        period--;
      }
#endif
    }
  }
//~ #if !AVR
  //~ printf ("### CRC(%02x/%02x) = %02x, data[%i] =", seed, poly, crc, p - (uint8_t *) data);
  //~ for (i = 0; i < p - (uint8_t *) data; i++) printf (" %02x", ((uint8_t *) data) [i]);
  //~ putchar ('\n');
//~ #endif
  return crc;
}


static inline uint8_t BrCalcCheck8 (const void *data, uint8_t bytes) {
  // Return an 8-bit checksum of the passed array.
  return CalcCRC (data, bytes, CRC8_SEED, ((CRC8_POLY << 1) & 0xff) | 1);
}


static inline uint8_t BrCalcCheck4 (const void *data, uint8_t bytes) {
  // Return a 4-bit checksum in the upper 4 bits of the returned value.
  return CalcCRC (data, bytes, CRC4_SEED << 4, ((CRC4_POLY << 5) & 0xf0) | 0x10);
}





// ***** Test Program *****


#ifdef TEST_CRC


int main (int argc, char **argv) {
  uint8_t buf[32], crc;
  int len;

  // CRC8: Polynomial primitive? ...
  puts ("Testing CRC8 ...");
  bzero (buf, sizeof (buf));
  crc = BrCalcCheck8 (buf, 32);
    // Run polynomial in autonomous mode. Init state (= seed) must be reached
    // for the first time after exactly 2^8 - 1 iterations.

  // CRC8: Shortest accepted all-1 message ...
  memset (buf, 0xff, sizeof (buf));
  for (len = 1; len < sizeof (buf); len++) {
    if (BrCalcCheck8 (buf, len) == 0xff) {
      printf ("  shortest accepted all-1 message: %i bytes\n", len);
      break;
    }
  }

  // CRC4: Polynomial primitive? ...
  puts ("Testing CRC4 ...");
  bzero (buf, sizeof (buf));
  crc = BrCalcCheck4 (buf, 4);
    // Run polynomial in autonomous mode. Init state (= seed) must be reached
    // for the first time after exactly 2^4 - 1 iterations.

  // CRC4: Shortest accepted all-1 message ...
  memset (buf, 0xff, sizeof (buf));
  for (len = 1; len < sizeof (buf); len++) {
    if (BrCalcCheck4 (buf, len) == 0xf0) {
      printf ("  shortest accepted all-1 message: %i bytes\n", len);
      break;
    }
  }

  // Done ...
  puts ("All tests passed.");
  return 0;
}


#endif // TEST_CRC





// *************************** Requests + Replies ******************************


int8_t BrRequestSize (uint8_t op) {      // Total number of byte of a message (including checksum)
  if (BR_OP_IS_REG_READ (op))  return 2;
  if (BR_OP_IS_REG_WRITE (op)) return 3;
  if (BR_OP_IS_MEM_READ (op))  return 3;
  if (BR_OP_IS_MEM_WRITE (op)) return 3 + BR_MEM_BLOCKSIZE;
  return 2;                             // illegal operation
}


int8_t BrReplySize (uint8_t op) {        // Total number of byte of a reply (including status/checksum)
  if (BR_OP_IS_REG_READ (op)) return 2;
  if (BR_OP_IS_MEM_READ (op)) return 2 + BR_MEM_BLOCKSIZE;
  return 1;                             // default (no data to return)
}


#if !AVR
void BrRequestPackage (TBrRequest *msg) {
  msg->check = BrCalcCheck8 (&msg->op, BrRequestSize (msg->op) - 1);
}
#endif


EBrStatus BrRequestCheck (TBrRequest *msg, int8_t bytes) {
  uint8_t check;
  int8_t len;

  // Completeness...
  if (bytes < BR_REQUEST_SIZE_MIN) return brIncomplete;
  len = BrRequestSize (msg->op);
  if (bytes < len) return brIncomplete;

  // Checksum...
  check = BrCalcCheck8 (&msg->op, bytes-1);
  if (check != msg->check) return brRequestCheckError;

  // OK...
  return brOk;
}


void BrReplyPackage (TBrReply *reply, int8_t len) {
  if (len > 2) reply->memRead.dataCheck = BrCalcCheck8 (&reply->memRead.data, BR_MEM_BLOCKSIZE);
    // replies with >2 bytes are memory reads => calculate inner data check first.
  reply->status &= 0x0f;      // clear check part of status byte (important for calculation!)
  reply->status |= BrCalcCheck4 (reply, len);
}


#if !AVR
EBrStatus BrReplyCheck (TBrReply *reply, uint8_t op, int8_t bytes) {
  uint8_t checkAndStatus;
  int8_t len;

  // Completeness...
  if (bytes < BR_REPLY_SIZE_MIN) return brIncomplete;
  checkAndStatus = reply->status;
  len = ((checkAndStatus & 0x0f) == (uint8_t) brOk) ? BrReplySize (op) : BR_REPLY_SIZE_STATUS;
    // If the status indicates an error, the reply does not (need to) contain more data.
  if (bytes < len) return brIncomplete;

  // Checksum...
  reply->status &= 0x0f;
  if ((checkAndStatus & 0xf0) != BrCalcCheck4 (reply, len)) return brReplyCheckError;
  if ((checkAndStatus & 0x0f) == (uint8_t) brOk && BR_OP_IS_MEM_READ(op))
    if (reply->memRead.dataCheck != BrCalcCheck8 (&reply->memRead.data, BR_MEM_BLOCKSIZE)) return brReplyCheckError;

  // OK...
  return brOk;
}
#endif
