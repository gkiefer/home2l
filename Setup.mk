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


# This file is included by all Makefiles of the Home2L project and
# sets up some variables related to the build environment.





############################## Basic setup #####################################


# Build directory...
HOME2L_BUILD ?= /tmp/home2l-build


# Installation directory ...
HOME2L_INSTALL ?= /opt/home2l


# Host and target architecture...
HOST_ARCH := $(shell dpkg --print-architecture)
ARCH ?= $(HOST_ARCH)


# Release build (with optimizations, stripped; default: off) ...
RELEASE ?= 0





######################### Build environment configuration ######################


# This section replaces the 'configure' task of the "autoconf" tools.
# Adaptations to the build environment (compiler to use, library
# search paths etc.) must be made here.
#
# The following settings work for Debian Stretch (9.0) as of 2018-01-12.
# It may be necessary to adapt them for future Debian versions or other
# Linux distributions.


# Shell for 'make'...
#   The Makefiles are tested with 'bash' as the standard shell.
#   'dash' (the Debian default) does not work, since its integrated 'echo'
#   command does not support the '-e' option.
SHELL := /bin/bash


# Python (used in 'resources') ...
PYTHON_INCLUDE := /usr/include/python3.7


# C/C++ Compiler & strip option for 'install'...
#
# Note [2017-07-22]: Starting with Debian Stretch, the GCC tries to produce
#       position-independent binaries by default. To maintain compatibility
#       with the pre-compiled libraries under 'external', the option
#       '-no-pie' has been added to all 'CC' invocations below. As soon as
#       all external libraries have been recompiled successfully with default
#       GCC 6 options, this option may be removed again.
#
# 'STRIP' is the option passed to 'install(1)' to strip binaries during installation.
#
ifeq ($(ARCH),$(HOST_ARCH))
  CC := g++ -no-pie
  STRIP := -s
else
  ifeq ($(ARCH),amd64)
    # Note: Cross-building for 'amd64' on 'i386' under Debian Jessie (8.0) requires
    #       the package 'g++-multilib' (and dependencies). However, 'gcc-multilib'
    #       conflicts with the 'armhf' cross-building tools (see below).
    #       The workaround is to install 'g++-4.9-multilib' instead.
    #
    #       Under Debian Stretch, things appear to be similar. Here, 'g++-6-multilib'
    #       has been installed manually.
    #
    #       Under Debian Buster, things appear to be similar again. Here,
    #       'g++-8-multilib' has been installed manually. The same holds for
    #       compiling 'i386' binaries on an 'amd64' machine.
    ifeq ($(HOST_ARCH),i386)
      CC := g++ -m64 -no-pie
      STRIP := -s
    endif
  endif
  ifeq ($(ARCH),i386)
    # Note: Cross-building for 'i386' on 'amd64' is only rarely tested.
    ifeq ($(HOST_ARCH),amd64)
      CC := g++ -m32 -no-pie
      STRIP := -s
    endif
  endif
  ifeq ($(ARCH),armhf)
    # Note: Crossbuilding for 'armhf' under Debian Jessie (8.0) requires the
    #       package 'crossbuild-essential-armhf' from
    #       'deb http://emdebian.org/tools/debian/ jessie main'.
    # Note [2017-07-22]: The option '-static-libstdc++' is added for armhf to create
    #       binaries with the chance to run under Debian Jessie and Debian Stretch.
    CC := arm-linux-gnueabihf-g++ -no-pie -static-libstdc++
    STRIP := -s --strip-program=arm-linux-gnueabihf-strip
  endif
endif
ifndef CC
$(error Cannot build for $(ARCH) on a $(HOST_ARCH) machine!)
endif





#################### Compile-time configuration ################################


# C/C++ Setting: General ...
WITH_DEBUG ?= 1
  # Enable debug output
  #   1: Debug output can be enabled at runtime with option 'debug = 1'
  #   0: Debug output disabled (reduces code size)
WITH_CLEANMEM ?= 0
  # Enable extra code to tidily cleanup the heap on application exit and to perform other things
  # to avoid potentially false warnings by memory checking tools.
  # This does not improve the performance or memory consumption of any application in any way
  # and may even increase the code size and reduce performance.
  # Use this option for debugging your code for memory leaks with 'valgrind' and friends.


# C/C++ Setting: Resources-specific features ...
WITH_PYTHON ?= 1
  # Enable Python API (requires SWIG and Python dev. packages)
WITH_READLINE ?= 1
  # Enable GNU Readline for the shell (may be disabled for tiny devices).


# C/C++ Setting: WallClock-specific features ...
WITH_PHONE ?= 1
	# Build with phone applet (requires PJSIP or libLinphone to build)
WITH_CALENDAR ?= 1
	# Build with calendar applet (requires the 'remind' command line tool to run)
WITH_MUSIC ?= 1
	# Build with music applet (requires 'mpdclient' to build and 'mpd' to run)
WITH_GSTREAMER ?= $(WITH_MUSIC)
	# Build music applet with the capability to stream audio back to the local device
	# (requires 'GStreamer' to build)


# Default compiler and linker settings ...
CFLAGS := -MMD -g -Wall -pthread -I.
  # -MMD : Create make dependency files
  # -I.  : Make sure that includes are first searched in the local directory (config.H!)
  #  -DBUILD_OS=\"Debian\" -DBUILD_ARCH=\"$(ARCH)\"
LDFLAGS := -pthread
SRC :=


# Release settings modifications ...
ifeq ($(RELEASE),1)     # Optimize for speed, but do not sacrifice code size too much
CFLAGS += -O2
CC_SUFF :=
else    # Optimize debugging experience
CFLAGS += # -Og
CC_SUFF := '*'
endif


# WORKAROUND [2018-09-26]:
#   When cross-compiling for the armhf architecture, strange segfaults with classes
#   with virtual methods occur:
#
#     Program terminated with signal SIGSEGV, Segmentation fault.
#     0  0x005419e4 in typeinfo for CRcEventDriver ()
#
#   This only happens if any -O* option (-Og, -O2, -O1, -Os) is used.
#   As a workaround, we filter out any -O* option from the CFLAGS here.
#   To be tested again after the next Debian/GCC release.
ifeq ($(ARCH),armhf)
CFLAGS := $(filter-out -O%,$(CFLAGS))
endif






############### Common variables and targets for subproject Makefiles ##########


# Version ...
ifeq ($(BUILD_VERSION),)
BUILD_VERSION := $(shell git describe --tags --long --dirty='*' --abbrev=4 --always 2>/dev/null || echo dev)
endif
BUILD_DATE := $(shell date +%Y-%m-%d)


# Author ...
HOME2L_AUTHOR := "Gundolf Kiefer"
HOME2L_URL := "https://gkiefer.github.io/home2l"


# Directory of .o files
DIR_OBJ := $(HOME2L_BUILD)/$(ARCH)/$(HOME2L_MOD)


# Common dependencies ...
default: all

clean-build:
	rm -fr $(HOME2L_BUILD) core *~

build: build-arch build-indep
build-indep:
build-arch:

install-indep: build-indep
install-arch: build-arch

veryclean:


# Common compilation rules ...

$(DIR_OBJ)/%.o: %.C
	@echo CC$(CC_SUFF) $<
	@mkdir -p $(dir $@)
	@$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(DIR_OBJ)/%-pic.o: %.C
	@echo CC$(CC_SUFF) $< "(-fPIC)"
	@mkdir -p $(dir $@)
	@$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) $< -o $@





############################## config.[HC] #####################################


# C/C++ Setting: General...
CFG_H_CONTENT := "\n\#define WITH_DEBUG" $(WITH_DEBUG)
CFG_H_CONTENT += "\n\#define WITH_CLEANMEM" $(WITH_CLEANMEM)

# C/C++ Setting: Resources...
CFG_H_CONTENT += "\n"
CFG_H_CONTENT += "\n\#define WITH_PYTHON" $(WITH_PYTHON)
CFG_H_CONTENT += "\n\#define WITH_READLINE" $(WITH_READLINE)

# C/C++ Setting: WallClock...
CFG_H_CONTENT += "\n"
CFG_H_CONTENT += "\n\#define WITH_PHONE" $(WITH_PHONE)
CFG_H_CONTENT += "\n\#define WITH_CALENDAR" $(WITH_CALENDAR)
CFG_H_CONTENT += "\n\#define WITH_MUSIC" $(WITH_MUSIC)
CFG_H_CONTENT += "\n\#define WITH_GSTREAMER" $(WITH_GSTREAMER)


# Rules...
$(HOME2L_BUILD)/$(ARCH)/config.H: config
$(HOME2L_BUILD)/$(ARCH)/config.C: config

$(HOME2L_BUILD)/$(ARCH)/config.o: $(HOME2L_BUILD)/$(ARCH)/config.C
	@echo CC$(CC_SUFF) $<
	@mkdir -p $(dir $@)
	@$(CC) -c -fPIC $(CPPFLAGS) $(CFLAGS) $< -o $@

.PHONY: config
config:
	@echo "Updating 'config.H' and 'config.C'..."
	@mkdir -p $(HOME2L_BUILD)/$(ARCH); \
	cd $(HOME2L_BUILD)/$(ARCH); \
	echo -e "#ifndef _CONFIG_\n#define _CONFIG_\n" \
	    "\nextern const char *buildVersion;\nextern const char *buildDate;\n" \
	    "\nextern const char *home2lAuthor;\nextern const char *home2lUrl;\n" \
	    "\n#define BUILD_OS \"Debian\"\n#define BUILD_ARCH \""$(ARCH)"\"\n" \
	    "\n#define HOME2L_AUTHOR \""$(HOME2L_AUTHOR)"\"\n#define HOME2L_URL \""$(HOME2L_URL)"\"\n" \
	    $(CFG_H_CONTENT) \
	    "\n\n#endif" \
	    | sed 's/[[:space:]]*$$//g' > config-new.H; \
	diff -q config.H config-new.H > /dev/null 2>&1 || mv config-new.H config.H; \
	rm -f config-new.H; \
	echo -e "const char *buildVersion = \""$(BUILD_VERSION)"\";\nconst char *buildDate = \""$(BUILD_DATE)"\";" \
	    "\nconst char *home2lAuthor = \""$(HOME2L_AUTHOR)"\";\nconst char *home2lUrl = \""$(HOME2L_URL)"\";" \
	    | sed 's/[[:space:]]*$$//g' > config-new.C; \
	diff -q config.C config-new.C > /dev/null 2>&1 || mv config-new.C config.C; \
	rm -f config-new.C


# Set some variables...
DEP_CONFIG := $(HOME2L_BUILD)/$(ARCH)/config.H $(HOME2L_BUILD)/$(ARCH)/config.o
	# Linker targets must depend on this to make sure that an up-to-date 'config.[HC]' combination is generated.

CFLAGS += -I$(HOME2L_BUILD)/$(ARCH)
LDFLAGS += $(HOME2L_BUILD)/$(ARCH)/config.o
