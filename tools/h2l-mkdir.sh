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


# Setup a given directory in the 'var' or 'tmp' domain. Purpose is the
# automatic generation of temporary storage locations even if
# '$HOME2L_ROOT/tmp' or '$HOME2L_ROOT/var' are symlinks pointing to
# an unitialized ram disk.


if [[ "$1" == "" ]]; then
  echo "Usage: ${0##*/}  <directory relative to HOME2L_ROOT>"
  echo "       ${0##*/}  -a <absolute path to directory>"
  exit 3
fi

if [[ "$1" == "-a" ]]; then
  DIR="$2"
  if [[ "$2" == "" ]]; then
    exit 3;
  fi
else
  DIR=$HOME2L_ROOT/$1
fi

# Return successfully if directory already exists and the caller has all necessary permissions...
test -d $DIR -a -r $DIR  -a -w $DIR -a -x $DIR && exit 0

# Try to create it with the 'home2l' group (don't worry on error)...
DEST=`readlink -m $DIR` || exit 1
sg home2l "mkdir -m 2775 -p $DEST" && exit 0

# Try to create it without setting to the 'home2l' group (must be succcessful now)...
mkdir -m 775 -p $DEST || exit 1
