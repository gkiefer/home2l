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


#include <mpd/client.h>

#include "resources.H"


#define MPD_INTERVAL 256    // interval between MPD server polls





// *************************** Environment Parameters **************************


ENV_PARA_INT ("music.maxPaused", envDrvMpdMaxPaused, 1800);
  /* Maximum time [seconds] a player can stay in "paused" mode before it is stopped
   */



// ***** Shared with 'wallclock/app_music.C' (copied identically) *****


ENV_PARA_INT ("music.port", envDrvMpdDefaultPort, 6600);
  /* Default port for MPD servers
   */

ENV_PARA_SPECIAL ("music.<MPD>.host", const char *, NULL);
  /* Network host name and optionally port of the given MPD instance.
   *
   * This variable implicitly declares the server with its symbolic name <MPD>.
   * If no port is given, the default port is assumed.
   */

ENV_PARA_SPECIAL ("music.<MPD>.password", const char *, NULL);
  /* Password of the MPD instance (optional, NOT IMPLEMENTED YET)
   */





// *************************** CMpdMonitor *************************************


class CMpdMonitor: public CThread {
  public:
    CMpdMonitor (CRcDriver *drv, const char *_id);
    ~CMpdMonitor ();

    void Iterate ();

    const char *ToStr (CString *) { return id.Get (); }   // for use with 'CDict'

  protected:
    virtual void *Run ();    // thread routine to perform 'mpd_connection_new()'
    bool CheckError ();

    CString id, mpdHost;
    int mpdPort;
    CResource *rc;
    bool connecting;
    struct mpd_connection *mpdConnection;
    CServiceKeeper keeper;
    TTicks tStopPause;
};


CMpdMonitor::CMpdMonitor (CRcDriver *drv, const char *_id) {
  CString s;

  id.Set (_id);
  rc = drv->RegisterResource (_id, rctPlayerState, false);
  connecting = false;
  mpdConnection = NULL;
  tStopPause = NEVER;
  keeper.Setup (TICKS_FROM_SECONDS (1), TICKS_FROM_SECONDS (300), TICKS_FROM_SECONDS (10));
  if (EnvGetHostAndPort (StringF (&s, "music.%s.host", _id), &mpdHost, &mpdPort, envDrvMpdDefaultPort, true))
    keeper.Open ();
  DEBUGF (1, ("MPD: Registering '%s'... %s", _id, keeper.ShouldBeOpen () ? "success" : "failed!"));
}


CMpdMonitor::~CMpdMonitor () {
  keeper.Close ();
  while (keeper.IsOpen ()) Iterate ();
  if (connecting) Join ();
}


void *CMpdMonitor::Run () {
  ATOMIC_WRITE (mpdConnection, mpd_connection_new (mpdHost.Get (), mpdPort, 3000));    // timeout = 3000ms
  ASSERT (mpdConnection != NULL);   // 'mpdConnection == NULL' only occurs when out of memory
  return NULL;
}


bool CMpdMonitor::CheckError () {
  enum mpd_error mpdError;
  const char *msg;

  mpdError = mpd_connection_get_error (mpdConnection);
  if (mpdError != MPD_ERROR_SUCCESS) {
    msg = mpd_connection_get_error_message (mpdConnection);
    DEBUGF (1, ("MPD '%s' (%s:%i): %s", id.Get (), mpdHost.Get (), mpdPort, msg));
    mpd_connection_free (mpdConnection);
    mpdConnection = NULL;
  }
  return mpdConnection != NULL;
}


void CMpdMonitor::Iterate () {
  ERctPlayerState playerState;
  struct mpd_status *mpdStatus;

  // Open/close as requested by the keeper ...
  if (keeper.OpenAttemptNow ()) {
    if (!connecting) {
      //~ INFOF (("### %s: Start connecting...", id.Get ()));
      ASSERT (mpdConnection == NULL);
      connecting = true;
      Start ();
    }
    else if (mpdConnection != NULL) {
      //~ INFOF (("### %s: Connection attempt done.", id.Get ()));
      Join ();
      connecting = false;
      keeper.ReportOpenAttempt (CheckError ());
    }
  }
  if (keeper.CloseNow ()) {
    mpd_connection_free (mpdConnection);
    mpdConnection = NULL;
    keeper.ReportClosed ();
  }

  // Query MPD status ...
  mpdStatus = NULL;
  if (keeper.IsOpen ()) {
    mpdStatus = mpd_run_status (mpdConnection);
    if (!mpdStatus) {
      CheckError ();
      keeper.ReportLost ();
    }
  }

  // Report player state ...
  if (mpdStatus) {
    switch (mpd_status_get_state (mpdStatus)) {
      case MPD_STATE_PLAY:  playerState = rcvPlayerPlaying; break;
      case MPD_STATE_PAUSE: playerState = rcvPlayerPaused; break;
      case MPD_STATE_STOP:  default: playerState = rcvPlayerStopped; break;
    }
    rc->ReportValue (playerState);
  }
  else rc->ReportUnknown ();

  // Stop if paused for too long ...
  if (mpdStatus) {
    if (playerState == rcvPlayerPaused) {
      if (tStopPause == NEVER) tStopPause = TicksNow () + TICKS_FROM_SECONDS (envDrvMpdMaxPaused);
      else if (TicksNow () >= tStopPause) {
        mpd_run_stop (mpdConnection);
        tStopPause += 5000;   // in case the stopping failed: try again in 5 seconds
      }
    }
    else tStopPause = NEVER;
  }

  // Cleanup ...
  if (mpdStatus) mpd_status_free (mpdStatus);
}





// *************************** Top-Level ***************************************


static CDict<CMpdMonitor> mpdDict;
static CTimer mpdTimer;


static void DrvMpdTimerFunc (CTimer *, void *) {
  int n;

  for (n = 0; n < mpdDict.Entries (); n++)
    mpdDict[n]->Iterate ();
}


static inline void DrvMpdInit (CRcDriver *drv) {
  CSplitString splitVarName;
  const char *id;
  int n, idx0, idx1;

  // Discover MPD servers ...
  EnvGetPrefixInterval ("music.", &idx0, &idx1);
  for (n = idx0; n < idx1; n++) {
    splitVarName.Set (EnvGetKey (n), 4, ".");
    if (splitVarName.Entries () == 3) if (strcmp (splitVarName.Get (2), "host") == 0) {
      id = splitVarName.Get (1);
      mpdDict.Set (id, new CMpdMonitor (drv, id));
    }
  }

  // Start timer ...
  mpdTimer.Set (0, MPD_INTERVAL, DrvMpdTimerFunc);
}


static inline void DrvMpdDone () {
  mpdTimer.Clear ();
  mpdDict.Clear ();
}




// *************************** Driver Function *********************************


HOME2L_DRIVER(mpd) (ERcDriverOperation op, CRcDriver *drv, CResource *rc, CRcValueState *vs) {
  switch (op) {

    case rcdOpInit:
      DrvMpdInit (drv);
      break;

    case rcdOpStop:
      DrvMpdDone ();
      break;

    case rcdOpDriveValue:
      // Nothing to do
      break;
  }
}
