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


#include "ui_base.H"

#include "system.H"
#include "apps.H"
#include "ui_screen.H"

#include <locale.h>



#define ICONS_DIR "share/icons"
#define FONT_FILE "share/fonts/"



static SDL_Window *sdlWindow = NULL;

SDL_Renderer *uiSdlRenderer = NULL;

// SDL_USEREVENT:
//    'code' = 0: main thread callback ('data1' = function, 'data2' = data)





// *************************** Environment Options *****************************


ENV_PARA_INT ("ui.longPushTime", envUiLongPushTime, 500);
  /* Time for a long push in milliseconds
   */
ENV_PARA_INT ("ui.longPushTolerance", envUiLongPushTolerance, 16);
  /* Tolerance for motion during a long push in pixels
   */
ENV_PARA_BOOL ("ui.resizable", envUiResizable, true);
  /* Selects whether the UI window is resizable on startup
   *
   * This determines the X window's "resizable" option when the application starts.
   * Under a tiling window manager (e.g. awesome), this also determines whether
   * the window opens as a floating window (false) or in tiling mode (true).
   *
   * The flag can be toggled at runtime by pushing the F12 button.
   */
ENV_PARA_NOVAR ("ui.audioDev", const char *, envUiAudioDev, NULL);
  /* Audio device for UI signaling (phone ringing, alarm clock)
   */





// *************************** Events ******************************************


static bool uiClosed = false;
static bool sdlPaused = false;


#pragma GCC diagnostic ignored "-Wunused-function"

static void PrintSDL_Event (SDL_Event *ev) {
  switch (ev->type) {
    case SDL_QUIT:
      INFO ("Event: SDL_Quit");
      break;
    case SDL_MOUSEMOTION:
      INFOF(("Event: SDL_MOUSEMOTION (timestamp = %i, x = %i, y = %i, xrel = %i, yrel = %i)",
            ev->motion.timestamp, ev->motion.x, ev->motion.y, ev->motion.xrel, ev->motion.yrel));
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      INFOF(("Event: SDL_MOUSEBUTTON%s (timestamp = %i, x = %i, y = %i, clicks = %i)",
            ev->type == SDL_MOUSEBUTTONDOWN ? "DOWN" : "UP",
            ev->button.timestamp, ev->button.x, ev->button.y, ev->button.clicks));
      break;
    case SDL_FINGERMOTION:
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
      INFOF(("Event: SDL_FINGER%s (timestamp = %i, touchId = %i, fingerId = %i, x = %i, y = %i, dx = %i, dy = %i, pressure = %f",
            ev->type == SDL_FINGERMOTION ? "MOTION" : ev->type == SDL_FINGERDOWN ? "DOWN" : "UP",
            ev->tfinger.timestamp, (int) ev->tfinger.touchId, (int) ev->tfinger.fingerId,
            (int) (ev->tfinger.x * UI_RES_X), (int) (ev->tfinger.y * UI_RES_Y),
            (int) (ev->tfinger.dx * UI_RES_X), (int) (ev->tfinger.dy * UI_RES_Y),
            ev->tfinger.pressure));
      break;
    default:
      INFOF(("Event: SDL_... (type = %i)", ev->type));
      //printf ("Event %i, WINDOWEVENT = %i\n", ev.type, (int) ev.window.event);
    }
}


static CTimer longPushTimer;
static SDL_Event longPushMouseEvent;


static void CbLongPushTimer (CTimer *, void *) {
  SDL_Event ev = longPushMouseEvent;
    // WORKAROUND [SDL 2.0.7, 2018-02-06]: The passed event is modified inside 'SDL_PushEvent ()'!
  SDL_PushEvent (&ev);
}


void UiIterate (bool noWait) {
  SDL_Event ev;
  CScreen *activeScreen = CScreen::ActiveScreen ();
  void (*func) (void *);
  ESystemMode newMode, lastMode;
  TTicksMonotonic t, t1;
  bool haveEvent;
  //~ int events;

  // Wait for an event...
  //~ INFO ("### Wait for event...");
  if (sdlPaused || SDL_HasEvent(SDL_APP_WILLENTERBACKGROUND) || SDL_HasEvent(SDL_APP_DIDENTERBACKGROUND)) {
    //~ INFO ("### Wait for event (Android/background mode) ...");
    // In Android, if the app is paused (-> sdlPaused == true), the function
    // 'Android_PumpEvents ()' in SDL2-2.0.3/src/video/android/SDL_androidevents.c
    // blocks forever (until resume) by default. To make sure that the Home2L timers continue
    // working, we try to avoid calling or have SDL calling 'SDL_PumpEvents ()' here...
    haveEvent = (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1);
    if (!noWait && !haveEvent) {
      t = TimerGetDelay ();
      if (t > 1000) t = 1000;   // wait for at most 1 second - the App may get resumed during the following 'Sleep' operation
         // Note: We may use a signal to quit sleeping in this case. However, there are regular timers (light sensor, phone)
         //       typically active, so that it is not worth spending much effort here.
      Sleep (t);
    }
  }
  else {
    //~ INFO ("### Wait for event (normal) ...");
    if (noWait) haveEvent = SDL_PollEvent (&ev) == 1;
    else {
      t = TicksMonotonicNow ();               // WORKAROUND (see below)
      haveEvent = SDL_WaitEventTimeout (&ev, t1 = TimerGetDelay ()) == 1;
      if (TicksMonotonicNow () - t > 2000) {  // WORKAROUND (see below)
        WARNINGF (("### SDL_WaitEventTimeout () returned late after %i ms: TimerGetDelay () = %i ms",
                 TicksMonotonicNow () - t, t1));
        // WORKAROUND (2019-05-20, SDL 2.07 on Android):
        //   Sometimes SDL_WaitEventTimeout () repeatedly waits for approx. 1 minute
        //   instead of the maximum wait time passed after some run time making the UI
        //   unusable.
        //   This workaround causes WallClock to abort (and be restarted) if this happens.
        _exit (3);      // exit explicitly, without showing the Android message box (an wait infinitly for user response)
      }
    }
  }

  // Handle all available events...
  //~ events = 0;
  while (haveEvent) {
    //~ INFOF (("### Have event (#%i, sdlPaused = %i): handle it...", events++, (int) sdlPaused));
    //~ PrintSDL_Event (&ev);

    // Handle system-level events...
    haveEvent = false;
    switch (ev.type) {

      // General/system events...
      case SDL_QUIT:
        uiClosed = true;
        break;

      case SDL_USEREVENT:
        switch (ev.user.code) {

          case evMainThreadCallback:
            func = (void (*) (void *)) ev.user.data1;
            func (ev.user.data2);
            break;

          case evSystemModeChanged:
            newMode = (ESystemMode) (uint32_t) (long) ev.user.data1;
            lastMode = (ESystemMode) (uint32_t) (long) ev.user.data2;
            //~ INFOF (("### evSystemModeChanged %i -> %i", lastMode, newMode));
            if (lastMode >= smStandby && newMode < smStandby)              // fall asleep ...
              if (CScreen::ActiveScreen ()) CScreen::ActiveScreen ()->Deactivate ();
            if (lastMode < smStandby && newMode >= smStandby) {            // wake up ...
              if (CScreen::ActiveScreen ()) CScreen::ActiveScreen ()->Activate ();
              else AppActivate (appIdHome);
            }

            if (newMode == smStandby) AppActivate (appIdHome);

            haveEvent = true;
            break;

          default:
            haveEvent = true;   // hand over other user events to the UI
        }
        break;

      case SDL_WINDOWEVENT:
        if (ev.window.event == SDL_WINDOWEVENT_EXPOSED)    // we need to redraw now
          if (activeScreen) activeScreen->Changed ();
        break;

      // Moving to background/foreground (Android) ...
#if ANDROID == 1
      case SDL_APP_WILLENTERBACKGROUND:
        INFO ("###   ... SDL_APP_WILLENTERBACKGROUND");
        sdlPaused = true;
        SystemReportUiVisibility (false);
        break;
      case SDL_APP_DIDENTERFOREGROUND:
        INFO ("###   ... SDL_APP_DIDENTERFOREGROUND");
        sdlPaused = false;
        CScreen::Refresh ();
        SystemReportUiVisibility (true);
        SystemWakeupStandby ();
        break;
#endif // ANDROID == 1

      // Key events...
      case SDL_KEYDOWN:
        SystemWakeup ();
        if (ev.key.keysym.mod & KMOD_CTRL && ev.key.keysym.sym == SDLK_q) {
          uiClosed = true;
          break;
        }
        if (ev.key.keysym.mod == KMOD_NONE) switch (ev.key.keysym.sym) {
#if ANDROID == 1
          case SDLK_AC_BACK:
            SystemGoBackground ();
            break;
#else // ANDROID == 0 ...
          case SDLK_F9:
            UiSetWindowFullScreen (false);
            UiSetWindowSize (UI_RES_X/2, UI_RES_Y/2);
            break;
          case SDLK_F10:
            UiSetWindowFullScreen (false);
            UiSetWindowSize (UI_RES_X, UI_RES_Y);
            break;
          case SDLK_F11:
            UiToggleWindowFullScreen ();
            break;
          case SDLK_F12:
            UiToggleWindowResizable ();
            // Give visual feedback...
            if (UiGetWindowResizable ())
              UiSetWindowSize (UI_RES_X * 17/16, UI_RES_Y);
            else {
              UiSetWindowSize (UI_RES_X, UI_RES_Y);
            }
            break;
#endif // ANDROID == 0
          default:
            haveEvent = true;
          }
        else haveEvent = true;
        break;

      // Mouse events...
      case SDL_MOUSEBUTTONDOWN:
        if (ev.button.clicks == 1 && !longPushTimer.Pending ()) {
          longPushMouseEvent = ev;
          longPushMouseEvent.button.timestamp = ev.button.timestamp + envUiLongPushTime;
          longPushMouseEvent.button.clicks = 2;
          longPushTimer.Set (TicksMonotonicNow () + envUiLongPushTime, 0, CbLongPushTimer);
          //~ INFOF (("### Setting long push event: x = %i/%i, y = %i/%i", ev.button.x, longPushMouseEvent.button.x, ev.button.y, longPushMouseEvent.button.y));
        }
        if (ev.button.clicks == 2) {
          // WORKAROUND [SDL 2.0.7, 2018-02-06]:
          //   Appearantly, SDL2 changes the coordinates of mouse events between pushing and handling,
          //   so that 'SDL_PushEvent (&longPushMouseEvent)' does not work.
          ev.button.x = longPushMouseEvent.button.x;
          ev.button.y = longPushMouseEvent.button.y;
          //~ INFOF (("### Received long push event: x = %i/%i, y = %i/%i", ev.button.x, longPushMouseEvent.button.x, ev.button.y, longPushMouseEvent.button.y));
        }
        SystemWakeup ();
        haveEvent = (SystemGetMode () >= smStandby);
          // (only) in "off" mode, consume this event, the user does not see where he is clicking/touching
        //~ INFOF (("### UiIterate: SDL_MOUSEBUTTONDOWN, haveEvent = %i", haveEvent));
        break;
      case SDL_MOUSEBUTTONUP:
        longPushTimer.Clear ();
        haveEvent = true;
        //~ INFOF (("### UiIterate: SDL_MOUSEBUTTONUP, haveEvent = %i", haveEvent));
        break;
      case SDL_MOUSEMOTION:
        if (abs (ev.motion.x - longPushMouseEvent.button.x) + abs (ev.motion.y - longPushMouseEvent.button.y) > envUiLongPushTolerance)
          longPushTimer.Clear ();
        haveEvent = (ev.motion.state & SDL_BUTTON_LMASK) != 0;
        break;
      default:
        haveEvent = true;
    }

    // Call user and screen handlers...
    // NOTE: The following calls may imply recursive calls of this 'Iterate ()', which may complete
    //       before this invocation continues. The following code must be robust against this (e.g.
    //       not perform duplicate work).
    if (haveEvent && activeScreen) activeScreen->HandleEvent (&ev);

    // Check for further pending events and eventually loop again...
    haveEvent = (SDL_PeepEvents (&ev, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) == 1);

  } // while (haveEvent)

  // Iterate timers...
  //~ INFO ("### TimerIterate...");
  TimerIterate ();
  //~ INFO ("### TimerIterate done.");

  // Update screen if changed ...
  if (!sdlPaused) CScreen::RenderUpdate ();
}


void UiQuit () {
  // Note: earlier versions if SDL required to call '_exit (0);' for Android here.
  uiClosed = true;
}


bool UiIsClosed () { return uiClosed; }


void UiPushUserEvent (EUserEvent code, void *data1, void *data2) {
  SDL_Event event;

  SDL_zero (event);
  event.type = SDL_USEREVENT;
  event.user.code = code;
  event.user.data1 = data1;
  event.user.data2 = data2;
  SDL_PushEvent (&event);
}





// *************************** SDL audio ***************************************


static CTimer audioTimer;
static int audioRepetitions;      // -1 : repeat forever (e.g. for phone ringing)
static TTicksMonotonic audioRepetitionGap;   // time to wait between two repetitions
static bool audioPlaying = false;

static const char *sdlAudioDeviceName = NULL;
static SDL_AudioDeviceID sdlAudioDeviceId = 0;

static SDL_AudioSpec sdlAudioSpec;
static Uint8 *sdlAudioBuf = NULL;
static Uint32 sdlAudioLen;


static void AudioIterate (CTimer *timer = NULL, void *data = NULL) {
  bool ok;

  // Open audio device...
  ok = true;
  if (!sdlAudioDeviceId) {
    sdlAudioDeviceId = SDL_OpenAudioDevice (sdlAudioDeviceName, 0, &sdlAudioSpec, NULL, 0);
      // Note: it is allowed to pass 'NULL' as the audio device name ('sdlAudioDeviceName')
    if (!sdlAudioDeviceId) {
      WARNINGF (("Could not open audio device '%s': %s\n", sdlAudioDeviceName ? sdlAudioDeviceName : "[default]", SDL_GetError()));
      ok = false;
    }
  }

  // Enqueue new data...
  if (ok) {
    if (SDL_QueueAudio (sdlAudioDeviceId, sdlAudioBuf, sdlAudioLen)) {
      WARNINGF (("Could not queue audio data: %s\n", SDL_GetError()));
      ok = false;
    }
    else SDL_PauseAudioDevice (sdlAudioDeviceId, 0);   // unpause device
  }

  //~ INFOF (("### fmt = %08x, bits per sample = %i", sdlAudioSpec.format, SDL_AUDIO_BITSIZE (sdlAudioSpec.format)));

  // Schedule next iteration...
  if (!ok) {
    audioTimer.Reschedule (TicksMonotonicNow () + 1000);   // had error: retry in one second
  }
  else {
    if (audioRepetitions == 0) AudioStop ();   // neither <0 (repeat forever) nor >0 (finite repetions left)
    else {
      audioTimer.Reschedule (
        TicksMonotonicNow ()
        + (1000 * 8 / SDL_AUDIO_BITSIZE (sdlAudioSpec.format) * sdlAudioLen / sdlAudioSpec.freq)
            // 1000 ms * 8 bits per byte / bits per sample * sdlAudioLen / frequency
        + audioRepetitionGap
      );
    }
    if (audioRepetitions > 0) audioRepetitions--;
  }
}


void AudioStart (const char *fileName, int repetitions, TTicks repetitionGap) {
  CString s;

  AudioStop ();
  if (!fileName || !repetitions) return;      // no file given -> keep silence

  // Load WAV file...
  if (!SDL_LoadWAV (EnvGetHome2lRootPath (&s, fileName), &sdlAudioSpec, &sdlAudioBuf, &sdlAudioLen)) {
    ERRORF (("Could not load audio file '%s': %s\n", fileName, SDL_GetError()));
  }

  // Start playing...
  audioPlaying = true;
  audioRepetitions = repetitions;
  audioRepetitionGap = repetitionGap;
  AudioIterate ();
}


void AudioStop () {
  audioTimer.Clear ();
  audioPlaying = false;
  if (sdlAudioDeviceId) {
    SDL_CloseAudioDevice (sdlAudioDeviceId);
    sdlAudioDeviceId = 0;
  }
  if (sdlAudioBuf) {
    SDL_FreeWAV (sdlAudioBuf);
    sdlAudioBuf = NULL;
  }
}


bool AudioIsPlaying () {
  return audioPlaying;
}


static void AudioInit () {
  int i, n;

  sdlAudioDeviceName = EnvGet (envUiAudioDevKey);
  if (sdlAudioDeviceName) if (sdlAudioDeviceName[0] == '\0') sdlAudioDeviceName = NULL;

  SDL_InitSubSystem (SDL_INIT_AUDIO);

  audioTimer.Set (AudioIterate);

  if (envDebug > 0) {
    n = SDL_GetNumAudioDevices (0);
    DEBUGF (1, ("SDL Audio: Driver = '%s'", SDL_GetCurrentAudioDriver ()));
    for (i = 0; i < n; ++i) {
      DEBUGF (1, ("SDL Audio: Playback device %d: '%s'", i, SDL_GetAudioDeviceName(i, 0)));
    }
  }
}





// *************************** TColor helpers *******************************


static inline Uint8 AddSubSat (Uint8 chan, int d) {
  int ret = ((int) chan) + d;
  if (ret < 0) ret = 0;
  if (ret > 255) ret = 255;
  return (Uint8) ret;
}


static inline Uint8 ScaleSat (Uint8 chan, int factor) {
  int ret = (((int) chan) * factor) >> 8;
  if (ret < 0) ret = 0;
  if (ret > 255) ret = 255;
  return (Uint8) ret;
}


TColor ColorSum (TColor col1, TColor col2) {
  TColor ret;
  ret.a = AddSubSat (col1.a, col2.a);
  ret.r = AddSubSat (col1.r, col2.r);
  ret.g = AddSubSat (col1.g, col2.g);
  ret.b = AddSubSat (col1.b, col2.b);
  return ret;
}


TColor ColorBrighter (TColor color, int d) {
  TColor ret;
  ret.a = color.a;
  ret.r = AddSubSat (color.r, d);
  ret.g = AddSubSat (color.g, d);
  ret.b = AddSubSat (color.b, d);
  return ret;
}


TColor ColorScale (TColor color, int factor) {
  TColor ret;
  ret.a = color.a;
  ret.r = ScaleSat (color.r, factor);
  ret.g = ScaleSat (color.g, factor);
  ret.b = ScaleSat (color.b, factor);
  return ret;
}


TColor ColorBlend (TColor color0, TColor color1, int weight1) {
  TColor ret;
  ret.a = color0.a + (Uint8) ((weight1 * (int) (color1.a - color0.a)) >> 8);
  ret.r = color0.r + (Uint8) ((weight1 * (int) (color1.r - color0.r)) >> 8);
  ret.g = color0.g + (Uint8) ((weight1 * (int) (color1.g - color0.g)) >> 8);
  ret.b = color0.b + (Uint8) ((weight1 * (int) (color1.b - color0.b)) >> 8);
  return ret;
}





// *************************** SDL_Rect helpers ********************************



const SDL_Rect rectScreen = { .x = 0, .y = 0, .w = UI_RES_X, .h = UI_RES_Y };


void RectAlign (SDL_Rect *rect, SDL_Rect container, int hAlign, int vAlign) {
  rect->x = container.x;
  rect->y = container.y;
  switch (hAlign) {
    case -1: break;
    case 0: rect->x += (container.w - rect->w) / 2; break;
    case 1: rect->x += (container.w - rect->w); break;
  }
  switch (vAlign) {
    case -1: break;
    case 0: rect->y += (container.h - rect->h) / 2; break;
    case 1: rect->y += (container.h - rect->h); break;
  }
}





// *************************** Surface helpers *********************************


void SurfaceNormalize (SDL_Surface **pSurf) {
  SDL_Surface *newSurf;

  if (!*pSurf) return;
  if ((*pSurf)->format->format != SELECTED_SDL_PIXELFORMAT) {
    newSurf = SDL_ConvertSurfaceFormat (*pSurf, SELECTED_SDL_PIXELFORMAT, 0);
    SDL_FreeSurface (*pSurf);
    *pSurf = newSurf;
  }
}

void SurfaceRecolor (SDL_Surface *surf, TColor color) {
  Uint32 *pixels, *line;
  Uint32 rgbColor;
  int x, y;

  //ASSERT (surf->format->format == SELECTED_SDL_PIXELFORMAT);
  ASSERT (SDL_LockSurface (surf) == 0);
  pixels = (Uint32 *) surf->pixels;

  rgbColor = ToUint32 (color) & COL_MASK_RGB;
  line = pixels;
  for (y = 0; y < surf->h; y++) {
    for (x = 0; x < surf->w; x++)
      line [x] = (line[x] & COL_MASK_A) | rgbColor;
    line += surf->pitch / sizeof (Uint32);
  }

  SDL_UnlockSurface (surf);
}


SDL_Surface *SurfaceGetOpaqueCopy (SDL_Surface *surf, TColor backColor) {
  SDL_Surface *ret;

  ret = CreateSurface (surf->w, surf->h);
  SDL_FillRect (ret, NULL, ToUint32 (backColor));
  SDL_SetSurfaceBlendMode (surf, SDL_BLENDMODE_BLEND);
  SDL_BlitSurface (surf, NULL, ret, NULL);
  return ret;
}


void SurfaceMakeTransparentMono (SDL_Surface *surf, Uint8 opaqueLevel) {
  Uint32 *pixels, *line, color, alpha, factor;
  int x, y;

  ASSERT (COL_MASK_R == 0x00ff0000 && COL_MASK_A == 0xff000000);
  ASSERT (surf->format->format == SELECTED_SDL_PIXELFORMAT);

  ASSERT (SDL_LockSurface (surf) == 0);
  pixels = (Uint32 *) surf->pixels;

  line = pixels;
  if (opaqueLevel == 0xff) {
    for (y = 0; y < surf->h; y++) {
      for (x = 0; x < surf->w; x++)
        line [x] = (line[x] << 8) | 0x00ffffff;
      line += surf->pitch / sizeof (Uint32);
    }
  }
  else {
    ASSERT (opaqueLevel != 0);
    factor = 0x10000 / ((Uint32) opaqueLevel);   // factor is in range 0x100..0x10000 -> 8 fractional bits
    color = (Uint32) opaqueLevel;
    color = color | (color << 8) | (color << 16);
    for (y = 0; y < surf->h; y++) {
      for (x = 0; x < surf->w; x++) {
        alpha = ( (Uint32) ((line[x] >> 16) & 0xff) * factor) >> 8;
        //~ if (line[x] != 0xff000000) INFOF (("###   pixel = %08x, factor = %x, alpha = %x, color = %08x", line[x], factor, alpha, color));
        if (alpha > 0xff) alpha = 0xff;
        line [x] = (alpha << 24) | color;
      }
      line += surf->pitch / sizeof (Uint32);
    }
  }

  SDL_UnlockSurface (surf);
}


void SurfaceBlit (SDL_Surface *src, SDL_Rect *srcRect, SDL_Surface *dst, SDL_Rect *dstRect, int hAlign, int vAlign, SDL_BlendMode blendMode) {
  SDL_Rect placeRect;

  if (!src) return;   // empty/no surface (nothing to blit)

  placeRect = srcRect ? *srcRect : Rect (src);
  RectAlign (&placeRect, dstRect ? *dstRect : Rect (dst), hAlign, vAlign);

  SDL_SetSurfaceBlendMode (src, blendMode);
  SDL_BlitSurface (src, srcRect, dst, &placeRect);
}


SDL_Surface *SurfaceGetScaledDownCopy (SDL_Surface *surf, int factor, bool preserveThinLines) {
  SDL_Surface *ret;
  Uint32 *srcPixels, *dstPixels, *src, *dst, pixel;
  Uint32 accu[4], weight;
  int n, x, y, dx, dy, srcPitch, dstPitch;

  // Sanity...
  if (!surf) return NULL;
  if (factor == 1) return SurfaceDup (surf);    // Shortcut
  ASSERT (surf->w % factor == 0 && surf->h % factor == 0);
  ASSERT (surf->format->format == SELECTED_SDL_PIXELFORMAT);

  // SDL_BlitScaled (surf, NULL, ret, NULL);    // [2019-03-01] SDL2 does not perform interpolation

  // Lock source surface...
  ASSERT (SDL_LockSurface (surf) == 0);
  srcPixels = (Uint32 *) surf->pixels;
  ASSERT (surf->pitch % sizeof (Uint32) == 0);
  srcPitch = surf->pitch / sizeof (Uint32);

  // Create and lock destination surface...
  ret = CreateSurface (surf->w / factor, surf->h / factor);
  ASSERT (SDL_LockSurface (ret) == 0);
  dstPixels = (Uint32 *) ret->pixels;
  ASSERT (ret->pitch % sizeof (Uint32) == 0);
  dstPitch = ret->pitch / sizeof (Uint32);

  // Scale down with avaraging ...
  weight = 0x10000 / (factor * factor);   // 16 fractional bits
  for (y = 0; y < ret->h; y++) {
    dst = dstPixels + y * dstPitch;
    for (x = 0; x < ret->w; x++) {
      accu[0] = accu[1] = accu[2] = accu[3] = 0;

      for (dy = 0; dy < factor; dy++) {
        src = srcPixels + (y * factor + dy) * srcPitch + x * factor;
        for (dx = 0; dx < factor; dx++) {
          pixel = *(src++);
          accu[0] += (pixel >> 24) & 0xff;
          accu[1] += (pixel >> 16) & 0xff;
          accu[2] += (pixel >> 8) & 0xff;
          accu[3] += pixel & 0xff;
        }
      }

      for (n = 0; n < 4; n++) accu[n] *= weight;
      if (preserveThinLines) accu[0] = 0xff0000 * pow ((double) (accu[0] >> 16) * (1.0/255.0), 0.2);
      pixel =   ((accu[0] << 8)   & 0xff000000)
              | ( accu[1]         & 0x00ff0000)
              | ((accu[2] >> 8)   & 0x0000ff00)
              | ((accu[3] >> 16)  & 0x000000ff);
      *(dst++) = pixel;
    }
  }

  // Done...
  SDL_UnlockSurface (surf);
  SDL_UnlockSurface (ret);
  return ret;
}


SDL_Surface *SurfaceGetFlippedAndRotatedCopy (SDL_Surface *surf, int orient) {
  SDL_Surface *ret;
  Uint32 *srcPixels, *dstPixels, *src, *dst, pixel;
  int x, y, w, h, srcPitch, dstPitch;
  int rotations;
  bool flipH, flipV;

  ASSERT (surf->format->format == SELECTED_SDL_PIXELFORMAT);

  flipH = ORIENT_FLIPH (orient);
  flipV = false;
  rotations = ORIENT_ROT (orient);

  // Transform rotations > 90° to two flips before the rotation...
  if (rotations >= 2) {
    flipH = !flipH;
    flipV = !flipV;
    rotations &= 1;
  }

  // Eventually swap flips to allow to rotate first before flipping...
  if (rotations) {    // can only be 0 or 1 now
    flipH = flipV ^ flipH;
    flipV = flipV ^ flipH;
    flipH = flipV ^ flipH;
  }

  // Copy the surface, lock the result and eventually rotate...
  if (!rotations) {

    // No rotation...
    ret = SurfaceDup (surf);
    ASSERT (SDL_LockSurface (ret) == 0);
    dstPixels = (Uint32 *) ret->pixels;
    ASSERT (ret->pitch % sizeof (Uint32) == 0);
    dstPitch = ret->pitch / sizeof (Uint32);
    w = surf->w;
    h = surf->h;
  }
  else {

    // With rotation: Translate + (un)flip horizontally ...
    flipH = !flipH;
    w = surf->h;
    h = surf->w;
    ret = CreateSurface (w, h);

    ASSERT (SDL_LockSurface (ret) == 0);
    dstPixels = (Uint32 *) ret->pixels;
    ASSERT (ret->pitch % sizeof (Uint32) == 0);
    dstPitch = ret->pitch / sizeof (Uint32);

    ASSERT (SDL_LockSurface (surf) == 0);
    srcPixels = (Uint32 *) surf->pixels;
    ASSERT (surf->pitch % sizeof (Uint32) == 0);
    srcPitch = surf->pitch / sizeof (Uint32);

    // Travers destination (not source) column-wise and hopefully benefit from write buffers...
    for (x = 0; x < w; x++) {
      src = srcPixels + srcPitch * x;
      dst = dstPixels + x;
      for (y = 0; y < h; y++) {
        *dst = *(src++);
        dst += dstPitch;
      }
    }

    SDL_UnlockSurface (surf);
  }

  // Eventually flip horizontally...
  if (flipH) {
    for (y = 0; y < h; y++) {
      dst = dstPixels + dstPitch * y;
      src = dst + w-1;
      while (src > dst) {
        pixel = *src;
        *src = *dst;
        *dst = pixel;
        //~ *dst = 0xffff0000;
        src--;
        dst++;
      }
    }
  }

  // Eventually flip vertically...
  if (flipV) {
    for (y = 0; y < h / 2; y++) {
      dst = dstPixels + dstPitch * y;
      src = dstPixels + dstPitch * (h-1 - y);
      for (x = 0; x < w; x++) {
        pixel = *src;
        *src = *dst;
        *dst = pixel;
        //~ *dst = 0xffff0000;
        src++;
        dst++;
      }
    }
  }

  // Done...
  SDL_UnlockSurface (ret);
  return ret;
}


SDL_Surface *SurfaceReadBmp (const char *fileName) {
  CString s;
  SDL_Surface *ret;

  fileName = EnvGetHome2lRootPath (&s, fileName);
  DEBUGF (1, ("Loading bitmap '%s'", fileName));
  ret = SDL_LoadBMP (fileName);
  SurfaceNormalize (&ret);    // tolerates 'ret == NULL'
  if (!ret) WARNINGF (("Unable to load bitmap '%s': %s", fileName, SDL_GetError ()));
  return ret;
}





// ********** CNetpbmReader **********



// Feeding the reader...
void CNetpbmReader::Clear () {
  SurfaceFree (&surf);
  state = NETPBM_IDLE;
}


void CNetpbmReader::Put (const char *line) {
  char arg[5];
  char *src, *dst;
  Uint32 *pixel;
  int val, x, y, c;
  bool ok;

  //~ puts ("\n\n\n\n");
  //~ puts (line);
  //~ puts ("\n\n\n\n");

  // Setup & sanity...
  if (state == NETPBM_IDLE) state = w = h = 0;
  if (state < 0 || !line) return;

  // Parsing loop...
  src = (char *) line;
  while (*src && state >= 0) {
    while (*src && *src != 'P' && (*src < '0' || *src > '9')) {   // advance to next argument...
      if (*src == '#') while (*src && *src != '\n') src++;      // skip comments
      else src++;
    }
    dst = arg;
    while (*src == 'P' || (*src >= '0' && *src <= '9')) {       // write out one word...
      if (dst < arg + sizeof(arg) - 1) *(dst++) = *src;
      src++;
    }
    *dst = '\0';
    //~ INFOF (("### NetPbm: '%s'", arg));

    // Process single argument...
    //~ INFOF(("# Read '%s', state = %i", arg, state));
    ok = IntFromString (arg, &val);
    if (state == 0) ok = true;        // The first entry is not an integer
    if (ok) switch (state) {
      case 0:       // Header ("P2" or "P3")...
        if (arg[0] == 'P' && arg[1] >= '2' && arg[1] <= '3' && arg[2] == '\0')
          format = arg[1] - '0';
        else ok = false;
        break;
      case 1:       // Width...
        w = val;
        break;
      case 2:       // Height...
        h = val;
        //~ INFOF (("### w = %i, h = %i", w, h));
        SurfaceSet (&surf, CreateSurface (w, h));
        break;
      case 3:       // Maximum color value (ignored)...
        break;
      default:      // Image data...
        // Calculate pixel address and (RGB) component...
        if (format == 2) {      // "P2" / grey
          c = 0;
          x = state - 4;
        }
        else {                  // "P3" / RGB
          c = state - 4;
          x = c / 3;
          c %= 3;
        }
        y = x / w;
        x %= w;
        //ASSERT (surf->format->format == SELECTED_SDL_PIXELFORMAT);
        ASSERT (SDL_LockSurface (surf) == 0);
        pixel = (Uint32 *) surf->pixels + y * (surf->pitch / sizeof (Uint32)) + x;
        if (format == 2)        // "P2" / grey
          *pixel = ToUint32 (255, 255, 255, val);
        else switch (c) {       // "P3" / RGB
          case 0: *pixel = ToUint32 (val, 0, 0); break;
          case 1: *pixel |= ToUint32 (0, val, 0); break;
          case 2: *pixel |= ToUint32 (0, 0, val); break;
        }
        SDL_UnlockSurface (surf);
    }
    if (ok) {
      state++;
      if (state >= 4 + w * h * (format == 2 ? 1 : 3)) state = NETPBM_SUCCESS;
      //~ INFOF(("# state = %i", state));
    }
    else {
      WARNING ("Unable to read Netpbm stream");
      SurfaceFree (&surf);
      state = NETPBM_ERROR;
    }
  }
}


bool CNetpbmReader::ReadFile (const char *fileName) {
  // TBD: Implement
  return false;
}


bool CNetpbmReader::ReadStream (int fd) {
  // TBD: Implement
  return false;
}


bool CNetpbmReader::ReadShell (CShell *shell) {
  // TBD: Implement
  return false;
}





// *************************** Icon handling ***********************************


#define MAX_ICON_NAME 64


struct TIconCacheItem {
  char name[MAX_ICON_NAME];
  TColor color, bgColor;
  int scaleDown, orient;
  SDL_Surface *sdlSurface;
  TIconCacheItem *next;
};


TIconCacheItem *iconCache = NULL;


static inline void IconInit () {}


static void IconDone () {
  TIconCacheItem *p;

  while (iconCache) {
    p = iconCache;
    iconCache = iconCache->next;
    SDL_FreeSurface (p->sdlSurface);
    delete p;
  }
}


SDL_Surface *IconGet (const char *name, TColor color, TColor bgColor, int scaleDown, int orient, bool preserveThinLines) {
  char fileName [512];
  TIconCacheItem *cacheItem, *baseCacheItem;
  SDL_Surface *surf, *surfBase;
  SDL_Palette *palette;
  SDL_Color sdlColor;
  int n, w;

  // Sanity...
  if (!name) return NULL;

  // Lookup in cache...
  baseCacheItem = NULL;
  for (cacheItem = iconCache; cacheItem; cacheItem = cacheItem->next) {
    if (strcmp (cacheItem->name, name) == 0 && cacheItem->scaleDown == scaleDown && cacheItem->orient == orient) {
      if (ToUint32 (cacheItem->color) == ToUint32 (color) && ToUint32 (cacheItem->bgColor) == ToUint32 (bgColor))
        return cacheItem->sdlSurface;   // Cache hit!
      else
        if (ToUint32 (cacheItem->bgColor) == ToUint32 (TRANSPARENT) && cacheItem->scaleDown == 1 && cacheItem->orient == 0)
          baseCacheItem = cacheItem;  // No hit, but a suitable base image
    }
  }

  // Cache miss: Get an appropriate base image (must be transparent, no scaling, uüpright orientation) ...
  if (!baseCacheItem) {

    // Load bitmap file ...
    snprintf (fileName, sizeof (fileName) - 1, "%s/share/icons/%s.bmp", EnvHome2lRoot (), name);
    DEBUGF (1, ("Loading icon '%s'", fileName));
    surfBase = SDL_LoadBMP (fileName);
    if (!surfBase)
      ERRORF (("Unable to load bitmap '%s': %s", fileName, SDL_GetError ()));
    DEBUGF (1, ("  bitmap '%s' loaded, pixel format: %s", fileName, SDL_GetPixelFormatName (surfBase->format->format)));
    palette = surfBase->format->palette;
    if (palette) {
      // We have a palette -> adopt it to the correct color efficiently...
      SDL_LockSurface (surfBase);
      for (n = 0; n < palette->ncolors; n++) {
        sdlColor = palette->colors[n];
        w = sdlColor.r;    // R component becomes opacity => we rely on a true grayscale image
        sdlColor.a = w;
        sdlColor.r = color.r;
        sdlColor.g = color.g;
        sdlColor.b = color.b;
        //INFOF(("  palette[%2i]: %08x -> %08x", n, palette->colors[n], sdlColor));
        palette->colors[n] = sdlColor;
      }
      SDL_UnlockSurface (surfBase);
      SurfaceNormalize (&surfBase);
    }
    else {
      ERRORF (("Unsupported format for Home2L icons (need grayscale + indexed): %s", fileName));
      //WARNING ("Bitmap format not optimal for efficiency. Ideal format would be: grayscale + indexed");
      // TBD: Change gray values to opacity, set color
      //SurfaceNormalize (&newItem.sdlSurface);
      //SurfaceRecolor (newItem.sdlSurface, color);
    }

    // Store base image in cache...
    baseCacheItem = new TIconCacheItem;
    strncpy (baseCacheItem->name, name, MAX_ICON_NAME-1);
    baseCacheItem->sdlSurface = surfBase;
    baseCacheItem->color = color;
    baseCacheItem->bgColor = TRANSPARENT;
    baseCacheItem->scaleDown = 1;
    baseCacheItem->orient = 0;
    baseCacheItem->next = iconCache;
    iconCache = baseCacheItem;
  }
  else surfBase = baseCacheItem->sdlSurface;
  surf = NULL;    // If this remains 'NULL', the base image can be returned.

  // Scale down if required ...
  if (scaleDown != 1)
    SurfaceSet (&surf, SurfaceGetScaledDownCopy (surf ? surf: surfBase, scaleDown, preserveThinLines));

  // Rotate and flip if required ...
  if (orient)
    SurfaceSet (&surf, SurfaceGetFlippedAndRotatedCopy (surf ? surf : surfBase, orient));

  // Re-color if required...
  if (ToUint32 (color) != ToUint32 (baseCacheItem->color)) {
    SurfaceSet (&surf, SurfaceDup (surf ? surf: surfBase));
    SurfaceRecolor (surf, color);
  }
  if (ToUint32 (bgColor) != ToUint32 (TRANSPARENT))
    SurfaceSet (&surf, SurfaceGetOpaqueCopy (surf ? surf : surfBase, bgColor));

  // Store result in cache if new ...
  if (surf) {
    cacheItem = new TIconCacheItem;
    strncpy (cacheItem->name, name, MAX_ICON_NAME-1);
    cacheItem->sdlSurface = surf;
    cacheItem->color = color;
    cacheItem->bgColor = bgColor;
    cacheItem->scaleDown = scaleDown;
    cacheItem->orient = orient;
    cacheItem->next = iconCache;
    iconCache = cacheItem;
  }

  // Done...
  return surf ? surf : surfBase;
}





// *************************** Font handling ***********************************


static const char *fontFileName [fntEND] = {
  "DejaVuSans.ttf",                 // fntNormal
  "DejaVuSans-Bold.ttf",            // fntBold
  "DejaVuSans-Oblique.ttf",         // fntItalic
  "DejaVuSans-BoldOblique.ttf",     // fntBoldItalic
  "DejaVuSans-ExtraLight.ttf",      // fntLight
  "DejaVuSansMono.ttf",             // fntMono
  "DejaVuSansMono-Bold.ttf",        // fntMonoBold
  "DejaVuSansMono-Oblique.ttf",     // fntMonoItalic
  "DejaVuSansMono-BoldOblique.ttf"  // fntMonoBoldItalic
};


struct TFontCacheItem {
  EFontStyle style;
  int size;
  TTF_Font *font;
  TFontCacheItem *next;
};


TFontCacheItem *fontCache = NULL;


static inline void FontInit () {}


static void FontDone () {
  TFontCacheItem *p;

  while (fontCache) {
    p = fontCache;
    fontCache = p->next;
    TTF_CloseFont (p->font);
    delete p;
  }
}


TTF_Font *FontGet (EFontStyle style, int size) {
  char fileName [300];
  TFontCacheItem *cacheItem;

  // Lookup cache...
  for (cacheItem = fontCache; cacheItem; cacheItem = cacheItem->next) {
    if (cacheItem->style == style && cacheItem->size == size)
      return cacheItem->font; // Cache hit!
  }

  // Load the font ...
  snprintf (fileName, 299, "%s/share/fonts/%s", EnvHome2lRoot (), fontFileName[style]);
  DEBUGF (1, ("Loading font '%s' (%ipt)", fileName, size));
  cacheItem = new TFontCacheItem;
  cacheItem->font = TTF_OpenFont (fileName, size);
  if (!cacheItem->font)
    ERRORF (("Unable to load font '%s'", fileName));

  // Store in cache...
  cacheItem->style = style;
  cacheItem->size = size;
  cacheItem->next = fontCache;
  fontCache = cacheItem;

  // Done...
  return cacheItem->font;
}


SDL_Surface *FontRenderText (TTF_Font *font, const char *text, TColor color) {
  SDL_Surface *surf = TTF_RenderUTF8_Blended (font, text, ToSDL_Color (color));
  SurfaceNormalize (&surf);
  return surf;
}


SDL_Surface *FontRenderText (TTF_Font *font, const char *text, TColor color, TColor bgColor) {
  SDL_Surface *surf = TTF_RenderUTF8_Shaded (font, text, ToSDL_Color (color), ToSDL_Color (bgColor));
  SurfaceNormalize (&surf);
  return surf;
}


int FontGetWidth (TTF_Font *font, const char *text, int textLen) {
  char *textCopy = NULL;
  int ret;

  if (textLen >= 0 && textLen < (int) strlen (text)) {
    textCopy = strdup (text);
    textCopy [textLen] = '\0';
    ret = FontGetWidth (font, textCopy, -1);
    free (textCopy);
  }
  else
    if (TTF_SizeUTF8 (font, text, &ret, NULL)) ret = 0;
  return ret;
}





// *************************** Complex text formatting *************************


// ***** CTextItem *****


class CTextItem {
  public:
    CTextItem () { text = NULL; surface = NULL; }
    ~CTextItem () { Done (); }
    void Done ();

    void SetText (const char *_text, int len = -1);
    void SetFormat (CTextFormat *_fmt) { fmt = *_fmt; }

    void Render ();

  protected:
    friend class CTextSet;

    char *text;
    CTextFormat fmt;

    SDL_Surface *surface;

    CTextItem *next;
};



void CTextItem::Done () {
  if (text) {
    free (text);
    text = NULL;
  }
  SurfaceFree (&surface);
}


void CTextItem::SetText (const char *_text, int len) {
  Done ();    // clean up earlier data
  if (len < 0) len = strlen (_text);
  text = (char *) malloc (len + 1);
  strncpy (text, _text, len);
  text[len] = '\0';
}


void CTextItem::Render () {
  if (surface) SDL_FreeSurface (surface);
  if (ToUint32 (fmt.bgColor) == ToUint32 (TRANSPARENT))
    surface = FontRenderText (fmt.font, text, fmt.color);
  else
    surface = FontRenderText (fmt.font, text, fmt.color, fmt.bgColor);
}



// ***** CTextSet *****


void CTextSet::Clear () {
  CTextItem *vic;

  SurfaceFree (&surface);
  while (firstItem) {
    vic = firstItem;
    firstItem = firstItem->next;
    delete vic;
  }
  height = 0;
}


void CTextSet::AddLines (const char *text, CTextFormat fmt, bool *retAbbreviated) {
  const char *p, *q, *wrap;
  char *w;
  CTextItem *textItem;
  int len, width, newHeight, lineHeight;
  bool abbreviated;

  // Sanity...
  abbreviated = false;
  newHeight = 0;
  lineHeight = FontGetHeight (fmt.font) + 2 * fmt.vSpace;
  if (fmt.maxHeight > 0 && lineHeight > fmt.maxHeight) {
    // not even a single line is possible => abbreviate to nothing
    abbreviated = true;
    text = NULL;
  }

  // Main loop...
  if (text) while (*text && !abbreviated) {
    p = text;
    while (*p != '\n' && *p != '\0') p++;

    textItem = new CTextItem ();
    textItem->SetFormat (&fmt);

    len = p - text;
    if (len > 0) textItem->SetText (text, len);
    else textItem->SetText ((char *) " ", 1);     // replace empty line with single space

    // Do line wrapping if necessary...
    if (fmt.maxWidth > 0 && len > 1) {
      if (FontGetWidth (fmt.font, textItem->text) > fmt.maxWidth) {
        wrap = NULL;
        // Search for ideal space to wrap...
        width = 0;
        for (q = text + 1; width < fmt.maxWidth && q < p; q++) {
          if (*q == ' ') {
            width = FontGetWidth (fmt.font, text, q-text);
            if (width <= fmt.maxWidth) wrap = q;
          }
        }
        // If no space found: try to break behind some punctuation mark...
        if (!wrap) {
          width = 0;
          for (q = text + 1; width < fmt.maxWidth && q < p; q++) {
            if (strchr (",.:;/=-+_", q[-1])) {
              width = FontGetWidth (fmt.font, text, q-text);
              if (width <= fmt.maxWidth) wrap = q;
            }
          }
        }
        // If no suitable position found: break at any character...
        if (!wrap) {
          wrap = text + 1;
          for (q = text + 1; width < fmt.maxWidth && q < p; q++) {
            width = FontGetWidth (fmt.font, text, q-text);
            if (width <= fmt.maxWidth) wrap = q;
          }
        }
        // Now 'wrap' is well-defined; Replace the text and update 'len'...
        len = wrap - text;
        textItem->SetText (text, len);
      }
    }

    // Link new text item...
    textItem->next = firstItem;
    firstItem = textItem;

    // Update 'text' to point to next line...
    text += len;
    if (*text == '\n' || *text == ' ') text++;

    // Update height and check if we must abbreviate after this line...
    newHeight += lineHeight;
    if (*text && fmt.maxHeight > 0 && newHeight + lineHeight > fmt.maxHeight) {
      // Yes, we must...
      abbreviated = true;
      if (len >= 3) {
        w = textItem->text + len;
        w[-1] = w[-2] = w[-3] = '.';
      }
    }
  }   // while (*test);

  // Done...
  height += newHeight;
  if (retAbbreviated) *retAbbreviated = abbreviated;
}


SDL_Surface *CTextSet::Render (SDL_Surface *dst, SDL_Rect *dstRect) {
  CTextItem *item;
  SDL_Rect placeRect, frameRect;
  int dstWidth, dstHeight, w, h;
  int yBottom, yCenter, yTop;

  // Sanity...
  if (!firstItem) return dst;

  // Render all items...
  for (item = firstItem; item; item = item->next) item->Render ();

  // Pass 1: Determine necessary dimensions and starting information for 'yCenter', 'yTop'...
  dstWidth = dstHeight = 0;
  yTop = yCenter = 0;
  for (item = firstItem; item; item = item->next) {
    w = item->surface->w + 2 * item->fmt.hSpace;
    h = item->surface->h + 2 * item->fmt.vSpace;
    if (w > dstWidth) dstWidth = w;
    dstHeight += h;
    switch (item->fmt.vAlign) {
      case -1: yTop += h;    break;
      case 0:  yCenter += h; break;
    }
  }

  // Create new dst or adjust dimensions to existing one...
  if (dstRect) {
    frameRect = *dstRect;
    if (!dst) {
      dst = CreateSurface (frameRect);
      frameRect.x = frameRect.y = 0;
    }
  }
  else {
    if (!dst) dst = CreateSurface (dstWidth, dstHeight);
    frameRect = Rect (dst);
  }
  SurfaceFillRect (dst, &frameRect, firstItem->fmt.bgColor);

  // Pass 2: Do the rendering...
  yBottom = frameRect.h;
  yCenter = (frameRect.h + yCenter) / 2;
  for (item = firstItem; item; item = item->next) {
    w = item->surface->w + 2 * item->fmt.hSpace;
    h = item->surface->h + 2 * item->fmt.vSpace;
    placeRect = Rect (item->surface);   // init width and height
    switch (item->fmt.hAlign) {
      case -1: placeRect.x = 0;                     break;
      case 0:  placeRect.x = (frameRect.w - w) / 2; break;
      case 1:  placeRect.x = frameRect.w - w;       break;
    }
    switch (item->fmt.vAlign) {
      case -1: yTop -= h;    placeRect.y = yTop;    break;
      case 0:  yCenter -= h; placeRect.y = yCenter; break;
      case 1:  yBottom -= h; placeRect.y = yBottom; break;
    }
    RectMove (&placeRect, frameRect.x + item->fmt.hSpace, frameRect.y + item->fmt.vSpace);

    SurfaceBlit (item->surface, NULL, dst, &placeRect);
  }

  // Done...
  return dst;
}


SDL_Surface *TextRender (const char *text, CTextFormat fmt, SDL_Surface *dst, SDL_Rect *dstRect, bool *retAbbreviated) {
  static const char *nothing = "\n";
  CTextSet textSet;

  if (!text) text = nothing;
  if (!text[0]) text = nothing;
  textSet.AddLines (text, fmt, retAbbreviated);
  return textSet.Render (dst, dstRect);
}





// *************************** General *****************************************


static bool uiWindowFullScreen = false, uiWindowResizable;


void UiInit (const char *windowTitle) {
  SDL_RendererInfo renInfo;
  bool accelerated;

  // Init SDL and SDL_TTF...
  SDL_SetHint (SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    // Prevent SDL2 from setting its own Ctrl-C signal handler (SIG_INT, SIG_QUIT).
    // Otherwise, the application cannot be normally killed or interrupted if it hangs
    // for some reason.
  if (SDL_Init (SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    ERROR ("'SDL_Init' failed");
  if (TTF_Init()) {
    SDL_Quit ();
    ERROR ("'TTF_Init' failed");
  }
  // Note: 'SDL_INIT_TIMER' should not be set (and the timer subsystem not used), since
  //   this has a strange effect on signal handlers, especially SIGTERM ("kill <pid>").
  //   See: http://stackoverflow.com/questions/20049025/cant-ctrl-c-my-sdl-apps-anymore
  //   Affected: SDL 2.0.3 (2016-03-12)

  // Init window...
  uiWindowResizable = envUiResizable;
  sdlWindow = SDL_CreateWindow (
    windowTitle,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    UI_RES_X, UI_RES_Y,
    SDL_WINDOW_SHOWN | (uiWindowResizable ? SDL_WINDOW_RESIZABLE : 0)
  );
  if (!sdlWindow) {
    UiDone ();
    ERRORF(("'SDL_CreateWindow' failed: %s", SDL_GetError ()));
  }

  // Init renderer...
  uiSdlRenderer = SDL_CreateRenderer (sdlWindow, -1, 0); // SDL_RENDERER_SOFTWARE);
    // replace last argument with 'SDL_RENDERER_SOFTWARE' to force software rendering
  if (!uiSdlRenderer) {
    UiDone ();
    ERRORF(("'SDL_CreateRenderer' failed: %s", SDL_GetError ()));
  }
  SDL_RenderSetLogicalSize (uiSdlRenderer, UI_RES_X, UI_RES_Y);
  SDL_GetRendererInfo (uiSdlRenderer, &renInfo);
  accelerated = (renInfo.flags & SDL_RENDERER_ACCELERATED) ? true : false;
  SDL_SetHint (SDL_HINT_RENDER_SCALE_QUALITY, accelerated ? "1" : "0");
  INFOF(("Using SDL renderer '%s' with %s", renInfo.name,
        accelerated ? "hardware acceleration" : "software rendering"));

  // Init audio...
  AudioInit ();

  // Init local subsystems...
  IconInit ();
  FontInit ();
}


void UiDone () {
  longPushTimer.Clear ();
  FontDone ();
  IconDone ();
  if (uiSdlRenderer) SDL_DestroyRenderer (uiSdlRenderer);
  uiSdlRenderer = NULL;
  if (sdlWindow) SDL_DestroyWindow (sdlWindow);
  sdlWindow = NULL;
  SDL_Quit ();
}


void UiGetWindowSize (int *w, int *h) {
  SDL_GetWindowSize (sdlWindow, w, h);
}


#if ANDROID == 0


void UiSetWindowSize (int w, int h) {
  SDL_SetWindowSize (sdlWindow, w, h);
}


bool UiGetWindowFullScreen () {
  return uiWindowFullScreen;
}


void UiSetWindowFullScreen (bool _fullScreen) {
  SDL_SetWindowFullscreen (sdlWindow, _fullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  uiWindowFullScreen = _fullScreen;
}


void UiToggleWindowFullScreen () {
  UiSetWindowFullScreen (!uiWindowFullScreen);
}


bool UiGetWindowResizable () {
  return uiWindowResizable;
}


void UiSetWindowResizable (bool _resizable) {
  SDL_SetWindowResizable (sdlWindow, _resizable ? SDL_TRUE : SDL_FALSE);
  if (!_resizable) SDL_SetWindowSize (sdlWindow, UI_RES_X, UI_RES_Y);
  uiWindowResizable = _resizable;
}


void UiToggleWindowResizable () {
  UiSetWindowResizable (!uiWindowResizable);
}


#endif // ANDROID == 0
