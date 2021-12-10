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
HOME2L_MOD ?= resources


# Setup...
include $(HOME2L_SRC)/Setup.mk
include $(HOME2L_SRC)/common/Env.mk
include $(HOME2L_SRC)/resources/Resources.mk


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
SRC_RCSHELL := $(SRC) $(RCSHELL).C
OBJ_RCSHELL := $(SRC_RCSHELL:%.C=$(DIR_OBJ)/%.o)

ifeq ($(WITH_READLINE),1)
RCSHELL_LDFLAGS += -lreadline
endif


$(RCSHELL_BIN): $(DEP_CONFIG) $(OBJ_RCSHELL)
	@echo LD$(LD_SUFF) $(RCSHELL)
	@$(CC) -o $@ $(OBJ_RCSHELL) $(RCSHELL_LDFLAGS)





############################## Server ##########################################


SERVER := home2l-server
SERVER_BIN := $(DIR_OBJ)/$(SERVER)
SRC_SERVER := $(SRC) $(SERVER).C
OBJ_SERVER := $(SRC_SERVER:%.C=$(DIR_OBJ)/%.o)

$(SERVER_BIN): $(DEP_CONFIG) $(OBJ_SERVER)
	@echo LD$(LD_SUFF) $(SERVER)
	@$(CC) -o $@ $(OBJ_SERVER) $(LDFLAGS)





############################## Python library ##################################


PYLIB := home2l
PYLIB_BIN := $(DIR_OBJ)/_$(PYLIB).so
SRC_PYLIB := $(SRC)
OBJ_PYLIB := $(SRC_PYLIB:%.C=$(DIR_OBJ)/%-pic.o) $(DIR_OBJ)/$(PYLIB)_wrap-pic.o


$(DIR_OBJ)/$(PYLIB).py: $(DEP_CONFIG) home2l-python.i resources.H ../common/env.H
	@echo SWIG $(PYLIB)
	@mkdir -p $(dir $@)
	@swig -c++ -python -DSWIG_MODULE=$(PYLIB) -o $(DIR_OBJ)/$(PYLIB)_wrap.C home2l-python.i

$(DIR_OBJ)/$(PYLIB)_wrap.C: $(DIR_OBJ)/$(PYLIB).py

$(DIR_OBJ)/$(PYLIB)_wrap-pic.o: $(DIR_OBJ)/$(PYLIB)_wrap.C
	@echo CC$(CC_SUFF) $(PYLIB)_wrap.C "(-fPIC)"
	@mkdir -p $(dir $@)
	@$(CC) -c -fPIC -I$(PYTHON_INCLUDE) $(CFLAGS) -Wno-deprecated-declarations $< -o $@

# WORKAROUND [2021-08-27]: The option '-Wno-deprecated-declarations' is to avoid
#   a deprecation warning for 'PyEval_InitThreads()' with Python 3.9.
#   It should be removed later.


$(PYLIB_BIN): $(DEP_CONFIG) $(OBJ_PYLIB)
	@echo LD$(LD_SUFF) $(notdir $(PYLIB_BIN))
	@$(CC) -shared $(LDFLAGS) -o $@ $(OBJ_PYLIB)


.PHONY: pylib
pylib: $(PYLIB_BIN) $(DIR_OBJ)/$(PYLIB).py


.PHONY: test
test:  $(PYLIB_BIN) $(DIR_OBJ)/$(PYLIB).py
	PYTHONPATH=$(DIR_OBJ) ./test-rulesapi





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
	install -Dp $(STRIP) -m 755 -t $$BIN $(RCSHELL_BIN) $(SERVER_BIN); \
	for CMD in list show follow get wait r+ request r- delrequest; do \
	  ln -sf $(RCSHELL) $$BIN/home2l-$$CMD; \
	done; \
	if [[ "$(WITH_PYTHON)" == "1" ]]; then \
	  install -Dp $(STRIP) -m 644 -t $$LIB $(PYLIB_BIN);  \
	  install -Dp          -m 644 -t $$LIB $(DIR_OBJ)/$(PYLIB).py;  \
	fi
