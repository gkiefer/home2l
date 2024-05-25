/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2024 Gundolf Kiefer
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


#include "ui_widgets.H"
#include "system.H"
#include "apps.H"

#include "floorplan.H"
#include "alarmclock.H"

#include <resources.H>

#include <stdio.h>
#include <time.h>





/* The following options select whether the resources visualized on the home
 * are kept subscribed even if the home screen is not visualized. By default,
 * the resources are unsibscribed whenever the home screen is left and subscribed
 * again when acitvated again. This may lead to a visible delay each time the
 * user returns to the home screen.
 */
#define SUBSCRIBE_PERMANENTLY 0
  // Set to keep resource subscriptions for the home screen permanently (never unsubscribe).
#define SUBSCRIBE_WHEN_SCREEN_ON 1
  // Set to keep the resource subscription for the home screen as long as the screen
  // is on (active or standby), but unsubscribe if the screen is switched off.





// *************************** Environment variables ***************************


ENV_PARA_STRING ("ui.sysinfoCmd", envSysinfoCmd, "bin/h2l-sysinfo.sh");
  /* Name of the sytem information script
   *
   * This script is executed repeatedly and its out displayed when the
   * user opens the "about" screen.
   */

ENV_PARA_STRING ("ui.sysinfoHost", envSysinfoHost, NULL);
  /* Host on which the system information script is executed
   *
   * If set, the system information script is executed on the given remote host.
   */

ENV_PARA_STRING ("ui.accessPointRc", envAccessPointRc, NULL);
  /* Resource (boolean) for local (wifi) access point status display
   */
ENV_PARA_STRING ("ui.bluetoothRc", envBluetoothRc, "/local/ui/bluetooth");
  /* Resource (boolean) for local bluetooth status display
   */

ENV_PARA_STRING ("ui.outdoorTempRc", envOutdoorTempRc, "/alias/weather/temp");
  /* Resource (temp) representing the outside temperature for the right info area (outdoor)
   */
ENV_PARA_STRING ("ui.outdoorData1Rc", envOutdoorData1Rc, NULL);
  /* Resource for the upper data field of the right info area (outdoor)
   */
ENV_PARA_STRING ("ui.outdoorData2Rc", envOutdoorData2Rc, NULL);
  /* Resource for the lower data field of the right info area (outdoor)
   */

ENV_PARA_STRING ("ui.indoorTempRc", envIndoorTempRc, NULL);
  /* Resource (temp) representing the outside temperature for the right info area (indoor)
   */
ENV_PARA_STRING ("ui.indoorData1Rc", envIndoorData1Rc, NULL);
  /* Resource for the upper data field of the right info area (indoor)
   */
ENV_PARA_STRING ("ui.indoorData2Rc", envIndoorData2Rc, NULL);
  /* Resource for the lower data field of the right info area (indoor)
   */

ENV_PARA_STRING ("ui.meterSolarPower", envMeterSolarPower, "/alias/electrical/solarPower");
  /* Resource (power) representing the solar power for the energy meter)
   */
ENV_PARA_STRING ("ui.meterExtPower", envMeterExtPower, "/alias/electrical/extPower");
  /* Resource (power) representing the externally delivered power for the energy meter)
   *
   * The value may be negative if the house is delivering electrical power.
   * Usually, the sum of \refenv{ui.solarPower} and \refenv{ui.extPower} is
   * the power consumed inside the house.
   */
ENV_PARA_FLOAT ("ui.meterMin", envMeterMin, -2930.0);
  /* Minimum value shown by the energy meter
   */
ENV_PARA_FLOAT ("ui.meterMax", envMeterMax, +3570.0);
  /* Maximum value shown by the energy meter
   */

ENV_PARA_STRING ("ui.radarEyeRc", envRadarEyeRc, "/alias/weather/radarEye");
  /* Resource for the radar eye as provided by the 'home2l-weather' driver
   */

ENV_PARA_INT ("ui.motionDetectorRetention", envMotionRetention, 300000);
  /* Retention time (ms) of the motion detector display (OBSOLETE)
   */



// OBSOLETE...
ENV_PARA_STRING ("ui.radarEye.host", envRadarEyeHost, NULL);
  /* Host to run 'ui.radarEye.cmd' on. (OBSOLETE: Use string from URI "/alias/ui/radarEye" instead)
   */
ENV_PARA_STRING ("ui.radarEye.cmd", envRadarEyeCmd, "cat $HOME2L_ROOT/tmp/weather/radarEye.pgm");
  /* Command to obtain .pgm file for the radar eye. (OBSOLETE: Use string from URI "/alias/ui/radarEye" instead)
   */

#define URI_RADAREYE_TRIGGER "/alias/ui/radarEyeTrigger"    // OBSOLETE





// *************************** Global variables ********************************


static class CScreenHome *scrHome = NULL;
static class CScreenInfo *scrInfo = NULL;

static CTimer animationTimer;
static SDL_Surface *surfDroids = NULL, *surfDroidsGrey = NULL;





// *************************** CWidgedMultiData ********************************


class CWidgetMultiData: public CFlatButton {
  // Widget cleverly display a temparature and/or two supplemental data values.
  public:
    CWidgetMultiData ();

    void SetResources (CResource *_rcMain, CResource *_rcSub1, CResource *_rcSub2);

    void SubscribeAll (CRcSubscriber *subscr);

    void Iterate ();

    void OnRcEvent (CResource *rc);    // To be called on any 'rceValueStateChanged' event
    virtual void OnPushed (bool longPushed);

  protected:
    CResource *rcData[3];
    bool rcChanged[3];
    bool showSub;
    TTicks tLastPushed;
};


CWidgetMultiData::CWidgetMultiData () {
  rcData[0] = rcData[1] = rcData[2] = NULL;
  rcChanged[0] = rcChanged[1] = rcChanged[2] = false;
  showSub = false;
  tLastPushed = NEVER;
}


void CWidgetMultiData::SetResources (CResource *_rcMain, CResource *_rcSub1, CResource *_rcSub2) {
  rcData[0] = _rcMain;
  rcData[1] = _rcSub1;
  rcData[2] = _rcSub2;
  rcChanged[0] = rcChanged[1] = rcChanged[2] = true;

  //~ SetLabelAlignment (1);    // right-justify labels
}


void CWidgetMultiData::SubscribeAll (CRcSubscriber *subscr) {
  for (int n = 0; n < 3; n++) subscr->AddResource (rcData[n]);
}


void CWidgetMultiData::Iterate () {
  CRcValueState vs[3];
  char buf[32];
  SDL_Surface *surf = NULL, *surf2 = NULL;
  SDL_Rect r;
  TTF_Font *font;
  bool valid[3], _showSub;
  int n, temp;

  // Return if nothing may have changed...
  if (!rcChanged[0] && !rcChanged[1] && !rcChanged[2] && !showSub && tLastPushed == NEVER) return;

  // Capture resources...
  for (n = 0; n < 3; n++) {
    if (rcData[n]) {
      rcData[n]->GetValueState (&vs[n]);
      valid[n] = vs[n].IsKnown ();
    }
    else valid[n] = false;
  }
  if (vs[0].Type () != rctTemp) valid[0] = false; // main field must be a temperature

  // Determine view to present...
  _showSub = false;                               // Default: main view
  if (tLastPushed != NEVER) if (TicksNowMonotonic () > tLastPushed + 10000)
    tLastPushed = NEVER;                          // Return to main view after time-out ...
  if (tLastPushed != NEVER) {                     // Sub view selected by button
    _showSub = true;
  }
  if (!valid[0]) _showSub = true;                 // Nothing to see in main view => force sub view
  if (!valid[1] && !valid[2]) _showSub = false;   // Nothing to see in sub view => force main view

  // Return if no visible change happened ...
  if (_showSub == showSub) {
    if ((!_showSub && !rcChanged[0]) ||
        (_showSub && !rcChanged[1] && !rcChanged[2])) return;
  }
  showSub = _showSub;

  // Draw the (main) view...
  surf = NULL;
  if (!showSub) {
    if (valid[0]) {

      // Main text...
      font = FontGet (fntLight, 96);
      temp = (int) round (vs[0].UnitFloat (rctTemp) * 10);
      sprintf (buf, (temp < -9 || temp >= 0) ? "%i°C" : "-%i°C", temp / 10);
      surf = FontRenderText (font, buf, WHITE);

      // Add fractional part in smaller font...
      sprintf (buf, ".%i", (temp < 0 ? -temp : temp) % 10);
      LangTranslateNumber (buf);
      r = Rect (surf);
      r.x = r.w - FontGetWidth (font, "°C") - 4;
      r.h -= 12;
      font = FontGet (fntLight, 32);
      surf2 = FontRenderText (font, buf, WHITE);
      SurfaceBlit (surf2, NULL, surf, &r, -1, 1);
      SurfaceFree (surf2);

      // Mark as updated ...
      rcChanged[0] = false;
    }
  }

  // Draw the (sub) view...
  else {
    font = FontGet (fntLight, 32);
    surf = CreateSurface (area.w - BUTTON_LABEL_BORDER, 96);
    SDL_FillRect (surf, NULL, ToUint32 (TRANSPARENT));

    if (valid[1]) {
      vs[1].ToHuman (buf, sizeof (buf));
      surf2 = FontRenderText (font, buf, WHITE); // LIGHT_GREY);
      SurfaceBlit (surf2, NULL, surf, NULL, 0, -1);
      SurfaceFree (surf2);
    }

    if (valid[2]) {
      vs[2].ToHuman (buf, sizeof (buf));
      surf2 = FontRenderText (font, buf, WHITE); // LIGHT_GREY);
      SurfaceBlit (surf2, NULL, surf, NULL, 0, 1);
      SurfaceFree (surf2);
    }
  }

  // Pass label with ownership ...
  SetLabel (surf, NULL, true);

  // Mark as updated ...
  rcChanged[1] = rcChanged[2] = false;
}


void CWidgetMultiData::OnRcEvent (CResource *rc) {
  for (int n = 0; n < 3; n++) if (rcData[n] == rc) rcChanged[n] = true;
}


void CWidgetMultiData::OnPushed (bool longPushed) {
  if (tLastPushed == NEVER) tLastPushed = TicksNowMonotonic ();
  else tLastPushed = NEVER;
  INFOF (("### CWidgetMultiData::OnPushed(): tLastPushed = %i", (int) tLastPushed));
  Iterate ();
}





// *************************** CWidgetEnergyMeter ******************************


class CWidgetEnergyMeter: public CWidget {
  // Widget for the (solar) energy meter.
  public:
    CWidgetEnergyMeter ();
    ~CWidgetEnergyMeter ();

    void SetArea (SDL_Rect _area);
    void SetResources (CResource *_rcSolarPower, CResource *_rcExtPower);
    void SubscribeAll (CRcSubscriber *subscr);
    void OnRcEvent (CResource *rc);    // To be called on any 'rceValueStateChanged' event

    void Iterate ();

  protected:
    CResource *rcSolarPower, *rcExtPower;
    SDL_Surface *surf;
    bool rcChanged;
};


CWidgetEnergyMeter::CWidgetEnergyMeter () {
  rcSolarPower = rcExtPower = NULL;
  surf = NULL;
  rcChanged = true;
}


CWidgetEnergyMeter::~CWidgetEnergyMeter () {
  SurfaceFree (&surf);
}


void CWidgetEnergyMeter::SetArea (SDL_Rect _area) {
  SurfaceFree (&surf);
  CWidget::SetArea (_area);
}


void CWidgetEnergyMeter::SetResources (CResource *_rcSolarPower, CResource *_rcExtPower) {
  rcSolarPower = _rcSolarPower;
  rcExtPower = _rcExtPower;
}


void CWidgetEnergyMeter::SubscribeAll (CRcSubscriber *subscr) {
  subscr->AddResource (rcSolarPower);
  subscr->AddResource (rcExtPower);
}


void CWidgetEnergyMeter::OnRcEvent (CResource *rc) {
  if (rc == rcSolarPower || rc == rcExtPower) rcChanged = true;
}


void CWidgetEnergyMeter::Iterate () {
  float scaleMin = envMeterMin, scaleMax = envMeterMax;
  SDL_Rect r;
  float valSolarPower, valExtPower;
  float pos0, posDperW, pos, posD;

  if (!rcChanged) return;

  // Get updated values ...
  valSolarPower = valExtPower = NAN;
  if (rcSolarPower) rcSolarPower->GetValue (&valSolarPower);
  if (rcExtPower) rcExtPower->GetValue (&valExtPower);

  //~ static int count = 0;
  //~ if (count++ > 20 && count < 30) valSolarPower = valExtPower = NAN;
  //~ valSolarPower = 5500.0;
  //~ valExtPower = -1200.0;

  // Draw surface ...
  if (isnan (valSolarPower) && isnan (valExtPower)) {

    // No numbers Make meter disappear ...
    if (surf) SurfaceFree (&surf);
  }
  else {

    // At least one number is defined ...
    if (isnan (valSolarPower)) valSolarPower = 0.0;         // assert no solar power
    if (isnan (valExtPower)) valExtPower = -valSolarPower;  // assert no power consumption

    // Adapt scale limits (shift, then zoom out as necessary) ...
    if (scaleMax < valSolarPower) {
      // shift both limits right ...
      scaleMin += (valSolarPower - scaleMax);
      scaleMax = valSolarPower;
    }
    if (scaleMin > -valExtPower) {
      // shift both limits left ...
      scaleMax += (-valExtPower - scaleMin);
      scaleMin = -valExtPower;
    }
    if (valSolarPower > scaleMax) scaleMax = valSolarPower;
      // the upper limit is too high again
      // => lower just the upper in order to scale

    // Init surface ...
    if (!surf) surf = CreateSurface (area.w, area.h);
    SurfaceFill (surf, BLACK);

    // Calculate geometry parameters ...
    posDperW = ((float) area.w) / (scaleMax - scaleMin);
    pos0 = posDperW * -scaleMin;

    r.y = 0;
    r.h = area.h;

    // Draw bars ...

    // ... solar plus ...
    r.x = round (pos0) + 2;   // "+2": ommit main tick mark
    r.w = round (-valExtPower * posDperW) - 2;
    if (r.w > 0) SurfaceFillRect (surf, &r, GREY);

    // ... consumption ...
    r.x += r.w;
    r.w = round ((valSolarPower + valExtPower) * posDperW);
    SurfaceFillRect (surf, &r, LIGHT_GREY);

    // Draw tick marks ...

    // ... main (zero) mark ...
    r.w = 3;
    r.x = round (pos0) - 1;
    SurfaceFillRect (surf, &r, WHITE);

    // ... side marks ...
    posD = posDperW * 1000.0;
    r.w = 3;
    for (pos = pos0 - posD; pos >= 0.0; pos -= posD) {
      r.x = round (pos) - 1;
      SurfaceFillRect (surf, &r, BLACK);
    }
    for (pos = pos0 + posD; pos < area.w; pos += posD) {
      r.x = round (pos) - 1;
      SurfaceFillRect (surf, &r, BLACK);
    }
  }

  // Done ...
  SetSurface (surf);
}







// *************************** CScreenHome *************************************

#define INFO_H 128    // must match height of radar eye and floorplan (FP_HEIGHT)
#define INFO_Y (UI_RES_Y - UI_BUTTONS_HEIGHT - INFO_H - 16)

#define CLOCK_Y 0
#define CLOCK_H (INFO_Y - 32 - CLOCK_Y)
#define CLOCK_W 1024        // clock is centered (so there is no parameter 'CLOCK_X')

#define ALARM_H 160
#define ALARM_W 160
#define ALARM_X (UI_RES_X - ALARM_W - 16)
#define ALARM_Y 32

#define RADIOS_X 16
#define RADIOS_Y 16
#define RADIO_W 72
#define RADIO_H 72

#define METER_H 12


#define RADAR_W 128             // must match INFO_H!
#define RADAR_H 128
#define RADAR_Y INFO_Y

#define FLOORPLAN_W FP_WIDTH    // must match INFO_H!
#define FLOORPLAN_H INFO_H
#define FLOORPLAN_Y INFO_Y

#define OUTDOOR_W ((UI_RES_X - RADAR_W - FLOORPLAN_W - 32) / 2)
#define OUTDOOR_H INFO_H
#define OUTDOOR_Y INFO_Y

#define INDOOR_W ((UI_RES_X - RADAR_W - FLOORPLAN_W - 32) / 2)
#define INDOOR_H INFO_H
#define INDOOR_Y INFO_Y

// Layout: outdoor - radar - indoor - floorplan ...
#define OUTDOOR_X 0
#define RADAR_X (OUTDOOR_X + OUTDOOR_W)
#define FLOORPLAN_X (RADAR_X + RADAR_W + 32)
#define INDOOR_X (FLOORPLAN_X + FLOORPLAN_W)




#define RADAREYE_DIRECT 1   // 0 = read radar eye from file (OBSOLETE); 1 = read from string resource


class CScreenHome: public CScreen {
  public:
    CScreenHome ();
    virtual ~CScreenHome ();

    void Setup ();    // requires 'srcInfo' to be defined before

    void Iterate (SDL_Surface *surfDroids, SDL_Rect *srcRect);
    void OnButtonPushed (CButton *btn, bool longPush);

    // Callbacks...
    virtual void Activate (bool on = true);

  protected:

    // Helpers...
    void SubscribeAll ();

    // General...
    CRcSubscriber subscr;
#if SUBSCRIBE_WHEN_SCREEN_ON && !SUBSCRIBE_PERMANENTLY
    ESystemMode lastSystemMode;
#endif

    // Launcher button bar...
    CFlatButton btnDroid, btnMail, btnWeb, btnAndroid;    // Fixed/built-in launcher buttons
    CFlatButton btnAppLaunch[appIdEND];                   // App launcher buttons

    // Time, date and alarm clock...
    CFlatButton btnTime, btnAlarmClock;
    CWidget wdgDate, wdgSecs;
    SDL_Surface *surfTime, *surfSecs, *surfDate;
    TDate lastDt;
    TTime lastTm;

    // Radios...
    CFlatButton btnAccessPoint, btnBluetooth;
    CResource *rcAccessPoint;

    // Energy meter ...
    CWidgetEnergyMeter wdgEnergyMeter;

    // Data displays (outdoor/left, indoor/right)...
    CWidgetMultiData wdgDataOutdoor, wdgDataIndoor;

    // Radar eye...
    CFlatButton btnRadarEye;
    CResource *rcRadarEye;
    CNetpbmReader radarEyeReader;
#if !RADAREYE_DIRECT
    CShellSession radarEyeShell;
    bool radarEyePending;
#endif

    // Mini floorplan...
    CWidgetFloorplan wdgFloorplan;
};



static void CbAndroidLaunch (CButton *, bool, void *data) {
  const char *appStr = (const char *) data;
  if (appStr) SystemLaunchApp (appStr);
  //~ if (appStr) INFOF (("### CbAndroidLaunch ('%s')", appStr));
}


#define MAX_BUTTONS (appIdEND + 10)


ENV_PARA_NOVAR ("ui.launchMail", const char *, envLaunchMail, NULL);
  /* Android intent to launch a mail program (optional, Android only)
   *
   * Only if set, a launch icon is shown on the home screen.
   */
ENV_PARA_NOVAR ("ui.launchWeb", const char *, envLaunchWeb, NULL);
  /* Android intent to launch a web browser (optional, Android only)
   *
   * Only if set, a launch icon is shown on the home screen.
   */
ENV_PARA_NOVAR ("ui.launchDesktop", const char *, envLaunchDesktop, false);
  /* If true, the home screen gets an icon to launch the Android desktop (Android only)
   */
ENV_PARA_NOVAR ("ui.launchWeather", const char *, envLaunchWeather, NULL);
  /* Android intent to launch a weather app (optional, Android only).
   *
   * If set, it will be launched if the weather area or radar eye are pushed.
   */


BUTTON_TRAMPOLINE (CbScreenHomeOnButtonPushed, CScreenHome, OnButtonPushed);


CScreenHome::CScreenHome () {
  rcRadarEye = NULL;
  surfTime = surfSecs = surfDate = NULL;
#if SUBSCRIBE_WHEN_SCREEN_ON && !SUBSCRIBE_PERMANENTLY
  lastSystemMode = smNone;
#endif
}


CScreenHome::~CScreenHome () {
  AlarmClockSetButton (NULL);
  SurfaceFree (&surfTime);
  SurfaceFree (&surfSecs);
  SurfaceFree (&surfDate);
}


void CScreenHome::SubscribeAll () {
  wdgEnergyMeter.SubscribeAll (&subscr);
  wdgDataOutdoor.SubscribeAll (&subscr);
  wdgDataIndoor.SubscribeAll (&subscr);
  subscr.Subscribe (rcRadarEye);
  subscr.Subscribe (rcAccessPoint);
}


void CScreenHome::Setup () {
  int fmtButtons [MAX_BUTTONS];
  CWidget *buttonWdgs[MAX_BUTTONS];
  SDL_Rect *layout;
  const char *str;
  int n, buttons;

  // Button bar: System button...
  buttons = 0;
  btnDroid.SetCbPushed (CbActivateScreen, (void *) scrInfo);
  btnDroid.SetHotkey (SDLK_i);
  buttonWdgs[buttons++] = &btnDroid;

  // Button bar: Applet launchers...
  for (n = 0; n < (int) appIdEND; n++) if (n != appIdHome && AppEnabled ((EAppId) n)) {
    AppCall ((EAppId) n, appOpLabel, &btnAppLaunch[n]);
    btnAppLaunch[n].SetCbPushed (CbAppActivate, (void *) (intptr_t) n);
    buttonWdgs[buttons++] = &btnAppLaunch[n];
  }

  // Button bar: Android launchers...
  if ( (str = EnvGet (envLaunchMailKey)) ) {
    btnMail.SetLabel (COL_APP_LABEL, "ic-email-48", _("Mail"), fntAppLabel);
    btnMail.SetCbPushed (CbAndroidLaunch, (void *) str);
    buttonWdgs[buttons++] = &btnMail;
  }
  if ( (str = EnvGet (envLaunchWebKey)) ) {
    btnWeb.SetLabel (COL_APP_LABEL, "ic-www-48", _("WWW"), fntAppLabel);
    btnWeb.SetCbPushed (CbAndroidLaunch, (void *) str);
    buttonWdgs[buttons++] = &btnWeb;
  }
  if ( (EnvGetBool (envLaunchDesktopKey, false)) ) {
    btnAndroid.SetLabel (COL_APP_LABEL, "ic-android-48", _("Android"), fntAppLabel);
    btnAndroid.SetCbPushed (CbScreenHomeOnButtonPushed, this);
    buttonWdgs[buttons++] = &btnAndroid;
  }

  // Layout button bar...
  fmtButtons[0] = UI_BUTTONS_BACKWIDTH;
  for (n = 1; n < buttons; n++) fmtButtons[n] = -1;
  fmtButtons[buttons] = 0;
  layout = LayoutRow (UI_BUTTONS_RECT, fmtButtons);
  for (n = 0; n < buttons; n++) {
    buttonWdgs[n]->SetArea (layout[n]);
    AddWidget (buttonWdgs[n]);
  }
  free (layout);

  // Time, date and alarm clock ...
  lastDt = DATE_OF(0, 0, 0);
  lastTm = TIME_OF(99, 0, 0);
  btnTime.SetCbPushed (CbScreenHomeOnButtonPushed, this);
  AddWidget (&btnTime);
  AddWidget (&wdgSecs);
  AddWidget (&wdgDate);
  btnAlarmClock.SetArea (Rect (ALARM_X, ALARM_Y, ALARM_W, ALARM_H));
  btnAlarmClock.SetHotkey (SDLK_a);
  AddWidget (&btnAlarmClock);
  AlarmClockSetButton (&btnAlarmClock);

  // Radios...
  btnAccessPoint.SetArea (Rect (RADIOS_X, RADIOS_Y, RADIO_W, RADIO_H));
  btnAccessPoint.SetCbPushed (CbScreenHomeOnButtonPushed, this);

  rcAccessPoint = CResource::Get (envAccessPointRc);

  btnBluetooth.SetArea (Rect (RADIOS_X, RADIOS_Y + RADIO_H, RADIO_W, RADIO_H));
  btnBluetooth.SetLabel (COL_APP_LABEL_LIVE, "ic-bluetooth-48");
  btnBluetooth.SetCbPushed (CbScreenHomeOnButtonPushed, this);

  // Energy meter ...
  wdgEnergyMeter.SetArea (Rect (0, 0, UI_RES_X, METER_H));
  wdgEnergyMeter.SetResources (RcGet (envMeterSolarPower), RcGet (envMeterExtPower));
  AddWidget (&wdgEnergyMeter);

  // Data displays ...
  wdgDataOutdoor.SetArea (Rect (OUTDOOR_X, OUTDOOR_Y, OUTDOOR_W, OUTDOOR_H));
  wdgDataOutdoor.SetResources (RcGet (envOutdoorTempRc), RcGet (envOutdoorData1Rc), RcGet (envOutdoorData2Rc));
  AddWidget (&wdgDataOutdoor);

  wdgDataIndoor.SetArea (Rect (INDOOR_X, INDOOR_Y, INDOOR_W, INDOOR_H));
  wdgDataIndoor.SetResources (RcGet (envIndoorTempRc), RcGet (envIndoorData1Rc), RcGet (envIndoorData2Rc));
  AddWidget (&wdgDataIndoor);

  // Radar eye ...
  btnRadarEye.SetArea (Rect (RADAR_X, RADAR_Y, RADAR_W, RADAR_H));
  btnRadarEye.SetHotkey (SDLK_w);
  btnRadarEye.SetCbPushed (CbAndroidLaunch, (void *) EnvGet (envLaunchWeatherKey));
#if RADAREYE_DIRECT
  rcRadarEye = CResource::Get (envRadarEyeRc);
#else
  rcRadarEye = CResource::Get (URI_RADAREYE_TRIGGER);
  radarEyePending = true;     // Try to load a radar eye right in the beginning.
#endif

  // Mini floorplan ...
  wdgFloorplan.Setup (FLOORPLAN_X, FLOORPLAN_Y);
  wdgFloorplan.SetHotkey (SDLK_f);
  if (wdgFloorplan.IsOk ()) AddWidget (&wdgFloorplan);

  // Subscribe to resources ...
  subscr.Register ("homescreen");

#if SUBSCRIBE_PERMANENTLY
  SubscribeAll ();
  wdgFloorplan.Activate ();
#endif
}


void CScreenHome::Iterate (SDL_Surface *surfDroids, SDL_Rect *srcRect) {
  CString s;
  char buf[64];
  TTicks now;
  TDate dt;
  TTime tm;
  SDL_Rect frame, r;
  TTF_Font *font;
  TColor col;
  CRcEvent ev;
  CResource *rc;
  CRcValueState *vs;

  // (Un-)subscribe to resources if appropriate...
#if SUBSCRIBE_WHEN_SCREEN_ON && !SUBSCRIBE_PERMANENTLY
  ESystemMode systemMode = SystemGetMode ();
  if (systemMode != lastSystemMode) {
    if (systemMode >= smStandby && lastSystemMode < smStandby) {
      SubscribeAll ();
      wdgFloorplan.Activate ();
    }
    else if (systemMode < smStandby && lastSystemMode >= smStandby) {
      subscr.Clear ();
      FloorplanUnsubscribeAll ();
    }
    lastSystemMode = systemMode;
  }
#endif

  // Return if not active...
  if (!IsActive ()) return;

  // Droid animation...
  btnDroid.SetLabel (surfDroidsGrey, srcRect);

  // Time & date area ...
  frame = Rect (0, CLOCK_Y, UI_RES_X, CLOCK_H);

  now = TicksNow ();
  TicksToDateTime (now, &dt, &tm);

  // ... Time (HH:MM) ...
  if (MINUTES_OF (tm) != MINUTES_OF (lastTm)) {
    font = FontGet (fntLight, 256);
    //~ font = FontGet (fntLight, 192);
    snprintf (buf, sizeof (buf), "%i:%02i", HOUR_OF (tm), MINUTE_OF (tm));
    SurfaceSet (&surfTime, FontRenderText (font, buf, WHITE)); // , BLACK));
    r = Rect (surfTime);
    RectAlign (&r, Rect (frame.x, frame.y + CLOCK_H * 1/8, CLOCK_W * 13/16, CLOCK_H * 6/8), 1, 1);
    r.y += (r.h - CLOCK_H * 6/8) / 2;
    r.h = CLOCK_H * 6/8;
    btnTime.SetArea (r);
    btnTime.SetLabel (surfTime);
  }

  // ... seconds ...
  if (SECOND_OF (tm) != SECOND_OF (lastTm)) {
    font = FontGet (fntLight, 48);
    snprintf (buf, sizeof (buf), ":%02i", SECOND_OF (tm));
    SurfaceSet (&surfSecs, FontRenderText (font, buf, WHITE, BLACK));
    r = Rect (surfSecs);
    RectAlign (&r, Rect (frame.x + CLOCK_W * 13/16, frame.y, CLOCK_W * 2/16, CLOCK_H * 6/8 - 4), -1, 1);
    wdgSecs.SetArea (r);
    wdgSecs.SetSurface (surfSecs);
  }

  // ... date ...
  if (dt != lastDt) {
    font = FontGet (fntLight, 48);
    // TRANSLATORS: Format string for the "<weekday>, <full date>" display on the home screen (de_DE: "%s, %i. %s %i")
    //              Arguments are: <week day name>, <day>, <month name>, <year>
    snprintf (buf, sizeof (buf), _("%1$s, %3$s %2$i, %4$i"), DayName (GetWeekDay (dt)), DAY_OF (dt), MonthName (MONTH_OF (dt)), YEAR_OF (dt));
    SurfaceSet (&surfDate, FontRenderText (font, buf, WHITE, BLACK));
    r = Rect (surfDate);
    RectAlign (&r, Rect (frame.x, frame.y + CLOCK_H * 6/8, CLOCK_W, CLOCK_H * 2/8), 0, 1);
    wdgDate.SetArea (r);
    wdgDate.SetSurface (surfDate);
  }

  // ... done with time and date ...
  lastTm = tm;
  lastDt = dt;

  // Bluetooth button ...
  if (SystemBluetoothGetState ()) AddWidget (&btnBluetooth);
  else DelWidget (&btnBluetooth);

  // Poll resources to see what has changed...
  while (subscr.PollEvent (&ev)) {
    //~ INFOF(("### Event: '%s'", ev.ToStr ()));
    if (ev.Type () == rceValueStateChanged) {
      rc = ev.Resource ();
      vs = ev.ValueState ();

      // Notify sub-objects...
      wdgEnergyMeter.OnRcEvent (rc);
      wdgDataOutdoor.OnRcEvent (rc);
      wdgDataIndoor.OnRcEvent (rc);

      // Radio display(s)...
      if (rc == rcAccessPoint) {
        if (!vs->IsKnown ())
          DelWidget (&btnAccessPoint);
        else {
          col = vs->IsValid () ? (vs->ValidBool (false) ? COL_APP_LABEL_LIVE : COL_APP_LABEL) : COL_APP_LABEL_BUSY;
          btnAccessPoint.SetLabel (col, "ic-wifi_tethering-48");
          AddWidget (&btnAccessPoint);
        }
      }

      // Radar eye...
      else if (rc == rcRadarEye) {
#if RADAREYE_DIRECT
        // Update radar eye ...
        if (vs->IsValid ()) {
          radarEyeReader.Put (vs->String ());
          btnRadarEye.SetLabel (SurfaceDup (radarEyeReader.Surface ()), NULL, true);
          radarEyeReader.Clear ();
          AddWidget (&btnRadarEye);
        }
        else
          DelWidget (&btnRadarEye);
#else
        radarEyePending = true;
#endif
      }
    }
  }

  // Deferred processing: Update energy meter and data displays ...
  wdgEnergyMeter.Iterate ();
  wdgDataOutdoor.Iterate ();
  wdgDataIndoor.Iterate ();

#if !RADAREYE_DIRECT
  // Radar eye file reader (OBSOLETE) ...
  if (radarEyeShell.IsRunning ()) {       // Currently reading...
    //~ INFO ("# radarEye: Running...");
    // Read from the shell and put to the pgm reader...
    while (radarEyeShell.ReadLine (&s)) radarEyeReader.Put (s.Get ());

    // If the image is complete: Show it...
    if (radarEyeReader.Success () || radarEyeReader.Error ()) {
      //~ INFOF(("### radarEye: Done"));
      btnRadarEye.SetLabel (SurfaceDup (radarEyeReader.Surface ()), NULL, true);
      radarEyeReader.Clear ();
    }
  }
  else {                                  // Currently not reading...
    if (radarEyePending) {
      //~ INFOF(("### radarEye: Start"));
      radarEyeShell.SetHost (envRadarEyeHost);
      radarEyeShell.Start (envRadarEyeCmd);
      radarEyeShell.WriteClose ();
      radarEyeReader.Clear ();
      radarEyePending = false;
    }
  }
#endif
}


void CScreenHome::OnButtonPushed (CButton *btn, bool longPush) {
  if (btn == &btnAndroid) {
    //~ RunInfoBox (_("Push the 'Home2L' icon\nto return here after\nyour Android activities."), IconGet ("ic-home2l-96"), -1);
    SystemGoBackground ();
  }
  else if (btn == &btnTime) {
    AlarmClockHandlePushed (false, longPush);
  }
  else if (btn == &btnAccessPoint) {
    if (rcAccessPoint->ValidBool ()) rcAccessPoint->DelRequest (NULL);
    else rcAccessPoint->SetRequest (true, NULL);
  }
  else if (btn == &btnBluetooth) {
    SystemBluetoothSet (false);
  }
}


void CScreenHome::Activate (bool on) {
  CScreen::Activate (on);

#if !SUBSCRIBE_PERMANENTLY && !SUBSCRIBE_WHEN_SCREEN_ON
  if (on) {
    SubscribeAll ();
  }
  else {
    subscr.Clear ();
    FloorplanUnsubscribeAll ();
  }
#endif
  wdgFloorplan.Activate (on);
  //~ subscr.PrintInfo (); rcTempOutside->PrintInfo ();
}





// *************************** CScreenInfo *************************************


class CScreenInfo: public CScreen {
  public:
    virtual ~CScreenInfo ();

    void Setup ();

    void Iterate (SDL_Surface *surfDroids, SDL_Rect *srcRect);

    void DisplayText (const char *text);

  protected:
    CButton btnBack, btnExit;
    CWidget wdgDroid, wdgVersion, wdgSysinfo;
    CCanvas cvsSysinfo;
    SDL_Surface *surfDroid = NULL, *surfVersion = NULL, *surfSysinfo = NULL;
};



// ***** Helpers *****

static CString sysinfoText;
static bool sysinfoInProgress = false; // only accessed in main thread
static CThread sysinfoThread;


static void SysinfoComplete (void *) {
  //~ INFO ("### SysinfoComplete ()...");
  scrInfo->DisplayText (sysinfoText.Get ());
  sysinfoThread.Join ();
  sysinfoInProgress = false;
}


static void *SysinfoThreadRoutine (void *) {
  //~ static const char scriptName[] = "h2l-sysinfo.sh 2>&1";
  CShellBare shell;
  CString cmd, line;

  //~ INFO ("### SysinfoThreadRoutine (): Starting");

  // Run script...
  sysinfoText.Clear ();
  if (!envSysinfoCmd) sysinfoText.SetF ("No system info cmmand defined (%s)", envSysinfoCmdKey);
  else {
    EnvGetHome2lRootPath (&cmd, envSysinfoCmd);
    shell.SetHost (envSysinfoHost);
    shell.Start (cmd.Get (), true);
    while (!shell.ReadClosed ()) {
      shell.WaitUntilReadable ();
      if (shell.ReadLine (&line)) {
        sysinfoText.Append (line);
        sysinfoText.Append ("\n");
      }
    }
    //~ INFO ("### SysinfoThreadRoutine (): shell.ReadClosed ()!");
  }

  // Handle error...
  if (sysinfoText.IsEmpty ()) sysinfoText.SetF ("Failed to run '%s'!", cmd.Get ());

  // Let main thread do the rest...
  MainThreadCallback (SysinfoComplete, NULL);
  //~ INFO ("### SysinfoThreadRoutine (): Ending");
  return NULL;
}


static void StartSysinfo () {
  ASSERT (!sysinfoInProgress && !sysinfoThread.IsRunning ());
  //~ INFO ("### StartSysinfo ()...");
  sysinfoInProgress = true;
  sysinfoThread.Start (SysinfoThreadRoutine);
}



// ***** CScreenInfo *****


CScreenInfo::~CScreenInfo () {
  SurfaceFree (&surfDroid);
  SurfaceFree (&surfVersion);
  SurfaceFree (&surfSysinfo);
}


static void CbExit (CButton *, bool, void *) {
  UiQuit ();
}


void CScreenInfo::Setup () {
  CTextSet textSet;
  SDL_Rect r, *layout;

  // Button(s)...
  layout = LayoutRowEqually (UI_BUTTONS_RECT, 2);
  btnBack.Set (layout[0], DARK_GREY, IconGet ("ic-back-48"));
  btnBack.SetHotkey (SDLK_ESCAPE);
  btnBack.SetCbPushed (CbAppEscape);
  AddWidget (&btnBack);
  btnExit.Set (layout[1], DARK_GREY, _("Quit Home2L"));
  btnExit.SetHotkey (SDLK_q);
  btnExit.SetCbPushed (CbExit);
  AddWidget (&btnExit);
  free (layout);

  // Droid widget...
  SurfaceSet (&surfDroid, CreateSurface (48, 48));
  r = Rect (0, 0, 4*48, 4*48);
  RectCenter (&r, Rect (0, (UI_RES_Y - UI_BUTTONS_HEIGHT) * 5/8, UI_RES_X * 3/8 + 32, (UI_RES_Y - UI_BUTTONS_HEIGHT) * 3/8));
  wdgDroid.SetArea (r);
  wdgDroid.SetSurface (surfDroid);
  AddWidget (&wdgDroid);

  // Titel/version widget...
  textSet.Clear ();
  textSet.AddLines ("Home2L\n" WALLCLOCK_NAME, CTextFormat (FontGet (fntBoldItalic, 60), WHITE, BLACK, 0, 1));
  textSet.AddLines (buildVersion, CTextFormat (FontGet (fntNormal, 20), WHITE, BLACK, 0, 1));
  textSet.AddLines (buildDate, CTextFormat (FontGet (fntNormal, 20), WHITE, BLACK, 0, 1));
  textSet.AddLines ("\n", CTextFormat (FontGet (fntNormal, 20), WHITE, BLACK, 0, 1));
  textSet.AddLines ("by " HOME2L_AUTHOR "\n", CTextFormat (FontGet (fntBold, 32), WHITE, BLACK, 0, 1));
  textSet.AddLines ("\n" HOME2L_URL, CTextFormat (FontGet (fntItalic, 20), WHITE, BLACK, 0, 1));
  SurfaceSet (&surfVersion, textSet.Render ());
  r = Rect (surfVersion);
  RectCenter (&r, Rect (0, 0, UI_RES_X * 3/8 + 32, (UI_RES_Y - UI_BUTTONS_HEIGHT) * 5/8));
  wdgVersion.Set (surfVersion, r.x, r.y);
  AddWidget (&wdgVersion);

  // Sysinfo canvas + widget...
  cvsSysinfo.SetArea (Rect (UI_RES_X * 3/8 + 32, 0, UI_RES_X * 5/8 - 32, UI_RES_Y - UI_BUTTONS_HEIGHT));
  //~ cvsSysinfo.SetBackColor (RED);
  cvsSysinfo.AddWidget (&wdgSysinfo);
  AddWidget (&cvsSysinfo);
}


void CScreenInfo::Iterate (SDL_Surface *surfDroids, SDL_Rect *srcRect) {
  static int lastSysinfoTime = -1;
  int curTime;

  if (IsActive ()) {

    // Droid animation...
    SDL_FillRect (surfDroid, NULL, ToUint32 (BLACK));
    SDL_SetSurfaceBlendMode (surfDroids, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface (surfDroids, srcRect, surfDroid, NULL);
    wdgDroid.SetSurface (surfDroid);

    // Sysinfo update...
    curTime = (int) SDL_GetTicks ();
    if (!sysinfoInProgress && curTime > lastSysinfoTime + 1000) {
      //~ INFO ("### Starting sysinfo");
      StartSysinfo ();
      lastSysinfoTime = curTime;
    }
  }
}


void CScreenInfo::DisplayText (const char *text) {
  SDL_Rect *cr, *wr;

  SurfaceFree (&surfSysinfo);
  surfSysinfo = TextRender (text, CTextFormat (FontGet (fntMono, 12), WHITE, BLACK, -1, 0));
  wdgSysinfo.SetArea (Rect (surfSysinfo));
  wdgSysinfo.SetSurface (surfSysinfo);
  wr = wdgSysinfo.GetArea ();
  cr = cvsSysinfo.GetVirtArea ();
  if (wr->w != cr->w || wr->h != cr->h)   // widget size changed? (different dimensions than current virtual area?)
    cvsSysinfo.SetVirtArea (Rect (cr->x, cr->y, wr->w, wr->h));
}





// ***************** Droid animation ***********************


static void InitDroidAnimation (void) {
  SDL_Surface *surfDigits;
  SDL_Rect srcRect, dstRect;
  const char *droidId;
  int n, k;

  droidId = EnvDroidId ();
  surfDroids = IconGet ("droids-empty");
  surfDigits = IconGet ("droids-digits");
  for (n = 0; n < 3; n++) {
    srcRect = Rect (0, (droidId[n] - '0') * 16, 16, 16);
    for (k = 0; k < 10; k++) {
      dstRect = Rect (n * 16, k * 48 + 16, 16, 16);
      SDL_BlitSurface (surfDigits, &srcRect, surfDroids, &dstRect);
    }
  }
  surfDroidsGrey = SurfaceDup (surfDroids);
  SurfaceRecolor (surfDroidsGrey, COL_APP_LABEL);
}


static void CbAnimationTimer (CTimer *, void *) {
  static int droidFrame;
  SDL_Rect srcRect;

  droidFrame = (droidFrame + 1) % 8;
  srcRect = Rect (0, droidFrame * 48, 48, 48);
  scrHome->Iterate (surfDroids, &srcRect);
  scrInfo->Iterate (surfDroids, &srcRect);
}





// ***************** Main functions ************************


void *AppFuncHome (int appOp, void *) {
  switch (appOp) {

    case appOpInit:
      scrInfo = new CScreenInfo ();
      scrInfo->Setup ();
      scrHome = new CScreenHome ();
      scrHome->Setup ();

      InitDroidAnimation ();   // load droid animations
      animationTimer.Set (0, 128, CbAnimationTimer, NULL);

      return (void *) AppFuncHome;   // report success

    case appOpDone:
      animationTimer.Clear ();
      delete scrInfo;
      delete scrHome;
      break;

    case appOpActivate:
      scrHome->Activate ();
      break;
  }
  return NULL;
}
