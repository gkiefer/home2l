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


MYDIR := $(dir $(lastword $(MAKEFILE_LIST)))

ifneq (,$(wildcard $(MYDIR)usr/$(ARCH)/include))

  # Use these sources...
	CFLAGS += -I$(MYDIR)usr/$(ARCH)/include
	LDFLAGS += -L$(MYDIR)usr/$(ARCH)/lib  -llinphone -lbellesip -lbctoolbox \
						-lmediastreamer_voip -lmediastreamer_base -lortp -lsqlite3 -lz -llzma \
						-lxml2 -ldl -lm -lsoup-2.4 -lgobject-2.0 -lglib-2.0 -lantlr3c -lpolarssl \
						-lspeex -lspeexdsp -lasound -lv4l2 -lavcodec -lavutil -lswscale -lvpx \
						-lX11 -lXext # -lXv
		# Line 1: directly required by this tool
		# Line 2: required by liblinphone
		# Line 3: required by bellesip
		# Line 4: required by mediastreamer
		# Line 5: required by libSDL2

else

  # Use standard Debian packages, not these sources...
	ifeq ($(ARCH),amd64)
		PKG_ENV := PKG_CONFIG_LIBDIR=/usr/lib/x86_64-linux-gnu/pkgconfig
	else ifeq ($(ARCH),i386)
		PKG_ENV := PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig
	else ifeq ($(ARCH),armhf)
		PKG_ENV := PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig
	else
		PKG_ENV :=
	endif

	CFLAGS += $(shell $(PKG_ENV) pkg-config --cflags linphone) -Wno-deprecated-declarations
	LDFLAGS += $(shell $(PKG_ENV) pkg-config --libs linphone) -llinphone

endif
