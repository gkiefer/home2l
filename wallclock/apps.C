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


#include "apps.H"





// ***************** App database **************************


struct TAppDesc {
  const FAppFunc *func;
  const char *enableKey;
  bool enabled;
};


// Declare prototypes for all app functions...
#define APP(NAME, ENV_PREFIX) FAppFunc AppFunc##NAME;
#include "apps_config.H"
#undef APP


// Applet description table...
static TAppDesc appTable [appIdEND] = {
#define APP(NAME, ENV_PREFIX) { AppFunc##NAME, ENV_PREFIX ".enable", false },
#include "apps_config.H"
#undef APP
};





// ***************** Helpers for launch buttons *******************


TTF_Font *fntAppLabel = NULL;





// ***************** Interface functions *******************


void AppsInit () {
  int n;

  fntAppLabel = FontGet (fntNormal, 24);

  for (n = 0; n < (int) appIdEND; n++) if (n != appIdHome) {
    appTable[n].enabled = EnvGetBool (appTable[n].enableKey, false);
    if (appTable[n].enabled) if (!AppCall ((EAppId) n, appOpInit)) appTable[n].enabled = false;
  }

  appTable[appIdHome].enabled = true;
  AppCall (appIdHome, appOpInit);   // this must be last because of the main menu
}


void AppsDone () {
  int n;

  for (n = 0; n < appIdEND; n++) if (appTable[n].enabled) AppCall ((EAppId) n, appOpDone);
}


void *AppCall (EAppId appId, int appOp, void *data) {
  ASSERT (appId >= 0 && appId < appIdEND);
  if (!appTable[appId].enabled) return NULL;
  return appTable[appId].func (appOp, data);
}


bool AppEnabled (EAppId appId) {
  return appTable[appId].enabled;
}


void CbAppEscape (class CButton *, bool, void *) {
  AppActivate (appIdHome);
}


void CbAppActivate (class CButton *, bool longPush, void *appId) {
  AppCall ((EAppId) (intptr_t) appId, longPush ? appOpLongPush : appOpActivate);
}
