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


#include "resources.H"
#include "phone.H"


#define DOORMAN_INTERVAL 64
  // Interval (in ticks) for running 'CPhone::Iterate ()'.


ENV_PARA_STRING ("doorman.buttonRc", envButtonRc, NULL);
  /* External resource representing the bell button (optional; type must be 'bool')
   *
   * There are two options to connect to a door button, which is either by defining
   * an external resource using this parameter or by using the internal
   * resource \refrc{doorman/button}. If the external resource is defined,
   * both resources are logically OR'ed internally.
   */
ENV_PARA_INT ("doorman.buttonInertia", envButtonInertia, 2000);
  /* Minimum allowed time (in ms) between two button pushes
   *
   * Button pushes are ignored if the previous push is less than this time ago.
   */

ENV_PARA_STRING ("doorman.dial", envDial, NULL);
  /* Default number to dial if the bell button is pushed
   */

ENV_PARA_STRING ("doorman.openerRc", envOpenerRc, NULL);
  /* External resource to activate if the opener signal is received (optional)
   */
ENV_PARA_INT ("doorman.openerDuration", envOpenerDuration, 1000);
  /* Duration (in ms) to activate the opener
   */
ENV_PARA_INT ("doorman.openerHangup", envOpenerHangup, 3000);
  /* Time (in ms) after which we hangup after the opener was activated (0 = no automatic hangup)
   */




// ***************** CDoorPhone ****************************


class CDoorPhone: public CPhone, public CTimer {
  public:
    CDoorPhone ();
    virtual ~CDoorPhone () {}

    void Setup ();     // read configuration settings and set up 'this'

    virtual void OnPhoneStateChanged (EPhoneState oldState);
    virtual void OnInfo (const char *msg) { INFO (msg); }
    virtual void OnDtmfReceived (char dtmf);

    virtual void OnTime ();

  protected:

    // Environment settings...
    CResource *rcExtButton, *rcExtOpener;

    // Work variables...
    CRcEventDriver *driver;
    CResource *rcDial, *rcButton, *rcPhoneState;
    CRcSubscriber subscriber;
    TTicks tHangup;             // time for auto-hangup (-1 = no auto-hangup)
    TTicks tButtonPushed;       // last time the button was pushed
};


CDoorPhone::CDoorPhone () {
  rcDial = rcButton = rcPhoneState = rcExtButton = rcExtOpener = NULL;
  tHangup = tButtonPushed = -1;
}


void CDoorPhone::Setup () {
  CString tmpDir;

  // Read phone-specific configuration settings...
  rcExtButton = envButtonRc ? CResource::Get (envButtonRc) : NULL;
  rcExtOpener = envOpenerRc ? CResource::Get (envOpenerRc) : NULL;

  // Sanity checks...
  //   TBD

  // Setup phone...
  EnvGetHome2lTmpPath (&tmpDir, EnvInstanceName ());
  EnvMkTmpDir (tmpDir.Get ());
  CPhone::Setup (
      EnvInstanceName (),
      pmAudio | pmVideoIn,
      envDebug >= 3,
      tmpDir.Get ());
  Register ();
  SetAutoAccept ();

  // Setup resources...
  driver = RcRegisterDriver ("doorman", rcsValid);

  rcButton = RcRegisterResource (driver, "button", rctBool, true);
  rcButton->SetDefault (false);
    /* [RC:doorman] Virtual bell button of the specified doorphone
     *
     * Driving this resource to true or false is equivalent to pushing or releasing
     * a door bell button. To trigger a bell ring, a push and release event must
     * occur.
     *
     * There are two options to connect to a door button, which is either by defining
     * an external resource using this parameter or by using the internal
     * resource \refenv{doorman.buttonRc}.
     * Internally, both resources are logically OR'ed.
     */

  rcDial = RcRegisterResource (driver, "dial", rctString, true);
    /* [RC:doorman] Number to dial for the specified doorphone
     *
     * This is the number dialed if the door button is pushed. The default value
     * is set to the configuration parameter \refenv{doorman.dial}.
     * This resource allows to change the number to dial dynamically, for example,
     * in order to temporarily redirect door bell calls to a mobile phone when
     * out of home.
     */
  if (envDial) rcDial->SetDefault (envDial);

  //~ rcOpener = RcRegisterResource (s.Get (), "opener", rctBool, true);  // [RC:-]
  //~ rcOpener->SetDefault (false);

  rcPhoneState = RcRegisterResource (driver, "phone", rctPhoneState, false);
    /* [RC:doorman] Report phone state
     */
  rcPhoneState->ReportValue (rcvPhoneIdle);

  // Setup subscriber...
  //~ INFOF (("### Subscribing '%s'...", rcExtButton));
  subscriber.Register (EnvInstanceName ());
  if (rcExtButton) subscriber.Subscribe (rcExtButton);

  // Setup timer ...
  CTimer::Set (0, DOORMAN_INTERVAL);
}


void CDoorPhone::OnTime () {
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
    if (MILLIS_FROM_TICKS (envButtonInertia) > 0 && tButtonPushed > 0) {
      if (now < tButtonPushed + MILLIS_FROM_TICKS (envButtonInertia)) buttonPushed = false;
    }
    tButtonPushed = now;
  }
  if (buttonPushed) {
    if (GetState () == psIdle) {     // Phone idle => dial...
      rcDial->ValidString (&dial);
      if (dial.IsEmpty ()) WARNING ("No valid number to dial defined");
      else {
        INFOF (("Button pushed: Dialing '%s'", dial.Get ()));
        Dial (dial);
      }
    }
    else {                           // Else => hangup...
      INFO ("Button pushed: Hanging up");
      Hangup ();
    }
  }

  // Check for auto-hangup...
  if (tHangup >= 0) if (TicksNow () >= tHangup) {
    INFO ("No reply: Auto-hanging up");
    Hangup ();
    tHangup = -1;
  }

  // Iterate underlying phone...
  CPhone::Iterate ();
}


void CDoorPhone::OnPhoneStateChanged (EPhoneState oldState) {
  ERctPhoneState reportedState;

  // Call subclass ...
  CPhone::OnPhoneStateChanged (oldState);

  // Report to resource ...
  switch (state) {
    case psNone:
    case psIdle:                 ///< Phone is idle.
      reportedState = rcvPhoneIdle;
      break;
    case psRinging:
      reportedState = rcvPhoneRinging;
      break;
    default:
      reportedState = rcvPhoneInCall;
  }
  if (rcPhoneState) rcPhoneState->ReportValue (reportedState);

  //~ // Debugging ...
  //~ INFO ("# OnPhoneStateChanged");
  //~ switch (state) {
    //~ case psRinging:
      //~ INFOF (("# ... call received from '%s' (should pick up and establish now)", GetPeerUrl ()));
      //~ break;
    //~ case psInCall:
      //~ INFOF (("# ... call established."));
      //~ break;
    //~ case psIdle:
      //~ INFOF (("# ... idle now."));
      //~ break;
    //~ default:
      //~ break;
  //~ }
}


void CDoorPhone::OnDtmfReceived (char dtmf) {
  TTicks now;

  if (dtmf == '#') {
    INFOF (("# Opening door: %s", rcExtOpener->Uri ()));
    now = TicksNow ();
    //~ rcOpener->SetRequest (EnvInstanceName (), true, rcPrioNormal, 0, now + openerDuration);
    if (rcExtOpener) rcExtOpener->SetRequest (true, NULL, rcPrioNormal, 0, now + TICKS_FROM_MILLIS (envOpenerDuration));
    if (envOpenerHangup) tHangup = now + TICKS_FROM_MILLIS (envOpenerHangup);     // init hangup timer
  }
}





// ***************** main **********************************


int main (int argc, char **argv) {
  CDoorPhone phone;

  // Init Home2L ...
  EnvInit (argc, argv);
  RcInit (true);

  // Init phone ...
  phone.Setup ();

  // Main Home2L loop ...
  RcRun ();
  RcDone ();    // We only get here after an interrruption ...
  return 1;
}
