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
HOME2L_SRC ?= ..
HOME2L_MOD ?= tools


# Setup...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk


#~ CFLAGS += $(CFLAGS_ENV)


all: build





############################## Daemon ##########################################


DAEMON := home2l-daemon
DAEMON_BIN := $(DIR_OBJ)/$(DAEMON)
SRC_DAEMON := $(SRC) $(DAEMON).C
OBJ_DAEMON := $(SRC_DAEMON:%.C=$(DIR_OBJ)/%.o)


$(DAEMON_BIN): $(DEP_CONFIG) $(OBJ_DAEMON)
	@echo LD$(LD_SUFF) $(DAEMON)
	@$(CC) -o $@ $(OBJ_DAEMON) $(LDFLAGS)


# Automatic dependencies...
-include $(OBJ_DAEMON:%.o=%.d)
#~ -include $(patsubst %.o,%.d,$(OBJ_DAEMON))




############################## Misc. Scripts ###################################


SCRIPTS := home2l home2l-adb home2l-install home2l-rollout home2l-sudo home2l-demo home2l-usbtree
INITD := initd-home2l




############################## etc/floorplan ###################################


etc/floorplan.fpo:
	$(HOME2L_SRC)/wallclock/home2l-fpc etc/floorplan.svg





############################## Build system targets ############################


clean: clean-build
	rm -fr etc/floorplan.fpo


build-indep: etc/floorplan.fpo


build-arch: $(DAEMON_BIN)


install-indep:
	@echo "Creating "$(HOME2L_INSTALL)"/VERSION..."; echo "Home2l "$(BUILD_VERSION)" "$(BUILD_DATE) > $(HOME2L_INSTALL)/VERSION; chmod 644 $(HOME2L_INSTALL)/VERSION
	install -Dp -m 755 -t $(HOME2L_INSTALL)/bin $(SCRIPTS)
	install -Dp -m 644 -t $(HOME2L_INSTALL) env.sh README
	install -Dp -m 755 -t $(HOME2L_INSTALL)/install $(INITD)
	install -Dp -m 755 -t $(HOME2L_INSTALL)/bin h2l-mkdir.sh
	if [ ! -e $(HOME2L_INSTALL)/etc ]; then \
	  find etc/ -type f -exec install -Dpm 644 "{}" "$(HOME2L_INSTALL)/{}" \; ; \
	  chmod a+x $(HOME2L_INSTALL)/etc/rules-* ; \
	fi


install-arch:
	BIN=$(HOME2L_INSTALL)/bin/$(ARCH);   \
	install -Dp $(STRIP) -m 755 -t $$BIN $(DAEMON_BIN);   \
