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


#include "ui_screen.H"    // SDL includes must be first
#include "env.H"
#include "system.H"
#include "apps.H"
#include "floorplan.H"
#include "alarmclock.H"

#include <resources.H>

#include <stdio.h>


ENV_PARA_BOOL ("home2l.unconfigured", envUnconfigured, false);
  /* Print information for new users
   *
   * If set, the WallClock app shows an info box on startup indicating that
   * this installation is still unconfigured and how it should be configured.
   * This option should only be set by the factory config file.
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


static inline void TestDefaultPixelTypes () {
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
  surf = IconGet ("ic-home2l-96");
  INFOF(("  IconGet ('ic-home2l'): %s", SDL_GetPixelFormatName (surf->format->format)));

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


static inline void ShowUnconfiguredInfo () {
  CString s;

  RunMessageBox (
    _("Welcome!"),
    StringF (&s,
      _("The Home2L %s is successfully installed an running,\n"
        "but still unconfigured on this device. To use all its great features,\n"
        "it should be integrated into a Home2L building installation.\n"
        "Please consult the Home2L Book available at\n"
        "\n"
        "%s\n"
        "\n"
        "for further information. To disable this message, remove\n"
        "the line 'home2l.unconfigured = 1' from 'etc/home2l.conf'.\n"
      ), WALLCLOCK_NAME, HOME2L_URL
    ),
    mbmOk,
    IconGet ("ic-home2l-96")
  );
}


int main (int argc, char **argv) {

  // Pre-Initialization ...
  //   These are initializations that must not depend on any other module,
  //   but 'Env...' depends on them.
#if ANDROID
  INFO ("Home2L (native) started");
  argv[0] = (char *) "home2l-wallclock";
  SystemPreInit ();
#endif

  // Initialization ...
  EnvInit (argc, argv);
  EnvEnablePersistence ();    // This is presently needed by: APP_MUSIC, ALARMCLOCK
  RcInit (true);
  UiInit ("Home2L - " WALLCLOCK_NAME);
  ScreenInit ();
  //~ TestDefaultPixelTypes ();
  SystemInit ();
  RcStart ();
  FloorplanInit ();
  AlarmClockInit ();
  AppsInit ();

  // Main loop ...
  AppCall (appIdHome, appOpActivate);
  if (envUnconfigured) ShowUnconfiguredInfo ();
  while (!UiIsClosed ()) UiIterate ();

  // Done ...
  AppsDone ();
  AlarmClockDone ();
  FloorplanDone ();
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
