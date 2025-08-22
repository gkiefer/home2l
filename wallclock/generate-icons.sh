#!/bin/bash

# This file is part of the Home2L project.
#
# (C) 2015-2024 Gundolf Kiefer
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



# Setup ...
cd icons
DST_DIR=../icons.build
FLOORPLAN_SVG=../../showcase/etc/floorplan.svg    # floorplan template
rm -fr $DST_DIR
mkdir -p $DST_DIR



# SVG icons...
../home2l-icon -d $DST_DIR ic-*.svg



# Floorplan template icons...

#   Get available templates...
TPLS_RAW=`inkscape $FLOORPLAN_SVG -S 2>/dev/null | grep ^tpl\.`
TPLS=""
for T in $TPLS_RAW; do
  OBJ_NAME=${T%%,*}
  BASE=${OBJ_NAME#tpl.}
  TPLS="$TPLS $BASE"
done
#~ echo "### Templates: $TPLS"

#   Convert them...
for BASE in $TPLS; do
  PNG=$DST_DIR/$BASE.png    # base .png from inkscape
  BMP=$DST_DIR/fp-$BASE.bmp # destination file name
  echo "FLOORPLAN $BASE"
  inkscape $FLOORPLAN_SVG -i tpl.$BASE -d 768 -o $PNG >/dev/null 2>&1     # '-d 768' refers to a scale factor of 8 (id 3) (96 * 8 dpi)
  convert $PNG -channel R -separate -colors 256 -compress None BMP3:$BMP
  rm $PNG
done



# Droid digits...
for f in droids-empty droids-digits; do
  echo "IMAGE $f"
  convert freedroid/$f.png -channel A -separate -colors 256 -compress None BMP3:$DST_DIR/$f.bmp
done



# Others...
for n in phone-incall phone-ringing phone-ringing-door phone-ringing-gate pic-wakeup; do
  if [ -e $n.svg ]; then
    echo "IMAGE $n"
    inkscape $n.svg -w 480 -h 480 -o $n.png > /dev/null 2>&1
    convert $n.png -geometry 480x480 -channel A -separate -colors 256 -compress None BMP3:$n.bmp
    mv $n.bmp $DST_DIR
    rm $n.png
  fi
  if [ -e $n.png ]; then
    echo "IMAGE $n"
    convert $n.png tmp.bmp
    convert tmp.bmp -channel R -separate -colors 256 -compress None BMP3:$DST_DIR/$n.bmp
    rm tmp.bmp
  fi
done



# Fix permissions...
chmod 755 $DST_DIR
chmod 644 $DST_DIR/*.bmp
