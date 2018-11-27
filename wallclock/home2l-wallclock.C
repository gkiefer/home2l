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


#include "ui_screen.H"    // SDL includes must be first
#include "env.H"
#include "system.H"
#include "apps.H"
#include "alarmclock.H"

#include <resources.H>

#include <stdio.h>


ENV_PARA_STRING ("sys.androidGateway", envAndroidGateway, NULL);
  /* Gateway IP adress for Android app
   *
   * If set, a default route is set with the given IP adress as a gateway
   * by the Android app. This is useful if the device is conntected to the
   * LAN in a way not directly supported by the Android UI, such as an OpenVPN
   * tunnel over the USB cable.
   */





// ************ Helpers and Information displays ***********


void PrintRendererInfo () {
  SDL_RendererInfo renInfo;
  int drivers, n;

  drivers = SDL_GetNumRenderDrivers ();
  if (!drivers)
    printf ("W: No SDL render drivers available!\n");
  else {
    for (n = 0; n < drivers; n++) {
      if (SDL_GetRenderDriverInfo (n, &renInfo) == 0) {
        printf ("I: Available SDL render driver #%i: '%s', max. texture: %ix%i, flags:%s%s%s%s.\n",
                n, renInfo.name, renInfo.max_texture_width, renInfo.max_texture_height,
                renInfo.flags & SDL_RENDERER_SOFTWARE ? " SOFTWARE" : "",
                renInfo.flags & SDL_RENDERER_ACCELERATED ? " ACCELERATED" : "",
                renInfo.flags & SDL_RENDERER_PRESENTVSYNC ? " PRESENTVSYNC" : "",
                renInfo.flags & SDL_RENDERER_TARGETTEXTURE ? " TARGETTEXTURE" : "");
      }
      else
        printf ("W: Unable to get info on render driver #%i.\n", n);
    }
  }
}


void TestDefaultPixelTypes () {
  SDL_Renderer *ren;
  SDL_RendererInfo renInfo;
  //SDL_Texture *tex;
  SDL_Surface *surf;
  TTF_Font *font;
  int n;

  INFO ("Checking for default pixel types...");

  // Renderer...
  ren = UiGetSdlRenderer ();
  SDL_GetRendererInfo (ren, &renInfo);
  for (n = 0; n < (int) renInfo.num_texture_formats; n++)
    INFOF(("  SDL_Renderer [%i]:         %s", n, SDL_GetPixelFormatName (renInfo.texture_formats[n])));

  // Image...
  surf = IconGet ("ic_volume_up-96");
  INFOF(("  IconGet ('ic_volume_up'): %s", SDL_GetPixelFormatName (surf->format->format)));

  // Font...
  font = FontGet (fntNormal, 24);
  surf = TTF_RenderUTF8_Shaded (font, "Hello World!", ToSDL_Color (YELLOW), ToSDL_Color (BLACK));
  INFOF(("  TTF_RenderUTF8_Shaded:    %s", SDL_GetPixelFormatName (surf->format->format)));
  SDL_FreeSurface (surf);
  surf = TTF_RenderUTF8_Blended (font, "Hello World!", ToSDL_Color (YELLOW));
  INFOF(("  TTF_RenderUTF8_Blended:   %s", SDL_GetPixelFormatName (surf->format->format)));
  SDL_FreeSurface (surf);
}


/*
  Debian x86, 2014-12-07:
    SDL_Renderer [0]:         SDL_PIXELFORMAT_ARGB8888
    SDL_Renderer [1]:         SDL_PIXELFORMAT_YV12
    SDL_Renderer [2]:         SDL_PIXELFORMAT_IYUV
    IconGet ('ic_volume_up'): SDL_PIXELFORMAT_ARGB8888
    TTF_RenderUTF8_Shaded:    SDL_PIXELFORMAT_INDEX8
    TTF_RenderUTF8_Blended:   SDL_PIXELFORMAT_ARGB8888
*/


int main (int argc, char **argv) {

#if ANDROID
  INFO ("Home2L started");
  argv[0] = (char *) "home2l-wallclock";
#endif

  EnvInit (argc, argv);
  EnvInitPersistence ();    // This is presently needed by: APP_MUSIC
#if ANDROID
  CString s;
  const char *gw = EnvGet (envAndroidGatewayKey);
  //~ INFOF (("### envAndroidGateway = '%s'", gw));
  s.SetF ("test -e /debian/home || su -c '%s/bin/android-init.sh %s' &", EnvHome2lRoot (), gw ? gw : "");
  system (s.Get ());
#endif
  RcInit (true);
  UiInit ("Home2L - " WALLCLOCK_NAME);
  ScreenInit ();
  //~ TestDefaultPixelTypes ();
  SystemInit ();
  AppsInit ();
  RcStart ();
  AlarmClockInit ();

  AppCall (appIdHome, appOpActivate);
  while (!UiIsClosed ()) UiIterate ();

  AlarmClockDone ();
  AppsDone ();
  SystemDone ();
  ScreenDone ();
  UiDone ();
  RcDone ();
  EnvDone ();
#if ANDROID
  _exit (0);    // Force exit. Otherwise, the process may not be terminated properly.
#else
  return 0;
#endif
}
