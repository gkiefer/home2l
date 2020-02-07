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


#~ MYDIR := $(dir $(lastword $(MAKEFILE_LIST)))
MYDIR := $(HOME2L_SRC)/resources

CFLAGS_RC := -I$(MYDIR)
LDFLAGS_RC := -ldl

SRC_RC := $(MYDIR)/rc_core.C $(MYDIR)/rc_drivers.C $(MYDIR)/resources.C

CFLAGS += $(CFLAGS_RC)
LDFLAGS += $(LDFLAGS_RC)
SRC += $(SRC_RC)
