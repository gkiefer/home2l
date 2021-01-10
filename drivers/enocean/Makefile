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
HOME2L_MOD ?= drivers/enocean


# Setup...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include $(HOME2L_SRC)/resources/Resources.mk


# Common targets...
all: build





############################## Driver ##########################################


DRIVER := home2l-drv-enocean
DRIVER_BIN := $(DIR_OBJ)/$(DRIVER).so

SRC_DRIVER := enocean.C $(DRIVER).C
OBJ_DRIVER := $(SRC_DRIVER:%.C=$(DIR_OBJ)/%-pic.o)


$(DRIVER_BIN): $(DEP_CONFIG) $(OBJ_DRIVER)
	@echo LD $(DRIVER)
	@$(CC) -shared -o $@ $(LDFLAGS) $(OBJ_DRIVER)





############################## Tool ############################################


TOOL := home2l-enocean2l
TOOL_BIN := $(DIR_OBJ)/$(TOOL)

SRC_TOOL := $(SRC) enocean.C $(TOOL).C
OBJ_TOOL := $(SRC_TOOL:%.C=$(DIR_OBJ)/%.o)


$(TOOL_BIN): $(DEP_CONFIG) $(OBJ_TOOL)
	@echo LD $(TOOL)
	@$(CC) -o $@ $(OBJ_TOOL) $(LDFLAGS)





############################## Common rules & targets ##########################


# Automatic dependencies...
-include $(OBJ_DRIVER:%.o=%.d)
-include $(OBJ_TOOL:%.o=%.d)





############################## Build system targets ############################


clean: clean-build


build-arch: $(DRIVER_BIN) $(TOOL_BIN)


install-arch:
	BIN=$(HOME2L_INSTALL)/bin/$(ARCH); \
	LIB=$(HOME2L_INSTALL)/lib/$(ARCH); \
	install -Dp $(STRIP) -m 644 -t $$LIB $(DRIVER_BIN); \
	install -Dp $(STRIP) -m 755 -t $$BIN $(TOOL_BIN)
