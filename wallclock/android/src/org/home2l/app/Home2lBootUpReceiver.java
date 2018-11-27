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


package org.home2l.app;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;


public class Home2lBootUpReceiver extends BroadcastReceiver {
  @Override
  public void onReceive (Context context, Intent intent) {
    if (intent.getAction().equals (Intent.ACTION_BOOT_COMPLETED)) {
      Intent i = new Intent (context, Home2lActivity.class);
      i.addFlags (Intent.FLAG_ACTIVITY_NEW_TASK);
      context.startActivity (i);
    }
  }
}


