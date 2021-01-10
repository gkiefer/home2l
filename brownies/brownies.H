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


#ifndef _BROWNIES_
#define _BROWNIES_

#include "env.H"
#include "resources.H"


extern "C" {
#include "avr/interface.h"
}


/** @file
 *
 * @defgroup brownies Brownies
 * @brief The *Home2L Brownies* library.
 * @{
 *
 * @defgroup brownies_linux Host Library
 * @brief Linux host library to use and communicate with *Brownies*.
 *
 * @defgroup brownies_interface Interface
 * @brief Specification of the microcontroller-host interface: two-wire bus protocol, register map, common constants etc. .
 *
 * @defgroup brownies_features Microcontroller: Feature Selection
 * @brief Microcontroller firmware feature options.
 *
 * @defgroup brownies_pins Microcontroller: Pin Assignments
 * @brief Definitions for pin assignments.
 *
 * @defgroup brownies_firmware Microcontroller: API
 * @brief Microcontroller firmware basic modules, relevant for implementing new features.
 *
 *
 *
 * @addtogroup brownies_linux
 *
 * This module contains all functionality to access *Home2L Brownies* from
 * Linux hosts. This includes the *Resources* driver functionality.
 *
 * Notes
 * -----
 *
 * ### Threading
 *
 * Unless specified otherwise, the methods in this module are *not* thread-safe.
 * All actions related to certain a @ref CBrownieSet or a @ref CBrownieLink object
 * or a combination thereof must originate from the same thread (called the
 * "Brownie thread"), which does not necessarily have to be main UI thread.
 *
 * Timers (@ref CTimer) are not used in this module.
 *
 * @{
 */





// *************************** Basics ******************************************


#define BR_CONF_DEFAULT_NAME "brownies.conf"


extern const char *envBrDatabaseFile;   ///< @brief Name of the selected database file (read-only).
extern const char *envBrLinkDev;        ///< @brief Name of the selected link device (read-only).


const char *BrStatusStr (EBrStatus s);
  ///< @brief Get a readable string for Brownie status codes.

const char *BrMcuStr (int mcuType);
  ///< @brief Get a readable string (for example, "t84") for an MCU model ID (see BR_MCU_* macros).
  /// On error, 'NULL' is returned.
int BrMcuFromStr (const char *mcuStr);
  ///< @brief Get the MCU model ID (see BR_MCU_* macros) from a string (for example, "t84").
  /// On error, @ref BR_MCU_NONE is returned.





// ***************** Brownie Feature and Config Records ************************


// ***** Configuration item descriptor *****


/// @internal Config item type
typedef enum {
  ctUint8,
  ctInt8,
  ctUint16,
  ctVersion,      // special: Version number (stored in TBrFeatureRecord.version*)
  ctFeatures,     // special: Feature information relevant for resources
  ctMcu,          // special: MCU model
  ctFw,           // special: Firmware name (stored in TBrFeatureRecord.fwName)
  ctId,           // special: ID (stored in EEPROM at BR_EEPROM_ID_BASE)
  ctShadesDelay,  // special: Shades delay (byte displayed as seconds)
  ctShadesSpeed   // special: Shades speed (byte displayed as seconds of 100% movement)
} EBrCfgType;


/// @internal Config item descriptor
struct TBrCfgDescriptor {
  const char *key;      // written key
  const char *fmt;      // format for output
  EBrCfgType type;
  int features;         // any of these features must be present, otherwise the variable is ignored
  int ofs;              // offset inside the TBrConfigRecord structure
  const char *help;
};


extern const TBrCfgDescriptor brCfgDescList[];  ///< @internal
extern const int brCfgDescs;                    ///< @internal



// ***** Feature record to/from string *****

uint32_t BrVersionGet (TBrFeatureRecord *featureRecord);
  ///< @brief Retrieve version from feature record.
static inline const char *BrVersionGetAsStr (CString *ret, TBrFeatureRecord *featureRecord) { return VersionToStr (ret, BrVersionGet (featureRecord)); }
  ///< @brief Get a readable string of the version fields of the feature record.
  /// The result is written to '*ret' and returned.
bool BrVersionFromStr (TBrFeatureRecord *featureRecord, const char *str);
  ///< @brief Set the version fields of the feature record according to the given version string.

const char *BrFeaturesToStr (CString *ret, TBrFeatureRecord *featureRecord);
  ///< @brief Get a readable string of the feature-related fields of the feature record.
  /// The result is written to '*ret' and returned.
bool BrFeaturesFromStr (TBrFeatureRecord *featureRecord, const char *str);
  ///< @brief Set the feature-related fields of the feature record according to the given string.





// *************************** CBrownie ****************************************


/** @brief Representation of a *Brownie* device
 */
class CBrownie {
  public:

    /// @name Initialization / setting / getting ...
    /// @{

    CBrownie () { features = 0; Clear (); }
    ~CBrownie () { Clear (); }

    void Clear ();

    void SetId (const char *id) { strncpy (idRecord, id, sizeof (idRecord)); idRecord[sizeof (idRecord) - 1] = '\0'; }
    void SetFeatureRecord (TBrFeatureRecord *_featureRecord) { featureRecord = *_featureRecord; }
    void SetConfigRecord (TBrConfigRecord *_configRecord) { configRecord = *_configRecord; }
    void SetDatabaseString (const char *s) { databaseString.Set (s); }

    bool SetFromStr (const char *str, CString *ret = NULL);
      ///< @brief Set fields in the feature record or config record according to a text line in database string syntax.
      ///
      /// If 'ret' is != NULL, a string showing the values of all options mentioned in 'str'
      /// is returned. It is allowed to pass option names without assignment in 'str' in
      /// order to obtatin the values this way.
      /// On error, a warning is emitted and 'false' is returned.
    const char *ToStr (CString *ret, bool withIdentification = true, bool withVersionInfo = true);
      ///< @brief Return the contents of the feature record and the config record as a string in the database syntax.
    const char *ToStr (bool withIdentification = true, bool withVersionInfo = true) { return ToStr (GetTTS (), withIdentification, withVersionInfo); }
      ///< @brief (TTS) Return the contents of the feature record and the config record as a string in the database syntax.

    /// @}
    /// @name Accessing fields ...
    /// @{

    int Adr () { return configRecord.adr; }
      ///< @brief Get the TWI address of the *Brownie*.
    const char *Id () { return idRecord; }
      ///< @brief Get the ID record (a null-terminated string).
    TBrFeatureRecord *FeatureRecord () { return &featureRecord; }
      ///< @brief Get the feature record.
    TBrConfigRecord *ConfigRecord () { return &configRecord; }
      ///< @brief Get the config record.

    const char *DatabaseString () { return databaseString.Get (); }
      ///< @brief Get options set in the database file as a string.

    /// @}
    /// @name Validity status ...
    /// @{

    bool IsValid () { return configRecord.adr != 0; }
      ///< @brief Data is generally valid.
    bool HasFeatures () { return featureRecord.features || featureRecord.gpiPresence || featureRecord.gpoPresence; }
      ///< @brief Feature record appears to be valid (either set from database or from device).

    bool HasDeviceFeatures () { return featureRecord.magic == BR_MAGIC; }
      ///< @brief Feature record is valid and comes from the device.
    bool HasDeviceConfig () { return configRecord.magic == BR_MAGIC; }
      ///< @brief Config record is valid and comes from the device.

    /// @}
    /// @name Compatibility and device checking ...
    /// @{

    bool IsCompatible (const char *_databaseString);
      ///< @brief Check if the current object, typically read back from a device,
      /// is compatible to the passed database string.

    bool UpdateFromDevice (class CBrownieLink *link);
      ///< @brief Read out device, check for compatibility, and update the feature record and config record on success.
      /// @param link is the link, the address must be set appropriately before.
      /// @return true on success; on failure, link->Status() may be called to determine if it was a link
      ///   or a compatibility failure, in which case the link status would be brOk.

     /// @}

  protected:
    const char *GetOptValue (int optIdx, CString *ret);
    bool SetOptValue (int optIdx, const char *str);
    void CheckDeviceForResources (CBrownieLink *link);

    // Database and hardware info ...
    TBrIdRecord idRecord;
    TBrFeatureRecord featureRecord;
    TBrConfigRecord configRecord;
    CString databaseString;

    // Resources (with interface calls for @ref CBrownieSet) ...
    friend class CBrownieSet;
    friend class CBrRcDriver;

    class CBrFeature *featureList[8]; // array of device feature objects (use 'sizeof(featureList)' to avoid buffer overflows)
    int features;                     // number of device features
    bool deviceChecked;               // device has been checked, either with or without success:
                                      // if true, but HasDeviceFeatures() or HasDeviceConfig() returns false, it should not be checked again but treated as unusable
    bool unknownChanges;              // There was a failure reading the "changed" register, we may have missed changes

    void RegisterAllResources (class CRcDriver *rcDriver, class CBrownieLink *link = NULL);
      // Register all resources and create device feature objects;
      // This is done based on the database, However, later, on the first
      // invocation of 'Iterate()', the features of the real hardware should be validated against
      // this for the case the database is out of date.
      // If 'link != NULL' and the 'features' configuration option is not set in the database,
      // the device is checked immediately.
      // If 'link == NULL', the 'features' configuration option must be set in the database,
      // otherwise, no resources will be registered for the device.
    unsigned Iterate (class CBrownieLink *link, bool fast);
      // Check the "changed" register BR_REG_CHANGED and do all necessary actions.
      //
      // 'link' may be 'NULL', in which case time-outs should be checked and resources
      // be invalidated accordingly.
      //
      // If 'fast' is 'true', a notification was received for this
      // subnet. Hence, we must only read registers of time-critical features
      // (= those with a set "changed" bit) to save time in favour of some other
      // node that caused the notification.
      //
      // Returns the contents of register BR_REG_CHANGED (presently only
      // relevant for hubs and their BR_CHANGED_CHILD bit, other may return 0).
      //
    void CheckExpiration ();
      // Invalidate all expireable resources on expiration
    void DriveValue (class CBrownieLink *link, class CResource *rc, class CRcValueState *vs);
      // Unlike CBrownieSet::DriveValue(), this method is always called from the
      // Brownie thread, so no care of concurrency is necessary.
};





// *************************** CBrownieSet *************************************


/** @brief Set of *Brownies*
 */
class CBrownieSet {
  public:

    /// @name Initialization / setting / getting ...
    /// @{

    CBrownieSet ();
    ~CBrownieSet () { ResourcesDone (); Clear (); }

    void Clear ();

    CBrownie *Get (const char *id) { int *pAdr = adrMap.Get (id); return pAdr ? Get (*pAdr) : NULL; }
    CBrownie *Get (int adr) { return (adr < 0 || adr > 127) ? NULL : brList[adr]; }
      ///< @brief Get a reference to the brownie for reading.
      /// To modify a brownie object, it must be first removed and then set again.

    void Set (CBrownie *brownie);   ///< @brief Add/Change brownie (takes ownership of @ref CBrownie object).
    CBrownie *Unlink (int adr);     ///< @brief Forget brownie and return it (caller becomes owner of @ref CBrownie object).

    void Del (int adr);             ///< @brief Delete database entry.

    /// @}
    /// @name Database file ...
    /// @{

    bool ReadDatabase (const char *fileName = NULL);
      ///< @brief Read a database file.
      /// @param fileName is the name of the database file relative to the etc domain (default: use 'br.config')
      ///
    bool WriteDatabase (const char *fileName = NULL);
      ///< @brief Write the set as a database file.
      /// @param fileName is the name of the database file relative to PWD (default: use stdout)
      ///

    /// @}
    /// @name Resources ...
    /// These methods are for use by the *Brownie* resource driver.
    /// @{

    void ResourcesInit (CRcEventDriver *_rcDriver, class CBrownieLink *_rcLink);
      ///< @brief Register all resources and assign a driver and link to the Brownie set.
      ///
      /// @param _rcDriver is an @ref CRcDriver object which must have been initialized before,
      ///    usually with the driver name "brownies". This driver will be called to update resource changes,
      ///    and its DriveValue() method should immediately call CBrownieSet::DriveValue().
      /// @param _rcLink is the link to be used for accessing the devices.
      ///
    void ResourcesIterate (bool noLink = false, bool noSleep = false);
      ///< @brief Query *Brownies* and report any pending resource changes.
      ///
      /// In detail, the following actions are typically performed:
      /// - Eventually sleep until 'br.minScanInterval' time has passed since the last invocation.
      /// - Query the "changed" registers of *all* primary hubs (fast mode).
      /// - Eventually pursue notifications from subnets (fast mode).
      /// - Iterate/update one or a few node completely (usually non-hub, but also hub).
      ///
      /// @param noLink prohibits the use of the TWI link, to be used in maintenance mode
      ///   to let the resources expire eventually.
      /// @param noSleep prohibits sleeping, even if less than 'br.minScanInterval' ticks
      ///   have passed since the last invocation
      ///
    void ResourcesDone ();
      ///< @brief Forget about the driver / link and clean up all resources-related data.
      ///
      /// This must be called from the drivers 'rcdStop' operation or after the driver was stopped.
      /// It is not allowed to call ResourcesDriverValue() or ResourcesIterate() after this.
      ///

    /// @}

  protected:
    CBrownie *brList[128];
    CDictFast<int> adrMap;

    // Resources ...
    class CRcEventDriver *rcDriver;
    class CBrownieLink *rcLink;
    int rcLastCheckedAdr;         // next address to be polled normally (not fast)
    TTicksMonotonic tLastIterate; // time of last entry of 'ResourcesIterate ()'
};





// *************************** CBrownieLink ***********************************


/// @brief Brownie link interface type
typedef enum {
  ifNone = 0,     ///< (None)
  ifSocket,       ///< Unix domain socket (to connect to already running Brownie driver instance on the local machine)
  ifI2cDev,       ///< Linux i2c dev
  ifElvI2c        ///< ELV USB-i2c
} ETwiIfType;


const char *TwiIfTypeStr (ETwiIfType type);


/// @internal Socket operation (for maintenance socket communication)
enum ESocketOp {
  soSend = 0,     // request: head + data (<bytes>),  reply: head
  soFetch,        // request: head,                   reply: head + data <bytes>
  soStatReset,    // request: head,   no reply
  soStatFetch     // request: head,                   reply: head + string without \0 <bytes>
};


/// @internal Header for maintenance socket communication
struct TSocketHeader {
  ESocketOp op      : 3;    // Socket operation
  EBrStatus status  : 5;    // Status (for 'soSend', 'soFetch')
  int adr           : 8;    // TWI address (for 'soSend', 'soFetch')
  int bytes         : 16;   // amount of the remaining data (exception: 'soFetch' request, where this is the desired reply bytes)
};


/** @brief *Brownie* communication (TWI) link
 */
class CBrownieLink {
  public:

    /// @name Initialization, open and close ...
    /// @{

    CBrownieLink ();
    ~CBrownieLink () { Close (); }

    EBrStatus Open (const char *devName = NULL);
      ///< @brief Open link.
      /// @param devName is the device identifier or NULL to select the default defined by 'br.link'.
      /// @return success/error status (also stored in 'status').
    EBrStatus Reopen () { TwiOpen (false); return status; }
      ///< @brief Reopen a previously open link.
    void Close () { ServerStop (); TwiClose (); }

    const char *IfName () { return twiIfName.Get (); }
      ///< @brief Get the current interface file name.
    ETwiIfType IfType () { return twiIfType; }
      ///< @brief Get the current interface type.

    /// @}
    /// @name Accessing buffers and last status ...
    /// @{

    TBrRequest *Request () { return &request; }   ///< @brief Get pointer to the request buffer (e.g. for filling it).
    TBrReply *Reply () { return &reply; }         ///< @brief Get pointer to the reply buffer (e.g. for reading it).
    EBrStatus Status () { return status; }        ///< @brief Get the status of the last operation.

    /// @}
    /// @name Communication primitives ...
    ///
    /// For simple use, only Communicate() is needed.
    /// SendRequest() and FetchReply() may be used for split transactions with multiple brownies.
    /// Flush() and ClearBus() may be used for error maintenance to obtain a clear bus/brownie state.
    ///
    /// @{

    void ClearBus ();
      ///< @brief Issue bus clock pulses to resolve potential bus locking
    void Flush (int adr);
      ///< @brief (Try to) Read and discard a reply to make sure none is pending

    EBrStatus SendRequest (int adr, bool noResend = false);
      ///< @brief Send a request & perform possible error correction detected during transmission.
      ///
      /// The request must be copied before to the buffer returned by Request().
      /// Changes the last status field according to the return value.
      /// @param adr is the TWI address.
      /// @param noResend disables the resending of the request for error corrections.
      ///   This is required for reading registers, which are auto-modified by reading
      ///   (for example, BR_REG_CHANGED or BR_REG_MATRIX_EVENT).
      /// @return the error/success status.

    EBrStatus FetchReply (int adr, bool noResend = false);
      ///< @brief Fetch a reply & perform possible error correction.
      ///
      /// In case of an error, SendRequest() is called again since the brownie may have discarded
      /// the original request. Hence, the request must not be changed between calling
      /// SendRequest() and FetchReply().
      /// Changes the last status field according to the return value.
      /// On success, the reply can be fetched from the buffer returned by Reply().
      /// @param adr is the TWI address.
      /// @param noResend disables the resending of the request for error corrections.
      ///   This is required for reading registers, which are auto-modified by reading
      ///   (for example, BR_REG_CHANGED or BR_REG_MATRIX_EVENT).
      /// @return the error/success status.

    EBrStatus Communicate (int adr, bool noResend = false);
      ///< @brief Perform a complete communication cycle including possible error correction.
      ///
      /// Changes the last status field accordingly to the return value.
      /// @param adr is the TWI address.
      /// @param noResend disables the resending of the request for error corrections.
      ///   This is required for reading registers, which are auto-modified by reading
      ///   (for example, BR_REG_CHANGED or BR_REG_MATRIX_EVENT).
      /// @return the error/success status.

    /// @}
    /// @name Operations ...
    /// @{

    EBrStatus CheckDevice (int adr, CBrownie *brownie = NULL);
      ///< @brief Check the device for reachability and (optionally) return its feature and configuration records.
      /// @param adr is the device's TWI address.
      /// @param brownie is the object into which the ID, the feature, and the configuration record are read.
      ///   If set to 'NULL', the feature record and firmware version are not checked, no records are returned,
      ///   and the communication is much simpler and faster.

    EBrStatus RegRead (int adr, uint8_t reg, uint8_t *retVal, bool noResend = false);
    EBrStatus RegRead (CBrownie *brownie, uint8_t reg, uint8_t *retVal, bool noResend = false) { return RegRead (brownie->Adr (), reg, retVal, noResend); }
      ///< @brief Read a register.
    uint8_t RegReadNext (EBrStatus *status, int adr, uint8_t reg, bool noResend = false);
    uint8_t RegReadNext (EBrStatus *status, CBrownie *brownie, uint8_t reg, bool noResend = false) { return RegReadNext (status, brownie->Adr (), reg, noResend); }
      ///< @brief Read a register (alternative arguments).
      ///
      /// These methods are optimized for fetching multiple registers. The register contents are returned
      /// by the return value, and the status field is updated on the fly. If '*status' is not 'brOk'
      /// on method entry, nothing is done. This allows to read multiple registers without checking the
      /// status after each single one.
    EBrStatus RegWrite (int adr, uint8_t reg, uint8_t val, bool noResend = false);
    EBrStatus RegWrite (CBrownie *brownie, uint8_t reg, uint8_t val, bool noResend = false) { return RegWrite (brownie->Adr (), reg, val, noResend); }
      ///< @brief Write to a register.
    void RegWriteNext (EBrStatus *status, int adr, uint8_t reg, uint8_t val, bool noResend = false);
    void RegWriteNext (EBrStatus *status, CBrownie *brownie, uint8_t reg, uint8_t val, bool noResend = false) { return RegWriteNext (status, brownie->Adr (), reg, val, noResend); }
      ///< @brief Write a register (alternative arguments, see comment on RegReadNext()).

    EBrStatus MemRead (int adr, unsigned memAdr, int bytes, uint8_t *retData, bool printProgress = false);
      ///< @brief Read from memory.
    EBrStatus MemWrite (int adr, unsigned memAdr, int bytes, uint8_t *data, bool printProgress = false);
      ///< @brief Write to memory.

    /// @}
    /// @name Statistics ...
    /// @{

    void StatisticsReset (bool local = false);
      ///< @brief Reset all statistics counters.
      /// @param local is only relevant if the link type is @ref ifSocket.
      ///   If set to 'true', the statistics of the local socket connection are selected,
      ///   otherwise (by default), the statistics of the remote server are selected.
    const char *StatisticsStr (CString *ret, bool local = false);
      ///< @brief Get the statistics as a readable string.
      /// @param ret is the destination buffer.
      /// @param local is only relevant if the link type is @ref ifSocket.
      ///   If set to 'true', the statistics of the local socket connection are selected,
      ///   otherwise (by default), the statistics of the remote server are selected.
    void StatisticsAddIterateTimes (TTicksMonotonic tCycle, TTicksMonotonic tFastPoll, TTicksMonotonic tSlowPoll);
      ///< @brief @private Add times of an iteration cycle to the statistics (for CBrownieSet::Iterate() )

    /// @}
    /// @name Maintenance socket server ...
    /// @{

    bool ServerStart ();
      ///< @brief Start the socket server, if configured by 'br.serveSocket'.
      ///
      /// If none is configured, this method will fail silently.
      /// Otherwise, a warning is logged on failure.
      /// @return success status.
    void ServerStop ();
      ///< @brief Stop the socket server (if it was running).
    bool ServerIterate (TTicksMonotonic maxSleepTime = -1);
      ///< @brief Perform all socket server services (if enabled).
      /// @param maxSleepTime is the maximum time to sleep if a client is connected
      /// (<0 = wait unlimited until the connection proceeds).
      /// If no client is connected, the method never sleeps.
      /// @return 'true' if some client is connected and using this link.
      /// In this case, the link should not be used locally until 'false' is returned
      /// by this method again. Otherwise, 'brNoBus' is returned from all

    /// @}

  protected:
    void TwiOpen (bool warn);
    void TwiClose ();
    EBrStatus TwiSetAdr (int _twiAdr);    // Helper for TwiSend() and TwiFetch()
    EBrStatus TwiSend (int adr, const void *buf, int bytes);
    EBrStatus TwiFetch (int adr, void *buf, int bytes);

    CString twiIfName;
    ETwiIfType twiIfType;
    int twiFd, twiAdr;

    TBrRequest request;
    TBrReply reply;
    EBrStatus status;

    // Statistics ...
    TTicks tLastStatisticsReset;
    int requests, requestRetries[brEND], requestFailures[brEND];
    int replies, replyRetries[brEND], replyFailures[brEND];

    int rcIterations;
    TTicks rcTSumCycle, rcTSumFastPoll, rcTSumSlowPoll;
    TTicksMonotonic rcTCycleMin, rcTCycleMax, rcTFastPollMin, rcTFastPollMax, rcTSlowPollMin, rcTSlowPollMax;

    // Socket server ...
    int sockListenFd;       // listening socket (or <0, if none activated)
    int sockClientFd;       // currently connected socket client (or <0, if none is connected)
    TSocketHeader sockHead; // buffer for received header
    uint8_t *sockData;      // buffer for received data
    size_t sockRxBytes;     // number of received bytes (header + data)
};



/// @}  // brownies_linux
/// @}  // brownies


#endif // _BROWNIES_
