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





############################## Configuration ###################################


# For testing ...

#~ ARCH=armhf

#~ all:
#~ 	@cp -va $(DIR_OBJ)/home2l-brownie2l ./home2l-brownie2l.$(ARCH)





######################### Setup and local targets ##############################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ..
HOME2L_MOD ?= brownies


# Setup ...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include $(HOME2L_SRC)/resources/Resources.mk
include $(HOME2L_SRC)/brownies/Brownies.mk


# Generic C/C++ flags ...
#~ SANITIZE := -fsanitize=address   #-fsanitize=thread # 'thread' requires libtsan, which is only available for amd64 (Debian Jessie)
CFLAGS += $(SANITIZE)
LDFLAGS += $(SANITIZE)


# Common targets...
all: build




############################## Brownie2L #######################################


BROWNIE2L := home2l-brownie2l
BROWNIE2L_BIN := $(DIR_OBJ)/$(BROWNIE2L)
BROWNIE2L_SRC := $(SRC) $(BROWNIE2L).C
BROWNIE2L_OBJ := $(BROWNIE2L_SRC:%.C=$(DIR_OBJ)/%.o)

ifeq ($(WITH_READLINE),1)
LDFLAGS += -lreadline
endif


$(BROWNIE2L_BIN): $(DEP_CONFIG) $(BROWNIE2L_OBJ)
	@echo LD$(LD_SUFF) $(BROWNIE2L)
	@$(CC) -o $@ $(BROWNIE2L_OBJ) $(LDFLAGS)





############################## Common rules & targets ##########################


# Automatic dependencies...
-include $(BROWNIE2L_OBJ:%.o=%.d)





############################## AVR Firmwares ###################################


BUILD_DIR_AVR=$(HOME2L_BUILD)/avr


.PHONY: avr
avr:
	@mkdir -p $(BUILD_DIR_AVR)
	$(MAKE) -C avr BUILD_VERSION=$(BUILD_VERSION) BUILD_DIR=$(BUILD_DIR_AVR) all





############################## Build system targets ############################


clean: clean-build
	$(MAKE) -C avr clean


build-indep: avr

build-arch: $(BROWNIE2L_BIN)


install-indep:
	install -Dp -m 644 -t $(HOME2L_INSTALL)/share/brownies $(BUILD_DIR_AVR)/*.elf

install-arch:
	BIN=$(HOME2L_INSTALL)/bin/$(ARCH);   \
	LIB=$(HOME2L_INSTALL)/lib/$(ARCH);   \
	install -Dp $(STRIP) -m 755 -t $$BIN $(BROWNIE2L_BIN)
