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

import android.util.Log;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.ContentResolver;
import android.content.BroadcastReceiver;
import android.content.res.AssetManager;
import android.media.AudioManager;
import android.view.View;
import android.view.WindowManager;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.app.Activity;
import android.app.Service;
import android.provider.Settings;
import android.content.pm.ActivityInfo;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothProfile;

import android.widget.Toast;
import android.app.AlertDialog;
import android.content.DialogInterface;

import java.lang.Runtime;
import java.util.concurrent.Semaphore;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.ByteArrayOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;

import org.freedesktop.gstreamer.GStreamer;



public class Home2l {
  protected static Activity activity = null;
  protected static View immView = null;   // view to set immersive mode on (TBD: presently unused)
  //~ protected static AudioManager audioManager = null;
  protected static WakeLock wakeLock = null;
  protected static boolean keepScreenOn = true;

  protected static Home2lSync2l sync2l = null;

  protected static File home2lRootDir = null;


  // ***** Contexts *****

  static Activity TheActivity () { return activity; }
  static Service TheService () { return Home2lService.TheService (); }


  // ***** Init/Done *****

  public static void init (Activity _activity, View _immView) {
    logDebug ("### Home2l.init () called.");

    // Init global variables...
    activity = _activity;
    immView = _immView;
    showToast ("Starting WallClock ...", true);
    home2lRootDir = new File (activity.getFilesDir (), "/home2l");

    // Determine host name from property 'net.hostname', since in native code
    // 'gethostname(2)' appears to return "localhost" all the time...
    String hostName = "";
    try {
      Process proc = Runtime.getRuntime().exec("getprop net.hostname");
      hostName = new BufferedReader(new InputStreamReader (proc.getInputStream())).readLine();
      proc.destroy();
    } catch (Exception e) { e.printStackTrace (); }
    //~ Home2l.logInfo ("### net.hostname = '" + hostName + "'");

    // Call native initialization...
    initNative ();
    putEnvNative ("sys.machineName", hostName);
    putEnvNative ("sys.rootDir", home2lRootDir.getPath ());
    //~ audioManager = (AudioManager) activity.getSystemService (Context.AUDIO_SERVICE);

    // Acquire wakelock...
    PowerManager powerManager = (PowerManager) activity.getSystemService (Context.POWER_SERVICE);
    wakeLock = powerManager.newWakeLock (PowerManager.PARTIAL_WAKE_LOCK, "Home2lWakelock");
    wakeLock.acquire();

    activity.getWindow().addFlags (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON |
                                   WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

    // Start Home2l Service...
    activity.startService (new Intent(activity, Home2lService.class));

    // Register startup receiver(s)...
    //~ activity.registerReceiver (new StartupReceiver (), new IntentFilter (Intent.ACTION_SCREEN_OFF));
    //~ activity.registerReceiver (new StartupReceiver (), new IntentFilter (Intent.ACTION_BOOT_COMPLETED));  // does not work(?!)

    // Init GStreamer...
    if (Home2lConfig.withGstreamer) try {
      GStreamer.init (_activity);
        // Note [2018-02-05]:
        //   In addition to this, 'gst_init_check ()' will be called inside the Home2l native code.
        //   We assume, that both calls are useful and do not harm each other.
    } catch (Exception e) { e.printStackTrace (); }
  }

  public static native void initNative ();

  public static void done () {
    // [2017-07-30] NOTE: This method is not reached reliably, depending on how the app ends.
    logDebug ("### Home2l.done () called.");

    // Release wake lock...
    wakeLock.release ();

    // Stop sync2l...
    if (sync2l != null) sync2l.done ();
    sync2l = null;
  }

  public static void aboutToExit () {

    // Stop the service...
    //~ if (activity != null) activity.stopService (new Intent (activity, Home2lService.class));
    Service s = Home2lService.TheService ();
    if (s != null) s.stopSelf ();
  }



  // ***** Logging from Java code *****

  public static void logDebug (String msg) { Log.d ("home2l", msg); }   // TBD: filter on 'debug' flag (currently unused)
  public static void logInfo (String msg) { Log.i ("home2l", msg); }    // TBD: implement toast (currently unused)
  public static void logWarning (String msg) { Log.w ("home2l", msg); }
  public static void logError (String msg) { Log.e ("home2l", msg); }   // TBD: print message box & exit (currently unused)



  // ***** Asset access *****

  public static String assetLoadTextFile (String relPath) {
    byte[] buf = new byte[4096];
    int bytes;

    //~ logInfo ("### assetLoadTextFile ('" + relPath + "'");
    try {

      AssetManager assetManager = activity.getAssets();
      InputStream src = assetManager.open (relPath);
        // (Info) To get list of all assets [https://stackoverflow.com/questions/12387637/how-to-access-file-under-assets-folder]:
        //    String[] files = assetManager.list("");
      ByteArrayOutputStream dst = new ByteArrayOutputStream();
      while ((bytes = src.read (buf)) != -1) dst.write (buf, 0, bytes);
      return dst.toString("UTF-8");

    } catch (Exception e) {

      e.printStackTrace ();
      return null;

    }
  }

  public static boolean assetCopyFileToInternal (String relPath) {
    byte[] buf = new byte[4096];
    int bytes;

    //~ logInfo ("### assetCopyFileToInternal ('" + relPath + "'");
    try {

      // Create destination directory as necessary ...
      File dstFile = new File (home2lRootDir, relPath);
      File dstDir = dstFile.getParentFile ();
      if (dstDir != null) dstDir.mkdirs ();

      // Copy the file ...
      InputStream src = activity.getAssets().open (relPath);
      OutputStream dst = new FileOutputStream (dstFile);
      while ((bytes = src.read (buf)) != -1) dst.write (buf, 0, bytes);
      return true;

    } catch (Exception e) {

      e.printStackTrace ();
      return false;

    }
  }



  // ***** Logging additions / from C/C++ code *****

  // Logging from native code is done using the native function '__android_log_print()'.
  // The following functions are used from the native logging functions, and they
  // may be called from Java directly.

  public static void showMessage (final String title, final String msg) {

    final Semaphore dlgSemaphore = new Semaphore (0, true);

    Runnable dlgRunnable = new Runnable() {
      public void run () {
        AlertDialog.Builder dlgAlert  = new AlertDialog.Builder (TheActivity ());
        dlgAlert.setTitle("Home2L: " + title);
        dlgAlert.setMessage (msg);
        dlgAlert.setPositiveButton ("OK",
          new DialogInterface.OnClickListener () {
            public void onClick (DialogInterface dialog, int which) {
              dlgSemaphore.release();
            }
          });
        dlgAlert.setCancelable (false);
        dlgAlert.create ().show ();
        }
    };

    logInfo ("### showMessage ('" + title + "', '" + msg + "'");
    activity.runOnUiThread (dlgRunnable);
    try {
      dlgSemaphore.acquire ();
    } catch (InterruptedException e) {}
  }

  public static void showToast (final String msg, final boolean showLonger) {
    activity.runOnUiThread (new Runnable () { public void run () {
      Toast.makeText (TheActivity(), "Home2L: " + msg, showLonger ? Toast.LENGTH_LONG : Toast.LENGTH_SHORT).show();
    }});
  }



  // ***** Home2l environment *****

  public static native void putEnvNative (String key, String value);



  // ***** Executing shell commands *****

  protected static void shellCommand (String cmd, boolean wait) {
    try {
      Process p = Runtime.getRuntime ().exec (cmd);
      if (wait) p.waitFor ();
    }
    catch (Exception e) {
      logWarning ("Failed to execute shell command '" + cmd + "': " + e.toString ());
    }
  }



  // ***** System mode *****

  protected static void doSetDisplayBrightness (float brightness) {
    try {
      //~ logInfo ("setDisplayBrightness (" + brightness + ")");

      WindowManager.LayoutParams lp = activity.getWindow ().getAttributes ();
      lp.screenBrightness = brightness;
      activity.getWindow ().setAttributes (lp);

      /* Alternative solution, sets the global brightness setting and requires
       * the permission <uses-permission android:name="android.permission.WRITE_SETTINGS"/>:
       *
       * android.provider.Settings.System.putInt (activity.getContentResolver (),
       *      android.provider.Settings.System.SCREEN_BRIGHTNESS, (int) (255.0 * brightness));
       */
    }
    catch (Exception e) { e.printStackTrace (); }
  }

  public static void setDisplayBrightness (float brightness) {
    final float _brightness = brightness;
    activity.runOnUiThread (
      new Runnable () { public void run () { doSetDisplayBrightness (_brightness); } }
    );
  }

  public static void setKeepScreenOn (boolean on) {
    final boolean _on = on;
    activity.runOnUiThread (new Runnable () { public void run () {
      try {
        if (_on != keepScreenOn) {
          logInfo ("### setKeepScreenOn (" + (_on ? "true" : "false") + ")");
          if (_on) {

            // Prevent screen from being switched off...
            activity.getWindow().addFlags (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

            // Switch on display...
            PowerManager powerManager = (PowerManager) activity.getSystemService (Context.POWER_SERVICE);
            WakeLock wl = powerManager.newWakeLock (PowerManager.FULL_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP, "Home2lWakelock-full");
            wl.acquire ();
            wl.release ();
          }
          else {
            // Allow screen to be switched off and device to go to deep sleep...
            activity.getWindow().clearFlags (WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
          }
          keepScreenOn = _on;
        }
      } catch (Exception e) { e.printStackTrace (); }
    }});
  }



  // ***** Foreground/background & activity launching...

  //~ protected static boolean uiVisible = true;
    //~ // Is 'true' if the App is actually visible and not paused.
    //~ // (presently unused)

  //~ public static void setUiVisible (boolean _uiVisible) {
    //~ final boolean fUiVisible = _uiVisible;
    //~ activity.runOnUiThread (new Runnable () { public void run () {
      //~ uiVisible = fUiVisible;
    //~ }});
  //~ }

  protected static boolean inBackground = false;
    // Heuristically determines if the App is currently in the background.
    // (see comment in 'Home2lActivity.java:onResume ()' for details)

  public static void setBackground (boolean _inBackground) {
    final boolean fInBackground = _inBackground;
    activity.runOnUiThread (new Runnable () { public void run () {
      inBackground = fInBackground;
    }});
  }

  public static void goForeground () {
    activity.runOnUiThread (new Runnable () { public void run () {
      //~ logInfo ("### doGoForeBackground (fore)");
      if (inBackground) {
        //~ logInfo ("###   ... launching myself...");

        // [2016-09-19] The following code contains several variants of flags and intent creation with the
        //   goal to bring the 'Home2lActivity' to the front in any case. However, if another app
        //   has been launched before using 'launchApp', this does not work. Instead, the other app
        //   is brought to the front. Unfortunately, this "feature" is not documented anywhere.
        //   In the same situation, the command
        //
        //      > [adb shell] am start org.home2l.app/.Home2lActivity
        //
        //   works well and just as expected.
        //
        // [2018-05-19] UPDATE: The below lines do not work in the following cases:
        //    a) after another app was lanched (via 'launchApp')
        //    b) when invoked from 'StartupReceiver.run ()'

        //~ Intent i = activity.getPackageManager().getLaunchIntentForPackage ("org.home2l.app");
        //~ i.addFlags (Intent.FLAG_ACTIVITY_NEW_TASK);   // Variant B (does not work)
        //~ i.addFlags (Intent.FLAG_ACTIVITY_REORDER_TO_FRONT);   // Variant B (does not work)
        //~ i.addFlags (Intent.FLAG_ACTIVITY_CLEAR_TOP);   // Variant C (does not work)
        //~ activity.startActivity (i);            // go into foreground

        shellCommand ("su shell am start org.home2l.app/.Home2lActivity", true);
          // Workaround to make sure to bring us to front.

        inBackground = false;
      }
    }});
  }

  public static void goBackground () {
    activity.runOnUiThread (new Runnable () { public void run () {
      activity.startActivity (new Intent (Intent.ACTION_MAIN).addCategory (Intent.CATEGORY_HOME));
        // activity.moveTaskToBack (true);   // Alternativ (not working): go into background; later wakeup does not work!!
      inBackground = true;
    }});
  }

  public static void launchApp (String appStr) {
    logInfo ("Launching '" + appStr + "'");
    Intent i = activity.getPackageManager().getLaunchIntentForPackage (appStr);
    if (i != null) {
      i.addCategory (Intent.CATEGORY_LAUNCHER);
      activity.startActivity (i);
      inBackground = true;
    }
  }

  //~ protected static class StartupReceiver extends BroadcastReceiver {
    //~ @Override
    //~ public void onReceive(Context context, Intent intent) {
      //~ logInfo ("### StartupReceiver activated!");
      //~ goForeground ();
    //~ }
  //~ }



  // ***** Immersive Mode *****

  // see comment in 'Home2lActivity.java' on immersive mode: this code is presently unused (TBD)

  public static void doSetImmersiveMode () {
    // Set immersive mode to let the tool cover the complete screen and hide all Android widgets
    immView.setSystemUiVisibility (View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
  }

  public static void setImmersiveMode () {
    activity.runOnUiThread (new Runnable () { public void run () { doSetImmersiveMode (); } } );
  }



  // ***** Audio *****

  public static void setAudioNormal () { // TBD: deprecated; can be removed if all phone audio is directed via SDL
    try {
      logDebug ("'setAudioNormal' called");
      //~ audioManager.setMode (AudioManager.MODE_NORMAL);
      activity.setVolumeControlStream (AudioManager.USE_DEFAULT_STREAM_TYPE);
    } catch (Exception e) { e.printStackTrace (); }
  }

  public static void setAudioPhone () { // TBD: deprecated; can be removed if all phone audio is directed via SDL
    try {
      logDebug ("'setAudioPhone' called");
      //~ audioManager.setMode (AudioManager.MODE_IN_COMMUNICATION);
      activity.setVolumeControlStream (AudioManager.STREAM_VOICE_CALL);
    } catch (Exception e) { e.printStackTrace (); }
  }

  //~ public static void setAudioRinging () {
    //~ try {
      //~ logDebug ("'setAudioRinging' called");
      //~ audioManager.setMode (AudioManager.MODE_RINGTONE);
    //~ } catch (Exception e) { e.printStackTrace (); }
  //~ }



  // ***** Sync2l *****

  public static void enableSync2l (String pipeName) {
    logInfo ("Enabling Sync2l interface");
    sync2l = new Home2lSync2l (activity, pipeName); // "/data/home2l/var/sync2l");
  }



  // ***** Bluetooth *****

  public static void bluetoothSet (boolean on) {
    BluetoothAdapter bta = BluetoothAdapter.getDefaultAdapter ();
    if (on) bta.enable ();
    else bta.disable ();
  }

  public static int bluetoothPoll () {
    BluetoothAdapter bta = BluetoothAdapter.getDefaultAdapter ();
    int ret = 0;
    if (bta.isEnabled ()) {
      ret |= 1;
      if (bta.getProfileConnectionState (BluetoothProfile.A2DP) == BluetoothAdapter.STATE_CONNECTED) ret |= 2;
      else if (bta.getProfileConnectionState (BluetoothProfile.HEADSET) == BluetoothAdapter.STATE_CONNECTED) ret |= 2;
    }
    return ret;
  }
}
