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

#include "resources.H"

#include <errno.h>


#define LINK_LOST ((TTicks) NEVER + 1)    // constant to mark 'tNoLink' as "link lost permanently"





// *************************** Environment Parameters **************************


ENV_PARA_SPECIAL("enocean.device.<ID>", const char *, NULL);
  /* Define an \textit{EnOcean} device
   *
   * The string has the general syntax:
   * \begin{center} \small
   * \texttt{<profile>[:<device ID>][:<args>]}
   * \end{center}
   * Where:
   * \begin{description}
   *   \item[\texttt{<profile>}] is the three-part equipment profile ID as mentioned in the
   *       \textit{EnOcean Equipment Profiles} manual (for example, ''F6-10-00'' identifies
   *       a mechanical window handle).
   *   \item[\texttt{<device ID>}] is the 4-byte device ID given by 8 hexadecimal digits.
   *       If not set, \texttt{<ID>} is used.
   *   \item[\texttt{<args>}] are optional device-specific arguments.
   * \end{description}
   *
   * The \texttt{<ID>} part of the key can be chosen arbitrarily (or must match the device ID
   * if \texttt{<device ID>} is missing.
   *
   * The resource LID(s) will be derived from \texttt{<ID>}.
   */


ENV_PARA_INT ("enocean.maxAge", envEnoLinkMaxAge, 15);
  /* Maximum time in minutes before the unavailabilty of the link is reported
   *
   * If the EnOcean link device has failed for this time, all resources are invalidated
   * and their state is set to \refapic{rcsUnknown}.
   *
   * \textit{Note:} By their construction, it is impossible to detect the failure or
   * absence of an energy harvesting sensor within a specific time. The reason is that
   * such devices only send telegrams on events (such as button pushes or window handle
   * movement) and remain silent at other times.
   *
   * Hence, the \textit{Home2L Resources} convention that failures are reported actively
   * cannot be guaranteed here. Only the failure of the link device can and is reported
   * actively. However, to avoid potentially annoying invalidations if, for example,
   * the USB stick is replugged, it is reasonable to enter a longer
   * time period (longer than \refenv{rc.maxAge}) here.
   */


ENV_PARA_STRING ("enocean.windowHandle.init", envEnoWindowHandleInit, NULL);
  /* Initialization state for window handles
   *
   * This defines the initialization state of window handle devices when the
   * driver is initialized. Possible values are those of the 'rctWindowState'
   * resource type ("closed", "tilted", "open"). By default, the resource is
   * initialized as "unkown". If this option is set, the respective value is set.
   *
   * By construction, energy harvesting devices submit their state only when
   * they are moved/used. Since they cannot be queried for their states, so that
   * their resources must be initialized as "unkown" on initialitation to be correct.
   * This option allows to set them to a specific value instead.
   */





// *************************** Global Variables ********************************


static CRcDriver *enoRcDrv = NULL;





// *************************** Equipment Driver Management *********************


// ***** Profile helpers *****


static const char *ProfileToStr (uint32_t profile) {
  // return value valid until the next call of this function
  static char buf[9];
  snprintf (buf, sizeof (buf), "%02X-%02X-%02X",
            (int) ((profile >> 16) & 0xff), (int) ((profile >> 8) & 0xff), (int) (profile & 0xff));
  buf[sizeof(buf)-1] = '\0';  // sanity
  return buf;
}


static uint32_t ProfileFromStr (const char *str) {
  // returns 0 on error
  uint32_t rorg, func, type;

  if (sscanf (str, "%x-%x-%x", &rorg, &func, &type) != 3) return 0;
  if (rorg > 0xff || func > 0xff || type > 0xff) return 0;
  return (rorg << 16) | (func << 8) | type;
}





// ***** Equipment device base class (abstract) *****


class CEnoDevice {
  public:
    CEnoDevice (const char *_id, class CEnoDeviceClass *_deviceClass, uint32_t _deviceId);
    ~CEnoDevice () { Done (); }

    /// @name Virtual callbacks ...
    /// These methods must be overloaded by drivers.
    /// @{
    virtual void Init (const char *arg = NULL) = 0;
      ///< @brief Initialization. This method ...
      ///
      /// * must create and register all resources
      /// * may parse optional arguments passed as 'arg'
      ///
    virtual void Done () {}
      ///< @brief Cleanup.
      ///
      /// This method must clean up all private data structures.
      /// It is not necessary to unregister resources.
      ///
    virtual void OnLinkLost () = 0;
      ///< @brief Called whenever the link device has failed.
      ///
      /// This method must invoke invalidate all resources, usually by calling
      /// @ref CResource::ReportUnknown() for them.
      ///
    virtual void OnTelegram (CEnoTelegram *telegram) = 0;
      ///< @brief Called whenever a telegram for this device is received.
      ///
      /// This method must handle passed telegram and report resources values
      /// appropriately.
      ///
      /// The telegram integrity, the device ID and the correctness of the RORG value
      /// have been checked in advance and do not need to be checked here.
      /// On any other error, a warning should be logged and the telegram should be ignored.
      ///
    /// @}

    // Helpers ...
    /// @name Helpers for drivers ...
    /// @{
    static CRcDriver *RcDriver () { return enoRcDrv; }
      ///< @brief Get the Resource driver to register resources

    // Access by management ...
    const char *Id () { return id.Get (); }
    CEnoDeviceClass *Class () { return deviceClass; }
    uint32_t DeviceId () { return deviceId; }

  protected:
    CString id;
    class CEnoDeviceClass *deviceClass;
    uint32_t deviceId;
};


CEnoDevice::CEnoDevice (const char *_id, class CEnoDeviceClass *_deviceClass, uint32_t _deviceId) {
  id.Set (_id);
  deviceClass = _deviceClass;
  deviceId = _deviceId;
}



// ***** Equipment device class *****


typedef class CEnoDevice * (FEnoNewDevice) (const char *id, class CEnoDeviceClass *deviceClass, uint32_t deviceId);


class CEnoDeviceClass {
  public:
    CEnoDeviceClass (uint32_t _profile, FEnoNewDevice *_fNewDevice);

    // Accessing fields ...
    uint32_t Profile () { return profile; }
    uint8_t ProfileRorg () { return (uint8_t) (profile >> 16); }
    uint8_t ProfileFunc () { return (uint8_t) (profile >> 8); }
    uint8_t ProfileType () { return (uint8_t) profile; }

    // Create device object ...
    static CEnoDevice *NewDevice (uint32_t _profile, const char *id, uint32_t deviceId, const char *arg = NULL);
      ///< @brief Create a new device object.
      /// @param _profile is the desired profile.
      /// @param id is the local ID passed to the constructor.
      /// @param deviceId is the EnOcean device ID.
      /// @param arg are optional, device-specific arguments.
      /// @return new object (caller becomes owner) or NULL on error.
      /// In case of an error, a warning is emitted.

  protected:
    uint32_t profile;
    FEnoNewDevice *fNewDevice;

    // List management ...
    static class CEnoDeviceClass *first, **pLast;
    class CEnoDeviceClass *next;
};


CEnoDeviceClass *CEnoDeviceClass::first = NULL;
CEnoDeviceClass **CEnoDeviceClass::pLast = &CEnoDeviceClass::first;


CEnoDeviceClass::CEnoDeviceClass (uint32_t _profile, FEnoNewDevice *_fNewDevice) {
  profile = _profile;
  fNewDevice = _fNewDevice;

  // Append to the class list ...
  next = NULL;
  *pLast = this;
  pLast = &next;
}


CEnoDevice *CEnoDeviceClass::NewDevice (uint32_t _profile, const char *id, uint32_t deviceId, const char *arg) {
  CEnoDeviceClass *cls;
  CEnoDevice *dev;

  for (cls = first; cls; cls = cls->next)
    if (cls->profile == _profile) {
      dev = cls->fNewDevice (id, cls, deviceId);
      dev->Init (arg);
      return dev;
    }
  WARNINGF (("Unsupported device profile: %02X-%02X-%02X", (_profile >> 16) & 0xff, (_profile >> 8) & 0xff, _profile & 0xff));
  return NULL;
}



// ***** Equipment device class braces *****

/* The following macros replace the "class NAME: public CEnoDevice {" and the closing brace "};"
 * of the class declarations.
 *
 * It is not allowed to define a constructor - All class-specific initialization must be done in
 * the virtual Init() method instead.
 *
 * See CEnoDeviceWindowHandle for an example.
 */


#define ENO_DEVICE_CLASS_BEGIN(NAME, PROFILE) \
  static class CEnoDevice *_EnoNewDevice##NAME(const char *id, class CEnoDeviceClass *deviceClass, uint32_t deviceId); \
  static CEnoDeviceClass _EnoNewDeviceClass##NAME (PROFILE, _EnoNewDevice##NAME); \
  class NAME: public CEnoDevice { \
    public: \
      NAME (const char *_id, CEnoDeviceClass *_deviceClass, uint32_t _deviceId): CEnoDevice (_id, _deviceClass, _deviceId) {}


#define ENO_DEVICE_CLASS_END(NAME) \
  }; \
  static class CEnoDevice *_EnoNewDevice##NAME(const char *id, class CEnoDeviceClass *deviceClass, uint32_t deviceId) { \
    return new NAME (id, deviceClass, deviceId); \
  }





// *************************** Equipment Drivers *******************************


/* This section defines all supported equipment classes.
 *
 * The profile IDs and equipment specifications can be found in the
 * "EnOcean Equipment Profiles" document and the EEP catalog available at
 * https://www.enocean-alliance.org .
 *
 * The existing class definitions can be used as a template for new definitions.
 */



// ***** F6-01-01: Push Button *****

ENO_DEVICE_CLASS_BEGIN (CEnoDevicePushButton, 0xf60101)

  public:

    virtual void Init (const char *arg = NULL) {
      rc = RcRegisterResource (RcDriver (), id, rctBool, false);
    }

    virtual void OnLinkLost () {
      rc->ReportUnknown ();
    }

    virtual void OnTelegram (CEnoTelegram *telegram) {
      uint8_t db = telegram->Data () [0];
      rc->ReportValue ((bool) ((db & 0x08) != 0));
    }

  protected:
    CResource *rc;

ENO_DEVICE_CLASS_END (CEnoDevicePushButton)



// ***** F6-10-00: Window Handle *****

ENO_DEVICE_CLASS_BEGIN (CEnoDeviceWindowHandle, 0xf61000)

  public:

    virtual void Init (const char *arg = NULL) {
      rc = RcRegisterResource (RcDriver (), id, rctWindowState, false);
      if (envEnoWindowHandleInit) {
        CRcValueState vs (rctWindowState, envEnoWindowHandleInit);
        if (vs.IsValid ()) rc->ReportValueState (&vs);
        else WARNINGF (("Invalid window state value passed for '%': '%s'", envEnoWindowHandleInitKey, envEnoWindowHandleInit));
      }
      //~ INFOF (("### Registered resource '%s'", rc->Uri ()));
    }

    virtual void OnLinkLost () {
      rc->ReportUnknown ();
    }

    virtual void OnTelegram (CEnoTelegram *telegram) {
      uint8_t db = telegram->Data () [0];
      switch (db & 0xf0) {
        case 0xf0:      // Handle down (0b1111xxxx) ...
          rc->ReportValue (rcvWindowClosed);
          break;
        case 0xc0:      // Handle left or right (0b11x1xxxx) ...
        case 0xe0:
          rc->ReportValue (rcvWindowOpen);
          break;
        case 0xd0:      // Handle up (0b1101xxxx) ...
          rc->ReportValue (rcvWindowTilted);
          break;
        default:        // Undefined value ...
          rc->ReportUnknown ();
          WARNINGF (("CEnoDeviceWindowHandle: Invalid data in telegram: %02x", (int) db));
      }
    }

  protected:
    CResource *rc;

ENO_DEVICE_CLASS_END (CEnoDeviceWindowHandle)





// *************************** Top-Level ***************************************


static CThread driverThread;

static CEnoDevice **deviceList = NULL;
static int devices = 0;


static void *DriverThread (void *) {
  CEnoTelegram telegram;
  CEnoDevice *device;
  EEnoStatus status;
  static TTicks tNoLink = NEVER;
  int i;
  bool found;

  EnoInit ();
  do {
    status = EnoReceive (&telegram);
    if (status == enoOk) {
      if (tNoLink != NEVER) {
        INFO (("Link is back again."));
        tNoLink = NEVER;
      }
      found = false;
      for (i = 0; i < devices; i++) {
        device = deviceList[i];
        if (device->DeviceId () == telegram.DeviceId ()) {
          found = true;
          if (device->Class ()->ProfileRorg () != telegram.Rorg ())
            WARNINGF (("Received telegram with wrong RORG=%02X for device ID %08x (%s)",
                       (int) telegram.Rorg (), device->DeviceId (), ProfileToStr (device->Class ()->ProfileRorg ())));
          else
            device->OnTelegram (&telegram);
          break;
        }
      }
      if (!found) {
        CString s;
        DEBUGF (1, ("Unmatched telegram: %s", telegram.ToStr (&s)));
      }
    }
    else if (status == enoNoLink) {
      if (tNoLink == NEVER) tNoLink = TicksNowMonotonic ();
      else if (tNoLink != LINK_LOST) {
        if (TicksNowMonotonic () - tNoLink > TICKS_FROM_SECONDS (envEnoLinkMaxAge * 60)) {
          WARNINGF (("No link for more than %i minute(s): Reporting resources as unknown.", envEnoLinkMaxAge));
          for (i = 0; i < devices; i++) deviceList[i]->OnLinkLost ();
          tNoLink = LINK_LOST;
        }
      }
    }
  } while (status != enoInterrupted);
  EnoDone ();
  return NULL;
}


static void DriverInit () {
  CString prefix;
  CSplitString args;
  CEnoDevice *device;
  const char *key, *id;
  uint32_t deviceId, profile;
  int i, idx0, idx1, prefixLen;
  const char *errStr;

  prefix.SetC ("enocean.device.");
  prefixLen = prefix.Len ();
  EnvGetPrefixInterval (prefix.Get (), &idx0, &idx1);
  if (idx1 > idx0) deviceList = new CEnoDevice * [idx1 - idx0];
  devices = 0;
  for (i = idx0; i < idx1; i++) {
    key = EnvGetKey (i);
    id = key + prefixLen;
    args.Set (EnvGetVal (i), 3, ":");
    errStr = NULL;
    if (!id[0]) errStr = "Invalid key";
    else {
      if (args.Entries () < 1) errStr = "Empty definition string";
    }
    if (!errStr) {
      profile = ProfileFromStr (args[0]);
      if (!profile) errStr = "Invalid profile string";
    }
    if (!errStr) {
      if (!UnsignedFromString (args.Entries () > 1 ? args[1] : id, &deviceId, 16))
        errStr = "Invalid device ID";
    }
    if (!errStr) {
      device = CEnoDeviceClass::NewDevice (profile, id, deviceId, args.Entries () > 2 ? args[2] : NULL);
      if (!device) errStr = "Unsupported profile";
    }
    if (!errStr) {
      deviceList[devices++] = device;
    }
    else
      WARNINGF (("Invalid setting '%s': %s", key, errStr));
  }
  driverThread.Start (DriverThread);
}


static void DriverDone () {
  EnoInterrupt ();
  driverThread.Join ();
}





// *************************** Driver Entry ************************************


HOME2L_DRIVER(enocean) (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
  switch (op) {

    case rcdOpInit:
      enoRcDrv = drv;
      DriverInit ();
      break;

    case rcdOpStop:
      DriverDone ();
      enoRcDrv = NULL;
      break;

    default:
      break;
  }
}
