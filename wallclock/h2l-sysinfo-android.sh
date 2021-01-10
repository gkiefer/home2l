#!/system/bin/sh

# This file is part of the Home2L project.
#
# (C) 2015-2021 Gundolf Kiefer
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


echo `uname -n`" ("`uname -m`") - "`uname -srv`
echo
uptime -s

if [ -f /sys/class/backlight/panel/brightness ]; then
  echo
  echo "Display brightness: "`cat /sys/class/backlight/panel/brightness`"/255"
fi
#if [ -f /sys/class/sensors/light_sensor/lux -a -f /sys/class/backlight/panel/brightness ]; then
#  echo "Light: "`cat /sys/class/sensors/light_sensor/lux`" Lux, Display: "`cat /sys/class/backlight/panel/brightness`"/255"
#  echo
#fi

top -m 7 -d 1 -n 1 -s cpu | tail -12
top -m 7 -d 0 -n 1 -s rss | tail -9

#top -bn 1 -o "%CPU" | head -15
#top -bn 1 -o "%MEM" | head -15 | tail -10

echo
netstat -i

# echo
#/sbin/route
