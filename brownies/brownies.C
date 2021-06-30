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


#include "brownies.H"

#include <stddef.h>   // for offsetof()
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/socket.h>   // for socket server ...
#include <sys/un.h>
#include <linux/i2c-dev.h>




extern "C" {
#include "avr/interface.c"
#include "avr/shades.h"     // for calculations related to shades configuration parameters
}


// Compile-time assertions (see interface.h) ...
STATIC_ASSERT (BR_VROM_SIZE == sizeof (TBrFeatureRecord) && (BR_VROM_SIZE % BR_MEM_BLOCKSIZE) == 0);
STATIC_ASSERT (BR_EEPROM_ID_SIZE == sizeof (TBrIdRecord) && (BR_EEPROM_ID_SIZE % BR_MEM_BLOCKSIZE) == 0);
STATIC_ASSERT (BR_EEPROM_CFG_SIZE == sizeof (TBrConfigRecord) && (BR_EEPROM_CFG_SIZE % BR_MEM_BLOCKSIZE) == 0);





// *************************** Settings ****************************************



// ***** General *****


ENV_PARA_STRING("br.config", envBrDatabaseFile, "brownies.conf");
  /* Name of the Brownie database file (relative to the 'etc' domain)
   */

ENV_PARA_STRING("br.link", envBrLinkDev, "/dev/i2c-1");
  /* Link device (typically i2c) for communicating with brownies
   *
   * The path is absolute or relative to the Home2L 'tmp' directory. In practice,
   * the path will either point to a real i2c device (path is absolute) or to
   * a maintenance socket of another Home2L instance on the same machine (path
   * may be relative). If the special string "=" is given, the socket specified
   * by \refenv{br.serveSocket} is used.
   *
   * Supported i2c devices are Linux i2c devices and the 'ELV USB-i2c' adapter.
   * The type is auto-detected.
   */

ENV_PARA_STRING("br.serveSocket", envBrSocketName, NULL);
  /* Maintenance socket for the Brownie driver
   *
   * If set, the Brownie2L ('home2l-brownie2l') can connect to a running driver
   * and use its link for maintenance and viewing statistics. During the time
   * of the connection, the driver will pause all own link activities.
   *
   * The path is absolute or relative to the Home2L 'tmp' directory.
   */

ENV_PARA_INT("br.checksPerScan", envBrChecksPerScan, 1);
  /* Number of devices polled completely per fast scan
   *
   * Increasing this value will increase the general polling frequency of Brownie
   * devices at the expense of a decreased responsiveness on events with
   * notifications (e.g. button events or switch sensor events).
   *
   * As a rule of thumb, this value should be set such that the average times
   * for the "fast polling phase" and the "slow polling phase" shown in the
   * link statistics are in the same order of magnitude.
   */

ENV_PARA_INT("br.minScanInterval", envBrMinScanInterval, 64);
  /* Minimum polling interval [ms]
   *
   * Minimum time between starting two scans of the Brownie bus by the driver.
   * If scanning all devices takes less than this, the next scan will be delayed.
   * This avoids a high CPU load if only few or no devices are present.
   */

ENV_PARA_INT("br.featureTimeout", envBrFeatureTimeout, 5000);
  /* Time after which an unreachable feature resource is marked invalid
   */



// ***** GPIO *****


ENV_PARA_SPECIAL ("br.gpio.<brownieID>.<nn>.invert", bool, false);
  /* Invert a GPIO pin when reporting or driving
   *
   * If set, the respective Brownie GPIO pin is handled as low-active.
   * This affects both the reporting and driving of values.
   * Inside the Brownie firmware, the values are processed in their original
   * values. On the resource level, eventually negated values are used.
   *
   * <nn> is the 2-digit decimal GPIO number as in \refrc{brownies/<brownieID>/gpio/<nn>}
   * or \refrc{brownies/<brownieID>/gpio/<kk>}, respectively.
   */



// ***** Matrix *****


ENV_PARA_SPECIAL ("br.matrix.win.<brownieID>.<winID>", const char *, NULL);
  /* Define a window state resource
   *
   * This defines a resource representing a window state (type 'rctWindowState')
   * based on a set of one or two sensor elements. The syntax of a defintion is
   *
   * \begin{description}
   *   \item[\texttt{[-|+]s:<sensor>}] ~ \\
   *            Single sensor (0 = window open, 1 = window closed).
   *   \item[\texttt{[-|+]v:<lower>:<upper>}] ~ \\
   *            Two sensors mounted at the side border of the window.
   *            Both sensors = 0 indicate that the window is open,
   *            only the upper = 0 is interpreted as a tilted window.
   *   \item[\texttt{[-|+]h:<near>:<far>:<tth>}] ~ \\
   *            Two sensor mounted at the top border of the window,
   *            placed at the near and far end related to the hinge.
   *            Whether the window is open or tilted is determined
   *            dynamically by the order the switches open.
   *            'tth' is the time threshold in ms. If the near sensor
   *            opens less than this later than the far sensor, the
   *            window is considered to be tilted. Otherwise, it is
   *            considered open.
   * \end{description}
   *
   * The prefix (''-'' or ''+'') denotes whether the sensor value(s) have to be inverted.
   * By default (or with a ''+''), a closed window/sensor is represented by a value of 0.
   * With a prefix of ''-'', a closed window/sensor is represented by a value of 1.
   *
   * The sensors are identified by 2-digit numbers as the raw matrix IDs.
   *
   * Note for the horizontal ('h') variant: This is the dynamic case, where both the
   *   near and the far sensor typically open both the "tilted" and "open" case.
   *   When tilting, the sensors typically open approximatly at the same time
   *   (either may be slightly earlier, this may change). However, when opening,
   *   the near sensor may open approx. 500-1000 ms later. Certainly, this time
   *   may vary depending on the window handling and properties of the sensors.
   *   Hence, horizontal placement should be avoided as possible when placing
   *   the sensors.
   */



// ***** ADC *****


ENV_PARA_BOOL("br.adc.8bit", envBrAdc8Bit, false);
  /* Reduce the ADC precision to 8 bit to save communication bandwidth
   *
   * By default, ADC values are reported with maximum available precision
   * (10 bit for ATtiny MCUs). By activating this option, the precision is limited
   * to 8 bit in order to save communication bandwith.
   */



// ***** Temperature (ZACwire) *****


ENV_PARA_INT("br.temp.interval", envBrTempInterval, 5000);
  /* Approximate polling interval for temperature values
   */



// ***** Shades *****


ENV_PARA_STRING("br.shades.reqAttrs", envBrShadesReqAttrs, NULL);
  /* Request attributes for requests generated on button pushes [\refenv{rc.userReqAttrs}]
   *
   * If a shades button is pushed, a request is auto-generated (or removed) to let
   * the shades move up or down. This parameter defines the attributes of such
   * requests. For example, if the attribute string is "-31:00" and a user pushes a
   * button to close the shades, this overrides automatic rules until 7 a.m. on
   * the next morning. Afterwards, automatic rules may open them again.
   *
   * By default, the value of \refenv{rc.userReqAttrs} is used.
   *
   * Note: An eventual off-time attribute is set only after the button is released.
   */





// *************************** Basics ******************************************


const char *BrStatusStr (EBrStatus s) {
  static const char *msgStr [brEND] = {
    "Ok",
    "No or incomplete message (request or reply) received",
    "Message unchecked",
    "Invalid request message",
    "Invalid reply message",
    "Non-existing operation",
    "Operation not allowed",
    "Device is not a brownie",
    "No device at given address",
    "I/O error when accessing the TWI bus",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "No reply"
  };
  const char *msg = NULL;
  if (s >= 0 || s < brEND) msg = msgStr[s];
  return msg ? msg : "(invalid)";
}


static struct { int id; const char *const name; } brMcuNameMap[] = {
  { BR_MCU_ATTINY84, "t84" },
  { BR_MCU_ATTINY85, "t85" },
  { BR_MCU_ATTINY861, "t861" },
};


const char *BrMcuStr (int mcuType) {
  int i;

  for (i = 0; i < ENTRIES(brMcuNameMap); i++)
    if (brMcuNameMap[i].id == mcuType) return brMcuNameMap[i].name;
  return NULL;
}


int BrMcuFromStr (const char *mcuStr) {
  int i;

  if (mcuStr)    // sanity
    for (i = 0; i < ENTRIES(brMcuNameMap); i++)
      if (strcasecmp (mcuStr, brMcuNameMap[i].name) == 0) return brMcuNameMap[i].id;
  return BR_MCU_NONE;
}





// *************************** Brownie Description *****************************


#define CFG_OFS(X) offsetof(TBrConfigRecord, X)


const TBrCfgDescriptor brCfgDescList[] = {
  { "adr",        "%03i", ctUint8,    -1,  CFG_OFS(adr),  "Own TWI address" },
  { "id",        "%-12s", ctId,       -1,            -1,  "Brownie ID" },

  { "fw",        "%-12s", ctFw,       -1,            -1,  "Firmware name (read-only)" },
  { "mcu",        "%-4s", ctMcu,      -1,            -1,  "MCU model (read-only)" },
  { "version",      NULL, ctVersion,  -1,            -1,  "Version number (read-only)" },
  { "features",     NULL, ctFeatures, -1,            -1,  "Feature code (read-only)" },

  { "osccal",       "%i", ctUint8,  BR_FEATURE_TIMER,    CFG_OFS(oscCal),     "Timer calibration: OSCCAL register" },

  { "hub_maxadr", "%03i", ctUint8,  BR_FEATURE_TWIHUB,   CFG_OFS(hubMaxAdr),  "(hub) Hub subnet end address" },
  { "hub_speed",    "%i", ctUint8,  BR_FEATURE_TWIHUB,   CFG_OFS(hubSpeed),   "(hub) Hub and TWI master delay factor (adds a delay of ~ n*10µs per bit)" },

  { "sha0_du",    "%.3f", ctShadesDelay,  BR_FEATURE_SHADES_0, CFG_OFS(shadesDelayUp[0]),    "(shades) Shades #0 calibration: up delay [s]" },
  { "sha0_dd",    "%.3f", ctShadesDelay,  BR_FEATURE_SHADES_0, CFG_OFS(shadesDelayDown[0]),  "(shades) Shades #0 calibration: down delay [s]" },
  { "sha0_tu",    "%.2f", ctShadesSpeed,  BR_FEATURE_SHADES_0, CFG_OFS(shadesSpeedUp[0]),    "(shades) Shades #0 calibration: total time to move up [s]" },
  { "sha0_td",    "%.2f", ctShadesSpeed,  BR_FEATURE_SHADES_0, CFG_OFS(shadesSpeedDown[0]),  "(shades) Shades #0 calibration: total time to move down [s]" },
  { "sha1_du",    "%.3f", ctShadesDelay,  BR_FEATURE_SHADES_1, CFG_OFS(shadesDelayUp[1]),    "(shades) Shades #1 calibration: up delay [s]" },
  { "sha1_dd",    "%.3f", ctShadesDelay,  BR_FEATURE_SHADES_1, CFG_OFS(shadesDelayDown[1]),  "(shades) Shades #1 calibration: down delay [s]" },
  { "sha1_tu",    "%.2f", ctShadesSpeed,  BR_FEATURE_SHADES_1, CFG_OFS(shadesSpeedUp[1]),    "(shades) Shades #1 calibration: total time to move up [s]" },
  { "sha1_td",    "%.2f", ctShadesSpeed,  BR_FEATURE_SHADES_1, CFG_OFS(shadesSpeedDown[1]),  "(shades) Shades #1 calibration: total time to move down [s]" }

};


const int brCfgDescs = ((int) (sizeof (brCfgDescList) / sizeof (TBrCfgDescriptor)));





// *************************** Brownie Features ********************************


#define BR_POLL 0x100         // update regularly


class CBrFeature {
  public:

    // Subclass interface ...
    CBrFeature (CBrownie *_brownie);
      ///< Subclass constructor. It must:
      ///   a) register all resources with driver (passed a second argument), set their 'driverData' to '*this',
      ///   b) set 'expRcList'/'expRcs'.
    virtual ~CBrFeature () {}

    virtual unsigned Sensitivity () { return 0; }
      ///< Combined mask (logical OR) of @ref BR_POLL and 'BR_CHANGED_*' values indicating when
      /// this feature has to be updated.

    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) = 0;
      /// Must read out feature-related registers from device and eventually report changes to the resources.
      /// This method is called
      ///    a) if a change was detected in the "changed" register or
      ///    b) if the expiration time is less than @ref envBrFeatureTimeout in the future.
      /// Features without a "changed" bit can influence the polling interval by setting the expiration time
      /// to <next polling time> + 'envBrFeatureTimeout' by calling 'RefreshExpiration(<next polling time>)'.
      /// @param link is the hardware link.
      /// @param changed is an indication of the reason why this was called. Only bits returned by Sensitivity()
      ///     can be set here, and the BR_POLL bit is never set.
      /// @param initial indicates that this is the first call to this method or the first call after some
      ///     recovered failure. If this is set, all resource values should be reported freshly.

    virtual void DriveValue (class CBrownieLink *link, CResource *rc, CRcValueState *vs) { ASSERT (false); }
      ///< This must
      ///   a) drive the value to the device,
      ///   b) report the new (hopefully 'valid') value and state.
      /// Read-only features do not need to implement this.

    // Helpers ...
    void RefreshExpiration (TTicksMonotonic waitTime = 0);
    const char *MakeRcLid (const char *fmt, ...);

    // Service (called from 'CBrownie'/'CBrownieSet') ...
    CBrownie *Brownie () { return brownie; }
    void CheckExpiration ();
      // invalidate all expireable resources on expiration

  protected:
    friend class CBrownie;

    CBrownie *brownie;          // reference to owner (to identify brownie by driver)
    TTicksMonotonic expTime;    // expiration time (all resources of the same feature expire together)
                                //   'envBrFeatureTimeout' ticks before expiration, CBrFeature::Update () is called.
                                //   NEVER = do not check for expiration, but call 'CBrFeature::Update ()' now.
    CResource **expRcList;      // reference to array of resources to auto-expire (to be maintained by sub-class; may contain NULL entries)
    int expRcs;                 // number of resources to auto-expire
};



CBrFeature::CBrFeature (CBrownie *_brownie) {
  brownie = _brownie;
  expTime = NEVER;
  expRcList = NULL;
  expRcs = 0;
}


void CBrFeature::RefreshExpiration (TTicksMonotonic waitTime) {
  expTime = TicksMonotonicNow () + waitTime + envBrFeatureTimeout;
  //~ if (expRcs > 0) INFOF (("###     CBrFeature::RefreshExpiration: %s :%i", expRcList[0]->Lid (), (int) expTime));
}


void CBrFeature::CheckExpiration () {
  CResource *rc;
  int n;

  if (expTime != NEVER) if (TicksMonotonicNow () - expTime >= 0) {
    for (n = 0; n < expRcs; n++) if ( (rc = expRcList[n]) ) {
      //~ INFOF (("###   Expired: %s (now = %i, expTime = %i)", rc->Gid (), TicksMonotonicNow (), expTime));
      rc->ReportUnknown ();
    }
    expTime = NEVER;
  }
}


const char *CBrFeature::MakeRcLid (const char *fmt, ...) {
  static CString ret;
  va_list ap;

  ret.Set (brownie->Id ());
  ret.Append ('/');
  va_start (ap, fmt);
  ret.AppendFV (fmt, ap);
  va_end (ap);
  return ret.Get ();
}





// ***** CBrFeatureGpio *****


#define BR_GPIO_MAX 16      // GPIO number range


class CBrFeatureGpio: public CBrFeature {
  protected:
    CResource *rcList[BR_GPIO_MAX];
    uint16_t gpioState, gpioInvert;
    bool gpioStateValid;

  public:


    CBrFeatureGpio (CBrownie *_brownie, CRcDriver *drv): CBrFeature (_brownie) {
      CResource *rc;
      CString s;
      const char *lid;
      int n;

      //~ INFOF(("### Registering GPIO resource(s) for %03i", _brownie->Adr ()));

      // Variables ...
      gpioStateValid = false;
      gpioInvert = 0;

      // Register resources ...
      for (n = 0; n < BR_GPIO_MAX; n++) {
        lid = MakeRcLid ("gpio/%02i", n);
        rc = NULL;

        // Register as input if appropriate ...
        if (_brownie->FeatureRecord ()->gpiPresence & (1 << n))
          rc = RcRegisterResource (drv, lid, rctBool, false, this);
            /* [RC:brownies:<brownieID>/gpio/<nn>] Brownie GPIO (input)
             *
             * <nn> is the GPIO number, possible numbers are those with the respective bit set in
             * \refapic{SBrFeatureRecord::gpoPresence}.
             */

        // Register as output if appropriate ...
        if (_brownie->FeatureRecord ()->gpoPresence & (1 << n)) {
          rc = RcRegisterResource (drv, lid, rctBool, true, this);
          rc->SetDefault ((bool) (_brownie->FeatureRecord ()->gpoPreset & (1 << n)) != 0);
            /* [RC:brownies:<brownieID>/gpio/<kk>:<preset>] Brownie GPIO (output)
             *
             * <kk> is the GPIO number, possible numbers are those with the respective bit set in
             * \refapic{SBrFeatureRecord::gpoPresence}.
             *
             * <preset> is the preset value as defined by \refapic{SBrFeatureRecord::gpoPreset}
             * and is set as a default.
             */
        }

        // Get invert status ...
        rcList[n] = rc;
        if (rc) {
          if (EnvGetBool (StringF (&s, "br.gpio.%s.%02i.invert", brownie->Id (), n), false))    // default = 'false'
            gpioInvert |= (1 << n);
        }
      }
      //~ INFOF (("### gpioInvert = %04x", (int) gpioInvert));

      // Set 'expRcList' ...
      expRcList = &rcList[0];
      expRcs = BR_GPIO_MAX;
      while (expRcs && expRcList[0] == NULL) { expRcList++; expRcs--; }   // just some ...
      while (expRcs && expRcList[expRcs-1] == NULL) expRcs--;             // ... optimization
    }


    virtual unsigned Sensitivity () { return BR_CHANGED_GPIO; }


    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) {
      CResource *rc;
      EBrStatus status;
      unsigned gpioMask;
      uint8_t regVal = 0;
      int n;

      // Read registers and store result ...
      gpioMask = brownie->FeatureRecord ()->gpiPresence | brownie->FeatureRecord ()->gpoPresence;
      status = brOk;
      if (status == brOk && (gpioMask & 0x00ff)) {
        status = link->RegRead (brownie, BR_REG_GPIO_0, &regVal);
        gpioState = regVal;
      }
      if (status == brOk && (gpioMask & 0xff00)) {
        status = link->RegRead (brownie, BR_REG_GPIO_1, &regVal);
        gpioState |= ((unsigned) regVal) << 8;
      }

      // Report values and refresh expiration time on success ...
      if (status == brOk) {
        gpioStateValid = true;
        for (n = 0; n < BR_GPIO_MAX; n++) if ( (rc = rcList[n]) ) {
          rc->ReportValue (((gpioState ^ gpioInvert) & (1 << n)) != 0);
        }
        if (status == brOk) RefreshExpiration ();
      }
      else
        gpioStateValid = false;
    }


    virtual void DriveValue (class CBrownieLink *link, CResource *rc, CRcValueState *vs) {
      EBrStatus status;
      int n;
      uint8_t reg, mask, devState, newState;
      bool bitVal;

      //~ INFOF(("### CBrFeatureGpio::DriveValue (): %s <- %s", rc->Lid (), vs->ToStr ()));

      // Sanity ...
      if (!vs->IsValid ()) return;    // ignore "drive nothing" requests

      // Identify resource ...
      for (n = 0; n < BR_GPIO_MAX && rcList[n] != rc; n++);
      ASSERT (n < BR_GPIO_MAX);
      reg = n < 8 ? BR_REG_GPIO_0 : BR_REG_GPIO_1;
      mask = 1 << (n & 7);

      // Get current and new register value ...
      if (!gpioStateValid) Update (link, 0, false);
      devState = (uint8_t) ((reg == BR_REG_GPIO_0) ? gpioState : (gpioState >> 8));
      bitVal = vs->Bool ();
      if (mask & gpioInvert) bitVal = !bitVal;
      newState = bitVal ? (devState | mask) : (devState & ~mask);

      // Write and verify register ...
      status = link->RegWrite (brownie->Adr (), reg, newState);
      if (status == brOk) status = link->RegRead (brownie, reg, &devState);

      // Report resource value/state ...
      if (status == brOk) {
        rc->ReportValue ((devState & mask) != 0);
        gpioState = (reg == BR_REG_GPIO_0) ? ((gpioState & 0xff00) | devState)
                                           : ((gpioState & 0x00ff) | (((uint16_t) devState) << 8));
      }
      else {
        rc->ReportUnknown ();
        gpioStateValid = false;
      }
    }


};





// ***** CBrFeatureMatrix *****


enum EBrMatrixWindowType {
  mwtInvalid,       // no window

  mwtSingle,        // single contact; 0 = rcvWindowOpen, 1 = rcvWindowClosed

  mwtVertical,      // two contacts, vertically at the side (lower/upper);
                    //   1/1          = rcvWindowClosed
                    //   1/0          = rcvWindowTilted
                    //   0/0 (or 0/1) = rcvWindowOpen

  mwtHorizontal     // two contacts, horizontally at the top (near/far hinge)
                    //   1/1          = rcvWindowClosed
                    //   0/1          = rcvWindowTilted
                    //   1/0          = rcWindowOpen
                    //   0/0 after rcvWindowTilted or at most <n> ms after x/0  = rcWindowTilted
                    //   0/0 else     = rcWindowOpen
                    //
                    // Note: We do not make use of the BR_REG_MATRIX_ECYCLE register,
                    //       the result may be inaccurate.
};


class CBrMatrixWindow {
  protected:
    EBrMatrixWindowType type;
    int col[2], row[2];
    TTicksMonotonic tth;  // for 'mwtHorizontal': threshold time

    int vals;             // bit 0 = value at col[0]/row[0], bit 1 = value at col[1]/row[1]
    bool invert;          // 'true' = sensor values are inverted
    TTicksMonotonic tx0;  // for 'mwtHorizontal': the time + 'tth' at which the "0/x" state was reached or 'NEVER' if the state is not "0/x"
    ERctWindowState state;
    CResource *rc;

    friend class CBrFeatureMatrix;

  public:


    CBrMatrixWindow () { type = mwtInvalid; }


    void Set (const char *def, int matRows, int matCols, CRcDriver *rcDrv, const char *rcLid) {

      // Read and consume optional prefix ...
      invert = false;
      if (def[0] == '-') {
        invert = true;
        def++;
      }
      else if (def[0] == '+')
        def++;

      // Read type ...
      switch (def[0]) {
        case 's': type = mwtSingle;     break;
        case 'v': type = mwtVertical;   break;
        case 'h': type = mwtHorizontal; break;
        default:  type = mwtInvalid;    return;
      }

      // Read sensor ID #0 ...
      if (def[1] != ':') { type = mwtInvalid; return; }
      if (def[2] < '0' || def[2] >= ('0' + matRows)) { type = mwtInvalid; return; }
      else row[0] = def[2] - '0';
      if (def[3] < '0' || def[3] >= ('0' + matCols)) { type = mwtInvalid; return; }
      else col[0] = def[3] - '0';

      // Read sensor ID #1, if adequate ...
      if (type == mwtSingle) {
        if (def[4]) { type = mwtInvalid; return; }
        row[1] = col[1] = -1;
      }
      else {
        if (def[4] != ':') { type = mwtInvalid; return; }
        if (def[5] < '0' || def[5] >= ('0' + matRows)) { type = mwtInvalid; return; }
        else row[1] = def[5] - '0';
        if (def[6] < '0' || def[6] >= ('0' + matCols)) { type = mwtInvalid; return; }
        else col[1] = def[6] - '0';
        if (type == mwtVertical && def[7]) { type = mwtInvalid; return; }
      }

      // Read 'tth' value ('mwtHorizontal' only) ...
      if (type == mwtHorizontal) {
        if (def[7] != ':') { type = mwtInvalid; return; }
        if (!IntFromString (def + 8, &tth)) { type = mwtInvalid; return; }
      }

      // Init variables ...
      vals = mwtSingle ? 2 : 3;     // assume a closed window ...
      state = rcvWindowClosed;
      tx0 = NEVER;

      // Register resource ...
      rc = RcRegisterResource (rcDrv, rcLid, rctWindowState, false);
        /* [RC:brownies:<brownieID>/matrix/win.<winID>] Brownie window state
         *
         * Reports a window state (closed/open/tilted) based on one or two matrix
         * sensor switches. The window must be declared by a
         * \refenv{br.matrix.win.<brownieID>.<winID>} configuration entry.
         */
    };


    void Update (int _row, int _col, bool val) {
      //~ INFOF (("### CBrMatrixWindow::Update (row = %i, col = %i, val = %i)", _row, _col, (int) val));
      if (invert) val = !val;
      if (_row == row[0] && _col == col[0]) { if (val) vals |= 2; else vals &= ~2; }
      if (_row == row[1] && _col == col[1]) { if (val) vals |= 1; else vals &= ~1; }
      switch (type) {
        case mwtSingle:
          state = vals ? rcvWindowClosed : rcvWindowOpen;
          break;
        case mwtVertical:
          state = ( vals == 3 ? rcvWindowClosed :
                    vals == 2 ? rcvWindowTilted :
                                rcvWindowOpen
                  );
          break;
        case mwtHorizontal:
            //   1/1          = rcvWindowClosed
            //   1/0          = rcvWindowTilted
            //   0/1          = rcWindowOpen
            //   0/0 after rcvWindowTilted or at most <n> ms after 0/x  = rcWindowTilted
            //   0/0 else     = rcWindowOpen
          state = ( vals == 3 ? rcvWindowClosed :
                    vals == 1 ? rcvWindowTilted :
                    vals == 2 ? rcvWindowOpen :
                    /* vals == 0 */ (state == rcvWindowTilted || tx0 == NEVER || TicksMonotonicNow () <= tx0) ?
                      rcvWindowTilted : rcvWindowOpen
                  );
          if (vals & 1) tx0 = NEVER;
          else if (tx0 == NEVER) tx0 = TicksMonotonicNow () + tth;
          break;
        default:
          break;
      }
      rc->ReportValue (state);
    }


};


class CBrFeatureMatrix: public CBrFeature {
  protected:
    CResource *rcMat[8 * 8];
    int matRows, matCols;
    uint8_t mat[8];   // internal representation of the matrix
    bool matValid;    // internal representation valid and up-to-date

    CBrMatrixWindow *winList;
    CBrMatrixWindow *winMat[8 * 8];      // references from matrix elements to relevant windows
    int wins;

  public:


    CBrFeatureMatrix (CBrownie *_brownie, CRcDriver *drv): CBrFeature (_brownie) {
      CString envPrefix;
      CResource *rc;
      const char *lid;
      //~ uint16_t featureSet;
      uint8_t matDim;
      int n, k, row, col, idx0, idx1, matSize;

      //~ INFOF(("### Registering matrix resource(s) for %03i", _brownie->Adr ()));

      // Variables ...
      matValid = false;

      // Determine dimensions ...
      matDim = _brownie->FeatureRecord ()->matDim;
      matRows = BR_MATDIM_ROWS (matDim);
      matCols = BR_MATDIM_COLS (matDim);
      matSize = matRows * matCols;

      // Register resources ...
      for (row = 0; row < matRows; row++) for (col = 0; col < matCols; col++) {
        lid = MakeRcLid ("matrix/%i%i", row, col);
        rc = RcRegisterResource (drv, lid, rctBool, false, this);
          /* [RC:brownies:<brownieID>/matrix/<nn>] Brownie sensor matrix value
           *
           * <nn> is a two-digit number, where the first digit represents the row
           * and the second digit represents the column of the respective sensor.
           */
        rcMat [row * matCols + col] = rc;
      }

      // Register window objects ...
      envPrefix.SetF ("br.matrix.win.%s.", brownie->Id ());
      for (n = 0; n < matSize; n++) winMat[n] = NULL;
      wins = 0;
      EnvGetPrefixInterval (envPrefix.Get (), &idx0, &idx1);
      if (idx1 > idx0) {
        winList = new CBrMatrixWindow [idx1 - idx0];
        for (n = idx0; n < idx1; n++) {
          winList[wins].Set (EnvGetVal (n), matRows, matCols, drv,
                             MakeRcLid ("matrix/win.%s", EnvGetKey (n) + envPrefix.Len ()));
          if (winList[wins].type == mwtInvalid)
            WARNINGF (("Ignoring invalid matrix/window setting: %s = %s", EnvGetKey (n), EnvGetVal (n)));
          else {
            for (k = 0; k < 2; k++) {
              row = winList[wins].row[k];
              col = winList[wins].col[k];
              if (row >= 0 && col >= 0) winMat[row * matCols + col] = &winList[wins];
            }
            wins++;
          }
        }
      }
      else winList = NULL;

      // Set expiration list ...
      expRcList = new CResource * [matRows * matCols + wins];
      for (n = 0; n < matSize; n++) expRcList[n] = rcMat[n];
      for (n = 0; n < wins; n++) expRcList[matSize + n] = winList[n].rc;
      expRcs = matSize + wins;
    }


    virtual ~CBrFeatureMatrix () {
      if (wins) delete [] winList;
      delete [] expRcList;
    }


    virtual unsigned Sensitivity () { return BR_CHANGED_MATRIX; }


    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) {
      EBrStatus status;
      int row, col, idx;
      uint8_t ev;
      bool val;

      //~ INFO ("# CBrFeatureMatrix::Update ()");

      // Invalidate after expiration ...
      if (expTime == NEVER) matValid = false;

      // If necessary, read the complete matrix to synchronize with the queue,  ...
      if (!matValid) {
        //~ INFO ("#   full read-out");

        // Clear queue ...
        status = link->RegWrite (brownie->Adr (), BR_REG_MATRIX_EVENT, BR_MATRIX_EV_EMPTY);
        // Read out full matrix ...
        for (row = 0; row < matRows; row++)
          mat[row] = link->RegReadNext (&status, brownie->Adr (), BR_REG_MATRIX_0 + row);
        // We set 'matValid = true' later after processing the queue.
        // This way, we can check 'matValid' inside the queue loop to check if
        // we can rely on the order.

        // Report resource values ...
        if (status == brOk) for (row = 0; row < matRows; row++) for (col = 0; col < matCols; col++) {
          val = (mat[row] & (1 << col)) != 0;
          idx = row * matCols + col;
          rcMat[idx]->ReportValue (val);
          if (winMat[idx]) winMat[idx]->Update (row, col, val);
        }
      }

      // Process all pending events ...
      do {
        status = link->RegRead (brownie, BR_REG_MATRIX_EVENT, &ev, true);
        if (status == brOk) {
          if (ev == BR_MATRIX_EV_EMPTY) {    // Done: Leave the loop and mark matrix clean
            //~ INFO ("#   queue: empty");
            matValid = true;
            RefreshExpiration ();
            break;
          }
          else if (ev == BR_MATRIX_EV_OVERFLOW) {
            //~ INFO ("#   queue: overflow");
            matValid = false;                // Overflow: Mark the matrix invalid and leave the loop
            break;
          }
          else {                             // Process event ...
            row = (ev >> BR_MATRIX_EV_ROW_SHIFT) & 7;
            col = (ev >> BR_MATRIX_EV_COL_SHIFT) & 7;
            val = (ev & (1 << BR_MATRIX_EV_VAL_SHIFT)) != 0;
            //~ INFOF (("#   queue: %i%i = %i", row, col, val));
            if (val) mat[row] |=  (1 << col);
            else     mat[row] &= ~(1 << col);
            idx = row * matCols + col;
            rcMat[idx]->ReportValue (val);
            if (winMat[idx]) winMat[idx]->Update (row, col, val);
          }
        }
        //~ else INFOF (("#   queue: error: %s", BrStatusStr (status)));
      } while (status == brOk);
    }


};





// ***** CBrFeatureAdc *****


class CBrFeatureAdc: public CBrFeature {
  protected:
    int idx;
    CResource *rcAdc;

  public:


    CBrFeatureAdc (CBrownie *_brownie, CRcDriver *drv, int _idx): CBrFeature (_brownie) {
      ///< Subclass constructor. It must:
      ///   a) register all resources with driver, Set 'driverData' to '*this',
      ///   b) set 'expRcList'/'expRcs'.

      // Init fields ...
      idx = _idx;

      // Sanity ...
      ASSERT (_idx >= 0 && _idx <= 1);
      if (_brownie->FeatureRecord ()->features & BR_FEATURE_ADC_PASSIVE) {
        WARNINGF (("ADCs in passive mode are not supported - disabling ADC #%i feature for Brownie %03i (%s)."
                   "Please configure the Brownie with ADC_PERIOD > 0.", idx, _brownie->Adr (), _brownie->Id ()));
        /* TBD: Implement on-demand (passive mode) ADC feature:
         * - Add new resource 'period' (type 'rctTime') without default, kept 'rcsUnknown' without requests
         * - Do not poll if 'period' resource is unknown
         * - If set, poll in the given interval and immediately when known
         */
         rcAdc = NULL;
         expRcList = NULL;    // set empty expiration list ...
         expRcs = 0;
      }

      // Register resource ...
      const char *lid = MakeRcLid ("adc%i", idx);
      rcAdc = RcRegisterResource (drv, lid, rctPercent, false, this);
        /* [RC:brownies:<brownieID>/adc<0|1>] Brownie analog (ADC) value
         */

      // Set expiration list ...
      expRcList = &rcAdc;
      expRcs = 1;
    }


    virtual ~CBrFeatureAdc () {}


    virtual unsigned Sensitivity () { return BR_CHANGED_ADC /* | BR_POLL */; }


    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) {
      /// Must read out feature-related registers from device and eventually report changes to the resources.
      /// This method is called
      ///    a) if a change was detected in the "changed" register or
      ///    b) if the expiration time is less than @ref envBrFeatureTimeout in the future.
      /// Features without a "changed" bit can influence the polling interval by setting the expiration time
      /// to <next polling time> + 'envBrFeatureTimeout'.
      /// @param link is the hardware link.
      /// @param changed is an indication of the reason why this was called. Only bits returned by Sensitivity()
      ///     can be set here, and the BR_POLL bit is never set.
      /// @param initial indicates that this is the first call to this method or the first call after some
      ///     recovered failure. If this is set, all resource values should be reported freshly.
      EBrStatus status;
      int adcRaw;
      float adcPercent;

      // Check: envBrAdc8Bit

      // Read registers ...
      status = brOk;
      if (envBrAdc8Bit) {
        adcRaw = 0;
        status = brOk;
      }
      else adcRaw = link->RegReadNext (&status, brownie, idx == 0 ? BR_REG_ADC_0_LO : BR_REG_ADC_1_LO);
      adcRaw |= ((int) link->RegReadNext (&status, brownie, idx == 0 ? BR_REG_ADC_0_HI : BR_REG_ADC_1_HI)) << 8;

      // Report values and refresh expiration time on success ...
      if (status == brOk) {
        adcPercent = adcRaw * (100.0f / (float) 0xff00);
        if (adcPercent < 0.0) adcPercent = 0.0;
        if (adcPercent > 100.0) adcPercent = 100.0;
        rcAdc->ReportValue (adcPercent);
        RefreshExpiration ();
      }
    }
};





// ***** CBrFeatureUart *****


static void SocketServerStop (int *listenFd, const char *pathName) {
  if (*listenFd >= 0) {
    close (*listenFd);
    *listenFd = -1;
    unlink (pathName);   // remove socket special file
    DEBUGF (1, ("Stopped socket server: %s", pathName));
  }
}


static int SocketServerStart (const char *pathName, int backlog = 1) {
  ///< Create, bind and listen to a new socket.
  /// @param pathName is the absolute path name of the socket, usually in the 'tmp' domain.
  ///   The socket and its containing directory is created as necessary.
  /// @param backlock is the maximum length to which the queue of pending connections may grow (see listen(3)).
  /// @return file handle or -1 on error. On error, a warning is emitted.
  struct sockaddr_un sockAdr;
  CString s;
  int sockListenFd;

  // Prepare owning directory ...
  s.SetC (pathName);
  s.PathGoUp ();
  MakeDir (s.Get ());
  unlink (pathName);   // remove socket special file

  // Sanity ...
  if ((size_t) strlen (pathName) > (sizeof (sockAdr.sun_path) - 1)) {
    WARNINGF (("Socket pathname is too long: %s", pathName));
    sockListenFd = -1;
  }

  // Create and bind socket ...
  else {
    sockListenFd = socket (AF_UNIX, SOCK_STREAM, 0);
    sockAdr.sun_family = AF_UNIX;
    strcpy (sockAdr.sun_path, pathName);    // length has been checked above
  }
  if (sockListenFd > 0) if (bind (sockListenFd, (struct sockaddr *) &sockAdr, sizeof (sockAdr)) != 0) {
    WARNINGF (("Failed to create socket %s: %s", pathName, strerror (errno)));
    SocketServerStop (&sockListenFd, pathName);
  }
  if (sockListenFd > 0) if (chmod (sockAdr.sun_path, S_IRWXU | S_IRWXG) != 0) {
    WARNINGF (("Failed set permission of socket %s: %s", pathName, strerror (errno)));
    SocketServerStop (&sockListenFd, pathName);
  }

  // Listen ...
  if (sockListenFd > 0) if (fcntl (sockListenFd, F_SETFL, fcntl (sockListenFd, F_GETFL, 0) | O_NONBLOCK) != 0) {
    WARNINGF (("Failed to make socket %s non-blocking: %s", pathName, strerror (errno)));
    SocketServerStop (&sockListenFd, pathName);
  }
  if (sockListenFd > 0) if (listen (sockListenFd, backlog) != 0) {
    WARNINGF (("Failed to listen on socket %s: %s", pathName, strerror (errno)));
    SocketServerStop (&sockListenFd, pathName);
  }

  // Done ...
  DEBUGF (1, ("Stopped socket server: %s", pathName));
  return sockListenFd;
}


static int SocketServerAccept (int listenFd, const char *name, bool nonBlocking) {
  ///< Accept an incoming connection.
  /// @param listenFd is the file descriptor of the listening socket.
  /// @param name is the name of the socket for logging an estanblished connection (or NULL to not log).
  /// @param nonBlocking lets the client connection to switch to non-blocking mode.
  /// @return file descriptor of the new connection or -1 if none was accepted.
  struct ucred ucred;
  socklen_t len;
  int clientFd;

  // Sanity ...
  if (listenFd < 0) return -1;

  // Try to accept new connection ...
  clientFd = accept (listenFd, NULL, NULL);

  // Make client connection non-blocking...
  if (clientFd >= 0 && nonBlocking) {
    if (fcntl (clientFd, F_SETFL, fcntl (clientFd, F_GETFL, 0) | O_NONBLOCK) < 0) {
      WARNINGF (("%s: Failed to make socket connection non-blocking (fd = %i): %s", name ? name : "?", clientFd, strerror (errno)));
      close (clientFd);
      clientFd = -1;
    }
  }

  // On success: Try to get peer credentials and log connection ...
  if (clientFd >= 0 && name) {
    len = sizeof (struct ucred);
    if (getsockopt (clientFd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) != 0)
      INFOF (("%s: Connection established from unkown client (failed to get peer credentials)", name));
    else
      INFOF (("%s: Connection established from (PID=%i, UID=%i, GID=%i)", name, ucred.pid, ucred.uid, ucred.gid));
  }

  // Done ...
  return clientFd;
}


static void SocketServerClose (int *clientFd, const char *name, const char *reason) {
  ///< Close a client connection based on the 'errno' value after a Read() or Write() operation.
  close (*clientFd);
  *clientFd = -1;
  INFOF (("%s: Connection closed: %s", name, reason));
}


static void SocketServerCloseLostClient (int *clientFd, const char *name) {
  ///< Close a client connection based on the 'errno' value after a Read() or Write() operation.
  if (errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    SocketServerClose (clientFd, name, strerror (errno));
}


class CBrFeatureUart: public CBrFeature {
  protected:
    CString sockName;
    int sockListenFd, sockClientFd;
    int uartStatus, bytes;   // cached UART status register (BR_REG_UART_STATUS)

  public:


    // Subclass interface ...
    CBrFeatureUart (CBrownie *_brownie, CRcDriver *): CBrFeature (_brownie) {

      // No resources ...
      expRcList = NULL;
      expRcs = 0;

      // Create, bind and listen to socket ...
      sockName.SetF ("%s/brownies/%s.uart", EnvHome2lTmp (), brownie->Id ());
      sockListenFd = SocketServerStart (sockName.Get ());
      sockClientFd = -1;

      // Clear UART status ...
      uartStatus = -1;
    }


    virtual ~CBrFeatureUart () {
      SocketServerStop (&sockListenFd, sockName.Get ());
    }


    virtual unsigned Sensitivity () { return BR_CHANGED_UART | BR_POLL; }
      // polling mainly to check for new socket connections and data from socket


    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) {
      EBrStatus status = brOk;
      TTicksMonotonic tBreak;
      uint8_t buf[16];
      int i, bytes, newSockClientFd;

      // Reset variables ...
      status = brOk;
      tBreak = 0;

      // Note on the handling of link errors:
      //
      // a) In general, Brownie link errors effectively lead to a cancellation of the whole Update() method.
      //    This is reasonable, since these are usually permanent errors persistent after some retries.
      //
      // b) Receiving and transmitting must be done with the 'noResend' flag set, since
      //    reading the UART RX register or writing to the UART TX register has side effects.
      //    In these cases, we just skip the respective bytes and continue.
      //    (The bytes are lost then as during a normal transfer error.)

      // Check for new connection ...
      newSockClientFd = SocketServerAccept (sockListenFd, sockName.Get (), true);
      if (newSockClientFd >= 0) {

        // Close (preempt) old connection ...
        //
        //   NOTE: This is an unusual convention:
        //     a) Access is mutually exclusive (only one client possible at a time),
        //     b) BUT a new connection attempt causes the existing one to be closed (preempted).
        //   The reason for this is that the server cannot reliably detect whether the previous
        //   client is gone if the client does not properly close the socket.
        //   New connections would be blocked out forever.
        //
        //   => The user must take special care that the socket is used exclusively!
        //
        // TBD: Implement a time-out, so that preemption only happens if there was no communication
        //   for this amount of time?
        //
        if (sockClientFd >= 0) {
          INFOF (("%s: Preempting previous connection", sockName.Get ()));
          close (sockClientFd);
        }
        sockClientFd = newSockClientFd;

        // New connection: Reset UART ...
        link->RegWriteNext (&status, brownie, BR_REG_UART_CTRL, BR_UART_CTRL_RESET_RX | BR_UART_CTRL_RESET_TX | BR_UART_CTRL_RESET_FLAGS);
        uartStatus = -1;
      }

      // If some client connected: Read status, report errors, prepare for transfers ...
      if (sockClientFd >= 0) {
        if (uartStatus < 0 || initial || (changed & BR_CHANGED_UART)) {
          uartStatus = link->RegReadNext (&status, brownie, BR_REG_UART_STATUS);
          if (status != brOk) uartStatus = -1;
          else {
            if (uartStatus & BR_UART_STATUS_OVERFLOW)
              WARNINGF (("Brownie %03i (%s) reports a UART buffer overflow.", brownie->Adr (), brownie->Id ()));
            if (uartStatus & BR_UART_STATUS_ERROR)
              WARNINGF (("Brownie %03i (%s) reports a UART parity or frame error.", brownie->Adr (), brownie->Id ()));
            if (uartStatus & (BR_UART_STATUS_OVERFLOW | BR_UART_STATUS_ERROR)) {
              link->RegWriteNext (&status, brownie, BR_REG_UART_CTRL, BR_UART_CTRL_RESET_FLAGS);
              if (status != brOk) uartStatus = -1;
            }
          }
        }

        // Init time-out time ...
        tBreak = TicksMonotonicNow () + envBrMinScanInterval * 2;
      }

      // Receive from UART as many bytes as possible ...
      if (status == brOk && sockClientFd >= 0) do {

        // Get bytes to read ...
        ASSERT (uartStatus >= 0);
        bytes = (uartStatus & BR_UART_STATUS_RX_MASK) >> BR_UART_STATUS_RX_SHIFT;

        // Transfer bytes (read from Brownie, push to socket) ...
        for (i = 0; i < bytes; i++) {
          status = link->RegRead (brownie, BR_REG_UART_RX, &buf[0], false);    // no resend
          if (status != brOk)
            WARNINGF (("Brownie %03i (%s): Dropped a byte from UART: %s",
                      brownie->Adr (), brownie->Id (), BrStatusStr (status)));
          else if (Write (sockClientFd, &buf[0], 1) != 1) {
            // Note: We rely on OS to buffer her. If the byte could not be transferred, it is dropped
            //   silently. However, more bytes are not read this time to avoid more loss.
            // TBD: Check, what happened if the client has disconnected.
            SocketServerCloseLostClient (&sockClientFd, sockName.Get ());
            break;
          }
        }

        // Read status register again for next iteration ...
        uartStatus = link->RegReadNext (&status, brownie, BR_REG_UART_STATUS);
        if (status != brOk) uartStatus = -1;
      }
      while (status == brOk && sockClientFd >= 0 && (uartStatus & BR_UART_STATUS_RX_MASK) && TicksMonotonicNow () < tBreak);
        // Repeat as long as:
        //   a) link and socket are up and OK
        //   b) Brownie UART buffer still contains unread bytes
        //   c) no time-out occured

      // Transfer to UART as many bytes as possible ...
      if (status == brOk && sockClientFd >= 0) do {

        // Get transferrable bytes ...
        ASSERT (uartStatus >= 0);
        bytes = (uartStatus & BR_UART_STATUS_TX_MASK) >> BR_UART_STATUS_TX_SHIFT;   // max. #bytes accepted by Brownie
        if (bytes > (int) sizeof (buf)) bytes = sizeof (buf);                       // max. #bytes accepted by buffer

        // Transfer bytes (read socket, push to Brownie) ...
        bytes = Read (sockClientFd, buf, bytes);        // max. #bytes read from socket
        if (bytes == 0 && errno == 0)
          SocketServerClose (&sockClientFd, sockName.Get (), "Connection closed by client");
        SocketServerCloseLostClient (&sockClientFd, sockName.Get ());
        //~ INFOF (("### Transferring %i bytes...", bytes));
        for (i = 0; i < bytes; i++) {
          status = link->RegWrite (brownie, BR_REG_UART_TX, buf[i], false);   // no resend
            // Errors here may effectively cause all bytes from socket to be dropped silently
          if (status != brOk)
            WARNINGF (("Brownie %03i (%s): Dropped a byte (0x%02x) to send for UART: %s",
                      brownie->Adr (), brownie->Id (), (int) buf[i], BrStatusStr (status)));
        }

        // Read status register again for next iteration ...
        uartStatus = link->RegReadNext (&status, brownie, BR_REG_UART_STATUS);
        if (status != brOk) uartStatus = -1;
      }
      while (status == brOk && sockClientFd >= 0 && bytes > 0 && (uartStatus & BR_UART_STATUS_TX_MASK) && TicksMonotonicNow () < tBreak);
        // Repeat as long as:
        //   a) link and socket are up and OK
        //   b) socket has delivered >0 bytes and may thus deliver more bytes
        //   c) Brownie UART buffer still has space for TX bytes
        //   d) no time-out occured

      // Trigger next poll ...
      if (status == brOk) RefreshExpiration (0);
    }
};





// ***** CBrFeatureTemperature *****


class CBrFeatureTemperature: public CBrFeature {
  protected:
    CResource *rcTemp;

  public:


    CBrFeatureTemperature (CBrownie *_brownie, CRcDriver *drv): CBrFeature (_brownie) {
      //~ INFOF(("### Registering temperature resource(s) for %03i", _brownie->Adr ()));

      // Register resource ...
      const char *lid = MakeRcLid ("temp");
      rcTemp = RcRegisterResource (drv, lid, rctTemp, false, this);
        /* [RC:brownies:<brownieID>/temp] Brownie temperature sensor value
         */

      // Set expiration list ...
      expRcList = &rcTemp;
      expRcs = 1;
    }


    virtual ~CBrFeatureTemperature () {}


    virtual unsigned Sensitivity () { return BR_POLL /* BR_CHANGED_TEMP */; }


    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) {
      EBrStatus status;
      unsigned tempRaw;
      float tempFloat;
      uint8_t regVal;

      //~ INFOF(("### CBrFeatureTemperature::Update (%03i)", brownie->Adr ()));

      // Read registers ...
      status = link->RegRead (brownie, BR_REG_TEMP_LO, &regVal);
      tempRaw = regVal;
      if (status == brOk) status = link->RegRead (brownie, BR_REG_TEMP_HI, &regVal);
      tempRaw |= ((unsigned) regVal) << 8;

      // Report result ...
      if (status == brOk) {
        //~ INFOF (("###   tempRaw = %04x\n", (unsigned) tempRaw));
        if (tempRaw & 1) {
          tempFloat = -50.0 + (tempRaw >> 1) * (200.0 / 2047.0);
          rcTemp->ReportValue (tempFloat);
          RefreshExpiration (envBrTempInterval);
        }
        else rcTemp->ReportUnknown ();
      }
    }
};





// ***** CBrFeatureShades *****


#define rcActUp (rcList[0])  // IMPORTANT: The order of resources matches the order of bits in BR_REG_SHADES_STATUS.
#define rcActDn (rcList[1])
#define rcBtnUp (rcList[2])
#define rcBtnDn (rcList[3])
#define rcPos (rcList[4])


class CBrFeatureShades: public CBrFeature {
  protected:
    CBrFeatureShades *primary;    // reference to primary shades or NULL, if 'this' is the primary
    CResource *rcList[5];
    uint8_t state;        // contents of BR_REG_SHADES_STATUS; the lower 4 bits are ours
    uint8_t rExt;         // contents of BR_REG_SHADES_REXT
    uint8_t rIntLocked;   // last RINT value if the button is still down (-> user request is locked with no end time)
    bool polling;         // shades are moving (frequent updates desired)


    static CRcRequest *NewUserRequest (int pos) {
      CRcRequest *req = new CRcRequest ((float) pos, NULL, rcPrioUser);
        // init with current position and default attributes
      req->SetAttrsFromStr (envBrShadesReqAttrs ? envBrShadesReqAttrs : RcGetUserRequestAttrs ());
        // set request user attributes
      req->SetGid (RcGetUserRequestId ());
        // set GID
      return req;
    }


  public:


    CBrFeatureShades (CBrownie *_brownie, CRcDriver *drv, const char *idStr = CString::emptyStr, CBrFeatureShades *_primary = NULL): CBrFeature (_brownie) {
      const char *lid;

      //~ INFOF(("### Registering shades resource(s) for %03i (%s)", _brownie->Adr (), !_primary ? "primary" : "secondary"));

      // Variables ...
      primary = _primary;
      state = 0;
      polling = false;
      rExt = rIntLocked = 0xff;

      // Register resources ...
      lid = MakeRcLid ("shades%s/pos", idStr);
      rcPos = RcRegisterResource (drv, lid, rctPercent, true, this);
        /* [RC:brownies:<brownieID>/shades<n>/pos] Brownie shades/actuator position
         *
         * Current position of an actuator. An 'rcBusy' status indicates that the actuator
         * is currently active / moving.
         *
         * <n> is the index of the actuator: 0 or 1 if the Brownie drives two actuators
         * or always 0, if there is only one.
         *
         * The driver issues automatic user requests if one of the buttons are pushed.
         * The attributes of such requests are specified by \refenv{br.shades.reqAttrs},
         * \refenv{rc.userReqId}, and \refenv{rc.userReqAttrs}.
         */
      lid = MakeRcLid ("shades%s/actUp", idStr);
      rcActUp = RcRegisterResource (drv, lid, rctBool, false, this);
        /* [RC:brownies:<brownieID>/shades<n>/actUp] Brownie actuator is powered in the "up" direction
         *
         * This resource reflects the actual (raw) state of the actuator, and is 'true' iff the
         * engine is presently powered in the "up" direction. This resource is read-only, to
         * manipulate the actuator, a request must be issued for the
         * \refrc{brownies/<brownieID>/shades<n>/pos} resource.
         *
         * <n> is the index of the actuator (0 or 1).
         */
      lid = MakeRcLid ("shades%s/actDn", idStr);
      rcActDn = RcRegisterResource (drv, lid, rctBool, false, this);
        /* [RC:brownies:<brownieID>/shades<n>/actDown] Brownie actuator is powered in the "down" direction
         *
         * This resource reflects the actual (raw) state of the actuator, and is 'true' iff the
         * engine is presently powered in the "down" direction. This resource is read-only, to
         * manipulate the actuator, a request must be issued for the
         * \refrc{brownies/<brownieID>/shades<n>/pos} resource.
         *
         * <n> is the index of the actuator (0 or 1).
         */
      lid = MakeRcLid ("shades%s/btnUp", idStr);
      rcBtnUp = RcRegisterResource (drv, lid, rctBool, false, this);
        /* [RC:brownies:<brownieID>/shades<n>/btnUp] Brownie actuator's "up" button is pushed
         *
         * This is the actual (raw) state of the actuator's "up" button.
         *
         * <n> is the index of the actuator (0 or 1).
         */
      lid = MakeRcLid ("shades%s/btnDn", idStr);
      rcBtnDn = RcRegisterResource (drv, lid, rctBool, false, this);
        /* [RC:brownies:<brownieID>/shades<n>/btnDn] Brownie actuator's "down" button is pushed
         *
         * This is the actual (raw) state of the actuator's "down" button.
         *
         * <n> is the index of the actuator (0 or 1).
         */

      // Set exiration list ...
      expRcList = &rcList[0];
      expRcs = 5;
    }


    virtual ~CBrFeatureShades () {}


    virtual unsigned Sensitivity () { return polling ? (BR_POLL | BR_CHANGED_SHADES) : BR_CHANGED_SHADES; }
      // enable polling when the shades need polling


    virtual void Update (CBrownieLink *link, unsigned changed, bool initial) {
      static const uint8_t actMask = BR_SHADES_0_ACT_UP | BR_SHADES_0_ACT_DN | BR_SHADES_1_ACT_UP | BR_SHADES_1_ACT_DN;
      CRcRequest *req;
      EBrStatus status;
      uint8_t regPos, regRInt, regRExt, pos, rInt, _state;
      int n;

      //~ INFOF(("### CBrFeatureShades::Update (%03i, %s)", brownie->Adr (), !primary ? "primary" : "secondary"));

      // Read out all relevant registers: button/actuator status, current position and RINT ...
      if (!primary) {
        status = link->RegRead (brownie, BR_REG_SHADES_STATUS, &_state);
        _state ^= ((((_state & actMask) >> 1) & _state) | (((_state & actMask) << 1) & _state)) & actMask;
          // Change all actUp = actDn = 1 combinations (= reverse wait) to actUp = actDn = 0

        regPos = BR_REG_SHADES_0_POS;
        regRInt = BR_REG_SHADES_0_RINT;
        regRExt = BR_REG_SHADES_0_REXT;
      }
      else {    // secondary ...
        _state = primary->state >> 4;
        status = brOk;

        regPos = BR_REG_SHADES_1_POS;
        regRInt = BR_REG_SHADES_1_RINT;
        regRExt = BR_REG_SHADES_1_REXT;
      }
      //~ INFOF (("### _state = 0x%1x, polling = %i", _state, (int) polling));
      pos = link->RegReadNext (&status, brownie->Adr (), regPos);
      rInt = link->RegReadNext (&status, brownie->Adr (), regRInt);
      polling = (status != brOk)
                || ((_state & (BR_SHADES_0_ACT_UP | BR_SHADES_0_ACT_DN)) != 0)
                || (pos > 100);

      // Return on failure ...
      if (status != brOk) return;

      // Refresh driven REXT after Brownie reboot ...
      //   If a Brownie is rebooted, it forgets its position and RINT/REXT values.
      //   We refresh REXT here. The sitation is detected via an invalidated position.
      if (pos > 100) link->RegWrite (brownie->Adr (), regRExt, rExt);

      // Report resource values ...
      if (initial || (_state != state))
        for (n = 0; n < 4; n++) rcList[n]->ReportValue ((_state & (1 << n)) != 0);
      if (pos == 0xff) rcPos->ReportUnknown ();
      else rcPos->ReportValue ((float) pos, polling ? rcsBusy : rcsValid);
      RefreshExpiration ();

      // Create user request on device button pushes ...
      //   This is done based on the RINT register, since the device does all debouncing and cannot
      //   loose button events due to bus delays.
      if (rInt <= 100 && pos <= 100) {         // RINT has been set due to a device button push ...
        // We skip this step in the special case of an unknown position because otherwise
        // the shades would be startable, but not stoppable by the device buttons. If a device button is
        // pushed while the engine is active, the device sets RINT to the current position, which is the
        // unknown or passive value of 0xff if the position is unkown.
        //~ INFOF (("### Caught RINT=%i ...", rInt));
        //
        // TBD: If the communication has been stopped (e.g. by a brownie2l socket connection) for some time,
        //   and the Brownie auto-moves the shades based on a SHADES_x_RINT_FAILSAFE setting, this auto-set RINT
        //   value will be read back here, and it will appear as if the user has pushed a button for that failsafe
        //   position. Fix?

        // Clear RINT ...
        if (rExt > 100) link->RegWrite (brownie->Adr (), regRExt, rInt);
          // If 'rExt' is unset, set it temporarily to 'rInt' first to avoid a stop-start cycle of the engine.
        link->RegWrite (brownie->Adr (), regRInt, 0xff);

        // Set user request ...
        req = NewUserRequest (rInt);
        if ((_state & (BR_SHADES_0_BTN_UP | BR_SHADES_0_BTN_DN)) != 0) {
              // a (the) button is still down now: Remove off-time
          req->SetTimeOff (NEVER);
          rIntLocked = rInt;          // remember to set off-time when button is released
        }
        //~ INFOF (("###    setting '%s' ...", req->ToStr ()));
        rcPos->SetRequest (req);
      }
      if (rIntLocked <= 100) {        // have a locked request due to a locked device button?
        if ((_state & (BR_SHADES_0_BTN_UP | BR_SHADES_0_BTN_DN)) == 0) {
          // both buttons are up: can replace it by a normal request ...
          //~ INFOF (("###    unlocking pos %i ...", (int) rIntLocked));
          rcPos->SetRequest (NewUserRequest (rIntLocked));
          rIntLocked = 0xff;
        }
      }

      // Write back new state ...
      state = _state;
    }


    virtual void DriveValue (class CBrownieLink *link, CResource *rc, CRcValueState *vs) {
      float rExtFloat;
      uint8_t regRExt, regRInt;

      // Sanity ...
      ASSERTF (rc == rcPos, ("### rc = %s, but it should be: rcPos = %s", rc ? rc->Uri () : "(null)", rcPos->Uri () ));

      //~ INFOF (("### CBrFeatureShades::DriveValue (%s)", vs->ToStr ()));
      if (!primary) { regRInt = BR_REG_SHADES_0_RINT; regRExt = BR_REG_SHADES_0_REXT; }
      else          { regRInt = BR_REG_SHADES_1_RINT; regRExt = BR_REG_SHADES_1_REXT; }

      // Drive ...
      if (vs->IsValid ()) {   // Normal case ...
        rExtFloat = vs->UnitFloat (rctPercent);
        if (rExtFloat < 0.0 || rExtFloat > 100.0) return;   // ignore invalid values
        rExt = rExtFloat;
        link->RegWrite (brownie->Adr (), regRExt, rExt);
      }
      else {                  // All requests gone: Stop actuators ...
        link->RegWrite (brownie->Adr (), regRInt, 0xff);
          // Clear RINT to avoid unexpected actuator starting;
          // We are about to hand over to device-internal control by writing 0xff to REXT.
          // This is why we clear RINT here.
        link->RegWrite (brownie->Adr (), regRExt, 0xff);    // clear REXT
        rExt = 0xff;

        // Report state change ...
        rc->ReportState (rcsValid);   // Report the current position as no longer busy.
      }
    }


};





// *************************** CBrownie ****************************************


// ***** Helpers *****


uint32_t BrVersionGet (TBrFeatureRecord *featureRecord) {
  return VersionCompose (
            featureRecord->versionMajor, featureRecord->versionMinor,
            featureRecord->versionRevision >> 1, (featureRecord->versionRevision & 1) ? true : false
          );
}


bool BrVersionNumberFromStr (TBrFeatureRecord *featureRecord, const char *str) {
  uint32_t ver;

  ver = VersionFromStr (str);
  if (!ver) return false;

  featureRecord->versionMajor = (uint8_t) VersionMajor (ver);
  featureRecord->versionMinor = (uint8_t) VersionMinor (ver);
  featureRecord->versionRevision = (uint16_t) (VersionMinor (ver) << 1) + (VersionDirty (ver) ? 1 : 0);
  return true;
}


const char *BrFeaturesToStr (CString *ret, TBrFeatureRecord *featureRecord) {
  uint8_t *p;

  ret->Clear ();
  //~ for (p = (uint8_t *) &featureRecord->features; p < (uint8_t *) &featureRecord->fwName; p++)
  for (p = ((uint8_t *) &featureRecord) + brFeatureRecordRcVec0; p < ((uint8_t *) &featureRecord) + brFeatureRecordRcVec1; p++)
    ret->AppendF ("%02x", (int) *p);
  return ret->Get ();
}


bool BrFeaturesFromStr (TBrFeatureRecord *featureRecord, const char *str) {
  char buf[3], *endPtr;
  int val;
  uint8_t *p;

  buf[2] = '\0';
  //~ for (p = (uint8_t *) &featureRecord->features; p < (uint8_t *) &featureRecord->fwName; p++) {
  for (p = ((uint8_t *) &featureRecord) + brFeatureRecordRcVec0; p < ((uint8_t *) &featureRecord) + brFeatureRecordRcVec1; p++) {
    if (!(buf[0] = *(str++))) return false;
    if (!(buf[1] = *(str++))) return false;
    val = strtol (buf, &endPtr, 16);
    if (endPtr != &buf[2]) return false;
    *p = (uint8_t) val;
  }
  return true;
}



// ***** Class helpers *****


const char *CBrownie::GetOptValue (int optIdx, CString *ret) {
  const TBrCfgDescriptor *opt = &brCfgDescList[optIdx];

  if (!ret) ret = GetTTS ();
  switch (opt->type) {
    case ctUint8:
      ret->SetF (opt->fmt, (int) (* ((uint8_t *) &configRecord + opt->ofs)) );
      break;
    case ctInt8:
      ret->SetF (opt->fmt, (int) (* ((int8_t *) &configRecord + opt->ofs)) );
      break;
    case ctUint16:
      ret->SetF (opt->fmt, (int) (* (uint16_t *) ((int8_t *) &configRecord + opt->ofs)) );
      break;

    case ctVersion:
      BrVersionGetAsStr (ret, &featureRecord);
      break;
    case ctFeatures:
      BrFeaturesToStr (ret, &featureRecord);
      break;
    case ctMcu:
      ret->SetF (opt->fmt, BrMcuStr (featureRecord.mcuType));
      break;
    case ctFw:
      ret->SetF (opt->fmt, featureRecord.fwName);
      break;
    case ctId:
      ret->SetF (opt->fmt, idRecord);
      break;

    case ctShadesDelay:
      ret->SetF (opt->fmt, ShadesDelayFromByte (* ((uint8_t *) &configRecord + opt->ofs)) );
      break;
    case ctShadesSpeed:
      ret->SetF (opt->fmt, ShadesSpeedFromByte (* ((uint8_t *) &configRecord + opt->ofs)) );
      break;

    default:
      ASSERT(false);
  }
  return ret->Get ();
}


bool CBrownie::SetOptValue (int optIdx, const char *str) {
  const TBrCfgDescriptor *opt = &brCfgDescList[optIdx];
  float valFloat;
  int valInt;
  uint8_t valByte;

  switch (opt->type) {
    case ctUint8:
      valInt = ValidIntFromString (str, INT_MAX);
      if (valInt < 0 || valInt > 255) return false;
      * ((uint8_t *) &configRecord + opt->ofs) = (uint8_t) valInt;
      break;
    case ctInt8:
      valInt = ValidIntFromString (str, INT_MAX);
      if (valInt < -128 || valInt > 127) return false;
      * ((int8_t *) &configRecord + opt->ofs) = (int8_t) valInt;
      break;
    case ctUint16:
      valInt = ValidIntFromString (str, INT_MAX);
      if (valInt < 0 || valInt > 65535) return false;
      * (uint16_t *) ((uint8_t *) &configRecord + opt->ofs) = (uint16_t) valInt;
      break;

    case ctVersion:
      if (!BrVersionNumberFromStr (&featureRecord, str)) return false;
      break;
    case ctFeatures:
      if (!BrFeaturesFromStr (&featureRecord, str)) return false;
      break;
    case ctMcu:
      valInt = BrMcuFromStr (str);
      if (valInt == BR_MCU_NONE) return false;
      featureRecord.mcuType = (uint8_t) valInt;
      break;
    case ctFw:
      bzero (&featureRecord.fwName, sizeof (featureRecord.fwName));
      strncpy (featureRecord.fwName, str, sizeof (featureRecord.fwName) - 1);
      break;
    case ctId:
      if (strlen (str) >= sizeof (idRecord))
        WARNINGF (("ID exceeds the maximum of %i characters: '%s'", sizeof (idRecord) - 1, str));
      bzero (idRecord, sizeof (idRecord));
      strncpy (idRecord, str, sizeof (idRecord) - 1);
      break;

    case ctShadesDelay:
      valFloat = ValidFloatFromString (str, -1.0);
      //~ INFOF(("### valFloat = %f", valFloat));
      if (!ShadesDelayToByte (valFloat, &valByte)) return false;
      * ((uint8_t *) &configRecord + opt->ofs) = valByte;
      break;
    case ctShadesSpeed:
      valFloat = ValidFloatFromString (str, -1.0);
      if (!ShadesSpeedToByte (valFloat, &valByte)) return false;
      * ((uint8_t *) &configRecord + opt->ofs) = valByte;
      break;
    default:
      ASSERT(false);
  }
  return true;
}





// ***** Interface methods *****


void CBrownie::Clear () {
  int n;

  for (n = 0; n < features; n++) delete featureList[n];
  features = 0;
  deviceChecked = false;
  unknownChanges = true;

  bzero (&idRecord, sizeof (idRecord));
  bzero (&featureRecord, sizeof (featureRecord));
  bzero (&configRecord, sizeof (configRecord));
  databaseString.Clear ();
}


bool CBrownie::SetFromStr (const char *str, CString *ret) {
  CSplitString argv, keyVal;
  CString s;
  const char *key, *val;
  int n, optIdx;
  bool ok;

  ok = true;
  if (ret) ret->Clear ();
  argv.Set (str);
  for (n = 0; n < argv.Entries () && argv[n][0] != '#'; n++) {

    // Get key and value...
    keyVal.Set (argv[n], 2, "=");
    key = keyVal[0];
    val = (keyVal.Entries () >= 2) ? keyVal[1] : NULL;
    if (!val && key[0] >= '0' && key[0] <= '9') {     // allow "adr" field without explicit key...
      val = key;
      key = "adr";
    }

    // Lookup key...
    for (optIdx = 0; optIdx < brCfgDescs; optIdx++)
      if (strcmp (key, brCfgDescList[optIdx].key) == 0) break;
    if (optIdx >= brCfgDescs) {
      WARNINGF (("Illegal option key in assignment: %s - ignoring", argv[n]));
      ok = false;
    }
    else {

      // Set Value...
      if (val) {
        if (!SetOptValue (optIdx, val)) {
          WARNINGF (("Illegal option value in assignment: %s", argv[n]));
          ok = false;
        }
      }

      // Output value if requested...
      if (ret) ret->AppendF ("%s=%s ", key, GetOptValue (optIdx, &s));
    }
  }

  // Done...
  if (ret) ret->Strip ();
  return ok;
}


const char *CBrownie::ToStr (CString *ret, bool withIdentification, bool withVersionInfo) {
  CString s;
  const TBrCfgDescriptor *opt;
  int n;

  // Go ahead...
  ret->Clear ();
  for (n = 0; n < brCfgDescs; n++) {
    opt = &brCfgDescList[n];
    if  ( (withIdentification || (opt->type != ctId && opt->ofs != CFG_OFS(adr))) &&  // identification?
          (withVersionInfo || (opt->type == ctId || opt->ofs >= 0)) &&                // read-only (= version info)?
          (!featureRecord.magic || (opt->features & featureRecord.features) != 0) &&  // relevant (= feature present)?
          (opt->type != ctVersion || BrVersionGet (&featureRecord))                   // if version: valid?
        )
      ret->AppendF ("%s=%s ", brCfgDescList[n].key, GetOptValue (n, &s));
    //~ INFOF (("### %s (%04x/%04x) -> %s", brCfgDescList[n].key, featureRecord.features, opt->features, ret->Get ()));
  }

  // Done...
  ret->Strip ();
  return ret->Get ();
}




// ***** Compatibility and device checking *****


bool CBrownie::IsCompatible (const char *_databaseString) {
  CBrownie tmp;
  CString s;

  // Create temporary copy of 'this'...
  tmp.SetId (Id ());
  tmp.SetFeatureRecord (FeatureRecord ());
  tmp.SetConfigRecord (ConfigRecord ());

  // Apply settings from '_dataBaseString' ...
  tmp.SetFromStr (_databaseString, &s);

  // Check if that changed something...
  if (strcmp (idRecord, tmp.idRecord) != 0) {
    //~ INFO ("### incompatible ID");
    return false;
  }
  if (HasDeviceFeatures ())
    if (bcmp (&featureRecord, &tmp.featureRecord, sizeof (featureRecord)) != 0) {
      //~ INFO ("### incompatible feature record");
      return false;
    }
  if (HasDeviceConfig ())
    if (bcmp (&configRecord, &tmp.configRecord, sizeof (configRecord)) != 0) {
      //~ INFO ("### incompatible config record");
      return false;
    }
  return true;
}


bool CBrownie::UpdateFromDevice (class CBrownieLink *link) {
  CBrownie devBrownie;

  //~ INFOF (("UpdateFromDevice (%03i) ...", Adr ()));

  // Read out device ...
  if (link->CheckDevice (Adr (), &devBrownie) != brOk) return false;

  // Check compatibility ...
  if (!IsCompatible (databaseString.Get ())) return false;

  // Update records ...
  if (devBrownie.HasDeviceFeatures ()) SetFeatureRecord (devBrownie.FeatureRecord ());
  if (devBrownie.HasDeviceConfig ()) SetConfigRecord (devBrownie.ConfigRecord ());

  // Done ...
  //~ if (HasDeviceFeatures () && HasDeviceConfig ())
    //~ INFOF (("UpdateFromDevice (%03i): success.", Adr ()));
  return HasDeviceFeatures () && HasDeviceConfig ();
}




// ***** Resources *****


void CBrownie::RegisterAllResources (class CRcDriver *rcDriver, class CBrownieLink *link) {
  class CBrFeatureShades *primaryShades;
  unsigned featureVector;
  int n;

  // If no feature information is available from the database: Try to get it from the device now ...
  if (link) CheckDeviceForResources (link);
  if (!HasFeatures ()) {
    if (link)
      DEBUGF (1, ("Failed to contact Brownie %03i:%s to obtain feature information", Adr (), Id ()));
    else
      DEBUGF (1, ("No feature information in the database for Brownie %03i:%s: no resources registered for it", Adr (), Id ()));
    return;
  }

  // Create feature objects ...
  featureVector = FeatureRecord ()->features;
  //~ INFOF (("### Registering resources for %03i: %04x", Adr (), featureVector));
  if (FeatureRecord ()->gpiPresence | FeatureRecord ()->gpoPresence)
    featureList[features++] = new CBrFeatureGpio (this, rcDriver);
  if (FeatureRecord ()->matDim)
    featureList[features++] = new CBrFeatureMatrix (this, rcDriver);
  if (featureVector & BR_FEATURE_ADC_0)
    featureList[features++] = new CBrFeatureAdc (this, rcDriver, 0);
  if (featureVector & BR_FEATURE_ADC_1)
    featureList[features++] = new CBrFeatureAdc (this, rcDriver, 1);
  if (featureVector & BR_FEATURE_UART)
    featureList[features++] = new CBrFeatureUart (this, rcDriver);
  if (featureVector & BR_FEATURE_TEMP)
    featureList[features++] = new CBrFeatureTemperature (this, rcDriver);
  if (featureVector & BR_FEATURE_SHADES_0) {
    if (featureVector & BR_FEATURE_SHADES_1) {    // have two shades?
      primaryShades = new CBrFeatureShades (this, rcDriver, "0", NULL);
      featureList[features++] = primaryShades;
      featureList[features++] = new CBrFeatureShades (this, rcDriver, "1", primaryShades);
        // Note: The secondary shades must be placed *after* the primary ones
    }
    else    // Just single shades ...
      featureList[features++] = new CBrFeatureShades (this, rcDriver);
  }
  ASSERT (features <= ENTRIES(featureList));      // check buffer overflow

  // Call the initial update for the features ...
  for (n = 0; n < features; n++) featureList[n]->Update (link, 0, true);
}


void CBrownie::CheckDeviceForResources (CBrownieLink *link) {
  int n;

  if (!deviceChecked) {
    //~ INFOF (("### CBrownie::CheckDeviceForResources (%03i, fast=%i)", Adr ()));
    if (UpdateFromDevice (link)) {
      deviceChecked = true;
      // Call initial updates for the features ...
      for (n = 0; n < features; n++) featureList[n]->Update (link, 0, true);
    }
    else
      if (link->Status () == brOk) {
        WARNINGF (("Brownie %03i:%s appears to deviate from the database: not reading data", Adr (), Id ()));
        deviceChecked = true;    // Mark device as checked, it is permanently unusable
      }
  }
}


unsigned CBrownie::Iterate (CBrownieLink *link, bool fast) {
  CBrFeature *feature;
  TTicksMonotonic now;
  EBrStatus status;
  uint8_t changedRaw;
  unsigned changed, sensitivity;
  bool update;
  int n;

  // Device sanity ...
  CheckDeviceForResources (link);
  if (!HasDeviceFeatures () || !HasDeviceConfig ()) return 0;   // device not accessible

  //~ INFOF (("### CBrownie::Iterate (%03i, fast=%i)", Adr (), (int) fast));

  // Read "changed" register ...
  status = link->RegRead (Adr (), BR_REG_CHANGED, &changedRaw, true);
  if (status != brOk) {
    unknownChanges = true;
    if (status == brRequestCheckError || status == brReplyCheckError) changedRaw = 0xff;
      // Transmission error (resending was disabled): Assume everything changed, then continue.
    else {
      // Some other error occured, the device is probably not accessible: Do not try to access feature registers.
      // => Report "nothing changed", since the return value is used to decide wether to dig into a
      //    subnet, which may be a bad idea if this is a defective hub.
      CheckExpiration ();
      return 0;
    }
  }
  else {
    //~ if (changedRaw & ~BR_CHANGED_TEMP) INFOF (("###               CBrownie::Iterate (%03i): changed = %04x", (int) Adr (), (int) changedRaw));
    if (unknownChanges) {
      changedRaw = 0xff;
        // We had a read failure earlier, but now it's ok again. Assume that everything changed during the failure time.
      unknownChanges = false;
    }
  }
  changed = (unsigned) changedRaw;
  //~ INFOF (("###   changed = 0x%02x", changed));

  // Iterate over features ...
  now = TicksMonotonicNow ();
  for (n = 0; n < features; n++) {            // Note: Positive order is required for the shades!
    feature = featureList[n];
    sensitivity = feature->Sensitivity ();
    update = ((sensitivity & changed) != 0);  // Always update if a sensitive "changed" bit is set
    if ((sensitivity & BR_POLL) || feature->expTime == NEVER) {
      // Feature requests polling or has expired:
      //   If the expiration time gets close, but not in "fast" mode: Update ...
      if (!fast && (feature->expTime == NEVER || (feature->expTime - now < envBrFeatureTimeout))) update = true;
    }
    else {
      // Feature does not request polling and has not expired:
      //   If the "changed" register was read successfully and no "changed" bit(s) are set:
      //   Refresh expiration time, no update necessary ...
      if (!update) feature->expTime = now + envBrFeatureTimeout;
    }
    //~ if ((sensitivity & BR_CHANGED_SHADES) && (changed & BR_CHANGED_SHADES))
      //~ INFOF (("###   changed shades: fast = %i, update = %i", (int) fast, (int) update));
    if (update) feature->Update (link, (changed & sensitivity), false);
    feature->CheckExpiration ();
  }

  // Done ...
  return changed;
}


void CBrownie::CheckExpiration () {
  int n;

  for (n = 0; n < features; n++) featureList[n]->CheckExpiration ();
}


void CBrownie::DriveValue (CBrownieLink *link, CResource *rc, CRcValueState *vs) {
  CBrFeature *feature;

  // Device sanity ...
  CheckDeviceForResources (link);
  if (!HasDeviceFeatures () || !HasDeviceConfig ()) {  // device not accessible ...
    rc->ReportUnknown ();
    return;
  }

  // Pass to feature ...
  feature = (CBrFeature *) rc->UserData ();
  feature->DriveValue (link, rc, vs);
}





// *************************** CBrownieSet *************************************


CBrownieSet::CBrownieSet () {
  bzero (brList, sizeof (brList));

  rcDriver = NULL;
  rcLink = NULL;
}


void CBrownieSet::Clear () {
  int n;
  for (n = 0; n < 128; n++) Del (n);
}


void CBrownieSet::Set (CBrownie *brownie) {
  int adr = brownie->ConfigRecord ()->adr;
  const char *id = brownie->Id ();
  int *pAdr2;

  // Sanity...
  if (adr > 127 || !id[0]) {
    WARNINGF (("CBrownieSet::Set() called with illegal address (%03i) or id ('%s') - discarding brownie", adr, id));
    delete brownie;
    return;
  }
  pAdr2 = adrMap.Get (id);
  if (pAdr2) if (*pAdr2 != adr) {
    WARNINGF (("CBrownieSet::Set(): Duplicate ID ('%s') used for addresses %03i and %03i - discarding %03i", id, *pAdr2, adr, adr));
    delete brownie;
    return;
  }

  // Delete old entry if applicable ...
  if (brList[adr]) delete brList[adr];

  // Add new entry...
  brList[adr] = brownie;
  adrMap.Set (id, &adr);
  //~ adrMap.Dump ();
}


CBrownie *CBrownieSet::Unlink (int adr) {
  CBrownie *ret = Get (adr);
  if (ret) {
    adrMap.Del (ret->Id ());
    brList[adr] = NULL;
  }
  return ret;
}


void CBrownieSet::Del (int adr) {
  CBrownie *br = Unlink (adr);
  if (br) delete br;
}


bool CBrownieSet::ReadDatabase (const char *fileName) {
  CBrownie *brownie;
  CString s, fileStr, lineStr;
  int fd;
  bool ok, ret;

  // Clear...
  Clear ();

  // Open file...
  if (!fileName) fileName = EnvGetHome2lEtcPath (&s, envBrDatabaseFile);
  fd = open (fileName, O_RDONLY);
  if (fd < 0) {
    DEBUGF(1, ("Failed to read '%s'.", fileName));
    return false;
  }
  if (fileName != envBrDatabaseFile)    // write back resolved path
    envBrDatabaseFile = EnvPut (envBrDatabaseFileKey, fileName);

  // Read file line by line ...
  ret = true;
  while (fileStr.AppendFromFile (fd)) while (fileStr.ReadLine (&lineStr)) {
    lineStr.Strip ();
    if (lineStr[0] && lineStr[0] != '#') {    // ignore empty lines and lines with comments
      brownie = new CBrownie;
      brownie->SetDatabaseString (lineStr.Get ());
      ok = brownie->SetFromStr (lineStr.Get ());
      if (!ok || !brownie->Adr ()) {
        WARNINGF (("Invalid line in '%s': '%s'", fileName, lineStr.Get ()));
        ok = false;
      }
      if (ok) if (Get (brownie->Adr ())) {
        WARNINGF (("Redefined address in '%s': '%s'", fileName, lineStr.Get ()));
        ok = false;
      }
      if (ok) if (Get (brownie->Id ())) {
        WARNINGF (("Redefined ID in '%s': '%s'", fileName, lineStr.Get ()));
        ok = false;
      }
      if (ok) Set (brownie);
      else {
        delete brownie;
        ret = false;
      }
    }
  }

  // Close file & done...
  close (fd);
  return ret;
}


bool CBrownieSet::WriteDatabase (const char *fileName) {
  CBrownie *brownie;
  CString s;
  FILE *f;
  int adr;

  // Open output file ...
  if (fileName) {
    f = fopen (fileName, "w");
    if (!f) {
      WARNINGF (("Failed to open '%s' for writing", fileName));
      return false;
    }
  }
  else f = stdout;

  // Write valid entries...
  for (adr = 0; adr < 128; adr++) {
    brownie = brList[adr];
    if (brownie) fprintf (f, "%s\n", brownie->ToStr (&s));
  }

  // Close file & done...
  if (f != stdout) fclose (f);
  return true;
}





// ***** Resources *****


void CBrownieSet::ResourcesInit (CRcEventDriver *_rcDriver, class CBrownieLink *_rcLink) {
  int adr;

  // Sanity ...
  ResourcesDone ();

  // Init driver/link structures ...
  rcDriver = _rcDriver;
  rcLink = _rcLink;
  rcLastCheckedAdr = 0;
  tLastIterate = NEVER;

  // Register all resources ...
  for (adr = 0; adr < 128; adr++) if (brList[adr]) if (brList[adr]->IsValid ())
    brList[adr]->RegisterAllResources (rcDriver, _rcLink);  // remove '_rcLink' to disable device accesses at this point
}


void CBrownieSet::ResourcesIterate (bool noLink, bool noSleep) {
  CRcEvent ev;
  CBrownie *brownie;
  TTicksMonotonic tIterate, tEndFastPoll, tEndSlowPoll;
  unsigned changed;
  int n, adr, hubMaxAdr;

  //~ INFOF (("### CBrownieSet::ResourcesIterate (noLink = %i, noSleep = %i)", (int) noLink, (int) noSleep));
  //~ INFOF (("### %8i:   Entry (tLastIterate = %i) ...", (int) TicksMonotonicNow (), (int) tLastIterate));

  // Sanity ...
  ASSERT (rcDriver && rcLink);

  // Sleep if necessary ...
  tIterate = TicksMonotonicNow ();
  if (!noSleep && tLastIterate != NEVER && tIterate - tLastIterate < envBrMinScanInterval) {
    Sleep (envBrMinScanInterval - (tIterate - tLastIterate));
    tIterate = TicksMonotonicNow ();
  }

  // Handle "no link" case ...
  if (noLink) {

    // Link unavailable: Just check resource expirations ...
    for (adr = 0; adr < 128; adr++) if ( (brownie = brList[adr]) )
      brownie->CheckExpiration ();
    tLastIterate = NEVER;
    return;
  }

  //~ INFOF (("### %8i: CBrownieSet::ResourcesIterate: Drive events first ...", (int) tIterate));

  // Process queued drive events ...
  while (rcDriver->PollEvent (&ev)) {
    ASSERT (ev.Type () == rceDriveValue);
    brownie = ((CBrFeature *) ev.Resource ()->UserData ())->Brownie ();
    brownie->DriveValue (rcLink, ev.Resource (), ev.ValueState ());
  }

  // Fast poll: Query "changed" registers of *all* immediately connected devices ...
  //~ INFOF (("### %8i:   Fast Poll ...", (int) TicksMonotonicNow ()));
  adr = 0;
  while (adr < 128) {
    if (brList[adr]) {

      // Query and handle "changed" register of current device ...
      changed = brList[adr]->Iterate (rcLink, true);    // Check and process 'changed' features (= iterate in fast mode)
      //~ if (changed) INFOF (("###               %03i: changed = %04x", adr, changed));

      // Skip subnet if no change is expected ...
      if ((brList[adr]->FeatureRecord ()->features & BR_FEATURE_TWIHUB)   // device is a hub?
            && (changed & BR_CHANGED_CHILD) == 0) {                       // no notifcation from subnet?
        hubMaxAdr = brList[adr]->ConfigRecord ()->hubMaxAdr;              // => jump to the end of subnet ...
        if (hubMaxAdr > adr) adr = hubMaxAdr;  // sanity (never walk backwards)
      }
    }

    // Next address ...
    adr++;
  }
  tEndFastPoll = TicksMonotonicNow ();

  // Slow poll: Iterate a few devices in a circular way ...
  //~ INFOF (("### %8i:   Full Poll ...", (int) TicksMonotonicNow ()));
  adr = rcLastCheckedAdr;
  for (n = 0; n < envBrChecksPerScan; n++) {

    // Search for next device ...
    do { adr = (adr + 1) % (sizeof (brList) / sizeof (brList[0])); }
      while (!brList[adr] && adr != rcLastCheckedAdr);

    // Check the device (full, non-fast mode) ...
    //~ INFOF (("### %8i:     Checking %03i...", (int) TicksMonotonicNow (), (int) adr));
    if (brList[adr]) brList[adr]->Iterate (rcLink, false);

    // If we have fewer checkable devices than given by 'envBrChecksPerScan', do not iterate any of them twice...
    if (adr == rcLastCheckedAdr) break;
  }
  rcLastCheckedAdr = adr;
  tEndSlowPoll = TicksMonotonicNow ();

  // Statistics ...
  //~ INFOF (("### %8i:   Statistics ...", (int) TicksMonotonicNow ()));
  if (tLastIterate != NEVER)
    rcLink->StatisticsAddIterateTimes (tIterate - tLastIterate, tEndFastPoll - tIterate, tEndSlowPoll - tEndFastPoll);
  tLastIterate = tIterate;
}


void CBrownieSet::ResourcesDone () {
  rcDriver = NULL;
  rcLink = NULL;
}





// ********************** TWI Interface Drivers ********************************



// ***** Local Socket Interface *****


static inline int IfSocketInit (const char *ifName) {
  struct sockaddr_un adr;
  int fd;

  // Create socket ...
  if ( (fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
    DEBUGF (1, ("%s: Failed to create socket.", ifName));
    return -1;
  }

  // Prepare 'adr' ...
  adr.sun_family = AF_UNIX;
  strncpy (adr.sun_path, ifName, sizeof (adr.sun_path));
  adr.sun_path [sizeof (adr.sun_path) - 1] = '\0';

  // Try to connect ...
  if (connect (fd, (struct sockaddr *) &adr, sizeof (adr)) != 0) {
    DEBUGF (1, ("%s: Not a socket: %s .", ifName, adr.sun_path));
    close (fd);
    return -1;      // failure
  }

  // Ignore 'SIGPIPE' signals...
  //   Such signals may occur on writes if the socket connection is lost and by
  //   default, the program would exit then.
  // TBD: Move this to a more global place, e.g. EnvInit()?
  signal (SIGPIPE, SIG_IGN);

  // Success ...
  return fd;
}


static inline EBrStatus IfSocketSend (int fd, int adr, const void *buf, int bytes, const char *ifName) {
  TSocketHeader head;

  // Send socket request ...
  head.op = soSend;
  head.status = brOk;
  head.adr = (uint8_t) adr;
  head.bytes = bytes;
  if (Write (fd, &head, sizeof (head)) != sizeof (head)) {
    DEBUGF(1, ("%s: Failed to send header: %s", ifName, strerror (errno)));
    return brNoBus;
  }
  if (Write (fd, buf, bytes) != (size_t) bytes) {
    DEBUGF(1, ("%s: Failed to send %i bytes: %s", ifName, bytes, strerror (errno)));
    return brNoBus;
  }

  // Get socket response ...
  if (Read (fd, &head, sizeof (head)) != sizeof (head)) {
    DEBUGF(1, ("%s: No response: %s", ifName, bytes, strerror (errno)));
    return brNoBus;
  }
  if (head.op != soSend) {
    WARNINGF (("%s: Received unexpected response - closing connection.", ifName));
    return brNoBus;   // We do not close here, the recovery procedure in 'CBrownieLink::SetAdr ()' will do this.
  }

  // Success ...
  return head.status;
}


static inline EBrStatus IfSocketFetch (int fd, int adr, void *buf, int bytes, const char *ifName) {
  TSocketHeader head;

  // Send socket request ...
  head.op = soFetch;
  head.status = brOk;
  head.adr = (uint8_t) adr;
  head.bytes = bytes;
  if (Write (fd, &head, sizeof (head)) != sizeof (head)) {
    DEBUGF(1, ("%s: Failed to send header: %s", ifName, strerror (errno)));
    return brNoBus;
  }

  // Get socket response ...
  if (Read (fd, &head, sizeof (head)) != sizeof (head)) {
    DEBUGF(1, ("%s: Failed to fetch header: %s", ifName, strerror (errno)));
    return brNoBus;   // We do not close here, the recovery procedure in 'CBrownieLink::SetAdr ()' will do this.
  }
  //~ INFOF (("###   head.op = %i, head.adr = %i, head.bytes = %i", (int) head.op, (int) head.adr, (int) head.bytes));
  if (head.op != soFetch || head.adr != adr || head.bytes != bytes) {
    WARNINGF (("%s: Received unexpected data (received/expected): adr = %03i/%03i, bytes = %i/%i", ifName, (int) head.adr, adr, (int) head.bytes, bytes));
    return brNoBus;
  }
  if (Read (fd, buf, bytes) != (size_t) bytes) {
    DEBUGF(1, ("%s: Failed to fetch %i bytes: %s", ifName, strerror (errno)));
    return brNoBus;
  }

  // Success ...
  return head.status;
}





// ***** Linux i2c dev Interface *****


/* This is the default driver. It accesses /dev/i2c* device files handled
 * by the 'i2c_dev' kernel driver.
 */


static inline bool IfI2cDevInit (int fd, const char *ifName) {
  if (ioctl (fd, I2C_SLAVE, 127) < 0) {
    DEBUGF (1, ("%s: No i2c_dev device.", ifName));
    return false;
  }
  return true;
}


static inline void IfI2cDevDone (int fd, const char *ifName) {}


static inline EBrStatus IfI2cDevSetAdr (int fd, int adr, const char *ifName) {
  if (ioctl (fd, I2C_SLAVE, adr) < 0) {
    WARNINGF (("%s: Failed to set address to %03i: %s", ifName, adr, strerror (errno)));
    return brNoBus;
  }
  return brOk;
}


static inline EBrStatus IfI2cDevSend (int fd, int adr, const void *buf, int bytes, const char *ifName) {
  int bytesTransferred = write (fd, buf, bytes);
  if (bytesTransferred < 0) {
    DEBUGF(1, ("%s: Failed to send %i bytes: %s", ifName, bytes, strerror (errno)));
    return brNoDevice;
  }
  if (bytesTransferred < bytes) return brRequestCheckError;
  return brOk;
}


static inline EBrStatus IfI2cDevFetch (int fd, int adr, void *buf, int bytes, const char *ifName) {
  int bytesTransferred = read (fd, buf, bytes);
  if (bytesTransferred < 0) {
    DEBUGF(1, ("%s: Failed to fetch %i bytes: %s", ifName, bytes, strerror (errno)));
    return brNoDevice;
  }
  if (bytesTransferred != bytes) return brReplyCheckError;
  return brOk;
}





// ***** ELV USB-i2c Interface *****


/* Sample output from the interface to the '?' command:
 *
 *  [empty line]
 *  ELV USB-I2C-Interface v1.8 (Cal:41)
 *  Last Adress:0x00
 *  Baudrate:115200 bit/s
 *  I2C-Clock:99632 Hz
 *  Y00
 *  Y10
 *  Y20
 *  Y30
 *  Y40
 *  Y50
 *  Y60
 *  Y70
 */


#include <termios.h>
#include <unistd.h>


#define ELV_GREETING_MAX_SIZE 512
#define ELV_GREETING_LINES 13
#define ELV_VERSION_LINE 1


static inline bool IfElvI2cInit (int fd, const char *ifName) {
  static const char configY[8] = {
      '1',  // Y01: Do not return CR/LF after read data byte sequence
      '1',  // Y11: When writing, stop after a slave's NACK
      '0',  // Y21: When reading, do not automatically send NACK for last byte
      '1',  // Y31: When writing, return 'K' or 'N' for ACK or NACK, respectively
      '1',  // Y41: Omit space after each byte of returned data
      '0',  // Y5x: (for macros only)
      '0',  // Y6x: (for macros only)
      '0'   // Y71: Send data as decimal (not hex) numbers
      // After factory reset, all settings default to '0'.
    };
  struct termios ts;
  char buf[ELV_GREETING_MAX_SIZE], *lineArr[ELV_GREETING_LINES];
  char cmd[32];
  char *p, c;
  int n, bytes, lines, verMajor, verMinor;
  bool ok;

  // Check for serial interface and set parameters ...
  ok = (tcgetattr (fd, &ts) == 0);
  if (ok) {

    // Set "raw" mode (see cfmakeraw(), which is a non-standard function) ...
    ts.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    ts.c_oflag &= ~OPOST;
    ts.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    ts.c_cflag &= ~(CSIZE | PARENB);
    ts.c_cflag |= CS8;

    // Set timeout to 0.5 seconds ...
    ts.c_cc[VMIN] = 0;
    ts.c_cc[VTIME] = 5;

    // Set baudrate ...
    if (cfsetispeed (&ts, B115200) < 0) ok = false;
    if (cfsetospeed (&ts, B115200) < 0) ok = false;

    // Apply parameters ...
    if (ok) if (tcsetattr (fd, TCSANOW, &ts) < 0) ok = false;
  }
  if (!ok) {
    DEBUGF (1, ("%s: Not an ELV interface (no TTY).", ifName));
    return false;
  }

  // Flush input and output buffers of the serial device ...
  tcflush (fd, TCIOFLUSH);

  // Query the interface status ...
  lines = 0;
  p = buf;
  lineArr[0] = buf;
  if (write (fd, "?", 1) != 1) ok = false;
  else {
    while (lines < ELV_GREETING_LINES) {
      if (p >= buf + ELV_GREETING_MAX_SIZE - 1) { ok = false; break; }
      if (read (fd, &c, 1) != 1) { ok = false; break; }
      if (c == '\n') {      // Note: This assums that the ELV interface sends "\r\n" at the end of each line!
        if (p > buf) p[-1] = '\0';
        lines++;
        if (lines < ELV_GREETING_LINES) lineArr[lines] = p;
      }
      else *(p++) = c;
    }
    *(p++) = '\0';
  }
  //~ for (n = 0; n < lines; n++) INFOF (("### IfElvI2cInit (): %2i: %s", n, lineArr[n]));

  // Check version string and options ...
  if (ok) {
    if (sscanf (lineArr[ELV_VERSION_LINE], "ELV USB-I2C-Interface v%i.%i", &verMajor, &verMinor) != 2) ok = false;
    if (verMajor != 1 || verMinor != 8) // ok = false;
      WARNINGF (("%s: Untested ELV firmware version v%i.%i, supported firmware is v1.8. Problems may occur.", ifName, verMajor, verMinor));
  }
  bytes = 0;
  for (n = 0; n < lines; n++) if ((p = lineArr[n]) [0] == 'Y') {
    if (p[1] < '0' || p[1] > '7') ok = false;
    else {
      c = configY[p[1] - '0'];
      if (p[2] != c) {
        cmd[bytes++] = 'Y';
        cmd[bytes++] = p[1];
        cmd[bytes++] = c;
      }
    }
  }
  if (!ok) {
    DEBUGF (1, ("%s: Not an ELV interface (invalid reply or unsupported firmware version).", ifName));
    return false;
  }

  // Set options ...
  if (bytes > 0) {
    cmd[bytes] = '\0';
    DEBUGF (1, ("%s: Configuring ELV interface: %s", ifName, cmd));
    if (write (fd, cmd, bytes) != bytes) ok = false;
  }

  // Done ...
  return ok;
}


static inline void IfElvI2cDone (int fd, const char *ifName) {}


static inline EBrStatus IfElvI2cSetAdr (int fd, int adr, const char *ifName) { return brOk; }


static inline EBrStatus IfElvI2cSend (int fd, int adr, const void *buf, int bytes, const char *ifName) {
  char msg[8 + 2*BR_REQUEST_SIZE_MAX];
  char c;
  int n;

  // Sanity ...
  ASSERT (bytes <= BR_REQUEST_SIZE_MAX);

  // Flush input and output buffers of the serial device ...
  tcflush (fd, TCIOFLUSH);
    // Note: All communication is (and must be!) implemented in a way that all expected
    //       bytes are consumed completely inside the respective 'IfElvI2c...' function.
    //       The _only_ purpose for flushing here is to facilitate recovery if something
    //       unexpected happend (e.g. unexpected plugging of the device by the user)

  // Send command ...
  sprintf (msg, "S%02x", adr << 1);
  for (n = 0; n < bytes; n++) sprintf (&msg[3 + 2*n], "%02x", (int) ((uint8_t *) buf) [n]);
  strcat (&msg[3 + 2*bytes], "P");
  //~ INFOF (("### IfElvI2cSend (): -> '%s'", msg));
  if (Write (fd, msg, 4 + 2*bytes) != (size_t) (4 + 2*bytes)) {
    DEBUGF (1, ("%s: Failed to write to ELV interface: %s", ifName, strerror (errno)));
    return brNoBus;
  }

  // Get and check ACKs ...
  for (n = -1; n < bytes; n++) {
    if (read (fd, &c, 1) < 1) {
      DEBUGF (1, ("%s: Failed to read from ELV interface: %s", ifName, strerror (errno)));
      c = '\0';
    }
    //~ INFOF (("### IfElvI2cSend (): <- '%c'", c));
    if (c != 'K' && c != 'k') {     // No ACK ...
      if (c == 'N' || c == 'n') {   // NACK ...
        if (n < 0) {      // NACK to address byte: Normal case if no device is present.
          DEBUGF (2, ("%s: Got NACK while adressing (device not present?)", ifName));
          if (bytes > 0) read (fd, &c, 1);
            // Try to read one more 'N';
            // With Y11, ELV sends two 'N', one for the address, one for the first data byte, then it stops.
            // The address 'N' was already consumed, here, we read the second.
          return brNoDevice;
        }
        else {      // NACK to a data byte: Something went definitely wrong
          DEBUGF (1, ("%s: Got NACK while sending", ifName));
          return brRequestCheckError;
        }
      }
      else {                        // Neither ACK nor NACK: Report interface failure ...
        DEBUGF (1, ("%s: Interface problem (received neither ACK nor NACK)", ifName));
        return brNoBus;
      }
    }
  }

  // Success ...
  return brOk;
}


static inline EBrStatus IfElvI2cFetch (int fd, int adr, void *buf, int bytes, const char *ifName) {
  char msg[8];
  int n, val;

  // Sanity ...
  ASSERT (bytes <= 0xff);

  // Flush input and output buffers of the serial device ...
  tcflush (fd, TCIOFLUSH);
    // Note: All communication is (and must be!) implemented in a way that all expected
    //       bytes are consumed completely inside the respective 'IfElvI2c...' function.
    //       The _only_ purpose for flushing here is to facilitate recovery if something
    //       unexpected happend (e.g. unexpected plugging of the device by the user)

  // Send command ...
  sprintf (msg, "S%02x%02xP", (adr << 1) + 1, bytes);
  if (Write (fd, msg, 6) < 6) {
    DEBUGF (1, ("%s: Failed to write to ELV interface: %s", ifName, strerror (errno)));
    return brNoBus;
  }

  // Get response ...
  msg[2] = '\0';
  for (n = 0; n < bytes; n++) {
    if (Read (fd, msg, 2) < 2) {
      DEBUGF (1, ("%s: Failed to read from ELV interface: %s", ifName, strerror (errno)));
      return brNoBus;
    }

    // Handle "Err: ..." messages ...
    if (msg[0] == '\r' || msg[0] == '\n') {
      // If the ELV interface received a NACK after read addressing (a common case if the
      // device is not present), it sends "\r\nErr:TWI READ\r\n". Since this code does
      // not trigger other errors (and is free of bugs...), we assume here
      // that a read addressing NACK happend.
      DEBUGF (2, ("%s: Got NACK while adressing (device not present?)", ifName));
      do {
        if (Read (fd, msg, 1) < 1) break;
      } while (msg[0] == '\n');
      return brNoDevice;
    }
    val = ValidIntFromString (msg, -1, 16);
    if (val < 0 || val > 0xff) {
      DEBUGF (1, ("%s: Invalid response from ELV interface: '%s'", ifName, msg));
      return brNoBus;
    }
    ((uint8_t *) buf) [n] = (uint8_t) val;
  }

  // Success ...
  return brOk;
}





// *************************** CBrownieLink ***********************************


const char *TwiIfTypeStr (ETwiIfType type) {
  static const char *names[] = {
    "(none)",
    "local socket",
    "i2c_dev",
    "ELV USB-i2c"
  };
  if (type < sizeof(names) / sizeof(names[0]))
    return names[type];
  else return names[0];
}





// ***** Init/Done *****


CBrownieLink::CBrownieLink () {
  twiFd = twiAdr = sockListenFd = sockClientFd = -1;
  twiIfType = ifNone;
  status = brNoBus;
  StatisticsReset ();
  sockData = NULL;
}





// ***** TWI Base Functions *****


void CBrownieLink::TwiOpen (bool warn) {
  TwiClose ();

  // Clear interface type ...
  twiIfType = ifNone;

  // Try socket ...
  twiFd = IfSocketInit (twiIfName.Get ());
  if (twiFd >= 0) twiIfType = ifSocket;
  else {

    // Open device file ...
    twiFd = open (twiIfName.Get (), O_RDWR);
    if (twiFd < 0) {
      if (warn) WARNINGF (("Failed to open '%s'", twiIfName.Get ()));
    }
    else if (lockf (twiFd, F_TLOCK, 0) != 0) {
      if (warn) WARNINGF (("Failed to lock '%s': %s", twiIfName.Get (), strerror (errno)));
      close (twiFd);
      twiFd = -1;
    }
    else {
      // Determine interface type ...
      if (IfElvI2cInit (twiFd, twiIfName.Get ())) twiIfType = ifElvI2c;
      else if (IfI2cDevInit (twiFd, twiIfName.Get ())) twiIfType = ifI2cDev;
      else {
        close (twiFd);
        twiFd = -1;
      }
    }
  }

  // Done ...
  status = (twiFd >= 0) ? brOk : brNoBus;
}


void CBrownieLink::TwiClose () {
  if (twiFd >= 0) {
    switch (twiIfType) {
      case ifSocket:
        break;
      case ifI2cDev:
        IfI2cDevDone (twiFd, twiIfName.Get ());
        break;
      case ifElvI2c:
        IfElvI2cDone (twiFd, twiIfName.Get ());
        break;
      default:
        break;
    }
    close (twiFd);
    //~ INFOF (("Disconnected from '%s'.", twiIfName.Get ()));
  }
  twiFd = twiAdr = -1;
  status = brNoBus;
}


EBrStatus CBrownieLink::TwiSetAdr (int _twiAdr) {

  // Bus recovery ...
  if (status == brNoBus && twiFd >= 0) {
    WARNINGF(("%s: Bus connection lost: Recovering.", twiIfName.Get ()));
    TwiClose ();
    if (twiIfType == ifElvI2c) Sleep (300);      // Wait for 300ms to ignore remaining bytes from ELV device
    TwiOpen (true);
    if (status != brOk) {
      WARNINGF(("%s: Recovery failed!", twiIfName.Get ()));
      TwiClose ();
    }
  }

  // Sanity...
  if (twiFd < 0) status = brNoBus;    // TBD: Probably not necessary
  if (status != brNoBus) {
    if (_twiAdr < 0 || _twiAdr > 127) status = brNoDevice;
    else if (_twiAdr == twiAdr) status = brOk;
    else {

      // Set slave address...
      switch (twiIfType) {
        case ifSocket:
          status = brOk;
          break;
        case ifI2cDev:
          status = IfI2cDevSetAdr (twiFd, _twiAdr, twiIfName.Get ());
          break;
        case ifElvI2c:
          status = IfElvI2cSetAdr (twiFd, _twiAdr, twiIfName.Get ());
          break;
        default:
          status = brNoBus;
      }
    }
  }

  // Done ...
  twiAdr = (status == brOk) ? _twiAdr : -1;
  return status;
}


EBrStatus CBrownieLink::TwiSend (int adr, const void *buf, int bytes) {
  char dbg[3 * BR_REQUEST_SIZE_MAX + 8];   // for debug line
  int n;

  // Sanity...
  ASSERT (bytes <= BR_REQUEST_SIZE_MAX);

  // Do the transfer ...
  if (TwiSetAdr (adr) == brOk) switch (twiIfType) {
    case ifSocket:
      status = IfSocketSend (twiFd, twiAdr, buf, bytes, twiIfName.Get ());
      break;
    case ifI2cDev:
      status = IfI2cDevSend (twiFd, twiAdr, buf, bytes, twiIfName.Get ());
      break;
    case ifElvI2c:
      status = IfElvI2cSend (twiFd, twiAdr, buf, bytes, twiIfName.Get ());
      break;
    default:
      status = brNoBus;
  }

  // Debug output ...
  if (WITH_DEBUG && envDebug >= 2) {
    for (n = 0; n < bytes; n++) sprintf (&dbg[3*n], "%02x ", (int) ((uint8_t *) buf)[n]);
    DEBUGF (2, ("%s: -> (%03i) %s(%i bytes): %s", twiIfName.Get (), twiAdr, dbg, bytes, BrStatusStr (status)));
  }

  // Done ...
  return status;
}


EBrStatus CBrownieLink::TwiFetch (int adr, void *buf, int bytes) {
  char dbg[3 * BR_REPLY_SIZE_MAX + 8];   // for debug line
  int n;

  // Sanity...
  ASSERT (bytes <= BR_REPLY_SIZE_MAX);

  // Do the transfer ...
  if (TwiSetAdr (adr) == brOk) switch (twiIfType) {
    case ifSocket:
      status = IfSocketFetch (twiFd, twiAdr, buf, bytes, twiIfName.Get ());
      break;
    case ifI2cDev:
      status = IfI2cDevFetch (twiFd, twiAdr, buf, bytes, twiIfName.Get ());
      break;
    case ifElvI2c:
      status = IfElvI2cFetch (twiFd, twiAdr, buf, bytes, twiIfName.Get ());
      break;
    default:
      status = brNoBus;
  }

  // Debug output ...
  if (WITH_DEBUG && envDebug >= 2) {
    if (status == brNoBus || status == brNoDevice) dbg[0] = '\0';
    else for (n = 0; n < bytes; n++) sprintf (&dbg[3*n], "%02x ", (int) ((uint8_t *) buf)[n]);
    DEBUGF (2, ("%s: <- (%03i) %s(%i bytes): %s", twiIfName.Get (), twiAdr, dbg, bytes, BrStatusStr (status)));
  }

  // Done ...
  return status;
}





// ***** Statistics *****


void CBrownieLink::StatisticsReset (bool local) {
  TSocketHeader head;
  int n;

  // With local socket interface: Delegate to server ...
  if (twiIfType == ifSocket && !local) {
    head.op = soStatReset;
    head.status = brOk;
    head.adr = head.bytes = 0;
    if (Write (twiFd, &head, sizeof (head)) != sizeof (head))
      WARNINGF (("%s: Failed to submit a statistics reset request to socket server", twiIfName.Get ()));
    return;
  }

  // Link statistics ...
  requests = replies = 0;
  for (n = 0; n < brEND; n++)
    requestRetries[n] = requestFailures[n] = replyRetries[n] = replyFailures[n] = 0;

  // Resources statistics ...
  rcIterations = 0;
  rcTSumCycle = rcTSumFastPoll = rcTSumSlowPoll = 0;
  rcTCycleMin = rcTFastPollMin = rcTSlowPollMin = INT_MAX;
  rcTCycleMax = rcTFastPollMax = rcTSlowPollMax = 0;

  // Record reset time ...
  tLastStatisticsReset = TicksNow ();
}


const char *CBrownieLink::StatisticsStr (CString *ret, bool local) {
  CString s;
  TSocketHeader head;
  char *buf;
  int n, requestRetriesTotal, requestFailuresTotal, replyRetriesTotal, replyFailuresTotal;
  bool ok;

  // With local socket interface: Delegate to server ...
  if (twiIfType == ifSocket && !local) {

    // Send fetch request ...
    head.op = soStatFetch;
    head.status = brOk;
    head.adr = head.bytes = 0;
    ok = (Write (twiFd, &head, sizeof (head)) == sizeof (head));
    //~ if (!ok) INFO ("### Write not OK.");
    if (ok) ok = (Read (twiFd, &head, sizeof (head)) == sizeof (head));
    if (ok) {
      //~ INFO ("### Reading head OK.");
      buf = MALLOC (char, head.bytes + 1);
      ok = (Read (twiFd, buf, head.bytes) == (size_t) head.bytes);
      //~ if (ok) INFO ("### Reading data OK.");
      buf[head.bytes] = '\0';
      ret->SetO (buf);
    }
    if (!ok) {
      WARNINGF (("%s: Failed to fetch statistics from socket server", twiIfName.Get ()));
      ret->Clear ();
    }
  }

  // Determine statistics locally ...
  else {

    // Link statistics ...
    requestRetriesTotal = requestFailuresTotal = replyRetriesTotal = replyFailuresTotal = 0;
    for (n = 0; n < brEND; n++) {
      requestRetriesTotal += requestRetries[n];
      requestFailuresTotal += requestFailures[n];
      replyRetriesTotal += replyRetries[n];
      replyFailuresTotal += replyFailures[n];
    }
    ret->SetF ( "TWI Communication Statistics\n"
                "============================\n"
                "\n"
                "Sending                       | Fetching                     |\n"
                "Ops         Retries  Failures | Ops        Retries  Failures |\n"
                "------------------------------------------------------------------------------\n"
                "%9i %9i %9i |%9i %9i %9i | Reason\n"
                "------------------------------------------------------------------------------\n",
                requests, requestRetriesTotal, requestFailuresTotal,
                replies, replyRetriesTotal, replyFailuresTotal
              );
    for (n = 0; n < brEND; n++) {
      if (requestRetries[n] + requestFailures[n] + replyRetries[n] + replyFailures[n]) {
        ret->AppendF ("%19i %9i |%19i %9i |%3i %s\n",
                      requestRetries[n], requestFailures[n], replyRetries[n], replyFailures[n],
                      n, BrStatusStr ((EBrStatus) n));
      }
    }

    // Resources statistics ...
    if (rcIterations > 0) {
      ret->AppendF ("\n"
                    "Brownie Polling Statistics\n"
                    "==========================\n"
                    "\n"
                    "Time [ms]          |      Min.      Avg.      Max.\n"
                    "--------------------------------------------------\n"
                    "Full cycle         |%10i%10i%10i\n"
                    "Fast polling phase |%10i%10i%10i\n"
                    "Slow polling phase |%10i%10i%10i\n",
                    rcTCycleMin, (int) (rcTSumCycle / rcIterations), rcTCycleMax,
                    rcTFastPollMin, (int) (rcTSumFastPoll / rcIterations), rcTFastPollMax,
                    rcTSlowPollMin, (int) (rcTSumSlowPoll / rcIterations), rcTSlowPollMax
                  );
    }

    // Write source and time stamp ...
    ret->AppendF ("\nStatistics on '%s@%s<%i>' since %s.\n",
                  EnvInstanceName (), EnvMachineName (), EnvPid (),
                  TicksAbsToString (&s, tLastStatisticsReset));
  }

  // Done ...
  return ret->Get ();
}


void CBrownieLink::StatisticsAddIterateTimes (TTicksMonotonic tCycle, TTicksMonotonic tFastPoll, TTicksMonotonic tSlowPoll) {
  //~ INFOF (("### Stat times: %i/%i/%i", (int) tCycle, (int) tFastPoll, (int) tSlowPoll));
  rcIterations++;
  rcTSumCycle += tCycle;
  rcTSumFastPoll += tFastPoll;
  rcTSumSlowPoll += tSlowPoll;
  if (tCycle < rcTCycleMin) rcTCycleMin = tCycle;
  if (tCycle > rcTCycleMax) rcTCycleMax = tCycle;
  if (tFastPoll < rcTFastPollMin) rcTFastPollMin = tFastPoll;
  if (tFastPoll > rcTFastPollMax) rcTFastPollMax = tFastPoll;
  if (tSlowPoll < rcTSlowPollMin) rcTSlowPollMin = tSlowPoll;
  if (tSlowPoll > rcTSlowPollMax) rcTSlowPollMax = tSlowPoll;
}




// ***** Socket Server *****


bool CBrownieLink::ServerStart () {
  CString s;

  // Sanity ...
  ServerStop ();
  if (!envBrSocketName) return false;

  // Start socket ...
  EnvGetHome2lTmpPath (&s, envBrSocketName);
  sockListenFd = SocketServerStart (s.Get ());
  if (sockListenFd < 0) return false;

  // Success ...
  INFOF (("Starting socket server: %s", s.Get ()));
  return true;
}


void CBrownieLink::ServerStop () {
  CString s;

  FREEP(sockData);

  if (sockListenFd >= 0) {
    EnvGetHome2lTmpPath (&s, envBrSocketName);
    SocketServerStop (&sockListenFd, s.Get ());
    INFOF (("Stopped socket server: %s", s.Get ()));
  }
}


bool CBrownieLink::ServerIterate (TTicksMonotonic maxSleepTime) {
  CString s;

  // Accept new client ...
  if (sockClientFd < 0) {
    sockClientFd = SocketServerAccept (sockListenFd, envBrSocketName, true);
    if (sockClientFd >= 0) {
      // Success: Reset RX buffer ...
      sockRxBytes = 0;
    }
  }

  // Serve current client ...
  if (sockClientFd >= 0) {
    CSleeper sleeper;
    CString s;
    bool ok;

    sleeper.AddReadable (sockClientFd);
    sleeper.Sleep (maxSleepTime);
    if (sleeper.IsReadable (sockClientFd)) {

      // (Continue to) read incoming request ...
      ok = true;    // 'ok' denotes whether we could completely read the message
      if (sockRxBytes < sizeof (sockHead)) {
        // Header not yet read completely ...
        sockRxBytes += Read (sockClientFd, ((uint8_t *) &sockHead) + sockRxBytes, sizeof (sockHead) - sockRxBytes);
        ok = (sockRxBytes == sizeof (sockHead));
        if (ok) {
          FREEP (sockData);
          sockData = MALLOC (uint8_t, sockHead.bytes);
        }
      }
      if (ok) {     // header complete (may have been completed now or in a previous iteration) ...
        if (sockRxBytes < sizeof (sockHead) + sockHead.bytes
            && sockHead.op != soFetch) {  // special case: with 'soFetch', no bytes are delivered but to be delivered
          // Data not yet complete ...
          sockRxBytes += Read (sockClientFd, sockData + sockRxBytes - sizeof (sockHead),
                                             sizeof (sockHead) + sockHead.bytes - sockRxBytes);
          ok = (sockRxBytes == sizeof (sockHead) + sockHead.bytes);
        }
      }
      //~ INFOF (("### Socket readable: op = %i, bytes = %i, rxBytes = %i/%i",
              //~ sockRxBytes >= sizeof (sockHead) ? sockHead.op : -1,
              //~ sockRxBytes >= sizeof (sockHead) ? sockHead.bytes : -1,
              //~ sockRxBytes, sizeof (sockHead) + ((sockRxBytes >= sizeof (sockHead) && sockHead.op != soFetch) ? sockHead.bytes : 0)));

      // Handle request, if complete ...
      if (ok) switch (sockHead.op) {

        case soSend:
          sockHead.status = TwiSend (sockHead.adr, sockData, sockHead.bytes);
          sockHead.bytes = 0;
          ok = (Write (sockClientFd, &sockHead, sizeof (sockHead)) == sizeof (sockHead));
          break;

        case soFetch:
          //~ INFOF (("### Replying to 'soFetch': bytes = %i", (int) sockHead.bytes));
          //~ INFOF (("###   sockHead.op = %i, sockHead.adr = %i, sockHead.bytes = %i", (int) sockHead.op, (int) sockHead.adr, (int) sockHead.bytes));
          sockHead.status = TwiFetch (sockHead.adr, sockData, sockHead.bytes);
          ok = (Write (sockClientFd, &sockHead, sizeof (sockHead)) == sizeof (sockHead));
          if (ok) ok = (Write (sockClientFd, sockData, sockHead.bytes) == (size_t) sockHead.bytes);
          break;

        case soStatReset:
          StatisticsReset ();
          break;

        case soStatFetch:
          //~ INFO ("###   Replying to 'soStatFetch'");
          StatisticsStr (&s);
          sockHead.bytes = s.Len ();
          ok = (Write (sockClientFd, &sockHead, sizeof (sockHead)) == sizeof (sockHead));
          if (ok) ok = (Write (sockClientFd, s.Get (), sockHead.bytes) == (size_t) sockHead.bytes);
          break;

        default:
          WARNINGF(("%s: Received Illegal request", twiIfName.Get ()));
          ok = false;
      }

      // Close connection ...
      //   Note: We rely on the fact, that if the client closes the connection,
      //         we receive some kind of error in read(2), while at the same time,
      //         the 'sockClientFd' is considered readable by select(2).
      if (ok) sockRxBytes = 0;
      else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        close (sockClientFd);
        sockClientFd = -1;
        INFOF (("%s: Maintenance connection closed: %s",
                EnvGetHome2lTmpPath (&s, envBrSocketName), strerror (errno)));
      }
    }
  }

  // Done ...
  return (sockClientFd >= 0);
}





// ***** Communication *****


#define TWI_SEND_TRIES 3
#define TWI_FETCH_TRIES 3
#define TWI_FLUSH_TRIES 3


EBrStatus CBrownieLink::Open (const char *devName) {

  // Close if still open ...
  if (twiFd >= 0) TwiClose ();

  // Expand 'devName' into 'twiIfname' ...
  if (!devName) devName = envBrLinkDev;
  if (devName) if (devName[0] == '=' && devName[1] == '\0') devName = envBrSocketName;
  if (!devName) {
    WARNING ("No link device/socket specified for TWI (i2c) communication");
    twiIfName.Clear ();
    return brNoBus;
  }
  EnvGetHome2lTmpPath (&twiIfName, devName);

  // Open ...
  TwiOpen (true);
  status = (twiFd >= 0) ? brOk : brNoBus;
  return status;
}


void CBrownieLink::ClearBus () {
  const uint8_t data = 0xff;

  // Write 0xff to adress 127 ...
  if (TwiSend (127, &data, 1) != brOk)
    WARNINGF (("%s: Failed to write dummy package for bus clearing", twiIfName.Get ()));
}


void CBrownieLink::Flush (int adr) {
  EBrStatus st;
  TBrReply dummy;
  int n;

  // Fetch a reply...
  for (n = 0; n < TWI_FLUSH_TRIES; n++) {
    st = TwiFetch (adr, &dummy, sizeof (dummy));
    if (st != brNoDevice || st != brNoReply) break;
      // All other status codes indicate that we have successfully fetched a reply
      // and thus no reply may be pending.
  }
}


EBrStatus CBrownieLink::SendRequest (int adr, bool noResend) {
  int n, bytes;

  // Send message...
  requests++;
  BrRequestPackage (&request);
  bytes = BrRequestSize (request.op);

  // Try to send...
  for (n = 0; n < TWI_SEND_TRIES; n++) {
    if (n > 0) requestRetries[status]++;
    status = TwiSend (adr, &request, bytes);
    if (status == brOk || status == brNoBus || noResend) break;
      // We stop here ...
      //   a) on success,
      //   b) on general failure (no sense to retry),
      //   c) if resending is forbidden.
    //~ INFOF (("### CBrownieLink::SendRequest error: %s", BrStatusStr (status)));
    if (status != brNoDevice) Flush (adr);
      // Flush a reply eventually pending in the brownie
      // In the case of 'brNoDevice', we assume that the brownie has not noticed anything.
      // To skip flushing in this case, accelerates bus scanning with the ELV interface considerably!
  }

  // Done...
  if (status != brOk) requestFailures[status]++;
  return status;
}


EBrStatus CBrownieLink::FetchReply (int adr, bool noResend) {
  int n, bytes;

  // Get the reply...
  replies++;
  bytes = BrReplySize (request.op);

  // Try to fetch ...
  for (n = 0; n < TWI_FETCH_TRIES; n++) {

    // Not the first try ...
    if (n > 0) {
      if (noResend) break;
      replyRetries[status]++;
      SendRequest (adr);   // re-send the request
    }

    // Fetch ...
    status = TwiFetch (adr, &reply, bytes);
    if (status == brOk) status = BrReplyCheck (&reply, request.op, bytes);
      // Check message.
    if (status == brOk) status = (EBrStatus) (reply.status & 0x0f);
      // If everything worked fine so far: Set status to status reported by slave.
    if (status == brOk || status == brNoBus /* || status == brNoDevice */) break;
      // Success or general failure (no sense to retry).
      // Note: 'brNoDevice' is returned from TwiSend() and TwiFetch() and  if
      //       the system reports an I/O error via errno. This may be a time-out
      //       after a long waiting time, so a retry should be avoided.
      // TBD: Remove the last comment? 'brNoDevice' is mostly delivered after an adress NACK -> fast
      // TBD (better?): Treat I/O error as brNoBus?  Is this issue related to hanging after replugging the ELV interface?
  }

  // Done...
  if (status != brOk) replyFailures[status]++;
  return status;
}


EBrStatus CBrownieLink::Communicate (int adr, bool noResend) {
  SendRequest (adr, noResend);
  if (status == brOk) FetchReply (adr, noResend);
  return status;
}





// ***** Operations *****


EBrStatus CBrownieLink::CheckDevice (int adr, CBrownie *brownie) {
  uint32_t verBrownie, verHost;
  TBrFeatureRecord *fr;
  uint8_t val;

  if (!brownie) {

    // Short test: Just read BR_REG_MAGIC to test if there is a brownie ...
    RegRead (adr, BR_REG_MAGIC, &val);
    if (status == brOk)
      if (val != BR_MAGIC) status = brNoBrownie;
  }
  else {

    // Clear 'brownie'...
    brownie->Clear ();
    brownie->ConfigRecord ()->adr = (uint8_t) adr;

    // Read and check feature record (VROM) ...
    MemRead (adr, BR_MEM_ADR_VROM(0), sizeof (TBrFeatureRecord), (uint8_t *) brownie->FeatureRecord ());
    if (status == brOk)
      if (brownie->FeatureRecord ()->magic != BR_MAGIC) status = brNoBrownie;
    if (status != brOk) bzero (brownie->FeatureRecord (), sizeof (TBrFeatureRecord));

    // Check version and handle compatibility issues if the firmware version differs from the host software version ...
    if (status == brOk) {
      verBrownie = BrVersionGet (brownie->FeatureRecord ());
      if (verBrownie) {     // Test compilations have no / all-zero version (v0.0-0): Allow everything
        verHost = VersionGetOwn ();

        // Discard Brownies with firmware from the future ...
        if (verBrownie > verHost) {
          WARNINGF (("Firmware of brownie %03i is newer (%s) than that of the host (%s): Discarding device. Please upgrade your host software!",
                     adr, VersionToStr (GetTTS (), verBrownie), VersionGetOwnAsStr ()));
          status = brNoBrownie;
        }

        // Handle Brownies with older firmwares ...
        else if (verBrownie < VersionCompose (1, 1, 102)) {

          // Version 1.1.102 introduced major changes in the feature record:
          //   Restrict features and adapt record to allow firmware upgrades (but not more) ...
          WARNINGF (("Brownie %03i runs an incompatible firmware (%s): Disabling some features. Please upgrade the firmware!",
                    adr, VersionToStr (GetTTS (), verBrownie)));
          fr = brownie->FeatureRecord ();
          fr->features &= (BR_FEATURE_MAINTENANCE | BR_FEATURE_TIMER | BR_FEATURE_NOTIFY | BR_FEATURE_TWIHUB);
            // allow only core features
          fr->mcuType = fr->matDim;     // save 'mcuType', which was located in the place of 'matDim' in < v1.1.102
          bzero (&fr->gpiPresence, offsetof (TBrFeatureRecord, mcuType) - offsetof (TBrFeatureRecord, gpiPresence));
            // zero out everything else
        }
      }
    }

    // Read out brownie ID (EEPROM) ...
    if (status == brOk) {
      MemRead (adr, BR_MEM_ADR_EEPROM(BR_EEPROM_ID_BASE), BR_EEPROM_ID_SIZE, (uint8_t *) brownie->Id ());
      ((char *) brownie->Id ()) [BR_EEPROM_ID_SIZE-1] = '\0';   // to be on the safe side
    }

    // Read and check config record (EEPROM) ...
    if (status == brOk) {
      MemRead (adr, BR_MEM_ADR_EEPROM(BR_EEPROM_CFG_BASE), BR_EEPROM_CFG_SIZE, (uint8_t *) brownie->ConfigRecord ());
      if (status == brOk)
        if (brownie->ConfigRecord ()->magic != BR_MAGIC) status = brNoBrownie;
      if (status != brOk) {
        bzero (brownie->ConfigRecord (), sizeof (TBrConfigRecord));
        brownie->ConfigRecord ()->adr = (uint8_t) adr;
      }
    }
  }

  // Done...
  return status;
}


EBrStatus CBrownieLink::RegRead (int adr, uint8_t reg, uint8_t *retVal, bool noResend) {
  request.op = BR_OP_REG_READ (reg);
  status = Communicate (adr, noResend);
  if (status == brOk && retVal) *retVal = reply.regRead.val;
  return status;
}


uint8_t CBrownieLink::RegReadNext (EBrStatus *status, int adr, uint8_t reg, bool noResend) {
  uint8_t retVal = 0;

  if (*status == brOk) *status = RegRead (adr, reg, &retVal, noResend);
  return retVal;
}


EBrStatus CBrownieLink::RegWrite (int adr, uint8_t reg, uint8_t val, bool noResend) {
  request.op = BR_OP_REG_WRITE (reg);
  request.regWrite.val = val;
  return Communicate (adr, noResend);
}


void CBrownieLink::RegWriteNext (EBrStatus *status, int adr, uint8_t reg, uint8_t val, bool noResend) {
  if (*status == brOk) *status = RegWrite (adr, reg, val, noResend);
}


EBrStatus CBrownieLink::MemRead (int adr, unsigned memAdr, int bytes, uint8_t *retData, bool printProgress) {
  uint8_t *src;
  int ofs, hunk, blockAdr;

  blockAdr = memAdr >> BR_MEM_BLOCKSIZE_SHIFT;
  ofs = memAdr & (BR_MEM_BLOCKSIZE-1);   // Offset in first block

  status = brOk;
  while (bytes > 0 && status == brOk) {
    if (printProgress) {
      printf ("(%5i)\b\b\b\b\b\b\b", bytes);
      fflush (stdout);
    }
    request.op = BR_OP_MEM_READ (blockAdr);
    request.memRead.adr = (blockAdr & 0xff);
    status = Communicate (adr);
    if (status == brOk) {
      src = reply.memRead.data + ofs;
      hunk = MIN (BR_MEM_BLOCKSIZE - ofs, bytes);
      bytes -= hunk;
      while (hunk-- > 0) *(retData++) = *(src++);
      blockAdr++;
      ofs = 0;
    }
  }

  if (printProgress) printf ("       \b\b\b\b\b\b\b");
  return status;
}


EBrStatus CBrownieLink::MemWrite (int adr, unsigned memAdr, int bytes, uint8_t *data, bool printProgress) {
  // 'memAdr' must by aligned to units of BR_MEM_BLOCKSIZE.
  // If 'bytes' is not a multiple of BR_MEM_BLOCKSIZE (Flash: BR_FLASH_BLOCKSIZE),
  // the last block is filled (padded) with zeros.
  int blockAdr, blocks;

  blockAdr = memAdr / BR_MEM_BLOCKSIZE;
  ASSERT ((memAdr & (BR_MEM_BLOCKSIZE-1)) == 0);

  blocks = (bytes + BR_MEM_BLOCKSIZE - 1) / BR_MEM_BLOCKSIZE;
  if (BR_MEM_ADR_IS_FLASH(memAdr)) {
    ASSERT (BR_FLASH_PAGESIZE >= BR_MEM_BLOCKSIZE);
    blocks = blocks - 1 + BR_FLASH_PAGESIZE / BR_MEM_BLOCKSIZE - ((blocks - 1) % (BR_FLASH_PAGESIZE / BR_MEM_BLOCKSIZE));
  }

  status = brOk;
  while (blocks > 0 && status == brOk) {
    if (printProgress) {
      printf ("(%5i)\b\b\b\b\b\b\b", bytes);
      fflush (stdout);
    }
    request.op = BR_OP_MEM_WRITE (blockAdr);
    request.memWrite.adr = (blockAdr & 0xff);

    if (bytes > 0) memcpy (request.memWrite.data, data, MIN (BR_MEM_BLOCKSIZE, bytes));
    if (bytes < BR_MEM_BLOCKSIZE) bzero (&request.memWrite.data[bytes], BR_MEM_BLOCKSIZE - bytes);
    status = Communicate (adr);

    blockAdr++;
    blocks--;
    data += BR_MEM_BLOCKSIZE;
    bytes = (bytes < BR_MEM_BLOCKSIZE) ? 0 : bytes - BR_MEM_BLOCKSIZE;
  }

  if (printProgress) printf ("       \b\b\b\b\b\b\b");
  return status;
}
