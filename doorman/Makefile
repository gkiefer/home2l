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


# IP phone backend ...
PHONE_LIB ?= pjsip
#~ PHONE_LIB ?= linphone




######################### Setup and local targets ##############################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ..
HOME2L_MOD ?= doorman


# Setup ...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include $(HOME2L_SRC)/resources/Resources.mk


# Generic C/C++ flags ...
#~ SANITIZE := -fsanitize=address   #-fsanitize=thread # 'thread' requires libtsan, which is only available for amd64 (Debian Jessie)
CFLAGS += $(SANITIZE)
LDFLAGS += -rdynamic $(SANITIZE)


# Common targets...
all: build





############################## DoorMan #########################################


include ../external/$(PHONE_LIB)/Debian.mk


DOORMAN := home2l-doorman
DOORMAN_BIN := $(DIR_OBJ)/$(DOORMAN)
SRC_DOORMAN := $(SRC) ../common/phone.C ../common/phone-$(PHONE_LIB).C $(DOORMAN).C
OBJ_DOORMAN := $(SRC_DOORMAN:%.C=$(DIR_OBJ)/%.o)


$(DOORMAN_BIN): $(DEP_CONFIG) $(OBJ_DOORMAN)
	@echo LD$(LD_SUFF) $(DOORMAN)
	@$(CC) -o $@ $(OBJ_DOORMAN) $(LDFLAGS)
#~ 	cp $(DOORMAN_BIN) ./$(DOORMAN).$(ARCH)





############################## Common rules & targets ##########################


# Automatic dependencies...
-include $(OBJ_DOORMAN:%.o=%.d)


#~ print-test:
#~ 	echo $(SRC_ALL)
#~ 	echo $(patsubst %.C,%-$(ARCH).d,$(SRC_ALL))

#~ print-sources:
#~ 	@echo $(SRC)





############################## Helpers #########################################


# Install ...
install:
	make PARTS=doorman -C .. install





############################## Build system targets ############################


clean: clean-build


build-arch: $(DOORMAN_BIN)


install-arch:
	BIN=$(HOME2L_INSTALL)/bin/$(ARCH);   \
	LIB=$(HOME2L_INSTALL)/lib/$(ARCH);   \
	install -Dp $(STRIP) -m 755 -t $$BIN $(DOORMAN_BIN);   \
