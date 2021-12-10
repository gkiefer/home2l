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


#include "alarmclock.H"

#include "system.H"
#include "ui_widgets.H"
#include "apps.H"
#include "app_music.H"





// ***** Resources (static) *****


ENV_PARA_PATH ("alarm.ringFile", envAlarmRingFile, "share/sounds/alarm-classic.wav");
  /* Audio file to play if the music player fails or for pre-ringing
   */

ENV_PARA_INT ("alarm.ringGap", envAlarmRingGap, 0);
  /* Number of milliseconds to wait before playing the ring file again
   */

ENV_PARA_INT ("alarm.preRings", envAlarmPreRings, 0);
  /* Number of times the ring file is played before the music player is started
   */

ENV_PARA_INT ("alarm.snoozeMinutes", envAlarmSnoozeMinutes, 10);
  /* Number of snooze minutes
   */

ENV_PARA_INT ("alarm.tryTime", envTryTime, 15000);
  /* Maximum time in milliseconds to try playing music
   *
   * If the music is not playing after this amount of time, the alarm clock reverts
   * to ringing mode.
   */

ENV_PARA_INT ("alarm.minLevelDb", envMinLevelDb, -30);
  /* Minimum level (DB) required for music
   *
   * If the music is below this level (e.g., because a radio station sends silence),
   * the alarm clock reverts to ringing mode.
   *
   * NOTE: This option only works if the music is output locally using GStreamer.
   */

ENV_PARA_STRING ("alarm.extAlarmHost", envExtAlarmHost, NULL);
  /* Host to run an external alarm script on (local if unset)
   *
   * This can be used to implement a fallback wakeup (e.g. by a wakeup phone call),
   * if the wallclock fails for some reason.
   */

ENV_PARA_STRING ("alarm.extAlarmCmd", envExtAlarmCmd, NULL);
  /* Command to setup an external alarm
   *
   * This can be used to implement a fallback wakeup (e.g. by a wakeup phone call),
   * if the wallclock fails for some reason. The command will be executed as follows:
   *
   * \texttt{~~ <cmd> -i <hostname> <yyyy>-<mm>-<dd> <hh>:<mm>}
   */

ENV_PARA_INT ("alarm.extAlarmDelay", envExtAlarmDelay, 3);
  /* Delay of the external alarm setting
   *
   * Number of minutes n added to the set alarm time before transmitting the request
   * to the external alarm resource.
   *
   * In case of a failure in the "standby" or "snooze" state, the external alarm
   * will go off $n$ minutes after the time set.
   * In case of a failure during alarming, the external alarm will go off
   * between $n$ and $2n$ minutes after the time set.
   */



// ***** Resources (variable) *****


ENV_PARA_BOOL ("var.alarm.enable", envAlarmEnabled, false);
  /* Enable the alarm clock as a whole
   */

ENV_PARA_SPECIAL ("var.alarm.timeSet.<n>", int, NODEFAULT);
  /* Wake up time set for week day <$n$>
   *
   * The time is given in minutes after midnight.
   * Week days are numbered from 0 (Mon) to 6 (Sun).
   *
   * Values $<0$ denote that there is no alarm on the respective day.
   * Values $<-1$ denote a hint to the UI if the alarm on that day
   * is activated: The time is preset by the negated value.
   */

ENV_PARA_INT ("var.alarm.active", envAlarmActive, 0);
  /* Presently active alarm time (in minutes after the epoch).
   *
   * This variable is automatically set in a persistent way when an alarm
   * goes off and set to 0 when the user switches off the alarm.
   * It is used to recover the ringing state if the app crashes during alarm.
   */





// *************************** Variables & Helpers ***************************


static int timeSetList[7];          // cache of the 'var.alarm.timeSet' environment


// Drawing...
static CButton *acButton = NULL;
static SDL_Surface *acSurf = NULL;


// State and time(r)s...
static TAlarmClockState acState = acsDisabled;
static bool extAlarmBusy = false;   // true, if an external alarm script is running
static bool extAlarmError = false;  // true, if an external alarm script has failed
static CTimer acTimer;
static TTicks tSnooze = NEVER;      // time up to which to snooze
static TTicks tAlarm = NEVER;       // currently effective alarm time
static TTicks tInState = NEVER;     // time of the last state change
static TTicks tExtAlarm = -1;       // last set external alarm time; default = neither 'NEVER' nor a valid time






// *************************** Drawing ***************************************


#define ACSURF_W 160
//~ #define ACSURF_H 112


static void UpdateAcSurface () {
  SDL_Surface *surfText, *surfIcon;
  char buf[32];
  TTime tm;
  TDate dt;
  TColor col;

  //~ INFOF (("### UpdateAcSurface (): state = %i, tAlarm = %s", acState, TicksAbsToString (tAlarm)));
  switch (acState) {
    case acsDisabled:
      SurfaceFree (&acSurf);
      break;

    case acsStandby:
    case acsSnooze:
      // Snooze: snooze icon + time
      // else: alarm icon + time (if available)
      col = (extAlarmBusy || extAlarmError) ? COL_APP_LABEL_BUSY
            : acState == acsSnooze ? COL_APP_LABEL_LIVE : GREY;
      //    ... icon ...
      surfIcon = IconGet (acState == acsSnooze ? "ic-alarm_snooze-48" : "ic-alarm-48", col);
      //    ... time string ...
      if (tAlarm == NEVER) surfText = NULL;   // no time
      else {
        TicksToDateTime (tAlarm, &dt, &tm);
        if (tAlarm - TicksNow () <= TICKS_FROM_SECONDS (23*60*60)) {
          // next alarm is clearly within 24 hours: just show time...
          sprintf (buf, "%i:%02i", HOUR_OF (tm), MINUTE_OF (tm));
          surfText = FontRenderText (FontGet (fntNormal, 48), buf, col);
        }
        else {
          // next alarm is later: show weekday + time, font a bit smaller...
          sprintf (buf, "%s %i:%02i", DayNameShort (GetWeekDay (dt)), HOUR_OF (tm), MINUTE_OF (tm));
          surfText = FontRenderText (FontGet (fntNormal, 32), buf, col);
        }
      }
      //    ... compose ...
      if (surfText) {
        SurfaceSet (&acSurf, CreateSurface (ACSURF_W, surfIcon->h + surfText->h + 8));
        SDL_FillRect (acSurf, NULL, ToUint32 (TRANSPARENT));
        SurfaceBlit (surfIcon, NULL, acSurf, NULL, 0, -1);
        SurfaceBlit (surfText, NULL, acSurf, NULL, 0, 1);
        SurfaceFree (&surfText);
      }
      else
        SurfaceSet (&acSurf, SurfaceDup (surfIcon));
      break;

    case acsAlarmPreRinging:
    case acsAlarmMusicTrying:
    case acsAlarmMusicOk:
    case acsAlarmRinging:
      col = (extAlarmBusy || extAlarmError) ? COL_APP_LABEL_BUSY : COL_APP_LABEL_LIVE;
      SurfaceSet (&acSurf, SurfaceDup (IconGet ("ic-alarm-96", col)));
      break;

    default:
      ASSERT (false);
  }

  // Update button...
  if (acButton) acButton->SetLabel (acSurf);
}





// ************************* Iteration timer ***********************************


#define ALARM_POLL_INTERVAL 256   // should be power of 2


static void UpdateTimer () {
  TTicks tLeft;
  bool intervalTimer;

  intervalTimer = extAlarmBusy;             // need frequent interval timer?
  if (!intervalTimer) switch (acState) {    // can work with (power-)efficient single-shot timer?
    case acsDisabled:
      acTimer.Clear ();
      break;
    case acsStandby:  // standby-like states...
    case acsSnooze:
      if (tAlarm != NEVER) {
        tLeft = tAlarm - TicksNow ();
        if (tLeft < 0) tLeft = 0;
        if (tLeft > TICKS_FROM_SECONDS (600)) tLeft = TICKS_FROM_SECONDS (600);   // avoid overflows; have updates at least every 10 minutes (e.g. for the day display)
        else if (tLeft > TICKS_FROM_SECONDS (1)) tLeft = tLeft * 7/8;    // round down
        acTimer.Reschedule (TicksNowMonotonic () + (TTicks) tLeft);
      }
      else acTimer.Clear ();
      break;
    default:          // all remaining states are considered alarm states...
      intervalTimer = true;
  }

  if (intervalTimer) acTimer.Reschedule (-ALARM_POLL_INTERVAL, ALARM_POLL_INTERVAL);
}





// *************************** External Alarm ********************************


static void UpdateExtAlarm () {
  static CShellBare shell;
  static CString s;
  TTicks _tExtAlarm, tNow;
  TDate d;
  TTime t;

  // Sanity...
  if (!envExtAlarmCmd) return;    // No external alarm activated

  //~ INFOF (("### UpdateExtAlarm (), extAlarmBusy = %i, extAlarmError = %i", (int) extAlarmBusy, (int) extAlarmError));

  // Update external alarm state...
  if (extAlarmBusy) {
    if (!shell.IsRunning ()) {
      extAlarmBusy = false;
      if (shell.ExitCode () != 0) {
        //~ INFOF (("###   exitCode = %i", shell.ExitCode ()));
        extAlarmError = true;
        tExtAlarm = NEVER;
      }
      UpdateAcSurface ();
      UpdateTimer ();
    }
  }
  if (extAlarmBusy) return;     // do not continue if the shell is still busy

  // Determine time for external alarm...
  if (AlarmClockStateIsAlarm (acState)) {
    // alarm currently ongoing...
    _tExtAlarm = tExtAlarm;
    tNow = TicksNow ();
    if (tNow > tExtAlarm - TICKS_FROM_SECONDS (envExtAlarmDelay * 60))    // if too close ...
      _tExtAlarm += TICKS_FROM_SECONDS (envExtAlarmDelay * 60);           // ... push it further to the future
    if (tNow > _tExtAlarm)  // still in the past? (perhaps after a crash restart)
      _tExtAlarm = tNow + 2 * TICKS_FROM_SECONDS (envExtAlarmDelay * 60); // ... push it even further
  }
  else {
    // alarm may be set in the future...
    if (tAlarm != NEVER) _tExtAlarm = tAlarm + TICKS_FROM_SECONDS (envExtAlarmDelay * 60);
    else _tExtAlarm = NEVER;
  }

  // Issue script if adequate...
  //~ INFOF (("###   tAlarm = %s", TicksAbsToString (&s, tAlarm)));
  //~ INFOF (("###   tExtAlarm = %s", TicksAbsToString (&s, tExtAlarm)));
  //~ INFOF (("###   _tExtAlarm = %s", TicksAbsToString (&s, _tExtAlarm)));
  if (_tExtAlarm != tExtAlarm) {
    if (envExtAlarmHost) shell.SetHost (envExtAlarmHost);
    if (_tExtAlarm != NEVER) {
      TicksToDateTime (_tExtAlarm, &d, &t);
      s.SetF ("%s -i %s %04i%02i%02i%02i%02i", envExtAlarmCmd, EnvMachineName (),
              YEAR_OF (d), MONTH_OF (d), DAY_OF (d), HOUR_OF (t), MINUTE_OF (t));
    }
    else {
      s.SetF ("%s -i %s -", envExtAlarmCmd, EnvMachineName ());
    }
    //~ INFOF (("### UpdateExtAlarm (): Running '%s'", s.Get ()));
    extAlarmBusy = shell.Start (s.Get ());
    shell.WriteClose ();                // we are not writing anything
    extAlarmError = !extAlarmBusy;      // if 'shell.Start ()' returned false, report an error; otherwise, reset the error flag
    if (!extAlarmError) tExtAlarm = _tExtAlarm;
    UpdateAcSurface ();
    UpdateTimer ();
  }
}





// ************************* Iteration / State change **************************


static void SetPersistentTAlarm (TTicks _tAlarm) {
  if (_tAlarm > 0) envAlarmActive = SECONDS_FROM_TICKS (_tAlarm) / 60;
  else envAlarmActive = 0;

  EnvPut (envAlarmActiveKey, envAlarmActive);
  EnvFlush ();
}


static inline void ClearPersistentTAlarm () { SetPersistentTAlarm (0); }


static void UpdateTAlarm () {
  // Update alarm time / switch to next alarm time.
  // Week-daily alarms are interpreted as if they are all in the future. Hence,
  // special care has to be taken for the current day: This function must not be called
  // after today's alarm time unless a) the user has switched off the alarm or b) 'tSnozze'
  // has been set before.
  // 'tSnooze' is absolute and may thus be in the past safely.
  TTicks now, t;
  TDate nowDate;
  TTime nowTime;
  int n, nowWd, setMin;

  // Check if alarm is enabled...
  if (acState == acsDisabled) {
    tAlarm = NEVER;
  }

  // Check for an already active time...
  else if (envAlarmActive > 0) {
    tAlarm = TICKS_FROM_SECONDS (envAlarmActive * 60);
  }
  else {

    // Obtain current time...
    now = TicksNow ();
    TicksToDateTime (now, &nowDate, &nowTime);
    nowWd = GetWeekDay (nowDate);

    // Find next relevant weekday alarm...
    tAlarm = NEVER;
    for (n = 0; n < 8 && tAlarm == NEVER; n++) {
      // 8 iterations: today's week day can be relevant as "today" or "today in a week".
      setMin = timeSetList[(nowWd+n) % 7];
      if (setMin >= 0) {
        t = DateTimeToTicks (DateIncByDays (nowDate, n), setMin * 60);
        if (t >= now) tAlarm = t;     // today, but in the past would result in 't < now' and be ignored here
        //~ tAlarm = t;     // DEBUG: usually commented out
      }
    }

    // Handle snooze...
    if (acState == acsSnooze) if (tSnooze < tAlarm || tAlarm == NEVER) tAlarm = tSnooze;

  } // if (envAlarmActive > 0)

  // Update timer, surface, and external alarm ...
  UpdateAcSurface ();
  UpdateTimer ();
  UpdateExtAlarm ();
}


static void Iterate (CTimer *timer = NULL, void *data = NULL) {
  TAlarmClockState _acState = acState;
  TTicks t;
  bool updateTimer;

  //~ INFOF (("### Iterate (): state = %i, tAlarm = %s", acState, TicksAbsToString (tAlarm)));
  updateTimer = false;
  switch (acState) {
    case acsDisabled:
      break;
    case acsStandby:
    case acsSnooze:
      if (tAlarm != NEVER && TicksNow () >= tAlarm) {
        // Alarm goes off...
        SetPersistentTAlarm (tAlarm);
        SystemSetAudioNormal ();
        AudioStart (envAlarmRingFile, envAlarmPreRings, envAlarmRingGap);
        _acState = acsAlarmPreRinging;
        SystemGoForeground ();    // bring app to front
        SystemActiveLock ("_alarmclock");
      }
      updateTimer = true;     // Timer must become an interval timer OR we may have to check again
      break;
    case acsAlarmPreRinging:
      if (!AudioIsPlaying ()) {
        // Done with pre-ringing: (try to) switch on the music player...
        AppMusicPlayerOn ();
        _acState = acsAlarmMusicTrying;
      }
      break;
    case acsAlarmMusicTrying:
      if (AppMusicIsPlayingForSure (envMinLevelDb))
        _acState = acsAlarmMusicOk;
      else {
        t = TicksNow () - tInState;
        if (t >= 1000) AppMusicPlayerOn ();     // Try to restart music player once
        if (t >= envTryTime) {                  // Give up...
          AppMusicPlayerOff ();
          AudioStart (envAlarmRingFile, AUDIO_FOREVER, envAlarmRingGap);
          _acState = acsAlarmRinging;
        }
      }
      break;
    case acsAlarmMusicOk:
      if (!AppMusicIsPlayingForSure (envMinLevelDb))
        _acState = acsAlarmMusicTrying;
      break;
    case acsAlarmRinging:
      break;
    default:
      ASSERT (false);
  }

  // Perform state change...
  if (_acState != acState) {
    acState = _acState;
    tInState = TicksNow ();
    UpdateAcSurface ();
  }

  // Update timer if appropriate ...
  if (updateTimer) UpdateTimer ();

  // Update external alarm...
  UpdateExtAlarm ();
}


static void AlarmClockStop (TAlarmClockState toState) {
  TTicks now;

  // Stop ringing...
  switch (acState) {
    case acsAlarmPreRinging:
    case acsAlarmRinging:
      AudioStop ();
      break;
    case acsAlarmMusicTrying:
    case acsAlarmMusicOk:
      AppMusicPlayerOff ();
      break;
    default:
      break;
  }

  // Go to new state...
  tSnooze = NEVER;
  switch (toState) {
    case acsDisabled:
    case acsStandby:
      ClearPersistentTAlarm ();
      SystemActiveUnlock ("_alarmclock");
      break;
    case acsSnooze:
      now = TicksNow ();
      tSnooze = tAlarm;
      do {
        tSnooze += TICKS_FROM_SECONDS (envAlarmSnoozeMinutes * 60);
      } while (tSnooze <= now);
      SetPersistentTAlarm (tSnooze);
      break;
    default:
      ASSERT (false);
  }

  // Update alarm time and icon...
  if (toState != acState) {
    acState = toState;
    UpdateTAlarm ();
  }
}





// *************************** Setup dialog ************************************


#define UI_SPACE        12           // space between UI groups
#define UI_ROW_HEIGHT   (UI_BUTTONS_HEIGHT * 3/2)
#define UI_SLIDER_WIDTH UI_ROW_HEIGHT
#define UI_DEC_INC_WIDTH (UI_BUTTONS_HEIGHT * 2)

#define COL_AC_BUTTONS  GREY // ORANGE
#define COL_AC_MAIN     ORANGE
#define COL_AC_WORKDAY  DARK_DARK_GREY
#define COL_AC_WEEKEND  BLACK
#define COL_AC_TODAY    GREY

//~ #define HIST_BTNS 8


class CScreenSetAlarmClock: public CScreen {
  public:
    CScreenSetAlarmClock ();
    ~CScreenSetAlarmClock ();

    void Commit ();

    // UI callbacks...
    void OnButtonPushed (CButton *btn, bool longPush);
    void OnSliderValueChanged (CSlider *slider, int val, int lastVal);

    // Flags...
    bool AlarmEnabled () { return enabled; }

  protected:

    // View...
    CButton btnBack;
    CButton btnEnable;
    CWidget wdgTimes[7];
    CButton btnDays[7];
    //~ CButton btnHist[HIST_BTNS];
    CSlider sldHour, sldMin;
    CButton btnHourDec, btnHourInc, btnMinDec, btnMinInc;

    TTF_Font *fntNorm, *fntBig;

    // Modell...
    bool enabled;
    bool timeSetChanged[7];
    int curDay, today;
    //~ int timeHistList[HIST_BTNS];         // cache of the 'var.alarm.timeHist' environment

    // Helpers...
    void UpdateVisibility ();
    void UpdateDay (int d);
    void UpdateSliders ();
    void SetCurDay (int d);     // Change selected day; update sliders
    void SetCurTime (int t, bool updateSliders);    // Set time for current week day; update time widget and sliders if requested
};


static BUTTON_TRAMPOLINE(CbOnButtonPushed, CScreenSetAlarmClock, OnButtonPushed)
static SLIDER_TRAMPOLINE(CbOnSliderValueChanged, CScreenSetAlarmClock, OnSliderValueChanged)


void CScreenSetAlarmClock::UpdateVisibility () {
  int n;

  // Set main button contents ...
  btnEnable.SetLabel (WHITE, "ic-alarm-48", enabled ? _("Enabled") : _("Disabled"));
  btnEnable.SetColor (enabled ? COL_AC_MAIN : DARK_GREY);

  // Add/delete widgets as appropriate ...
  DelAllWidgets ();
  AddWidget (&btnBack);
  AddWidget (&btnEnable);
  if (enabled) {
    for (n = 0; n < 7; n++) {
      AddWidget (&wdgTimes[n]);
      AddWidget (&btnDays[n]);
    }
    AddWidget (&btnHourDec);
    AddWidget (&sldHour);
    AddWidget (&btnHourInc);
    AddWidget (&btnMinDec);
    AddWidget (&sldMin);
    AddWidget (&btnMinInc);
  }
}


void CScreenSetAlarmClock::UpdateDay (int d) {
  SDL_Surface *surf, *surfText;
  char buf[8];
  TColor col, colBack;
  int t;

  // Sanity...
  if (d < 0 || d > 6) return;

  // Determine color...
  col = (d < 5) ? COL_AC_WORKDAY : COL_AC_WEEKEND;
  if (d == today) col = COL_AC_TODAY;
  if (d == curDay) col = COL_AC_MAIN;
  colBack = ColorDarker (col, 0x20);

  // Update time display...
  t = timeSetList[d];
  sprintf (buf, (t >= 0) ? "%i:%02i" : "---", t / 60, t % 60);
  surfText = FontRenderText ((d == curDay) ? fntBig : fntNorm, buf, WHITE, colBack);
  surf = wdgTimes[d].GetSurface ();
  SDL_FillRect (surf, NULL, ToUint32 (colBack));
  SurfaceBlit (surfText, NULL, surf);
  SurfaceFree (surfText);
  wdgTimes[d].SetSurface (surf);

  // Update button ...
  btnDays[d].SetColor (col);
}


void CScreenSetAlarmClock::UpdateSliders () {
  int t = timeSetList[curDay];
  if (t == -1) t = 7 * 60;      // Default = 7:00
  if (t < 0) t = -t;
  sldHour.SetValue (t / 60);
  sldMin.SetValue (t % 60);
}


void CScreenSetAlarmClock::SetCurDay (int d) {
  int lastCurDay = curDay;

  curDay = d;
  UpdateDay (lastCurDay);
  if (lastCurDay != curDay) {
    UpdateSliders ();
    UpdateDay (curDay);
  }
}


void CScreenSetAlarmClock::SetCurTime (int t, bool updateSliders) {
  if (timeSetList[curDay] == t) return;

  timeSetList[curDay] = t;
  timeSetChanged[curDay] = true;
  UpdateDay (curDay);
  if (updateSliders) UpdateSliders ();
}


CScreenSetAlarmClock::CScreenSetAlarmClock () {
  SDL_Rect *layoutMain, *layoutRow;
  TDate dateNow;
  TTime timeNow;
  int n, selDay;

  // Constants...
  fntNorm = FontGet (fntNormal, 32);
  fntBig = FontGet (fntNormal, 48);

  // Modell...
  enabled = (acState != acsDisabled);
  for (n = 0; n < 7; n++) timeSetChanged[n] = false;
  TicksToDateTime (TicksNow (), &dateNow, &timeNow);
  today = GetWeekDay (dateNow);
  curDay = -1;

  // Layout...
  layoutMain = LayoutCol (RectScreen (), UI_SPACE,
                  -1,
                  UI_ROW_HEIGHT,      // [1] day time display
                  UI_ROW_HEIGHT,      // [2] day buttons
                  -1,
                  UI_ROW_HEIGHT,      // [4] hour slider
                  UI_ROW_HEIGHT,      // [5] minute slider
                  -1,
                  UI_BUTTONS_HEIGHT,  // [7] button bar
                  0);
  //~ layoutMain = LayoutCol (RectScreen (), UI_SPACE,
                  //~ UI_BUTTONS_HEIGHT,      // [0] title
                  //~ -1,
                  //~ UI_ROW_HEIGHT,      // [2] day time display
                  //~ UI_BUTTONS_HEIGHT,  // [3] day buttons
                  //~ -1,
                  //~ UI_BUTTONS_HEIGHT,  // [5] hour slider
                  //~ -1,
                  //~ UI_BUTTONS_HEIGHT,  // [7] minute slider
                  //~ -1,
                  //~ UI_BUTTONS_HEIGHT,  // [9] button bar
                  //~ 0);

  //    button and title bar...
  layoutRow = LayoutRowEqually (layoutMain[7], 2);

  btnBack.Set (layoutRow[0], COL_AC_MAIN, IconGet ("ic-back-48"));
  btnBack.SetCbPushed (CbOnButtonPushed, this);
  btnBack.SetHotkey (SDLK_ESCAPE);

  btnEnable.Set (layoutRow[1], COL_AC_MAIN);
  btnEnable.SetCbPushed (CbOnButtonPushed, this);
  btnEnable.SetHotkey (SDLK_SPACE);

  free (layoutRow);

  //    time displays...
  layoutRow = LayoutRowEqually (layoutMain[1], 7);
  for (n = 0; n < 7; n++)
    wdgTimes[n].Set (CreateSurface (layoutRow[n].w, layoutRow[n].h), layoutRow[n].x, layoutRow[n].y);
  free (layoutRow);

  //    day buttons...
  layoutRow = LayoutRowEqually (layoutMain[2], 7);
  for (n = 0; n < 7; n++) {
    btnDays[n].Set (layoutRow[n], COL_AC_WORKDAY, DayNameShort (n), WHITE, fntNorm);
    btnDays[n].SetCbPushed (CbOnButtonPushed, this);
  }
  free (layoutRow);

  //    hour slider...
  layoutRow = LayoutRow (layoutMain[4], 2 * UI_SPACE, UI_DEC_INC_WIDTH, -1, UI_DEC_INC_WIDTH, 0);
  btnHourDec.Set (layoutRow[0], COL_AC_BUTTONS, "- 1:00", WHITE, fntNorm);
  btnHourDec.SetCbPushed (CbOnButtonPushed, this);
  btnHourDec.SetHotkey (SDLK_DOWN);
  sldHour.SetFormat (COL_AC_MAIN, DARK_GREY, DARK_GREY, TRANSPARENT, UI_SLIDER_WIDTH);
  sldHour.SetArea (layoutRow[1]);
  sldHour.SetInterval (0, 23);
  sldHour.SetCbValueChanged (CbOnSliderValueChanged, this);
  btnHourInc.Set (layoutRow[2], COL_AC_BUTTONS, "+ 1:00", WHITE, fntNorm);
  btnHourInc.SetCbPushed (CbOnButtonPushed, this);
  btnHourInc.SetHotkey (SDLK_UP);
  free (layoutRow);

  //   minute slider...
  layoutRow = LayoutRow (layoutMain[5], 2 * UI_SPACE, UI_DEC_INC_WIDTH, -1, UI_DEC_INC_WIDTH, 0);
  btnMinDec.Set (layoutRow[0], COL_AC_BUTTONS, "- 0:10", WHITE, fntNorm);
  btnMinDec.SetCbPushed (CbOnButtonPushed, this);
  btnMinDec.SetHotkey (SDLK_LEFT);
  sldMin.SetFormat (COL_AC_MAIN, DARK_GREY, DARK_GREY, TRANSPARENT, UI_SLIDER_WIDTH);
  sldMin.SetArea (layoutRow[1]);
  sldMin.SetInterval (0, 59);
  sldMin.SetCbValueChanged (CbOnSliderValueChanged, this);
  btnMinInc.Set (layoutRow[2], COL_AC_BUTTONS, "+ 0:10", WHITE, fntNorm);
  btnMinInc.SetCbPushed (CbOnButtonPushed, this);
  btnMinInc.SetHotkey (SDLK_RIGHT);
  free (layoutRow);

  free (layoutMain);

  // Complete layout...
  for (n = 0; n < 7; n++) UpdateDay (n);
  if (tAlarm != NEVER && DateOfTicks (tAlarm) == dateNow) selDay = today;
  else if (timeNow < TIME_OF(18,0,0)) selDay = today;
  else selDay = (today + 1) % 7;
  SetCurDay (selDay);
  UpdateVisibility ();
}


CScreenSetAlarmClock::~CScreenSetAlarmClock () {
  int n;

  Commit ();
  for (n = 0; n < 7; n++) {
    SurfaceFree (wdgTimes[n].GetSurface ());
    wdgTimes[n].SetSurface (NULL);
  }
}


void CScreenSetAlarmClock::Commit () {
  CString s;
  int n;

  if (enabled != envAlarmEnabled) {
    envAlarmEnabled = enabled;
    EnvPut (envAlarmEnabledKey, enabled);
    AlarmClockEnableDisable (enabled);
  }
  for (n = 0; n < 7; n++) if (timeSetChanged[n]) {
    EnvPut (StringF (&s, "var.alarm.timeSet.%i", n), timeSetList[n]);
    timeSetChanged[n] = false;
  }
  EnvFlush ();      // flush to disk
  UpdateTAlarm ();
}


void CScreenSetAlarmClock::OnButtonPushed (CButton *btn, bool longPush) {
  int d, dLast, t, tSliders;

  if (btn == &btnBack) Return ();

  if (btn == &btnEnable) {
    enabled = !enabled;
    UpdateVisibility ();
  }

  tSliders = sldHour.GetValue () * 60 + sldMin.GetValue ();

  if (btn >= &btnDays[0] && btn < &btnDays[7]) {
    d = btn - btnDays;
    if (d == curDay) {
      t = timeSetList[curDay];
      if (t < 0) SetCurTime (tSliders, true);
      else SetCurTime (t ? -t : -1, true);
    }
    else {
      dLast = curDay;
      SetCurDay (d);
      if (longPush) if (dLast >= 0 && dLast < 7) SetCurTime (timeSetList[dLast], true);
    }
  }

  if (btn == &btnHourDec) SetCurTime ((tSliders + 23*60) % 1440, true);
  if (btn == &btnHourInc) SetCurTime ((tSliders +  1*60) % 1440, true);

  if (btn == &btnMinDec) {
    t = tSliders + 9;
    t = t - (t %10) + 24*60 - 10;
    SetCurTime (t % 1440, true);
  }
  if (btn == &btnMinInc) {
    t = tSliders + 10;
    t = t - (t %10);
    SetCurTime (t % 1440, true);
  }
}


void CScreenSetAlarmClock::OnSliderValueChanged (CSlider *slider, int, int) {
  SetCurTime (sldHour.GetValue () * 60 + sldMin.GetValue (), false);
}


void AlarmClockRunSetDialog () {
  CScreenSetAlarmClock scr;

  SystemActiveLock ("_alarmclock_setup");
  scr.Run ();
  scr.Commit ();
  SystemActiveUnlock ("_alarmclock_setup");
}





// *************************** Top-level *************************************


void AlarmClockInit () {
  CString s;
  int n;

  // Environment...
  EnvGetPath (envAlarmRingFileKey, envAlarmRingFile);

  // Read time settings...
  for (n = 0; n < 7; n++)
    timeSetList[n] = EnvGetInt (StringF (&s, "var.alarm.timeSet.%i", n), -1);

  // Init state according to the 'envAlarmEnabled' setting ...
  acState = envAlarmEnabled ? acsStandby : acsDisabled;
  //~ INFOF (("### envAlarmEnabled = %i / '%s'", envAlarmEnabled, EnvGet (envAlarmEnabledKey)));

  // Setup timer & times...
  acTimer.Set (Iterate);

  // Update everything...
  UpdateTAlarm ();
}


void AlarmClockDone () {
  AlarmClockStop (acsDisabled);
  acTimer.Clear ();
}


TAlarmClockState AlarmClockGetState () {
  return acState;
}


void AlarmClockEnableDisable (bool enable) {
  if (!enable) AlarmClockStop (acsDisabled);
  else {
    if (acState == acsDisabled) {
      acState = acsStandby;
      UpdateTAlarm ();
    }
  }
}


void AlarmClockOff () {
  if (acState != acsDisabled) AlarmClockStop (acsStandby);
}


void AlarmClockSnooze () {
  if (acState != acsDisabled) AlarmClockStop (acsSnooze);
}


static void CbAlarmClockOnButtonPushed (CButton *, bool longPush, void *) {
  AlarmClockHandlePushed (true, longPush);
}


void AlarmClockSetButton (class CButton *btn) {
  if (acButton) acButton->ClearLabel ();
  acButton = btn;
  if (acButton) {
    acButton->SetCbPushed (CbAlarmClockOnButtonPushed);
    UpdateAcSurface ();
  }
}


void AlarmClockHandlePushed (bool enableOff, bool longPush) {
  if (enableOff && longPush) AlarmClockOff ();             // main icon: long push switches off, normal push snoozes
  else {
    if (AlarmClockStateIsAlarm (acState)) AlarmClockSnooze (); // external snooze button: never switch off (too dangerous)
    else AlarmClockRunSetDialog ();                        // not ringing
  }
}
