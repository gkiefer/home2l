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


ifneq (,$(wildcard $(MYDIR)usr/$(ARCH)/include))
  # Use these sources...
  CFLAGS += -I$(MYDIR)usr/$(ARCH)/include
  LDFLAGS += -L$(MYDIR)usr/$(ARCH)/lib -lSDL2 -lSDL2_ttf -ldl -lX11 -lfreetype -lpng -lz -lsndio
else
  # Use the Debian packages, not these sources...
  CFLAGS += -I/usr/include/SDL2
  LDFLAGS += -lSDL2 -lSDL2_ttf
endif
