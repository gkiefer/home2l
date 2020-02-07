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


#include "resources.H"
#include "phone.H"


#define LINPHONE_INTERVAL 64


ENV_PARA_SPECIAL ("doorman.<ID>.enable", bool, false);
  /* Define and enable a door phone with ID <ID>
   */

ENV_PARA_SPECIAL ("doorman.<ID>.linphonerc", const char *, NULL);
  /* Linphone RC file for door phone <ID> (Linphone backend only)
   *
   * With the Linphone backend, all settings not directly accessible by Home2L
   * settings are made in the (custom) Linphone RC file.
   */
ENV_PARA_SPECIAL ("doorman.<ID>.register", const char *, NULL);
  /* Phone registration string
   */
ENV_PARA_SPECIAL ("doorman.<ID>.secret", const char *, NULL);
  /* Phone registration password
   */
ENV_PARA_SPECIAL ("doorman.<ID>.rotation", int, 0);
  /* Phone camera rotation
   */

ENV_PARA_SPECIAL ("doorman.<ID>.buttonRc", const char *, NULL);
  /* External resource representing the bell button (optional; type must be 'bool')
   *
   * There are two options to connect to a door button, which is either by defining
   * an external resource using this parameter or by using the internal
   * resource \refrc{doorman-ID/button}. If the external resource is defined,
   * both resources are logically OR'ed internally.
   */
ENV_PARA_SPECIAL ("doorman.<ID>.buttonInertia", int, 2000); static const int envButtonInertiaDefault = 2000;
  /* Minimum allowed time (ms) between two button pushes (default = 2000)
   *
   * Button pushes are ignored if the previous push is less than this time ago.
   */

ENV_PARA_SPECIAL ("doorman.<ID>.dial", const char *, NULL);
  /* Default number to dial if the bell button is pushed
   */

ENV_PARA_SPECIAL ("doorman.<ID>.openerRc", const char *, NULL);
  /* External resource to activate if the opener signal is received (optional)
   */
ENV_PARA_SPECIAL ("doorman.<ID>.openerDuration", int, 1000); static const int envOpenerDurationDefault = 1000;
  /* Duration (ms) to activate the opener
   */
ENV_PARA_SPECIAL ("doorman.<ID>.openerHangup", int, 0); static const int envOpenerHangupDefault = 0;
  /* Time (ms) after which we hangup after the opener was activated (0 = no hangup)
   */




// ***************** CDoorPhone ****************************


class CDoorPhone: public CPhone {
  public:
    CDoorPhone () { id = NULL; rcDial = rcButton = /* rcOpener = */ rcExtButton = rcExtOpener = NULL; tHangup = tButtonPushed = -1; }
    virtual ~CDoorPhone () {}

    void Setup (const char *_id);     // read configuration settings and sets up 'this'

    virtual void OnPhoneStateChanged (EPhoneState oldState);
    virtual void OnInfo (const char *msg) { INFO (msg); }
    virtual void OnDtmfReceived (char dtmf);

    void Iterate ();

    const char *ToStr () { return id; }

  protected:

    // Environment settings...
    const char *id;
    CResource *rcExtButton, *rcExtOpener;
    int camRotation;
    TTicks openerDuration, openerHangup, buttonInertia;

    // Work variables...
    CRcEventDriver *driver;
    CResource *rcDial, *rcButton /*, *rcOpener */;
    CRcSubscriber subscriber;   // deprecated
    TTicks tHangup;         // time for auto-hangup (-1 = no auto-hangup)
    TTicks tButtonPushed;   // last time the button was pushed
};


void CDoorPhone::Setup (const char *_id) {
  CString fmt, agent, tmpDir, s;
  const char *lpLinphoneRcFileName, *identity, *secret, *uri, *envDial;

  id = _id;

  // Read phone-specific configuration settings...
  fmt.SetF ("doorman.%s.%%s", id);
  lpLinphoneRcFileName = EnvGetPath (StringF (&s, fmt.Get (), "linphonerc"), NULL, true);
  identity = EnvGetString (StringF (&s, fmt.Get (), "register"), true);
  secret = EnvGetString (StringF (&s, fmt.Get (), "secret"), true);
  camRotation = EnvGetInt (StringF (&s, fmt.Get (), "rotation"), 0);
  uri = EnvGetString (StringF (&s, fmt.Get (), "buttonRc"));
  rcExtButton = uri ? CResource::Get (uri) : NULL;
  buttonInertia = EnvGetInt (StringF (&s, fmt.Get (), "buttonInertia"), envButtonInertiaDefault);
  envDial = EnvGetString (StringF (&s, fmt.Get (), "dial"), true);
  uri = EnvGetString (StringF (&s, fmt.Get (), "openerRc"));
  rcExtOpener = uri ? CResource::Get (uri) : NULL;
  openerDuration = EnvGetInt (StringF (&s, fmt.Get (), "openerDuration"), envOpenerDurationDefault);
  openerHangup = EnvGetInt (StringF (&s, fmt.Get (), "openerHangup"), envOpenerHangupDefault);

  // Sanity checks...
  //   TBD

  // Setup phone...
  INFOF (("Setting up phone '%s' using '%s'...", id, lpLinphoneRcFileName));
  agent.SetF ("home2l-%s", id);
  EnvGetHome2lTmpPath (&tmpDir, agent.Get ());
  EnvMkTmpDir (tmpDir.Get ());
  CPhone::Setup (
      agent.Get (),
      pmAudio | pmVideoIn,
      envDebug >= 3,
      tmpDir.Get (),
      lpLinphoneRcFileName
    );
  if (identity) Register (identity, secret);
  SetCamRotation (camRotation);
  SetAutoAccept ();

  // Setup resources...
  s.SetF ("doorman-%s", _id);
  driver = RcRegisterDriver (s.Get (), rcsValid);
  rcButton = RcRegisterResource (driver, "button", rctBool, true);
  rcButton->SetDefault (false);
    /* [RC:doorman-ID] Virtual bell button of the specified doorphone
     *
     * Driving this resource to true or false is equivalent to pushing or releasing
     * a door bell button. To trigger a bell ring, a push and release event must
     * occur.
     * The "ID" in the driver name is replaced by the name of the declared phone.
     *
     * There are two options to connect to a door button, which is either by defining
     * an external resource using this parameter or by using the internal
     * resource \refenv{doorman.<ID>.buttonRc}.
     * Internally, both resources are logically OR'ed.
     */
  rcDial = RcRegisterResource (driver, "dial", rctString, true);
    /* [RC:doorman-ID] Number to dial for the specified doorphone
     *
     * This is the number dialed if the door button is pushed. The default value
     * is set to the configuration parameter \refenv{doorman.<ID>.dial}.
     * This resource allows to change the number to dial dynamically, for example,
     * in order to temporarily redirect door bell calls to a mobile phone when
     * out of home.
     * The "ID" in the driver name is replaced by the name of the declared phone.
     */
  if (envDial) rcDial->SetDefault (envDial);
  //~ rcOpener = RcRegisterResource (s.Get (), "opener", rctBool, true);  // [RC:-]
  //~ rcOpener->SetDefault (false);

  // Setup subscriber...
  //~ INFOF (("### Subscribing '%s'...", rcExtButton));
  subscriber.Register (_id);
  if (rcExtButton) subscriber.Subscribe (rcExtButton);
}


void CDoorPhone::Iterate () {
  CRcEvent ev;
  bool buttonPushed;
  CString dial;
  TTicks now;

  buttonPushed = false;

  // Handle pending driver events...
  while (driver->PollEvent (&ev)) {
    //~ INFOF (("# Driver event: %s", ev.ToStr ()));
    if (ev.Type () == rceDriveValue && ev.Resource ()->Is (rcButton)) {
      if (ev.ValueState ()->ValidBool (false) == true) buttonPushed = true;
    }
  }

  // Handle pending subscriber events...
  while (subscriber.PollEvent (&ev)) {   // new event?
    //~ INFOF (("# Subscriber event: %s", ev.ToStr ()));
    if (ev.Type () == rceValueStateChanged && ev.ValueState ()->IsValid () && ev.Resource ()->Is (rcExtButton)) {   // valid event on push button?
      if (ev.ValueState ()->Bool () == true) buttonPushed = true;
    }
  }

  // Handle button push...
  if (buttonPushed) {
    now = TicksNow ();
    if (buttonInertia > 0 && tButtonPushed > 0) {
      if (now < tButtonPushed + buttonInertia) buttonPushed = false;
    }
    tButtonPushed = now;
  }
  if (buttonPushed) {
    if (GetState () == psIdle) {     // Phone idle => dial...
      rcDial->ValidString (&dial);
      if (dial.IsEmpty ()) WARNINGF (("%s: No valid number to dial defined", id));
      else {
        INFOF (("# %s: Dialing '%s'", id, dial.Get ()));
        Dial (dial);
      }
    }
    else {                           // Else => hangup...
      INFOF (("# %s: Hanging up", id));
      Hangup ();
    }
  }

  // Check for auto-hangup...
  if (tHangup >= 0) if (TicksNow () >= tHangup) {
    INFOF (("# %s: Auto-Hanging up", id));
    Hangup ();
    tHangup = -1;
  }

  // Iterate underlying phone...
  CPhone::Iterate ();
}


void CDoorPhone::OnPhoneStateChanged (EPhoneState oldState) {
  CPhone::OnPhoneStateChanged (oldState);

  INFOF (("# %s: OnPhoneStateChanged", id));
  switch (state) {
    case psRinging:
      INFOF (("# ... call received from '%s' (should pick up and establish now)", GetPeerUrl ()));
      break;
    case psInCall:
      INFOF (("# ... call established."));
      break;
    case psIdle:
      INFOF (("# ... idle now."));
      break;
    default:
      break;
  }
}


void CDoorPhone::OnDtmfReceived (char dtmf) {
  TTicks now;

  if (dtmf == '#') {
    INFOF (("# %s: Opening door: %s", id, rcExtOpener));
    now = TicksNow ();
    //~ rcOpener->SetRequest (EnvInstanceName (), true, rcPrioNormal, 0, now + openerDuration);
    if (rcExtOpener) rcExtOpener->SetRequest (true, NULL, rcPrioNormal, 0, now + openerDuration);
    if (openerHangup) tHangup = now + openerHangup;     // init hangup timer
  }
}





// ***************** Phone management **********************


static CDict<CDoorPhone> phoneDict;


static void PhonesInit () {
  CSplitString splitVarName;
  CString s;
  const char *phoneId;
  int n, idx0, idx1;

  // Discover phone names...
  EnvGetPrefixInterval ("doorman.", &idx0, &idx1);
  for (n = idx0; n < idx1; n++) {
    splitVarName.Set (EnvGetKey (n), 3, ".");
    if (splitVarName.Entries () < 3) {
      WARNINGF (("Strange environment setting: '%s'", EnvGetKey (n)));
      continue;
    }
    phoneId = splitVarName.Get (1);
    if (EnvGetBool (StringF (&s, "doorman.%s.enable", phoneId)))     // Phone enabled?
      phoneDict.Set (phoneId, new CDoorPhone ());
  }

  // Initialize phones...
  for (n = 0; n < phoneDict.Entries (); n++)
    phoneDict.Get (n)->Setup (phoneDict.GetKey (n));
}


static void PhonesIterate (class CTimer *, void *) {
  for (int n = 0; n < phoneDict.Entries (); n++)
    phoneDict.Get (n)->Iterate ();
}





// ***************** main **********************************


int main (int argc, char **argv) {
  CTimer timer;

  EnvInit (argc, argv);
  RcInit (true);
  PhonesInit ();

  timer.Set (0, LINPHONE_INTERVAL, PhonesIterate);
  RcRun ();

  // We never get here...
  return 1;   // only to avoid compiler warning
}
