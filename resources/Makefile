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
HOME2L_SRC ?= ..
HOME2L_MOD ?= resources


# Setup...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include Resources.mk


# Generic C/C++ flags...
#~ SANITIZE := -fsanitize=address -fno-omit-frame-pointer  #-fsanitize=thread # 'thread' requires libtsan, which is only available for amd64 (Debian Jessie)
CFLAGS += $(SANITIZE) $(CFLAGS_ENV) $(CFLAGS_RC)
LDFLAGS += $(LDFLAGS_RC) -rdynamic $(SANITIZE)


# Common targets...
all: build





############################## Shell ###########################################


RCSHELL := home2l-shell
RCSHELL_BIN := $(DIR_OBJ)/$(RCSHELL)
RCSHELL_LDFLAGS := $(LDFLAGS)
SRC_RCSHELL := $(SRC_ENV) $(SRC_RC) $(RCSHELL).C
OBJ_RCSHELL := $(SRC_RCSHELL:%.C=$(DIR_OBJ)/%.o)

ifeq ($(WITH_READLINE),1)
RCSHELL_LDFLAGS += -lreadline
endif


$(RCSHELL_BIN): $(DEP_CONFIG) $(OBJ_RCSHELL)
	@echo LD $(RCSHELL)
	@$(CC) -o $@ $(RCSHELL_LDFLAGS) $(OBJ_RCSHELL)





############################## Server ##########################################


SERVER := home2l-server
SERVER_BIN := $(DIR_OBJ)/$(SERVER)
SRC_SERVER := $(SRC_ENV) $(SRC_RC) $(SERVER).C
OBJ_SERVER := $(SRC_SERVER:%.C=$(DIR_OBJ)/%.o)

$(SERVER_BIN): $(DEP_CONFIG) $(OBJ_SERVER)
	@echo LD $(SERVER)
	@$(CC) -o $@ $(LDFLAGS) $(OBJ_SERVER)





############################## Python library ##################################


PYLIB := home2l
PYLIB_BIN := $(DIR_OBJ)/_$(PYLIB).so
SRC_PYLIB := $(SRC_ENV) $(SRC_RC)
OBJ_PYLIB := $(SRC_PYLIB:%.C=$(DIR_OBJ)/%-pic.o) $(DIR_OBJ)/$(PYLIB)_wrap-pic.o


$(DIR_OBJ)/$(PYLIB).py: $(DEP_CONFIG) home2l-python.i resources.H ../common/env.H
	@echo SWIG $(PYLIB)
	@mkdir -p $(dir $@)
	@swig -c++ -python -DSWIG_MODULE=$(PYLIB) -o $(DIR_OBJ)/$(PYLIB)_wrap.C home2l-python.i

$(DIR_OBJ)/$(PYLIB)_wrap.C: $(DIR_OBJ)/$(PYLIB).py

$(DIR_OBJ)/$(PYLIB)_wrap-pic.o: $(DIR_OBJ)/$(PYLIB)_wrap.C
	@echo CC$(CC_SUFF) $(PYLIB)_wrap.C "(-fPIC)"
	@mkdir -p $(dir $@)
	@$(CC) -c -fPIC -I$(PYTHON_INCLUDE) $(CFLAGS) $< -o $@


$(PYLIB_BIN): $(DEP_CONFIG) $(OBJ_PYLIB) $(DIR_OBJ)/$(PYLIB).py
	@echo LD $(notdir $(PYLIB_BIN))
	@$(CC) -shared $(LDFLAGS) -o $@ $(OBJ_PYLIB)


.PHONY: pylib
pylib: $(PYLIB_BIN)





############################## Common rules & targets ##########################


# Automatic dependencies...
OBJ_ALL := $(OBJ_RCSHELL) $(OBJ_SERVER) $(OBJ_PYLIB)
-include $(OBJ_ALL:%.o=%.d)





############################## Build system targets ############################


clean: clean-build
	rm -fr __pycache__ $(PYLIB).py*


build-arch: $(RCSHELL_BIN) $(SERVER_BIN)
ifeq ($(WITH_PYTHON),1)
build-arch: $(PYLIB_BIN)
endif


install-indep:
	install -Dp -m 755 -t $(HOME2L_INSTALL)/bin home2l-showhouse

install-arch:
	BIN=$(HOME2L_INSTALL)/bin/$(ARCH);   \
	LIB=$(HOME2L_INSTALL)/lib/$(ARCH);   \
	install -Dp $(STRIP) -m 755 -t $(HOME2L_INSTALL)/bin/$(ARCH) $(RCSHELL_BIN) $(SERVER_BIN); \
	if [[ "$(WITH_PYTHON)" == "1" ]]; then \
	  install -Dp $(STRIP) -m 644 -t $(HOME2L_INSTALL)/lib/$(ARCH) $(PYLIB_BIN);  \
	  install -Dp          -m 644 -t $(HOME2L_INSTALL)/lib/$(ARCH) $(DIR_OBJ)/$(PYLIB).py;  \
	fi
