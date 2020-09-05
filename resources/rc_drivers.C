/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2020 Gundolf Kiefer
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


#include "rc_drivers.H"

#include <time.h>
#include <math.h>





// ***** Support for dynamic library loading / binary drivers *****


#if ANDROID
#define NO_DYNLIBS_REASON "in Android"
#endif

#ifdef NO_DYNLIBS_REASON
#define WITH_DYNLIBS 0
#else
#define WITH_DYNLIBS 1
#endif

#if WITH_DYNLIBS
#include <dlfcn.h>    // for dynamic library loading
#endif





// *************************** Driver 'timer' **********************************


ENV_PARA_BOOL ("rc.timer", envRcTimer, true);
  /* Enable/disable the 'timer' driver
   */



// ***** Twilight calculations *****


static CResource *rcTwiDay00, *rcTwiDay06, *rcTwiDay12, *rcTwiDay18;
  // Boolean flags indicating day time according to official sunrise/sunset (00) as well as to
  // civil (06), nautical (12), and astronomical (18) twilight.
static CResource *rcTwiSunrise, *rcTwiDawn06, *rcTwiDawn12, *rcTwiDawn18;
  // Exact time in seconds since the Epoch (1970-01-01-000000 UTC) for sunrise/dawn on the current day
static CResource *rcTwiSunset, *rcTwiDusk06, *rcTwiDusk12, *rcTwiDusk18;
  // Exact time in seconds since the Epoch (1970-01-01-000000 UTC) for sunset/dusk on the current day

static TTicks twiPhaseTimes[8];     // 0: dawn18, 1: dawn12, ... 3: sunrise, 4: sunset, 5: dusk06, ... 7: dusk18
static int twiPhase;


static void TwiRegisterResources (CRcDriver *drv) {
  rcTwiDay00 = RcRegisterResource (drv, "twilight/day", rctBool, false);
    /* [RC:timer] Flag to indicate day time (time between official sunset and sunrise)
     */
  rcTwiDay06 = RcRegisterResource (drv, "twilight/day06", rctBool, false);
    /* [RC:timer] Flag to indicate civil day time (time between civil dawn and dusk)
     */
  rcTwiDay12 = RcRegisterResource (drv, "twilight/day12", rctBool, false);
    /* [RC:timer] Flag to indicate nautical day time (time between nautical dawn and dusk)
     */
  rcTwiDay18 = RcRegisterResource (drv, "twilight/day18", rctBool, false);
    /* [RC:timer] Flag to indicate astronomical day time (time between astronomical dawn and dusk)
     */

  rcTwiSunrise = RcRegisterResource (drv, "twilight/sunrise", rctTime, false);
    /* [RC:timer] Today's official sunrise time
     */
  rcTwiDawn06 = RcRegisterResource (drv, "twilight/dawn06", rctTime, false);
    /* [RC:timer] Today's civil dawn time
     */
  rcTwiDawn12 = RcRegisterResource (drv, "twilight/dawn12", rctTime, false);
    /* [RC:timer] Today's nautical dawn time
     */
  rcTwiDawn18 = RcRegisterResource (drv, "twilight/dawn18", rctTime, false);
    /* [RC:timer] Today's astronomical dawn time
     */

  rcTwiSunset = RcRegisterResource (drv, "twilight/sunset", rctTime, false);
    /* [RC:timer] Today's official sunset time
     */
  rcTwiDusk06 = RcRegisterResource (drv, "twilight/dusk06", rctTime, false);
    /* [RC:timer] Today's civil dusk time
     */
  rcTwiDusk12 = RcRegisterResource (drv, "twilight/dusk12", rctTime, false);
    /* [RC:timer] Today's nautical dusk time
     */
  rcTwiDusk18 = RcRegisterResource (drv, "twilight/dusk18", rctTime, false);
    /* [RC:timer] Today's astronomical dusk time
     */
}


static void TwiCalculate (TDate d) {
  // To be run daily (preferrably shortly after midnight): Recalculate all twilight times for the given date
  /* The following calculations are based on the approximations and formulae from:
   *
   *    http://lexikon.astronomie.info/zeitgleichung
   *
   * Comments are in german to simplify referencing.
   */
  double dayOfYear, hTimeDiff, radDeclination, latitude, timeDelta, cosDelta, h;
  TTime t;
  TTicks ticksDelta[4], ticksTrueNoon;
  TTicks dawn[4], dusk[4];   // dawn and dusk times
  int n;

  // Day of the year (1 = Jan 1, ...)...
  dayOfYear = DateDiffByDays (d, DATE_OF(YEAR_OF(d), 1, 1)) + 1;
  //~ INFOF (("### dayOfYear = %f, d = %08x, year = %i, d0 = %08x", dayOfYear, d, YEAR_OF(d), DATE_OF(YEAR_OF(d), 0, 1)));

  // Difference sun time - local time ("Zeitgleichung")...
  hTimeDiff = -0.171 * sin (0.0337 * dayOfYear + 0.465) - 0.1299 * sin (0.01787 * dayOfYear - 0.168);
  //~ INFOF (("### Zeitgleichung = %f Minuten", hTimeDiff * 60.0));

  // Declination of the sun (~ latitude of zenith)...
  radDeclination = 0.4095 * sin (0.016906 * (dayOfYear - 80.086));
  //~ INFOF (("### Deklination: %f°", radDeclination * 180.0 / M_PI));

  // Time delta: time between sunrise and sunset (or the respective dawn and dusk times)...
  latitude = EnvLocationLatitudeN () * M_PI / 180.0;
  for (n = 0; n < 4; n++) {       // steps of 6 degree
    h = (n == 0 ? -50.0/60.0 : -6.0 * n) / 180.0 * M_PI;
    cosDelta = (sin (h) - sin(latitude) * sin (radDeclination)) / (cos (latitude) * cos (radDeclination));
    if (cosDelta > 1.0) cosDelta = 1.0;     // no day (~ polar night) -> crop
    if (cosDelta < -1.0) cosDelta = -1.0;   // always day (~ midsummer night) -> crop
    timeDelta = 12 * acos (cosDelta) / M_PI;
    ticksDelta[n] = TICKS_FROM_SECONDS (timeDelta * 3600.0 + 0.5);    // round to next second
  }

  // True (sun) noon time...
  ticksTrueNoon = TicksOfDate (YEAR_OF(d), 1, 1) + TICKS_FROM_SECONDS (86400 * (dayOfYear - 1) + 43200); // -> local noon time
  TicksToDateTimeUTC (ticksTrueNoon, NULL, &t);   // t is UTC time of local noon
  ticksTrueNoon += TICKS_FROM_SECONDS (TIME_OF (12, 0, 0) - t);  // -> UTC noon time
  //~ INFOF (("### clock noon (local time at UTC noon) = %s", TicksToString (ticksTrueNoon, 0)));
  ticksTrueNoon -= TICKS_FROM_SECONDS (3600.0 * (EnvLocationLongitudeE () / 15.0 + hTimeDiff) + 0.5);  // correct by location and time difference
  //~ INFOF (("### true noon = %s", TicksToString (ticksTrueNoon, 0)));

  // Report results...
  for (n = 0; n < 4; n++) {
    //~ CString s1, s2; INFOF (("### Day time, h = %i°: %s - %s", n * 6, TicksToString (&s1, ticksTrueNoon - ticksDelta[n], 0), TicksToString (&s2, ticksTrueNoon + ticksDelta[n], 0)));
    dawn[n] = ticksTrueNoon - ticksDelta[n];
    dusk[n] = ticksTrueNoon + ticksDelta[n];
  }

  for (n = 0; n < 4; n++) twiPhaseTimes[n] = dawn[3-n];
  for (n = 0; n < 4; n++) twiPhaseTimes[n+4] = dusk[n];
  //~ for (n = 0; n < 8; n++) INFOF (("###   twiPhaseTimes[%i] = %lli", n, twiPhaseTimes[n]));
  twiPhase = -1;

  rcTwiSunrise->ReportValue (dawn[0]);
  rcTwiDawn06->ReportValue (dawn[1]);
  rcTwiDawn12->ReportValue (dawn[2]);
  rcTwiDawn18->ReportValue (dawn[3]);
  rcTwiSunset->ReportValue (dusk[0]);
  rcTwiDusk06->ReportValue (dusk[1]);
  rcTwiDusk12->ReportValue (dusk[2]);
  rcTwiDusk18->ReportValue (dusk[3]);
}


static void TwiUpdateFlags (TTicks now) {
  int level;

  // Update the flag resources related to twilight...
  if (twiPhase > 7) return;
  if (twiPhase >= 0) if (twiPhaseTimes[twiPhase] > now) return;   // nothing new since last call

  // Advance 'twiPhase'...
  do { twiPhase++; } while (twiPhaseTimes[twiPhase] <= now && twiPhase <= 8);

  // Determine flags...
  level = (twiPhase <= 4) ? twiPhase : 8-twiPhase;
  //~ INFOF (("### now = %lli, twiPhase = %i, level = %i", now, twiPhase, level));
  rcTwiDay00->ReportValue (level > 3 ? true : false);
  rcTwiDay06->ReportValue (level > 2 ? true : false);
  rcTwiDay12->ReportValue (level > 1 ? true : false);
  rcTwiDay18->ReportValue (level > 0 ? true : false);
}



// ***** Standard timers & driver interface *****

static CTimer drvTimerTimer;

static CResource *rcNow = NULL, *rcDaily = NULL, *rcHourly = NULL, *rcMinutely;


static void DrvTimerUpdate (CTimer *, void *x = NULL) {
  TTicks now;
  static TDate lastD = -1;
  static TTime lastT = -1;
  TDate d;
  TTime t;
  TTicksMonotonic delay;

  // Get current time...
  now = TicksNow ();

  // Update 'rcNow'...
  rcNow->ReportValue ((now+500) - (now+500) % 1000);    // round to nearst full second

  // Update periodic triggers...
  TicksToDateTime (now, &d, &t);
  if (MINUTES_OF (t) != MINUTES_OF (lastT)) {
    rcMinutely->ReportTrigger ();
    if (HOURS_OF (t) != HOURS_OF (lastT)) {
      rcHourly->ReportTrigger ();
      if (d != lastD /* && t >= TIME_OF(3,0,0) */) {
        rcDaily->ReportTrigger ();
        TwiCalculate (d);
        lastD = d;
      }
    }
    TwiUpdateFlags (now);
    lastT = t;
  }

  // Calculate delay for the next timer...
  delay = 1000 - (now % 1000);
  drvTimerTimer.Reschedule (TicksMonotonicNow () + delay);
}


void RcDriverFunc_timer (ERcDriverOperation op, CRcDriver *drv, CResource *, CRcValueState *) {
  switch (op) {

    case rcdOpInit:
      rcNow = RcRegisterResource (drv, "now", rctTime, false);
        /* [RC:timer] Current time (updated once per second)
         */

      rcDaily = RcRegisterResource (drv, "daily", rctTrigger, false);
        /* [RC:timer] Triggers once per day (shortly after midnight)
         */
      rcHourly = RcRegisterResource (drv, "hourly", rctTrigger, false);
        /* [RC:timer] Triggers once per hour (at full hour)
         */
      rcMinutely = RcRegisterResource (drv, "minutely", rctTrigger, false);
        /* [RC:timer] Triggers once per minute (at full minute)
         */
      TwiRegisterResources (drv);

      drvTimerTimer.Set (0, 0, DrvTimerUpdate);   // Update once on initialization
      break;

    case rcdOpStop:
      drvTimerTimer.Clear ();
      break;

    case rcdOpDriveValue:
      // nothing to do: everything is read-only
      break;
  }
}





// *************************** External drivers ********************************


/*  Invokations:
 *      <exec> -init                             : Initialize driver, driver must report its properties
 *      <exec> -poll                             : Driver is polled for new readable values (not in "keep running" mode)
 *      <exec> -restart                          : Restart driver (only after abnormal stop), driver does not need to report anything
 *      <exec> -drive <resource LID> <value>     : Drive a value; The driver must report the result by "v" messages
 *
 *  Interpreted <exec> outputs:
 *
 *    a) Initialization phase
 *
 *      d <resource LID> <options>      : declare resource
 *      p <poll interval>               : define the polling interval (0 = no polling; Default = no polling)
 *      .                               : initialization complete - enter polling mode
 *      :                               : initialization complete/restarting - enter "keep going" mode
 *
 *      The initialization phase must be completed as quickly as possible in the beginning.
 *
 *    b) Active phase
 *
 *      v <resource LID> <value/state>  : report a value/state
 *      p <poll interval>               : change the polling interval
 */


ENV_PARA_SPECIAL ("drv.<id>", const char *, NULL);
  /* Declare/load an external (binary or script-based) driver
   *
   * The argument <arg> may be one out of:
   * \begin{itemize}[a)]
   *   \item The name of a driver .so file (binary driver).
   *   \item The invocation of a script, including arguments.
   *   \item A '1', in which case <id> is used as <arg> (shortcut to enable binary drivers).
   *   \item If set to '0', the driver setting is ignored.
   * \end{itemize}
   *
   * Relative paths <name> are searched in:
   * \begin{itemize}
   *   \item <HOME2L\_ROOT>/etc[/<ARCH>]
   *   \item <HOME2L\_ROOT>/lib/<ARCH>/home2l-drv-<name>.so
   *   \item <HOME2L\_ROOT>/lib/<ARCH>/home2l-drv-<name>
   *   \item <HOME2L\_ROOT>/lib[/<ARCH>]
   *   \item <HOME2L\_ROOT>/
   * \end{itemize}
   *
   * Please refer to the \hyperref[sec:resources-drvdev-external]{section on writing external drivers}
   * in for further information on script-based drivers.
   */
ENV_PARA_INT ("rc.drvMinRunTime", envMinRunTime, 3000);
  /* Minimum run time of a properly configured external driver (ms)
   *
   * To avoid endless busy loops caused by drivers crashing repeatedly on their startup
   * (e.g. due to misconfiguration), a driver crashed on startup is not restarted
   * immediately again, but only after some delay.
   *
   * This is the time after which a crash is not handled as a startup crash.
   */
ENV_PARA_INT ("rc.drvCrashWait", envCrashWait, 60000);
  /* Waiting time (ms) after a startup crash before restarting an external driver
   *
   * To avoid endless busy loops caused by drivers crashing repeatedly on their startup
   * (e.g. due to misconfiguration), a driver crashed on startup is not restarted
   * immediately again, but only after some delay.
   *
   * This parameter specifies the waitung time.
   */
ENV_PARA_INT ("rc.drvMaxReportTime", envExtReportTime, 5000);
  /* Maximum time (ms) to wait until all external drivers have reported their resources
   */
ENV_PARA_INT ("rc.drvIterateWait", envIterateWait, 1000);
  /* Iteration interval (ms) for the manager of external drivers
   */


enum EExtDriverCmd {
  cmdQuit = 0,
  cmdIterate,         // (drv) Check if process is still alive and perform actions
  cmdInvokeInit,
  cmdInvokePoll,
  cmdInvokeRestart,
};


struct TExtDriverCmdRec {
  EExtDriverCmd cmd;
  class CExtDriver *drv;
};


class CExtDriver: public CRcDriver {
  public:
    CExtDriver (const char *_lid, const char *_shellCmd);

    bool InitComplete () { return initComplete; }

    static void ClassInit ();     // must be called before the first object is created
    static void ClassStart ();    // to be called to finalize the initialization phase
    static void ClassStop ();

    void PutCmd (EExtDriverCmd cmd, TTicksMonotonic t = 0, TTicksMonotonic interval = 0);

    static void ThreadRoutine ();

    void OnShellReadable ();            // [T:ext] invoked on shell event
    void OnIterate ();                  // [T:ext] invoked regularly each 'envIterateWait' milliseconds (or on demand)
    void OnInvoke (EExtDriverCmd cmd);  // [T:ext] invoked by a queued command

  protected:
    virtual void DriveValue (CResource *rc, CRcValueState *vs);

    // Dynamic object data ([T:ext], unless noted otherwise)...
    CExtDriver *next;
    CString shellCmd;
    bool initComplete;    // initialization complete, all resources declared
    bool keepRunning;     // true: tool keeps running after init; write values are written to stdin of script: '<exec name> set <rc LID> <options>'
                          // false: tool is restarted for each value change or each polling cycle, values are passed as arguments: '<exec name> set <rc LID> <options>'
    int pollInterval;     // polling interval in seconds
    bool pollPending;     // the polling interval has passed, but the shell was not available yet
    CTimer pollTimer;

    CShellBare shell;
    bool shellInUse;
    TTicks tStart;        // only valid if 'shellInUse == true'

    CMutex assignSetMutex;
    CDictFast<CRcValueState> assignSet;  // [T:any] set of pending assignments; key is 'CResource::Lid ()'

    // Class data...
    static CThread thread;          // [T:ext]
    static CExtDriver *first;       // [T:main / const]
    static CSleeper sleeper;        // [T:main / const]
    static CTimer iterateTimer;     // [T:main / const]
};



// ***** Implementation *****


CExtDriver *CExtDriver::first = NULL;
CThread CExtDriver::thread;
CSleeper CExtDriver::sleeper;
CTimer CExtDriver::iterateTimer;


static void *CExtDriverThreadRoutine (void *) {
  CExtDriver::ThreadRoutine ();
  return NULL;
}


static void CExtDriverIterateTimerCallback (CTimer *, void *) {
  ((CExtDriver *) NULL)->PutCmd (cmdIterate);
}


static void CExtDriverPollTimerCallback (CTimer *, void *data) {
  CExtDriver *eDrv = (CExtDriver *) data;
  eDrv->PutCmd (cmdInvokePoll);
}


CExtDriver::CExtDriver (const char *_lid, const char *_shellCmd): CRcDriver (_lid) {

  // Initialize variables...
  shellCmd.Set (_shellCmd);
  initComplete = keepRunning = false;
  pollInterval = 0;
  shellInUse = false;
  pollPending = false;

  // Add to linked list...
  next = first;
  first = this;

  // Schedule init command (init)...
  PutCmd (cmdInvokeInit);
}


void CExtDriver::ClassInit () {
  sleeper.EnableCmds (sizeof (TExtDriverCmdRec));
  thread.Start (CExtDriverThreadRoutine);
  iterateTimer.Set (envIterateWait, envIterateWait, CExtDriverIterateTimerCallback);
}


void CExtDriver::ClassStart () {
  CExtDriver *drv;
  int tMaxWait;

  // Wait until all external drivers have completed their initialization...
  tMaxWait = envExtReportTime;
  for (drv = first; drv && tMaxWait > 0; drv = drv->next) {
    while (!drv->InitComplete () && tMaxWait > 0) {
      Sleep (64);
      tMaxWait -= 64;
    }
  }
  for (drv = first; drv && tMaxWait > 0; drv = drv->next) {
    if (!drv->InitComplete ())
      WARNINGF(("Resource driver '%s' has not properly initialized itself - please fix the driver or disable it. Unexpected things may happen now."));
  }
}


void CExtDriver::ClassStop () {
  static const TExtDriverCmdRec crQuit = { cmdQuit, NULL };
  CExtDriver *drv;

  // Stop all timers...
  iterateTimer.Clear ();
  for (drv = first; drv; drv = drv->next) {
    drv->pollTimer.Clear ();
    CTimer::DelByCreator (drv);
  }

  // Quit thread...
  if (thread.IsRunning ()) {
    sleeper.PutCmd (&crQuit);
    thread.Join ();
  }
}


void CExtDriver::PutCmd (EExtDriverCmd cmd, TTicksMonotonic t, TTicksMonotonic interval) {
  TExtDriverCmdRec cr;

#if WITH_CLEANMEM
  bzero (&cr, sizeof (cr));
#endif
  cr.cmd = cmd;
  cr.drv = this;
  //~ INFOF(("### PutCmd (%i)", cmd));
  sleeper.PutCmd (&cr, t, interval);
}


void CExtDriver::DriveValue (CResource *rc, CRcValueState *vs) {
  ASSERT (vs && vs->IsValid ());

  assignSetMutex.Lock ();
  assignSet.Set (rc->Lid (), vs);
  PutCmd (cmdIterate);
  assignSetMutex.Unlock ();
  vs->SetState (rcsBusy);
}


void CExtDriver::OnShellReadable () {
  CString s, line;
  CSplitString arg;
  CResource *rc = NULL;
  CRcValueState vs;
  bool ok = false;

  //~ INFO("# OnShellReadable");
  while (shell.ReadLine (&line)) {
    //~ INFOF (("From '%s': %s", Lid (), line.Get ()));
    line.Strip ();
    arg.Set (line.Get (), 5);
    switch (line [0]) {

      case 'd': case 'D':    // d <resource LID> <type> (ro|wr) [ <default value> [ <default request attrs> ] ] : declare resource
        if (initComplete) {
          WARNINGF (("Declaration of a new resource after the initialization phase by driver '%s' - ignoring: %s", Lid (), line.Get ()));
          break;
        }
        ok = (arg.Entries () >= 4);
        if (ok) {
          rc = CResource::Register (this, arg[1], StringF (&s, "%s %s", arg[2], arg[3]));   // [RC:-] External drivers must document themselves
          ok = (rc != NULL);
          if (ok && arg.Entries () == 5) {
            CRcRequest req;
            req.SetPriority (rcPrioDefault);
            req.SetFromStr (arg[4]);
          }
        }
        break;

      case 'p': case 'P':   // p <poll interval>            : set polling interval
        ok = (arg.Entries () == 2);
        if (ok) ok = IntFromString (arg[1], &pollInterval);
        if (ok) {
          if (pollInterval > 0) pollTimer.Set (0, TICKS_FROM_SECONDS (pollInterval), CExtDriverPollTimerCallback, this);
          else pollTimer.Clear ();
        }
        break;

      case '.':             // initialization complete - enter polling mode
        initComplete = true;
        keepRunning = false;
        ok = true;
        break;

      case ':':             // initialization/restarting complete - enter "keep running" mode
        initComplete = true;
        keepRunning = true;
        ok = true;
        break;

      case 'v': case 'V':   // v <rcLid> ?|([~]<value>)  : report a value/state
        ok = (arg.Entries () == 3);
        if (ok) {
          rc = GetResource (arg[1]);
          ok = (rc != NULL);
        }
        if (ok) {
          vs.SetType (rc->Type ());
          ok = vs.SetFromStrFast (arg[2]);
          if (!ok) {
            WARNINGF (("Illegal value '%s' received - invalidating: '%s'", arg[2], line.Get ()));
            vs.Clear ();
            ok = true;
          }
        }
        if (ok)
          rc->ReportValueState (&vs);
        break;

      default:
        ok = false;
    }
    if (!ok) WARNINGF (("Illegal line received - ignoring: '%s'", line.Get ()));
  }
}


void CExtDriver::OnIterate () {
  CString s;
  TTicks tNow;
  int n;

  tNow = TicksNow ();

  // Check if process has died or exited ...
  if (shellInUse) {
    if (!shell.IsRunning ()) {
      shell.Wait ();
      shellInUse = false;
      if (keepRunning) {
        // A "keep running" process has died just now...
        WARNINGF (("Driver process '%s' died unexpectedly", Lid ()));
        if (tNow - tStart >= envMinRunTime) PutCmd (cmdInvokeRestart);
        else PutCmd (cmdInvokeRestart, TicksMonotonicNow () + envCrashWait);
      }
    }
  }

  // Process the 'assignSet'...
  assignSetMutex.Lock ();
  if (assignSet.Entries ()) {
    if (keepRunning) {
      if (shell.IsRunning ())
        for (n = 0; n < assignSet.Entries (); n++)
          shell.WriteLine (StringF (&s, "%s %s", assignSet.GetKey (n), assignSet.Get (n)->ToStr ()));
    }
    else {
      if (!shellInUse) {
        if (shell.Start (StringF (&s, "%s -drive %s %s", shellCmd.Get (), assignSet.GetKey (0), assignSet.Get (0)->ToStr ()))) {
          shellInUse = true;
          tStart = TicksNow ();
          assignSet.Del (0);
        }
      }
    }
  }
  assignSetMutex.Unlock ();

  // Trigger a new poll if one pending and shell is idle...
  if (!keepRunning && !shellInUse) PutCmd (cmdInvokePoll);
}


void CExtDriver::OnInvoke (EExtDriverCmd cmd) {
  CString s;
  const char *fmt;

  // Make command-dependent error checks and create format...
  switch (cmd) {
    case cmdInvokeInit:
      ASSERT (!shellInUse);
      fmt = "%s -init";
      break;
    case cmdInvokePoll:
      ASSERT (!keepRunning);
      if (shellInUse) {
        if (pollPending) WARNINGF (("Failed to poll driver process '%s': still running"));
          // Another poll is still pending: Warn because there may be a problem with the driver.
          // This one will be (reasonably) discarded.
        pollPending = true;
        return;
      }
      else {
        fmt = "%s -poll";
        pollPending = false;
      }
      break;
    case cmdInvokeRestart:
      ASSERT (!shellInUse);
      fmt = "%s -restart";
      break;
    default:
      ASSERT(false);
  };

  // Start shell command ...
  shellInUse = shell.Start (StringF (&s, fmt, shellCmd.Get ()));
  if (shellInUse) tStart = TicksNow ();
}



void CExtDriver::ThreadRoutine () {
  TExtDriverCmdRec cr;
  CExtDriver *drv;
  bool running;

  running = true;
  while (running) {

    // Sleep...
    //~ INFO("### Sleeping");
    sleeper.Prepare ();
    for (drv = first; drv; drv = drv->next) {
      drv->OnShellReadable ();            // Iterate shell
      sleeper.AddReadable (drv->shell.ReadFd ());
    }
    sleeper.Sleep ();
    //~ INFO("### Woke up");

    // Check commands...
    if (sleeper.GetCmd (&cr)) switch (cr.cmd) {
      case cmdQuit:
        running = false;
        break;
      case cmdIterate:
        if (cr.drv) cr.drv->OnIterate ();
        else for (drv = first; drv; drv = drv->next) drv->OnIterate ();
        break;
      case cmdInvokeInit:
      case cmdInvokePoll:
      case cmdInvokeRestart:
        cr.drv->OnInvoke (cr.cmd);
        break;
      default:
        ASSERT (false);
    }
  }

  // Stop & cleanup the processes; unregister all resources...
  for (drv = first; drv; drv = drv->next)
    if (drv->shellInUse) drv->shell.Kill ();
  for (drv = first; drv; drv = drv->next) {
    drv->shell.Wait ();
    drv->shellInUse = false;
    drv->ClearResources ();
  }
}





// *************************** Top-level functions *****************************


static CRcDriver *signalDriver = NULL;


static const char *drvSearchPath[] = {
  // format arguments: 1 = HOME2L_ROOT, 2 = arch, 3 = name
  "%1$s/etc/%3$s",
  "%1$s/etc/%2$s/%3$s",
  "%1$s/lib/%2$s/home2l-drv-%3$s.so",
  "%1$s/lib/home2l-drv-%3$s",
  "%1$s/lib/%2$s/%3$s",
  "%1$s/lib/%3$s",
  "%1$s/%3$s"
};


void RcDriversInit () {
  CDictFast<CString> drvDict;
  CSplitString args;
  CString s, cmd;
  const char *id, *cmdStr;
  CRcDriver *drv;
  int n, k, idx0, idx1;
  bool found, haveExternals, isBinary;
#if WITH_DYNLIBS
  void *dlHandle;
  FRcDriverFunc *driverFunc = NULL;
  const char *errStr;
#endif
  bool ok;

  // Register all internal drivers...
  signalDriver = new CRcDriver ("signal");
  signalDriver->Register ();
  if (envRcTimer) CRcDriver::RegisterAndInit ("timer", RcDriverFunc_timer);

  // Make a list of all binary and external drivers...
  //   Loading binary drivers may change the environment (i.e. add new statically
  //   declared variables). For this reason, we create a list of all drivers now.
  EnvGetPrefixInterval ("drv.", &idx0, &idx1);
  for (n = idx0; n < idx1; n++) {
    id = EnvGetKey (n) + 4;                   // 4 == strlen ("drv.") !!
    //~ INFOF (("### Driver #%i: '%s'", n, EnvGetKey (n)));
    if (strchr (id, '.') != NULL) continue;   // Skip keys like "drv.<id>.something"
    cmdStr = EnvGetVal (n);
    if (cmdStr[1] == '\0') {
      if (cmdStr[0] == '0') continue;         // driver is disabled
      if (cmdStr[0] == '1') cmdStr = id;      // driver is enabled with a "true" => set ID as name
    }
    s.SetC (cmdStr);
    drvDict.Set (id, &s);
  }

  // Register all binary and external drivers...
  haveExternals = false;
  for (n = 0; n < drvDict.Entries (); n++) {
    id = drvDict.GetKey (n);
    cmdStr = drvDict.Get (n)->Get ();
    //~ INFOF (("### Driver #%i: '%s'", n, id));

    // Expand (the first component of) command to an absolute path ...
    if (cmdStr[0] != '/') {   // relative path given?

      // Split the command and perform a path search...
      args.Set (cmdStr, 2);
      found = false;
      if (args.Entries () > 0) for (k = 0; k < (int) (sizeof (drvSearchPath) / sizeof (drvSearchPath[0])) && !found; k++) {
        s.SetF (drvSearchPath[k], EnvHome2lRoot (), EnvBuildArch (), args[0]);
        //~ INFOF (("### Trying '%s'", s.Get ()));
        if (access (s.Get (), R_OK) == 0) found = true;
      }
      if (!found) {
        WARNINGF(("Unable to find driver '%s' <%s>", id, cmdStr));
        continue;
      }

      // Re-assemble the command...
      if (args.Entries () > 1)
        cmd.SetF ("%s %s", s.Get (), args[1]);
      else
        cmd.SetO (s.Disown ());
      cmdStr = cmd.Get ();
    }
    isBinary = (strcmp (cmdStr + strlen (cmdStr) - 3, ".so") == 0);     // cmd ends with ".so"?

    // Sanity ...
    //   We check for a redefinition here. Otherwise strange (distracting) error messages may occor on dlopen().
    ok = true;
    if (RcGetDriver (id) == NULL)
      INFOF(("Registering %s driver '%s' <%s>", isBinary ? "binary" : "script", id, cmdStr));
    else {
      WARNINGF(("Redefinition of driver '%s' - skipping driver from config file.", id));
      ok = false;
    }

    // Handle binary driver...
    if (isBinary) {
#if WITH_DYNLIBS
      ok = true;
      // Load shared library ...
      dlHandle = dlopen (cmdStr, RTLD_NOW); // 'RTLD_NOW': resolve all undefined symbols in the library now (alt: RTLD_LAZY)
      if (!dlHandle) {
        WARNINGF (("Unable to open shared library: %s", dlerror ()));
        ok = false;
      }
      // Set/pre-initialize environment parameters...
      //   NOTE: This assumes that the shared library's '_init' function has been called by 'dlopen()', which
      //         calls the constructors of all static C++ objects. This behaviour may be Linux-specific.
      CEnvPara::GetAll ();
      // Get driver function...
      if (ok) {
        dlerror (); // clear error
        driverFunc = (FRcDriverFunc *) dlsym (dlHandle, StringF (&s, "Home2lRcDriverFunc_%s", id));
        errStr = dlerror ();
        if (errStr) {
          WARNINGF (("Shared library does not appear to be a Home2L driver: %s", errStr));
          ok = false;
        }
      }
      // Register the driver...
      if (ok) CRcDriver::RegisterAndInit (id, driverFunc);
#else
      WARNINGF (("Binary drivers are not supported " NO_DYNLIBS_REASON " - skipping '%s'.", id));
#endif
    }

    // Handle external (script) driver...
    else {
      if (!haveExternals) {
        CExtDriver::ClassInit ();     // only init if there are any external drivers
        haveExternals = true;
      }
      drv = new CExtDriver (id, cmdStr);
      drv->Register ();
    }
  }
}


void RcDriversStart () {
  CExtDriver::ClassStart ();
}


void RcDriversStop () {
  int n;

  CExtDriver::ClassStop ();     // stop all external drivers
  for (n = 0; n < driverMap.Entries (); n++) driverMap.Get (n)->Stop ();
}


void RcDriversDone () {
#if WITH_CLEANMEM
  int n;
  while ( (n = driverMap.Entries ()) > 0) driverMap.Get (n-1)->Unregister ();
#else
  driverMap.Clear ();
#endif
}


CResource *RcDriversAddSignal (const char *name, ERcType type) {
  ASSERT (signalDriver != NULL);
  //~ INFOF (("### RcDriversAddSignal ('%s', type = %i)", name, type));
  return CResource::Register (signalDriver, name, type, true);  // [RC:-] Signals must be documented by themselves
}


CResource *RcDriversAddSignal (const char *name, CRcValueState *vs) {
  CResource *rc;

  ASSERT (vs != NULL && signalDriver != NULL);
  //~ INFOF (("### RcDriversAddSignal ('%s', %s)", name, vs->ToStr ()));
  rc = CResource::Register (signalDriver, name, vs->Type (), true);  // [RC:-] Signals must be documented by themselves
  if (vs->IsValid ()) rc->SetDefault (vs);
  return rc;
}
