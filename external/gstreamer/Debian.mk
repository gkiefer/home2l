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


MYDIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Under Debian, we use the distribution gstreamer & glib packages:
#   > aptitude install libgstreamer1.0-dev libglib2.0-dev
#
# Version under Debian Stretch (2018-01-13):
#   libgstreamer1.0-dev         1.10.4-1
#   libglib2.0-dev              2.50.3-2
#

ifeq ($(ARCH),amd64)
	PKG_ENV := PKG_CONFIG_LIBDIR=/usr/lib/x86_64-linux-gnu/pkgconfig
else ifeq ($(ARCH),armhf)
	PKG_ENV := PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig
else
	PKG_ENV :=
endif

CFLAGS += $(shell $(PKG_ENV) pkg-config --cflags gstreamer-1.0)
LDFLAGS += $(shell $(PKG_ENV) pkg-config --libs gstreamer-1.0)



# Unsorted notes on compiling from source
# =======================================
#
#
# glib
# ----
# ...> wget https://download.gnome.org/sources/glib/2.55/glib-2.55.1.tar.xz
# ...> tar Jxf glib-2.55.1.tar.xz
# .../glib-2.55.1> ./configure --prefix=$PWD/../../usr.new/i386 --disable-shared --enable-static --disable-libmount --disable-fam
# .../glib-2.55.1>  make -j 8 install
#
#
# gstreamer
# ---------
#
# ...> wget https://gstreamer.freedesktop.org/src/gstreamer/gstreamer-1.12.4.tar.xz
# ...> tar Jxf gstreamer-1.12.4.tar.xz
# ...> ln -s gstreamer-1.12.4 gstreamer; cd gstreamer
# .../gstreamer> ./configure --prefix=$PWD/../../usr.new/i386 --disable-shared --enable-static --enable-static-plugins --disable-gst-debug
# .../gstreamer> make -j 8 install
