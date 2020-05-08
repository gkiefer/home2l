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


#ifndef _INTERFACE_
#define _INTERFACE_


/** @file
 *
 * @addtogroup brownies_interface
 *
 * This is the general interface file for *Home2L Brownies* containing common
 * definitions to be shared between *Brownies* and Linux hosts as well as
 * everything related to the communication protocol.
 *
 * This file is included by both the  Linux and the AVR C (AVR==1) projects.
 * All constants shared between Brownies and hosts are prefixed with 'BR_*'.
 *
 * @{
 */


#include <stdint.h>
#include <stdbool.h>





// *************************** General Device Settings *************************


/** @name Time Constants and Conversion ...
 *
 * The CPU frequency @ref BR_CPU_FREQ must match the AVR compiler/linker setting
 * of F_CPU.
 *
 * The *Brownie* firmware uses a hardware timer (no regular timer interrupt) to
 * measure time. The time unit is referred to as a *Brownie Tick*. The duration
 * of a *Brownie Tick* is / should be in the order of one millisecond, but the
 * exact duration somewhat depends on the hardware.
 *
 * @{
 */


#define BR_CPU_FREQ 1000000         ///< CPU clock frequency.

#if AVR && BR_CPU_FREQ != F_CPU
#error "The CPU frequency (F_CPU) must be set to 1 MHz / BR_CPU_FREQ"
#endif


#define BR_TICKS_PER_SECOND (((float) BR_CPU_FREQ) / 1024.0)
#define BR_TICKS_PER_MS (BR_TICKS_PER_SECOND / 1000.0)
#define BR_MS_PER_TICK (1.0 / BR_TICKS_PER_MS)

#define BR_TICKS_OF_MS(T) ((T) * BR_TICKS_PER_MS)
#define BR_MS_OF_TICKS(T) (((float) (T)) * BR_MS_PER_TICK)

#define BR_TICKS_NEVER 0


/// @}





// *************************** Protocol ****************************************


/** @defgroup brownies_interface_protocol Communication Protocol
 * @brief Communication message formats and *Brownie* status codes (@ref EBrStatus).
 *
 * This section contains all definition related to the generic request-reply
 * protocol used by the *Home2L Brownies* for TWI (aka i2c) communication.
 * Background information on the protocol can be found in the *Home2L Book*.
 *
 * **Notes on flash programming:**
 * - A flash page (SPM_PAGESIZE) usually contains multiple memory blocks (of size BR_MEM_BLOCKSIZE).
 * - The page is written back (only) when the last block of a page is written.
 * - During flash programming, no EEPROM writes are allowed.
 * - Flash writes to dangerous addresses (own code) are usually blocked out (@ref brForbidden).
 *
 *
 * @{
 */




#define BR_MEM_BLOCKSIZE_SHIFT 4
#define BR_MEM_BLOCKSIZE (1 << BR_MEM_BLOCKSIZE_SHIFT)
  ///< @brief Block size for 'memRead'/memWrite' operations.
  /// **Note:** This value is an integral part of the communication protocol and must not be changed.



/** Communication status.
 */
typedef enum {
  brOk = 0,             ///< Last command executed successfully
  brIncomplete,         ///< No or incomplete message/reply received
  brUnchecked,          ///< Message complete, but not yet checked for checksum
  brRequestCheckError,  ///< Checksum of request incorrect or message too short
  brReplyCheckError,    ///< Checksum of reply incorrect or message too short
  brIllegalOperation,   ///< Non-existing operation
  brForbidden,          ///< Operation not allowed
  brNoBrownie,          ///< (for masters) No brownie can be reached under a given address (wrong magic number)
  brNoDevice,           ///< (for masters) No device can be reached under a given address
  brNoBus,              ///< (for masters) General I/O error when accessing the TWI bus

  brNoReply = 0x0f,     ///< A device did not respond anything (SDA remained pulled up -> 0x[f]f)
  brEND
} EBrStatus;


/** @brief Request message.
 */
typedef struct SBrRequest {
  uint8_t check;        ///< Checksum (8 bits).
  uint8_t op;           ///< Operation.
  union {
    struct {            // Register write data ...
      uint8_t val;                    ///< (regWrite) Value to write.
    } regWrite;
    struct {            // Memory read data ...
      uint8_t adr;                    ///< (memRead) Memory block address
    } memRead;
    struct {            // Memory write ...
      uint8_t adr;                    ///< (memWrite) Memory block address
      uint8_t data[BR_MEM_BLOCKSIZE]; ///< (memWrite) Data to write
    } memWrite;
  };
} TBrRequest;


/** @brief Reply message.
 */
typedef struct SBrReply {
  uint8_t status;         ///< Checksum (bits 7..4) and status (bits 3:0) (@ref EBrStatus)
  union {
    struct {              // Register read ...
      uint8_t val;                    ///< (regRead) Value
    } regRead;
    struct {              // Memory read ...
      uint8_t data[BR_MEM_BLOCKSIZE]; ///< (memRead) Data
      uint8_t dataCheck;              ///< (memRead) 8-bit checksum for 'data'
    } memRead;
  };
} TBrReply;





// ***** Macros *****


// Request-related ...
#define BR_REQUEST_SIZE_MAX ((int) sizeof (TBrRequest))   ///< Maximum length of a request
#define BR_REQUEST_SIZE_MIN 2                             ///< Mminimum length of a valid request

// Reply-related ...
#define BR_REPLY_SIZE_MAX ((int) sizeof (TBrReply))       ///< Maximum length of a request
#define BR_REPLY_SIZE_MIN 1                               ///< Minimum length of a valid reply
#define BR_REPLY_SIZE_STATUS 1                            ///< Length of a status-only reply


// Contructing operation words ...
#define BR_OP_REG_READ(REG)  ((0x00 | (REG)))             ///< Build "register read" opcode
#define BR_OP_REG_WRITE(REG) ((0x40 | (REG)))             ///< Build "register write" opcode
#define BR_OP_MEM_READ(BLKADR)  (0x80 | ((BLKADR) >> 8))  ///< Build "memory read" opcode
#define BR_OP_MEM_WRITE(BLKADR) (0x90 | ((BLKADR) >> 8))  ///< Build "memory write" opcode

// Analyzing operation words ...
#define BR_OP_IS_REG_READ(OP)  (((OP) & 0xc0) == 0x00)
#define BR_OP_IS_REG_WRITE(OP) (((OP) & 0xc0) == 0x40)
#define BR_OP_IS_MEM_READ(OP)  (((OP) & 0xf0) == 0x80)
#define BR_OP_IS_MEM_WRITE(OP) (((OP) & 0xf0) == 0x90)





// ***** Functions *****


// Requests ...
int8_t BrRequestSize (uint8_t op);
  ///< @brief Get the size of a request message in bytes, depending on the operation.
#if !AVR
void BrRequestPackage (TBrRequest *msg);
  ///< @brief Complete the message for sending (i.e. add checksum).
#endif
EBrStatus BrRequestCheck (TBrRequest *msg, int8_t bytes);
  ///< @brief Check received message.
  /// 'bytes' is the number of valid bytes in the beginning of the message.


// Replies ...
int8_t BrReplySize (uint8_t op);
  ///< @brief Get the size of a reply message in bytes, depending on the operation.
void BrReplyPackage (TBrReply *reply, int8_t len);
  ///< @brief Complete the reply for sending (i.e. add checksum).
#if !AVR
EBrStatus BrReplyCheck (TBrReply *reply, uint8_t op, int8_t bytes);
  ///< @brief Check received message.
  /// 'bytes' is the number of received bytes.
#endif


/// @}      // brownies_interface_protocol





// ************ Brownie Memory Organization and Data Records *******************


/** @defgroup brownies_interface_memory *Brownie* Memory
 * @brief Reference documentation of the *Brownie* virtual memory organization.
 *
 * This section decribes the organization of the virtual memory as accessed
 * by the @ref BR_OP_MEM_READ() and @ref BR_OP_MEM_WRITE() operations.
 *
 * @nosubgrouping
 *
 * @{
 */



/// @name Memory Layout
/// @{

// Memory base pages ...
#define BR_MEM_PAGE_FLASH  0x0  ///< ... 0x7: Program flash memory pages (one page = 0x100 * BR_MEM_BLOCKSIZE bytes)
#define BR_MEM_PAGE_SRAM   0x8  ///< SRAM base page
#define BR_MEM_PAGE_EEPROM 0x9  ///< EEPROM base page
#define BR_MEM_PAGE_VROM   0xa  ///< Version ROM base page (mapped to data array in program)

// Identifying memory space region ...
#define BR_MEM_ADR_IS_FLASH(ADR)   (((ADR) >> (BR_MEM_BLOCKSIZE_SHIFT+8)) <= BR_MEM_PAGE_FLASH + 7)
#define BR_MEM_ADR_IS_SRAM(ADR)    (((ADR) >> (BR_MEM_BLOCKSIZE_SHIFT+8)) == BR_MEM_PAGE_SRAM)
#define BR_MEM_ADR_IS_EEPROM(ADR)  (((ADR) >> (BR_MEM_BLOCKSIZE_SHIFT+8)) == BR_MEM_PAGE_EEPROM)
#define BR_MEM_ADR_IS_VROM(ADR)    (((ADR) >> (BR_MEM_BLOCKSIZE_SHIFT+8)) == BR_MEM_PAGE_VROM)

// Building the address from offset inside a region ...
#define BR_MEM_ADR_FLASH(OFS)  ((OFS) | (BR_MEM_PAGE_FLASH  << (BR_MEM_BLOCKSIZE_SHIFT+8)))
#define BR_MEM_ADR_SRAM(OFS)   ((OFS) | (BR_MEM_PAGE_SRAM   << (BR_MEM_BLOCKSIZE_SHIFT+8)))
#define BR_MEM_ADR_EEPROM(OFS) ((OFS) | (BR_MEM_PAGE_EEPROM << (BR_MEM_BLOCKSIZE_SHIFT+8)))
#define BR_MEM_ADR_VROM(OFS)   ((OFS) | (BR_MEM_PAGE_VROM   << (BR_MEM_BLOCKSIZE_SHIFT+8)))

// Getting the offset inside a region ...
#define BR_MEM_OFS(ADR) ((ADR) & ((BR_MEM_ADR_IS_FLASH(ADR) ? 0x7ff : 0x0ff) << BR_MEM_BLOCKSIZE_SHIFT))



// Flash ...
#define BR_FLASH_PAGESIZE 0x40
  ///< @brief Size of a flash page to be used by communication peers (e.g. TWI masters).
  ///
  /// This must be the smallest common multiple of all possible SPM_PAGESIZE
  /// values (or power-of-two multiple thereof).
  ///
  /// **Note:** The value is typically a multiple of @ref BR_MEM_BLOCKSIZE.
  /// Memory writes to flash must be extended to a multiple of this,
  /// since the brownie would otherwise not write back the last page.
#define BR_FLASH_BASE_MAINTENANCE  0x0040
  ///< @brief Byte address defining the start of the maintenance system.
#define BR_FLASH_BASE_OPERATIONAL  0x0a00
  ///< @brief Byte address defining the border between the maintenance and application system.
  /// Addresses below this are reserved for the maintenance system.


// VROM ...
#define BR_VROM_SIZE        sizeof (TBrFeatureRecord)   ///< Size of the VROM


// EEPROM ...
#define BR_EEPROM_ID_BASE   0x0000                      ///< Location of the ID string in the EEPROM.
#define BR_EEPROM_ID_SIZE   sizeof (TBrIdRecord)        ///< Size of the ID record.

#define BR_EEPROM_CFG_BASE  0x0000 + BR_EEPROM_ID_SIZE  ///< Location of the configuration record in the EEPROM.
#define BR_EEPROM_CFG_SIZE  sizeof (TBrConfigRecord)    ///< Size of the configuration record.





// *************************** Brownie Data Records ****************************


/// @}
/// @name Version and Feature Record (VROM, Compile-Time) ...
/// @{


#define BR_MAGIC 0xb1    ///< Magic byte value to identify this device as a brownie.


/** @brief Brownie feature record (stored in VROM).
 *
 * This record describes the firmware version and available features.
 *
 * **Note:** The size of this structure must be equal to BR_VROM_SIZE and a multiple
 * of BR_MEM_BLOCKSIZE. This can be checked by compiling and running 'home2l-brownie2l'
 * (see entry of main() there).
 */
typedef struct SBrFeatureRecord {

  // Firmware version ...
  uint8_t   versionMajor;     ///< Version: major/minor/revision...
  uint8_t   versionMinor;
  uint16_t  versionRevision;

  // Feature presence ...
  uint16_t  features;         ///< Feature presence (see BR_FEATURE_... masks)

  uint16_t  gpiPresence;      ///< GPIO input presence mask (must be disjoint with output presence)
  uint16_t  gpiPullup;        ///< GPIO input pullup selection; bits for which the internal pullup is activated
  uint16_t  gpoPresence;      ///< GPIO output presence mask (must be disjoint with input presence)
  uint16_t  gpoPreset;        ///< GPIO output default state (will be set on init before Z-state is left)

  // MCU model ...
  uint8_t   mcuType;          ///< MCU type (see BR_MCU_... constants)

  // Written firmware name ...
  char      fwName[16];       ///< Written name of the firmware variant (base name of the .elf file)

  // Magic byte (for verification) ...
  uint8_t   magic;            ///< Brownie identification (always = BR_MAGIC)

} __attribute__((packed)) TBrFeatureRecord;



/// @}
/// @name ... Feature Bits ...
/// (for @ref SBrFeatureRecord.features)
/// @{

#define BR_FEATURE_MAINTENANCE 0x0001 ///< Is maintenance system (usually, everything else is ommitted/zero then)
#define BR_FEATURE_TIMER    0x0002
#define BR_FEATURE_NOTIFY   0x0004    ///< Does host notification (usually 0 for primary hubs and maintenance systems, else 1)
#define BR_FEATURE_TWIHUB   0x0008    ///< Is TWI hub
#define BR_FEATURE_MATRIX   0x0010    ///< Has a matrix sensor
#define BR_FEATURE_TEMP     0x0020    ///< Has temperature sensor (TSic 206/306 over ZACwire)
#define BR_FEATURE_ADC_0    0x0040    ///< Has ADC #0 (e.g. for analog temperatur)
#define BR_FEATURE_ADC_1    0x0080    ///< Has ADC #1 (e.g. for analog temperatur)
#define BR_FEATURE_SHADES_0 0x0100    ///< Has shades actuator #0
#define BR_FEATURE_SHADES_1 0x0200    ///< Has shades actuator #1
#define BR_FEATURE_MROWS    0x1c00    ///< Number of matrix rows - 1
#define BR_FEATURE_MROWS_SHIFT  (2+8)
#define BR_FEATURE_MCOLS    0xe000    ///< Number of matrix columns - 1
#define BR_FEATURE_MCOLS_SHIFT (5+8)



/// @}
/// @name ... MCU Type IDs ...
/// (for @ref SBrFeatureRecord.mcuType)
/// @{

#define BR_MCU_NONE       0
#define BR_MCU_ATTINY85   1
#define BR_MCU_ATTINY84   2
#define BR_MCU_ATTINY861  3





/// @}
/// @name Brownie ID (EEPROM, Run-Time Changable) ...
/// @{


/** @brief Brownie ID (stored in EEPROM).
 *
 * This is a null-terminated string containing a unique identifier for the brownie.
 * The size of this structure must be a multiple of @ref BR_MEM_BLOCKSIZE (presently 0x10).
 *
 * Note: The size of this structure must be equal to @ref BR_EEPROM_ID_SIZE and a multiple
 * of @ref BR_MEM_BLOCKSIZE. This can be checked by compiling and running 'home2l-brownie2l'
 * (see entry of main() there).
 */
typedef char TBrIdRecord[32];





/// @}
/// @name Configuration Record (EEPROM, Run-Time Changeable) ...
/// @{


/** @brief Brownie configuration record (stored in EEPROM).
 *
 * This record contains changeable configuration parameters.
 *
 * The configuration is stored in the EEPROM at address BR_EEPROM_CFG_BASE (see below).
 * On startup, an SRAM copy is made, accessible via 'brConfigRecord'. Hence, changes
 * to the EEPROM only take effect after the next reboot. This is particularly
 * relevant if the device address is changed.
 *
 * Note: The size of this structure must be equal to BR_EEPROM_CFG_SIZE and a multiple
 * of BR_MEM_BLOCKSIZE. This can be checked by compiling and running 'home2l-brownie2l'
 * (see entry of main() there).
 */
typedef struct SBrConfigRecord {
  uint8_t   adr;                  ///< Own TWI address
  uint8_t   magic;                ///< Identify as a *Brownie* (should always be BR_MAGIC)

  uint8_t   oscCal;               ///< Timer calibration: AVR's OSCCAL register (0xff = load factory default on boot)
  uint8_t   reserved1;

  int8_t    hubMaxAdr;            ///< @brief TWI hub subnet: Last address managed by this hub
                                  ///
                                  /// A TWI hub manages all addresses ranging from 'adr' to (including) 'hubMaxAdr'.
  uint8_t   hubSpeed;             ///< TWI master speed-down (1 ~= 100KHz; n ~= 100/n KHz)

  uint8_t   shadesDelayUp[2];     ///< Shades delay in ticks when starting to move up
  uint8_t   shadesDelayDown[2];   ///< Shades delay in ticks when starting to move down
  uint8_t   shadesSpeedUp[2];     ///< Shades motion up per tick
  uint8_t   shadesSpeedDown[2];   ///< Shades motion down per tick

  uint8_t   reserved[2];          ///< (Padding to fill up 16 bytes.)

} __attribute__((packed)) TBrConfigRecord;


/// @}


/// @}    // brownies_interface_memory





// *************************** Brownie Registers *******************************


/** @defgroup brownies_interface_registers *Brownie* Registers
 * @brief Reference documentation of the *Brownie* register map.
 *
 * For *Home2L Brownies*, the following macros decribe the organization of the
 * virtual registers as accessed by the 'regRead' and 'regWrite' operations.
 *
 * @{
 */



// ***** General Organization *****


/// @name General ...
/// @{
#define BR_REGISTERS         0x40   ///< Number of registers



// ***** Register Map *****


/// @}
/// @name Base registers ...
/// @{
#define BR_REG_CHANGED       0x00     ///< Change indicator register; Reading resets all bits.
#define   BR_CHANGED_CHILD     0x01   ///< -- [@ref BR_REG_CHANGED] (hubs only) any child has reported a change
#define   BR_CHANGED_GPIO      0x02   ///< -- [@ref BR_REG_CHANGED] any GPIO input changed
#define   BR_CHANGED_MATRIX    0x04   ///< -- [@ref BR_REG_CHANGED] any sensor matrix switch changed
#define   BR_CHANGED_TEMP      0x08   ///< -- [@ref BR_REG_CHANGED] temperature changed
#define   BR_CHANGED_SHADES    0x10   ///< -- [@ref BR_REG_CHANGED] state of any shades changed (actuator or button, not position)

#define BR_REG_GPIO_0        0x02     ///< GPIOs (0..7): One bit per GPIO
#define BR_REG_GPIO_1        0x03     ///< GPIOs (8..15, if present): One bit per GPIO

#define BR_REG_TICKS_LO      0x04     ///< Ticks timer (low byte)
#define BR_REG_TICKS_HI      0x05     ///< Ticks timer (high byte); low byte must be read first, reading low latches high

/// @}
/// @name Temperature registers ...
/// @{
#define BR_REG_TEMP_LO       0x06
#define BR_REG_TEMP_HI       0x07     ///< @brief temperature (little endian; reading low latches high).
  ///<
  /// Bits 12..1 contain the raw temperature value delivered by the TSIC206/306 device.
  /// Bit 0 is set if and only if the value ist valid. A value of 0x0000 indicates an unknown temperature.

/// @}
/// @name ADC registers ...
/// (NOT IMPLEMENTED YET)
/// @{
#define BR_REG_ADC_0_LO      0x08
#define BR_REG_ADC_0_HI      0x09     ///< ADC #0 (little endian; reading low latches high)

#define BR_REG_ADC_1_LO      0x0a
#define BR_REG_ADC_1_HI      0x0b     ///< ADC #1 (little endian; reading low latches high)

/// @}
/// @name Matrix registers ...
/// @{
#define BR_REG_MATRIX_0      0x10
#define BR_REG_MATRIX_1      0x11
#define BR_REG_MATRIX_2      0x12
#define BR_REG_MATRIX_3      0x13
#define BR_REG_MATRIX_4      0x14
#define BR_REG_MATRIX_5      0x15
#define BR_REG_MATRIX_6      0x16
#define BR_REG_MATRIX_7      0x17     ///< Sensor matrix: raw data (one byte per row, up to 8x8 = 64 bits).
#define BR_REG_MATRIX_EVENT  0x18     ///< @brief Sensor matrix: Next matrix event.
  ///<
  /// **Notes on matrix events:**
  ///
  /// Reading removes the oldest entry from the (internal) event queue,
  /// the overflow state is not left on read.
  ///
  /// Writing 0x80 clears the queue and overflow state.
  ///
  /// The event register allows to detect the precise order of events, which may be
  /// used to detect if a window is tilted or closed.
  ///
  /// This register is not synchronized with any of the raw matrix registers.
  /// Initialization/Recovery after overflow or read errors can be done as follows:
  ///   1. Clear event queue, then clear the BR_CHANGED_MATRIX bit.
  ///   2. Read matrix data.
  ///   3. Read out all pending events and apply to the master's representation of the matrix.
  /// From now, the master has an up-to-date representation, which can be kept
  /// up-to-date solely by reading events until an error occurs (read error or overflow).
  ///
  /// The cycle counter is incremented whenever the internal row counter wraps around.
  /// It helps to identify whether events have truly happend in the order the events are delivered
  /// by the queue. Two events e1 and e2 received in this order with ecycle values of c1/c2 and
  /// rows of r1/r2, respectively, are proven to have happened in this order, if
  ///
  ///    (c2 * \#rows + r2) - (c1 * \#rows + r1) >= \#rows
  ///
  /// If this condition is not met, the real order may be different due to sampling inaccuracies.
  /// If this is unwanted, the sampling frequency should be increased (MATRIX_T_PERIOD).
  ///
#define   BR_MATRIX_EV_VAL_SHIFT  6   ///< -- [@ref BR_REG_MATRIX_EVENT] Bit  6 = value
#define   BR_MATRIX_EV_ROW_SHIFT  3   ///< -- [@ref BR_REG_MATRIX_EVENT] Bits 5..3 = row
#define   BR_MATRIX_EV_COL_SHIFT  0   ///< -- [@ref BR_REG_MATRIX_EVENT] Bits 2..0 = col
#define   BR_MATRIX_EV_EMPTY    0x80  ///< -- [@ref BR_REG_MATRIX_EVENT] Special value: event queue empty
#define   BR_MATRIX_EV_OVERFLOW 0x81  ///< -- [@ref BR_REG_MATRIX_EVENT] Special value: overflow

#define BR_REG_MATRIX_ECYCLE 0x19     ///< Cycle counter of last read matrix event


/// @}
/// @name Shades registers ...
/// @{
#define BR_REG_SHADES_STATUS 0x20     ///< @brief Shades status register
  ///<
  /// **Notes on shades control:**
  ///
  /// - The status bits are read-only and provide direct access to the actuator and buttons
  ///
  /// - The *internal* request (RINT) is set ...
  ///
  ///     - ... to 0 (up) or 100 (down) if the respective button is pushed and the actuator is off
  ///
  ///     - ... to some value between 0 and 100 if any button is pushed and the actuator is on in any direction
  ///           (usually to stop moving);
  ///
  ///     - ... from outside to 0xff (but no other value)
  ///
  /// - The *external* request (REXT) is set from outside only.
  ///
  /// - REXT has strict priority over RINT. If both are 0xff, the shades are stopped.
  ///
  /// - ACT_UP/ACT_DN: If both are up, the motor has recently been stopped and is effectively off now.
  ///
  /// - Safety behavior:
  ///      If for a certain time (SHADES_TIMEOUT) no sign of life is received from the master,
  ///      REXT is cleared to 0xff, and RINT set to a pre-configured value (BR_SHADES_n_RINT_FAILSAFE).
  ///      Both values are compiled in (see config.h) to make sure they are never changed.
  ///      A sign of life can be: Reading BR_REG_CHANGE or BR_REG_SHADES_STATUS, writing to any request register.
  ///
  /// - For the Resources driver:
  ///    - RINT can be read back any time / permanently to maintain a synthetic request representing the
  ///      user behavior (or delete that request if RINT == 0xff).
  ///    - Driving a value = writing it to REXT
  ///    - Whenever 0xff is written to REXT, RINT should be cleared to 0xff before (to avoid unexpected starting).
  ///    - To be reported: value = POS, state "busy" if (ACT_UP | ACT_DN), state "unkown" if POS = 0xff.
  ///
  ///    - *NOTE:* It may happen that the final position reported by POS may be close, but different from
  ///            the effectively requested position due to inaccuracies.
  ///            a) This may change in the future.
  ///            b) The driver should implement some logic to round the reported position to the original driven
  ///               one if the state is "valid" (ACT_UP == ACT_DN == 0) and both values only differ slightly.
  ///
#define   BR_SHADES_0_ACT_UP   0x01   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #0 actor is currently moving up
#define   BR_SHADES_0_ACT_DN   0x02   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #0 actor is currently moving down
#define   BR_SHADES_0_BTN_UP   0x04   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #0 up button is pushed (= button down after debouncing)
#define   BR_SHADES_0_BTN_DN   0x08   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #0 down button is pushed (= button down after debouncing)
#define   BR_SHADES_1_ACT_UP   0x10   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #1 actor is currently moving up
#define   BR_SHADES_1_ACT_DN   0x20   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #1 actor is currently moving down
#define   BR_SHADES_1_BTN_UP   0x40   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #1 up button is pushed (= button down after debouncing)
#define   BR_SHADES_1_BTN_DN   0x80   ///< -- [@ref BR_REG_SHADES_STATUS] Shades #1 down button is pushed (= button down after debouncing)

#define BR_REG_SHADES_0_POS  0x22     ///< Shades #0: Current position (0..100);  0xff = "unknown"
#define BR_REG_SHADES_0_RINT 0x23     ///< Shades #0: Internal request (0..100 or 0xff = "none")
#define BR_REG_SHADES_0_REXT 0x24     ///< Shades #0: External request (0..100 or 0xff = "none")
#define BR_REG_SHADES_1_POS  0x25     ///< Shades #1: Current position (0..100);  0xff = "unknown"
#define BR_REG_SHADES_1_RINT 0x26     ///< Shades #1: Internal request (0..100 or 0xff = "none")
#define BR_REG_SHADES_1_REXT 0x27     ///< Shades #1: External request (0..100 or 0xff = "none")



/// @}
/// @name System control registers ...
/// @{
#define BR_REG_FWBASE        0x3d      ///< @brief Firmware base and boot vector location.
  ///<
  /// This is the base address of the active firmware in units of @ref BR_FLASH_PAGESIZE bytes
  /// (64/0x40 on ATtiny84, ATtiny861).
  /// Writing to this register followed by writing @ref BR_CTRL_REBOOT_NEWFW to @ref BR_REG_CTRL
  /// causes the interrupt table to be rewritten and the CPU reset.
  /// This register is initialized with the actual firmware base; reading it allows to locate the running
  /// firmware.
#define BR_REG_CTRL          0x3e      ///< Control register.
#define   BR_CTRL_UNLOCK_EEPROM     0x01  ///< -- [@ref BR_REG_CTRL] Setting this bit unlocks EEPROM for writing
#define   BR_CTRL_UNLOCK_FLASH      0x02  ///< -- [@ref BR_REG_CTRL] Setting this bit unlocks flash memory and SRAM
#define   BR_CTRL_HUB_RESURRECTION  0x04  ///< -- [@ref BR_REG_CTRL] Setting this bit puts TWI hub into resurrection mode

#define   BR_CTRL_REBOOT            0xe0  ///< -- [@ref BR_REG_CTRL] Writing this value lets the device reboot
#define   BR_CTRL_REBOOT_NEWFW      0xa0  ///< -- [@ref BR_REG_CTRL] Writing this value changes the interrupt table according to BR_REG_FWBASE and reboots the device

#define BR_REG_MAGIC         0x3f    ///< Magic value (returns @ref BR_MAGIC after reset)


/// @}    // name
/// @}    // brownies_interface_registers


/// @}    // brownies_interface


#endif // _INTERFACE_
