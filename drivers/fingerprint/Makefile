# This file is part of the Home2L project.
#
# (C) 2024 Gundolf Kiefer
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





######################### Setup and local targets ##############################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ../..
HOME2L_MOD ?= drivers/fingerprint


# Setup...
include $(HOME2L_SRC)/Setup.mk


SCRIPTS := home2l-drv-fingerprint r503.py





############################## Build system targets ############################


clean:
	rm -fr __pycache__

install-indep:
	install -Dp -m 755 -t $(HOME2L_INSTALL)/lib $(SCRIPTS)
	ln -sf ../lib/home2l-drv-fingerprint $(HOME2L_INSTALL)/bin/home2l-fingerprint2l
