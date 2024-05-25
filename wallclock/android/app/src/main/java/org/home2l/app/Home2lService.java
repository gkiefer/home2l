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


package org.home2l.app;

import android.app.Service;
import android.app.Notification;
import android.app.Notification.Builder;
import android.app.PendingIntent;
import android.content.Intent;
import android.os.IBinder;
import android.os.Build;
//~ import android.graphics.drawable.Icon;

import org.home2l.app.Home2l;


public class Home2lService extends Service {
  protected static Home2lService theService = null;      // is set in 'onStartCommand' to the actual service object

  private static final int NOTIFICATION_ID = 1;


  public static Home2lService TheService () { return theService; }


  @Override
  public int onStartCommand (Intent intent, int flags, int startId) {
    Home2l.logDebug ("### Home2lService.onStartCommand () ...");

    // Store the service reference...
    theService = this;

    if (Home2l.TheActivity () == null) {
      Home2l.logDebug ("###   ... activity not running: launching activity ...");

      // There is no activity running, but the service has been launched.
      //   This should only happen if the app has crashed and the service been relaunched by
      //   the system due to its 'START_STICKY' mode.
      // Launch the activity and stop the service for now (will be restarted by the activity).
      stopSelf ();
      startActivity (new Intent (this, Home2lActivity.class).addFlags (Intent.FLAG_ACTIVITY_NEW_TASK));
        // If 'FLAG_ACTIVITY_NEW_TASK' is not set, Android refuses to start the activity from outside of an Activity context.
    }
    else {
      Home2l.logDebug ("###   ... putting service into foreground ...");

      // Put service into the foreground...
      //~ if (Build.VERSION.SDK_INT <= 30) {
        // This is only allowed until API level 30 (see https://developer.android.com/reference/android/app/Service#startForeground(int,%20android.app.Notification)).
        // TBD: Find a solution for new API levels.
        startForeground (NOTIFICATION_ID, new Notification.Builder (Home2l.TheActivity ())
            .setContentTitle("Home2l")
            .setContentText("Push to activate the Home2L screen.")
            .setSmallIcon (R.mipmap.home2l_icon)
            .setContentIntent (
                PendingIntent.getActivity (
                  this, 0,
                  new Intent (this, Home2lActivity.class).addFlags (Intent.FLAG_ACTIVITY_NEW_TASK),
                  PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
                )
              )
            .build()
          );
      //~ }
    }

    // We want this service to continue running until it is explicitly stopped, so return sticky.
    return START_STICKY;
  }


  @Override
  public IBinder onBind (Intent intent) { return null; }


  @Override
  public void onDestroy () {
    if (theService == this) theService = null;
  }
}
