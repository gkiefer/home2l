#!/bin/bash

# This file is part of the Home2L project.
#
# (C) 2015-2020 Gundolf Kiefer
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


# This script sets up all GPIOs defined for the machine in $HOME2L_ROOT/etc/gpio.<host name>.
# The directory contains symlinks pointing at some entry like /sys/class/gpio/gpio<n>.
# The links will also be read by the GPIO resources driver, which must accept the same
# naming convention:
#
#    $HOME2L_ETC/gpio.<host name>/<port name>.<options>
#
# <options> is a sequence of characters and may include:
#
#    i - The port is an input.
#    0 - The port is an output with a default value of 0.
#    1 - The port is an output with a default value of 1.
#    n - The port is active-low (negated).
#

[[ "$HOME2L_ETC" == "" ]] && HOME2L_ETC=$HOME2L_ROOT/etc
ETC_DIR=$HOME2L_ETC/gpio.$HOSTNAME

shopt -s nullglob


# Export all requested GPIOs...
for GPIO in $ETC_DIR/?*.?*; do
  SYSDIR=`readlink $GPIO`
  SYSROOT=${SYSDIR%/*}
  ID=${SYSDIR##*/gpio}
  echo "Exporting GPIO $ID for '${GPIO##*/}'."

  # Sanity checking...
  test -w $SYSROOT/export || ( echo "Error: '$SYSROOT/export' is not writable!"; exit 3 )

  # Export pin...
  echo $ID > $SYSROOT/export 2>/dev/null    # ignore errors (pin may already be exported)
done


# Configure all GPIOs...
for GPIO in $ETC_DIR/?*.?*; do
  OPT=${GPIO##*.}
  SYSDIR=`readlink $GPIO`

  # Analyse options...
  DIRECTION="in"
  [[ "$OPT" =~ "o" ]] && DIRECTION="out"
  DEFAULT_VALUE=0
  [[ "$OPT" =~ "1" ]] && DEFAULT_VALUE=1
  ACTIVE_LOW=0
  [[ "$OPT" =~ "n" ]] && ACTIVE_LOW=1

  echo "Setting up '${GPIO##*/}' ($DIRECTION)."

  # Sanity checking...
  test -d $SYSDIR || sleep 1
  test -d $SYSDIR || ( echo "Error: '$SYSDIR' does not exist!"; exit 3 )
  for F in value active_low direction; do
    test -f $SYSDIR/$F || ( echo "Error: '$SYSDIR/$F' does not exist!"; exit 3 )
  done

  # Set options ...
  #   ... polarity ...
  echo $ACTIVE_LOW > $SYSDIR/active_low
  #   ... direction (in/out) ...
  echo $DIRECTION > $SYSDIR/direction
  #   ... default value ...
  [[ "$DIRECTION" == "out" ]] && echo $DEFAULT_VALUE > $SYSDIR/value
  #   ... enable interrupts on inputs ...
  #     Note: The GPIO driver presently does not support interrupts, since they
  #           did not work properly on the author's hardware. Once they are
  #           supported by the driver, the following code should be commented in
  #           to enable them.
  #           [2016-10-31: PI15/EINT27 refused to work completely after being set to 'edge'].
  #~ if [[ "$DIRECTION" == "in" ]]; then
  #~   test -f $SYSDIR/edge && echo "both" > $SYSDIR/edge
  #~ fi

  # Set permissions...
  chown root.home2l $SYSDIR/value
  if [[ "$DIRECTION" == "in" ]]; then
    chmod 440  $SYSDIR/value
  else
    chmod 660  $SYSDIR/value
  fi

done
