#!/system/xbin/bash

# This file is part of the Home2L project.
#
# (C) 2015-2018 Gundolf Kiefer
#
# Home2L is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Home2L is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Home2L. If not, see <https://www.gnu.org/licenses/>.


# This script prepares a freshly booted Android devices for running the Home2l app.
# It must be executed as follows: su -c '<Pathname of this script> [<gateway IP>]'.


# Set up cable connection over adb/OpenVPN if gateway is set...
if [[ "$1" != "" ]]; then
  # Add default route for cable connection (OpenVPN tunnel)...
  route add default gw $1 dev tun0
fi

# Exit if Debian is already running...
if [ -e /debian/home ]; then exit; fi

# Start Lil'Debi (requires automatic startup to be active)...
am broadcast -a android.intent.action.MEDIA_MOUNTED -n info.guardianproject.lildebi/.MediaMountedReceiver

# Switch on & off wifi to let DNS lookups work properly over cable (only once, if Debian was not yet running)...
if [[ "$1" != "" ]]; then
  svc wifi enable
  sleep 5
  svc wifi disable
fi
