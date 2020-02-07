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


# This Makefile fragment should be included by any C/C++ application using Home2L.


#~ MYDIR := $(dir $(lastword $(MAKEFILE_LIST)))
MYDIR := $(HOME2L_SRC)/common

CFLAGS_ENV := -I$(MYDIR)
SRC_ENV := $(MYDIR)/base.C $(MYDIR)/env.C

CFLAGS += $(CFLAGS_ENV)
SRC += $(SRC_ENV)
