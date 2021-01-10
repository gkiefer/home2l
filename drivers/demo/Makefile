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





############################## Setup & local targets ###########################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ../..
HOME2L_MOD ?= drivers/demo


# Setup...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include $(HOME2L_SRC)/resources/Resources.mk


# Common targets...
all: build





############################## Driver ##########################################


DRIVER := home2l-drv-demo
DRIVER_BIN := $(DIR_OBJ)/$(DRIVER).so

SRC_DRIVER := $(DRIVER).C
OBJ_DRIVER := $(SRC_DRIVER:%.C=$(DIR_OBJ)/%-pic.o)


$(DRIVER_BIN): $(DEP_CONFIG) $(OBJ_DRIVER)
	@echo LD $(DRIVER)
	@$(CC) -shared -o $@ $(LDFLAGS) $(OBJ_DRIVER)





############################## Common rules & targets ##########################


# Automatic dependencies...
-include $(OBJ_DRIVER:%.o=%.d)





############################## Build system targets ############################


clean: clean-build


build-arch: $(DRIVER_BIN)


install-arch:
	install -Dp $(STRIP) -m 644 -t $(HOME2L_INSTALL)/lib/$(ARCH) $(DRIVER_BIN)
