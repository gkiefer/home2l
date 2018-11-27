#!/bin/bash

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


#~ if [[ "$1" == "" ]]; then
  #~ echo "Usage: build-icons.sh <install dir>"
  #~ exit
#~ fi


# Setup ...
cd icons
INSTALL_DIR=../icons.build
rm -fr $INSTALL_DIR
mkdir -p $INSTALL_DIR


# SVG icons...
for f in ic-*.svg; do
  echo -n "ICON $f:"
  n=${f%%.svg}
  inkscape $n.svg -w 96 -h 96 -e $n.png > /dev/null
  for s in 24 48 96; do
    #convert $n.png -geometry $sx$s $n-$s.bmp # -negate
    convert $n.png -geometry $sx$s -channel A -separate -colors 256 -compress None BMP3:$n-$s.bmp
    mv $n-$s.bmp $INSTALL_DIR
    echo -n " $s"
  done
  rm $n.png
  echo
done


# Droid digits...
for f in droids-empty droids-digits; do
  echo "IMAGE $f"
  convert freedroid/$f.png -channel A -separate -colors 256 -compress None BMP3:$INSTALL_DIR/$f.bmp
done


# Others...
for n in phone-incall phone-ringing phone-ringing-door phone-ringing-gate pic-wakeup; do
  if [ -e $n.svg ]; then
    echo "IMAGE $n"
    inkscape $n.svg -w 480 -h 480 -e $n.png > /dev/null
    convert $n.png -geometry 480x480 -channel A -separate -colors 256 -compress None BMP3:$n.bmp
    mv $n.bmp $INSTALL_DIR
    rm $n.png
  fi
  if [ -e $n.png ]; then
    echo "IMAGE $n"
    convert $n.png tmp.bmp
    convert tmp.bmp -channel R -separate -colors 256 -compress None BMP3:$INSTALL_DIR/$n.bmp
    rm tmp.bmp
  fi
done


# Fix permissions...
chmod 755 $INSTALL_DIR
chmod 644 $INSTALL_DIR/*.bmp
