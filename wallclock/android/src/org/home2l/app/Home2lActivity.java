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


package org.home2l.app;

import java.util.Set;

import android.os.Bundle;
import android.view.*;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;

import org.libsdl.app.*;



public class Home2lActivity extends SDLActivity {

  @Override
  protected String[] getLibraries() {
    //~ Home2l.logInfo ("### Home2lConfig.nativeLibraries: ");
    return Home2lConfig.nativeLibraries;
  }

  @Override
  public void onCreate (Bundle savedInstanceState) {
    super.onCreate (savedInstanceState);

    Home2l.init (this, (View) mSurface);
    //~ Home2l.logInfo ("### Test info");

    //~ try {
      //~ ActivityInfo ai = getPackageManager().getActivityInfo(this.getComponentName(), PackageManager.GET_META_DATA);
      //~ Bundle metaData = ai.metaData;
      //~ Set<String> keys = metaData.keySet();
      //~ for (String key : keys) {
        //~ String value = metaData.getString (key);
        //~ Home2l.logInfo ("### MetaData: '" + key + "' = '" + value + "'");
        //~ // TBD: Pass the key/value pair to home2l/env
      //~ }
    //~ } catch (Exception e) { e.printStackTrace (); }
  }

  @Override
  protected void onDestroy () {
    Home2l.done ();
    super.onDestroy ();
  }

  // Events
  @Override
  protected void onPause() {
    super.onPause();
    //~ Home2l.setUiVisible (false);
  }

  @Override
  protected void onResume() {
    // [2016-09-22] The wakeup mechanism implemented in 'Home2l.ToForeground'
    //   causes irregular crashes if executed close in time with pause/resume events.
    //   To avoid this, we try to keep track of whether our App is the topmost activity.
    //   If so, 'Home2l.ToForeground' does not do anything.
    //   As of today, Android does not provide any API mechanism to query this. So we
    //   implement the following heuristic using the variable 'Home2l.toBackground':
    //   - Keep the variable up-to-date in 'Home2l.setBackground ()'.
    //   - Set to 'true' if we launch another app or explicitly go to background.
    //   - Set to 'false' in a "resume" event.
    //
    //   This heuristic has the limitation that, for example, the app is not
    //   brought to front by an incoming phone call if it was sent to back by some other
    //   method as described above.
    //
    super.onResume();
    //~ Home2l.setUiVisible (true);
    Home2l.setBackground (false);
  }

  /*
  // The following switches to immersive mode. Due to a bug in Android 4.4, immersive mode
  // is switched off after the on-screen keyboard was shown, and a workaround (switch
  // imm. mode on again after hiding the keyboard) still leads to a messy layout as of SDL 2.0.3.
  // For this reason, immersive mode support is commented out, but left here for potential
  // future use.
  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    if (hasFocus) Home2l.doSetImmersiveMode ();
    super.onWindowFocusChanged(hasFocus);
  }
  */
}
