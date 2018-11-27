/*
 *  This file is part of the Home2L project.
 *
 *  (C) 2015-2018 Gundolf Kiefer
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


ENV_PARA_STRING ("ui.daytimeRc", envDayTimeRc, "/local/timer/twilight/day");
  /* Resource (boolean) indicating daylight time
   */

ENV_PARA_STRING ("ui.accessPointRc", envAccessPointRc, "/local/accessPoint");
  /* Resource (boolean) for local (wifi) access point status display
   */
ENV_PARA_STRING ("ui.bluetoothRc", envBluetoothRc, "/local/ui/bluetooth");
  /* Resource (boolean) for local bluetooth status display
   */

ENV_PARA_STRING ("ui.weatherTempRc", envWeatherTempRc, "/alias/weather/temp");
  /* Resource (temp) representing the outside temperature for the right info area (weather)
   */
ENV_PARA_STRING ("ui.weatherData1Rc", envWeatherData1Rc, "/alias/ui/weatherData1");
  /* Resource for the upper data field of the right info area (weather)
   */
ENV_PARA_STRING ("ui.weatherData2Rc", envWeatherData2Rc, "/alias/ui/weatherData2");
  /* Resource for the lower data field of the right info area (weather)
   */

ENV_PARA_STRING ("ui.radarEyeRc", envRadarEyeRc, "/alias/weather/radarEye");
  /* Resource for the radar eye as provided by the 'home2l-weather' driver
   */

ENV_PARA_STRING ("ui.lockSensor1Rc", envLockSensor1Rc, "/alias/ui/lockSensor1");
  /* Resource (bool) for the first lock display in the left info area (building)
   */
ENV_PARA_STRING ("ui.lockSensor2Rc", envLockSensor2Rc, "/alias/ui/lockSensor2");
  /* Resource (bool) for the first lock display in the left info area (building)
   */
ENV_PARA_STRING ("ui.motionDetectorRc", envMotionRc, "/alias/ui/motionDetector");
  /* Resource (bool) for the motion detector display in the left info area (building)
   */
ENV_PARA_INT ("ui.motionDetectorRetention", envMotionRetention, 300000);
  /* Retention time (ms) of the motion detector display
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







// ***************** CFlatButton ***************************


class CFlatButton: public CButton {
  public:
    CFlatButton () { SetColor (BLACK, ToColor (0x600000)); }

  protected:
    virtual SDL_Surface *GetSurface ();
};


SDL_Surface *CFlatButton::GetSurface () {
  if (changed) {
    SurfaceSet (&surface, CreateSurface (area.w, area.h));
    SDL_FillRect (surface, NULL, ToUint32 (isDown ? colDown : colNorm));
    if (surfLabel) SurfaceBlit (surfLabel, NULL, surface, NULL, 0, 0, SDL_BLENDMODE_BLEND);
    changed = false;
  }
  return surface;
}





// ***************** CScreenHome ***************************

#define RADAR_H 128
#define RADAR_W 128

#define WEATHER_H RADAR_H
#define WEATHER_Y (UI_RES_Y - UI_BUTTONS_HEIGHT - WEATHER_H - 16)
#define WEATHER_X (UI_RES_X/2 + RADAR_W/2 + 16)
#define WEATHER_W (UI_RES_X - WEATHER_X - 32)

#define INFO_H RADAR_H
#define INFO_Y (UI_RES_Y - UI_BUTTONS_HEIGHT - INFO_H - 16)
#define INFO_X 16
#define INFO_W (UI_RES_X/2 - RADAR_W/2 - 32)

#define CLOCK_Y 0
#define CLOCK_H (WEATHER_Y - 32 - CLOCK_Y)
#define CLOCK_W 1024        // clock is centred (so there is no parameter 'CLOCK_X')

#define ALARM_H 160
#define ALARM_W 160
#define ALARM_X (UI_RES_X - ALARM_W - 16)
#define ALARM_Y 32

#define RADIOS_X INFO_X
#define RADIOS_Y INFO_X
#define RADIO_W 72
#define RADIO_H 72


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
    CResource *rcDayTime;
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

    // Right info display (weather)...
    CFlatButton btnWeather;
    CResource *rcTempOutside, *rcWeatherData1, *rcWeatherData2;

    // Radar eye...
    CFlatButton btnRadarEye;
    CResource *rcRadarEye;
    CNetpbmReader radarEyeReader;
#if !RADAREYE_DIRECT
    CShellSession radarEyeShell;
    bool radarEyePending;
#endif

    // Left info display (building / doors)...
    CWidget wdgLock1, wdgLock2, wdgMotion;
    CResource *rcLockSensor1, *rcLockSensor2, *rcMotion;
    TTicks tMotionOff;    // -1 = sensor is '1'; 0 = sensor is '0' or new & no retention
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
  rcTempOutside = rcWeatherData1 = rcWeatherData2 = rcRadarEye = rcLockSensor1 = rcLockSensor2 = rcMotion = rcDayTime = NULL;
  surfTime = surfSecs = surfDate = NULL;
  tMotionOff = 0;
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
  subscr.Subscribe (rcDayTime);

  subscr.Subscribe (rcAccessPoint);

  subscr.Subscribe (rcTempOutside);
  subscr.Subscribe (rcWeatherData1);
  subscr.Subscribe (rcWeatherData2);

  subscr.Subscribe (rcRadarEye);

  subscr.Subscribe (rcLockSensor1);
  subscr.Subscribe (rcLockSensor2);
  subscr.Subscribe (rcMotion);
}


void CScreenHome::Setup () {
  int fmtButtons [MAX_BUTTONS];
  CWidget *buttonWdgs[MAX_BUTTONS];
  SDL_Rect *layout;
  const char *str;
  int n, buttons;

  // General ...
  rcDayTime = CResource::Get (envDayTimeRc);

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
  btnAlarmClock.SetArea (Rect (ALARM_X, ALARM_Y, ALARM_W, ALARM_H));
  btnAlarmClock.SetHotkey (SDLK_a);
  AddWidget (&btnAlarmClock);
  AlarmClockSetButton (&btnAlarmClock);

  // Radios...
  btnAccessPoint.SetArea (Rect (RADIOS_X, RADIOS_Y, RADIO_W, RADIO_H));
  btnAccessPoint.SetCbPushed (CbScreenHomeOnButtonPushed, this);

  rcAccessPoint = CResource::Get (envAccessPointRc);

  btnBluetooth.SetArea (Rect (RADIOS_X, RADIOS_Y + RADIO_H, RADIO_W, RADIO_H));
  btnBluetooth.SetLabel (IconGet ("ic-bluetooth-48", COL_APP_LABEL_LIVE));
  btnBluetooth.SetCbPushed (CbScreenHomeOnButtonPushed, this);

  // Lower right info (weather) ...
  btnWeather.SetArea (Rect (WEATHER_X, WEATHER_Y, WEATHER_W, WEATHER_H));
  btnWeather.SetCbPushed (CbAndroidLaunch, (void *) EnvGet (envLaunchWeatherKey));
  btnWeather.SetHotkey (SDLK_w);
  AddWidget (&btnWeather);

  rcTempOutside = CResource::Get (envWeatherTempRc);
  rcWeatherData1 = CResource::Get (envWeatherData1Rc);
  rcWeatherData2 = CResource::Get (envWeatherData2Rc);

  // Radar eye ...
  btnRadarEye.SetArea (Rect ((UI_RES_X-RADAR_W) / 2, WEATHER_Y, RADAR_W, RADAR_H));
  btnRadarEye.SetCbPushed (CbAndroidLaunch, (void *) EnvGet (envLaunchWeatherKey));

#if RADAREYE_DIRECT
  rcRadarEye = CResource::Get (envRadarEyeRc);
#else
  rcRadarEye = CResource::Get (URI_RADAREYE_TRIGGER);
  radarEyePending = true;     // Try to load a radar eye right in the beginning.
#endif

  // Lower left info (building) ...
  wdgLock1.SetArea (Rect (INFO_X + 32, INFO_Y + (INFO_H-48)/2, 48, 48));
  wdgLock2.SetArea (Rect (INFO_X + 96, INFO_Y + (INFO_H-48)/2, 48, 48));
  wdgMotion.SetArea (Rect (INFO_X + 192, INFO_Y + (INFO_H-96)/2, 96, 96));

  rcLockSensor1 = CResource::Get (envLockSensor1Rc);
  rcLockSensor2 = CResource::Get (envLockSensor2Rc);
  rcMotion = CResource::Get (envMotionRc);

  // Subscribe to resources ...
  subscr.Register ("ScreenHome");

#if SUBSCRIBE_PERMANENTLY
  SubscribeAll ();
#endif
}


static void ScreenHomeFormatData (char *buf, int bufSize, CRcValueState *val) {
  switch (RcTypeGetBaseType (val->Type ())) {
    case rctFloat:
      sprintf (buf, "%.1f%s", val->GenericFloat (), RcTypeGetUnit (val->Type ()));
      LangTranslateNumber (buf);
      break;
    default:
      strncpy (buf, val->ToStr (), bufSize - 1); buf[bufSize - 1] = '\0';
  }
}


void CScreenHome::Iterate (SDL_Surface *surfDroids, SDL_Rect *srcRect) {
  CString s;
  char buf[30];
  TTicks now;
  TDate dt;
  TTime tm;
  SDL_Surface *surf = NULL, *surf2 = NULL;
  SDL_Rect frame, r;
  TTF_Font *font;
  TColor col;
  CRcEvent ev;
  CResource *rc;
  CRcValueState *vs;
  CRcValueState val;
  int tempOutside;
  bool isDay = false, lock1Closed, lock1Valid = false, lock2Closed, lock2Valid = false, doorRadarWeak, doorRadarStrong;
  bool changedWeather, changedLock1, changedLock2, changedMotion;

  // (Un-)subscribe to resources if appropriate...
#if SUBSCRIBE_WHEN_SCREEN_ON && !SUBSCRIBE_PERMANENTLY
  ESystemMode systemMode = SystemGetMode ();
  if (systemMode != lastSystemMode) {
    if (systemMode >= smStandby && lastSystemMode < smStandby) SubscribeAll ();
    else if (systemMode < smStandby && lastSystemMode >= smStandby) subscr.Clear ();
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
    sprintf (buf, "%i:%02i", HOUR_OF (tm), MINUTE_OF (tm));
    SurfaceSet (&surfTime, FontRenderText (font, buf, WHITE)); // , BLACK));
    r = Rect (surfTime);
    RectAlign (&r, Rect (frame.x, frame.y + CLOCK_H * 1/8, CLOCK_W * 13/16, CLOCK_H * 6/8), 1, 1);
    r.y += (r.h - CLOCK_H * 6/8) / 2;
    r.h = CLOCK_H * 6/8;
    btnTime.SetArea (r);
    btnTime.SetLabel (surfTime);
    AddWidget (&btnTime);
  }

  // ... seconds ...
  if (SECOND_OF (tm) != SECOND_OF (lastTm)) {
    font = FontGet (fntLight, 48);
    sprintf (buf, ":%02i", SECOND_OF (tm));
    SurfaceSet (&surfSecs, FontRenderText (font, buf, WHITE, BLACK));
    r = Rect (surfSecs);
    RectAlign (&r, Rect (frame.x + CLOCK_W * 13/16, frame.y, CLOCK_W * 2/16, CLOCK_H * 6/8 - 4), -1, 1);
    wdgSecs.SetArea (r);
    wdgSecs.SetSurface (surfSecs);
    AddWidget (&wdgSecs);
  }

  // ... date ...
  if (dt != lastDt) {
    font = FontGet (fntLight, 48);
    // TRANSLATORS: Format string for the "<weekday>, <full date>" display on the home screen (de_DE: "%s, %i. %s %i")
    //              Arguments are: <week day name>, <day>, <month name>, <year>
    sprintf (buf, _("%1$s, %3$s %2$i, %4$i"), DayName (GetWeekDay (dt)), DAY_OF (dt), MonthName (MONTH_OF (dt)), YEAR_OF (dt));
    SurfaceSet (&surfDate, FontRenderText (font, buf, WHITE, BLACK));
    r = Rect (surfDate);
    RectAlign (&r, Rect (frame.x, frame.y + CLOCK_H * 6/8, CLOCK_W, CLOCK_H * 2/8), 0, 1);
    wdgDate.SetArea (r);
    wdgDate.SetSurface (surfDate);
    AddWidget (&wdgDate);
  }

  // ... done with time and date ...
  lastTm = tm;
  lastDt = dt;

  // Bluetooth button ...
  if (SystemBluetoothGetState ()) AddWidget (&btnBluetooth);
  else DelWidget (&btnBluetooth);

  // Poll resources to see what has changed...
  changedWeather = changedLock1 = changedLock2 = changedMotion = false;
  doorRadarStrong = doorRadarWeak = false;
  while (subscr.PollEvent (&ev)) {
    //~ INFOF(("### Event: '%s'", ev.ToStr ()));
    if (ev.Type () == rceValueStateChanged) {
      rc = ev.Resource ();
      vs = ev.ValueState ();

      // Radio display(s)...
      if (rc == rcAccessPoint) {
        if (!vs->IsKnown ())
          DelWidget (&btnAccessPoint);
        else {
          col = vs->IsValid () ? (vs->ValidBool (false) ? COL_APP_LABEL_LIVE : COL_APP_LABEL) : COL_APP_LABEL_BUSY;
          btnAccessPoint.SetLabel (IconGet ("ic-wifi_tethering-48", col));
          AddWidget (&btnAccessPoint);
        }
      }

      // Right info display (weather)...
      //   Actual processing is deferred due to combined widget.
      else if (rc == rcTempOutside || rc == rcWeatherData1 || rc == rcWeatherData2) {
        changedWeather = true;
      }

      // Radar eye...
      else if (rc == rcRadarEye) {
#if RADAREYE_DIRECT
        // Update radar eye ...
        if (vs->IsValid ()) {
          radarEyeReader.Put (vs->String ());
          btnRadarEye.SetLabel (radarEyeReader.Surface ());
          radarEyeReader.Clear ();
          AddWidget (&btnRadarEye);
        }
        else
          DelWidget (&btnRadarEye);
#else
        radarEyePending = true;
#endif
      }

      // Left info display (building)...
      //   Actual processing is deferred due to account for day time highlighting.
      else if (rc == rcLockSensor1 || rc == rcLockSensor2 || rc == rcDayTime) {
        if (rc == rcLockSensor1 || rc == rcDayTime) {
          changedLock1 = true;
          lock1Valid = (rcLockSensor1->GetValue (&lock1Closed) == rcsValid);
        }
        if (rc == rcLockSensor2 || rc == rcDayTime) {
          changedLock2 = true;
          lock2Valid = (rcLockSensor2->GetValue (&lock2Closed) == rcsValid);
        }
        isDay = rcDayTime->ValidBool (false);
      }
      else if (rc == rcMotion) {
        changedMotion = true;
      }
    }
  }

  // Deferred processing: Redraw 'btnWeather' if changed...
  if (changedWeather) {

    // Outside (main) temperature...
    font = FontGet (fntLight, 96);
    rcTempOutside->GetValueState (&val);
    if (val.IsValid ()) {
      tempOutside = (int) round (val.UnitFloat (rctTemp) * 10);
      sprintf (buf, (tempOutside < -9 || tempOutside >= 0) ? "%i°C" : "-%i°C", tempOutside / 10);
      surf = FontRenderText (font, buf, WHITE, BLACK);
      r = Rect (surf);
      r.x = r.w - FontGetWidth (font, "°C") - 4;
      r.h -= 12;
      font = FontGet (fntLight, 32);
      sprintf (buf, ".%i", (tempOutside < 0 ? -tempOutside : tempOutside) % 10);
      LangTranslateNumber (buf);
      surf2 = FontRenderText (font, buf, WHITE, BLACK);
      SurfaceBlit (surf2, NULL, surf, &r, -1, 1);
      SurfaceFree (&surf2);
    }
    else surf = NULL; // FontRenderText (font, "-°C", WHITE, BLACK);   // no valid temperature

    // Create frame surface and put temperature into it...
    surf2 = surf;
    frame = *btnWeather.GetArea ();
    surf = CreateSurface (frame.w - 16, frame.h - 16);
    frame = Rect (surf);
    SDL_FillRect (surf, NULL, ToUint32 (BLACK)); // ToUint32 (TRANSPARENT));
    SurfaceBlit (surf2, NULL, surf, &frame, 1, 0);
    SurfaceFree (&surf2);

    // Add data1 and data2...
    font = FontGet (fntLight, 32);
    RectGrow (&frame, 0, -8);
    rcWeatherData1->GetValueState (&val);
    if (val.IsValid ()) {
      ScreenHomeFormatData (buf, sizeof (buf), &val);
      surf2 = FontRenderText (font, buf, LIGHT_GREY);
      r = Rect (frame.x, frame.y, WEATHER_W * 5/16, frame.h / 2);
      SurfaceBlit (surf2, NULL, surf, &r, 1, 0, SDL_BLENDMODE_BLEND);
      SurfaceFree (&surf2);
    }
    rcWeatherData2->GetValueState (&val);
    if (val.IsValid ()) {
      ScreenHomeFormatData (buf, sizeof (buf), &val);
      surf2 = FontRenderText (font, buf, LIGHT_GREY);
      r = Rect (frame.x, frame.y + frame.h / 2, WEATHER_W * 5/16, frame.h / 2);
      SurfaceBlit (surf2, NULL, surf, &r, 1, 0, SDL_BLENDMODE_BLEND);
      SurfaceFree (&surf2);
    }

    // Update label...
    SurfaceMakeTransparentMono (surf);
    btnWeather.SetLabel (surf);
    SurfaceFree (&surf);
  }

  // Deferred processing: Redraw 'wdgLock1' / 'wdgLock2' if changed...
  //~ if (changedLock1 || changedLock2) INFOF (("### changedLock1/2 = %i/%i, lock1Valid1/2 = %i/%i, lockClosed1/2 = %i/%i", changedLock1, changedLock2, lock1Valid, lock2Valid, lock1Closed, lock2Closed));
  if (changedLock1) {
    if (lock1Valid) {
      //~ INFOF(("### isDay = %i", isDay));
      wdgLock1.SetSurface (IconGet (lock1Closed ? "ic-padlock-48" : "ic-padlock-open-48", (!isDay && !lock1Closed) ? WHITE : GREY, BLACK));
      AddWidget (&wdgLock1);
    }
    else DelWidget (&wdgLock1);
  }
  if (changedLock2) {
    if (lock2Valid) {
      wdgLock2.SetSurface (IconGet (lock2Closed ? "ic-padlock-48" : "ic-padlock-open-48", (!isDay && !lock2Closed) ? WHITE : GREY, BLACK));
      AddWidget (&wdgLock2);
    }
    else DelWidget (&wdgLock2);
  }

  // Deferred processing: Redraw 'wdgMotion' if changed...
  if (tMotionOff > 0 && now >= tMotionOff) changedMotion = true;
  if (changedMotion) {
    // Determine 'doorRadarStrong' and 'doorRadarWeak'...
    //~ INFO (("### changedMotion"));
    doorRadarStrong = rcMotion->ValidBool (false);
    if (!doorRadarStrong && (tMotionOff == -1 || now < tMotionOff)) doorRadarWeak = true;
    else doorRadarWeak = false;
    // Draw icon...
    if (doorRadarStrong || doorRadarWeak) {
      wdgMotion.SetSurface (IconGet ("ic-walk-96", doorRadarStrong ? WHITE : GREY, BLACK));
      AddWidget (&wdgMotion);
    }
    else DelWidget (&wdgMotion);
    // Reset retention timer if appropriate...
    if (doorRadarStrong) tMotionOff = -1;      // Sensor is '1' now => go to that state
    else if (doorRadarWeak) tMotionOff = now + envMotionRetention; // Sensor was '1' and switched to '0' now => start retention
    //~ INFOF (("### tMotionOff == %lli", tMotionOff));
  }


#if !RADAREYE_DIRECT
  // Radar eye file reader (OBSOLETE) ...
  if (radarEyeShell.IsRunning ()) {       // Currently reading...
    //~ INFO ("# radarEye: Running...");
    // Read from the shell and put to the pgm reader...
    while (radarEyeShell.ReadLine (&s)) radarEyeReader.Put (s.Get ());

    // If the image is complete: Show it...
    if (radarEyeReader.Success () || radarEyeReader.Error ()) {
      //~ INFOF(("### radarEye: Done"));
      btnRadarEye.SetLabel (radarEyeReader.Surface ());
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
  if (on) SubscribeAll ();
  else subscr.Clear ();
#endif
  //~ subscr.PrintInfo (); rcTempOutside->PrintInfo ();
}





// ***************** CScreenInfo ***************************


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
  // printf ("### Output follows...\n%s\n### END of output\n", sysinfoText.c_str ());
  scrInfo->DisplayText (sysinfoText.Get ());
  sysinfoThread.Join ();
  sysinfoInProgress = false;
}


static void *SysinfoThreadRoutine (void *) {
  static const char scriptName[] = "h2l-sysinfo.sh 2>&1";
  CShellBare shell;
  CString line;

  // Run script...
  sysinfoText.Clear ();
  shell.Start (scriptName);
  while (!shell.ReadClosed ()) {
    shell.WaitUntilReadable ();
    if (shell.ReadLine (&line)) {
      sysinfoText.Append (line);
      sysinfoText.Append ("\n");
    }
  }
  if (sysinfoText.IsEmpty ()) sysinfoText.SetF ("Failed to run '%s'!", scriptName);

  // Let main thread do the rest...
  MainThreadCallback (SysinfoComplete, NULL);
  return NULL;
}


static void StartSysinfo () {
  ASSERT (!sysinfoInProgress && !sysinfoThread.IsRunning ());
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
      StartSysinfo ();
      lastSysinfoTime = curTime;
    }
  }
}


void CScreenInfo::DisplayText (const char *text) {
  SDL_Rect *cr, *wr;

  surfSysinfo = TextRender (text, CTextFormat (FontGet (fntMono, 12), WHITE, BLACK, -1, 0), surfSysinfo);
  wdgSysinfo.SetArea (Rect (surfSysinfo));
  wdgSysinfo.SetSurface (surfSysinfo);
  wr = wdgSysinfo.GetArea ();
  cr = cvsSysinfo.GetVirtArea ();
  if (wr->w != cr->w || wr->h != cr->h)
    cvsSysinfo.SetVirtArea (Rect (cvsSysinfo.GetArea ()->x, cvsSysinfo.GetArea ()->y, wr->w, wr->h));
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
