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





############################## Setup & local targets ###########################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ../..
HOME2L_MOD ?= drivers/gpio


# Setup...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include $(HOME2L_SRC)/resources/Resources.mk


# Generic C/C++ flags...
CFLAGS += $(CFLAGS_ENV) $(CFLAGS_RC)


# Common targets...
all: build





############################## GPIO ############################################


GPIO := home2l-drv-gpio
GPIO_BIN := $(DIR_OBJ)/$(GPIO).so

SRC_GPIO := $(GPIO).C
OBJ_GPIO := $(SRC_GPIO:%.C=$(DIR_OBJ)/%-pic.o)


$(GPIO_BIN): $(DEP_CONFIG) $(OBJ_GPIO)
	@echo LD $(GPIO)
	@$(CC) -shared -o $@ $(LDFLAGS) $(OBJ_GPIO)





############################## Common rules & targets ##########################


# Automatic dependencies...
-include $(OBJ_GPIO:%.o=%.d)





############################## Build system targets ############################


clean: clean-build


build-arch: $(GPIO_BIN)


install-arch:
	install -Dp $(STRIP) -m 644 -t $(HOME2L_INSTALL)/lib/$(ARCH) $(GPIO_BIN)


install-indep:
	install -Dp -m 755 -t $(HOME2L_INSTALL)/bin h2l-setup_gpio.sh
