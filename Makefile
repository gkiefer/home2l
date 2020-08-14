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


# This is the master Makefile for the Home2L build system.





############################## Configuration ###################################


# Preset architectures to build for ...
ifeq ($(CFG),minimal)
  # Minimal set ...
  ARCHS ?= $(shell dpkg --print-architecture)
else ifeq ($(CFG),demo)
  # Fair set to run the demos ...
  ARCHS ?= $(shell dpkg --print-architecture)
else
  # Default: All architectures ...
  ARCHS ?= amd64 armhf i386
endif


# Preset modules (sub-projects) to build ...
ifeq ($(CFG),minimal)
  # Minimal set ...
  MODS ?= tools resources
else ifeq ($(CFG),basic)
  # Basic set ...
  MODS ?= tools resources brownies
else ifeq ($(CFG),demo)
  # Modules for the demo image ...
  MODS ?= tools resources brownies wallclock doorman locales
else
  # Default: All modules...
  MODS ?= tools resources brownies wallclock doorman locales doc
endif


# Preset drivers to build ...
ifeq ($(CFG),minimal)
  # Minimal set ...
  DRVS ?=
else ifeq ($(CFG),basic)
  # Basic set ...
  DRVS ?= gpio mqtt brownies weather
else ifeq ($(CFG),demo)
  # Fair set to run the demos ...
  DRVS ?= demo gpio mqtt brownies weather
else
  # Default: All modules...
  DRVS ?= $(shell ls drivers)
endif


# Preset compile-time settings if CFG is set ...
#   By default, they are all set to '1' in the module Makefile
ifeq ($(CFG),minimal)
  # Minimal set ...
  export WITH_PYTHON ?= 0
  export WITH_READLINE ?= 0
else ifeq ($(CFG),basic)
  # (WallClock settings presently have no effect, since WallClock is excluded from the basic build)
  export WITH_ANDROID ?= 0
  export WITH_PHONE ?= 0
  export WITH_CALENDAR ?= 0
  export WITH_MUSIC ?= 0
  export WITH_GSTREAMER ?= 0
else ifeq ($(CFG),demo)
  # Fair set to run the demos ...
  export WITH_ANDROID ?= 0
  export WITH_PHONE ?= 1
  export WITH_GSTREAMER ?= 0
endif


# Preset release build...
export RELEASE ?= 1





##### Wrap up ... #####


# Handle single module/driver invocation ...
ifdef MOD
  MODS := $(MOD)
  DRVS :=
endif
ifdef DRV
  MODS :=
  DRVS := $(DRV)
endif


# Add DRVS to MODS ...
PREP_DRVS := $(DRVS:%=drivers/%)
  # put "driver/" in front of every driver name (absolute and relative)
PREP_DRVS := $(PREP_DRVS:drivers//%=/%)
  # remove "driver/" again, if the path name was absolute





############################## Setup ###########################################


# Export and preset build and install target directories...
#   PREFIX is accepted as INSTALL for compatibility reasons.
ifdef BUILD
HOME2L_BUILD := $(BUILD)
endif
ifdef INSTALL
HOME2L_INSTALL := $(INSTALL)
endif
ifdef PREFIX
HOME2L_INSTALL := $(PREFIX)
endif


# Report to sub-makes that they are called from the main build system ...
export HOME2L_BUILDSYSTEM := 1


# Include setup file ...
include Setup.mk





############################## Help ############################################


all: build


debug:
	@echo MODS=$(MODS)
	@echo DRVS=$(DRVS)
	@echo PREP_DRVS=$(PREP_DRVS)


help:
	@echo
	@echo "This is the master Makefile of the Home2L build system."
	@echo
	@echo "Targets:"
	@echo "  help:      Print this help"
	@echo
	@echo "  build:     Build everything (default target)"
	@echo "  install:   Install to \$$HOME2L_INSTALL [ /opt/home2l ]"
	@echo "  clean:     Clean everything (except binary doc files to be checked into the repository)"
	@echo "  veryclean: Clean really everything"
	@echo "  uninstall: Remove \$$HOME2L_INSTALL [ /opt/home2l ]"
	@echo
	@echo "  docker:        Build the docker showcase image locally (current working directory)"
	@echo "  docker-master: Build the docker showcase image for the master branch"
	@echo "  docker-run:    Run the docker showcase image (latest)"
	@echo
	@echo "Variables:"
	@echo "  ARCHS: List of architectures to build for (available: amd64 armhf i386) [ all ]"
	@echo "  MODS:  Modules (sub-projects) to build (space-separated list of directory names) [ all ]"
	@echo "  MOD:   If set, build only the given module (overrides MODS, DRVS)."
	@echo "  DRVS:  Drivers to build [ all integrated ]"
	@echo "  DRV:   If set, build only the given driver (overrides MODS, DRVS). It is allowed to pass an absolute path."
	@echo "  CFG:   Configuration preset"
	@echo "           'minimal' = only the very basic tools and module 'resources' for the host architecture"
	@echo "           'basic'   = same as 'minimal', but with python support and GPIO driver"
	@echo "           'demo'    = most common modules (including WallClock) for the host architecture"
	@echo "  INSTALL: Target directory for the installation [ $(HOME2L_INSTALL) ]"
	@echo "           This can be preset by the environment variable HOME2L_INSTALL."
	@echo "  BUILD:   Directory for intermediate files when building [ $(HOME2L_BUILD) ]"
	@echo "           This can be preset by the environment variable HOME2L_BUILD."
	@echo "  RELEASE: Select whether to compile with optimizations (1) or for debugging (0) [ 1 ]"
	@echo
	@echo "Examples:"
	@echo "  > make CFG=demo install     # Build and install everything for the demo"
	@echo "  > make                      # Build everything (full build)"
	@echo "  > make MODS=doorman install # Build and install only the DoorMan for all architectures"
	@echo
	@echo "A basic configuration (CFG=basic) is perfectly sufficient to run and develop"
	@echo "drivers and automation rules."
	@echo
	@echo "A full build may have additional requirements and may not work out-of-the-box."





############################## Docker Image ####################################


DOCKER_IMAGE=gkiefer/home2l
DOCKER_MASTER=https://github.com/gkiefer/home2l.git


# Build from local working directory ...
.PHONY: docker
docker:
	@TAG="$(BUILD_VERSION)"; TAG=$${TAG%\-*}; [[ "$$TAG" != "" ]] || TAG=work; \
	echo DOCKER $(DOCKER_IMAGE):$$TAG && \
	docker build --build-arg BUILD_VERSION=$$TAG -t $(DOCKER_IMAGE):$$TAG -t $(DOCKER_IMAGE):latest .


# Build from local master branch ...
.PHONY: docker-master
docker-master:
	@rm -fr $(HOME2L_BUILD)/docker && mkdir -p $(HOME2L_BUILD)/docker && \
	git clone -b master --single-branch file://$$PWD $(HOME2L_BUILD)/docker/home2l && \
	export BUILD_VERSION= && \
	$(MAKE) -C $(HOME2L_BUILD)/docker/home2l docker


# Run the container (latest) ...
.PHONY: docker-run
docker-run:
	@xhost +local: && \
	docker run -ti --rm --tmpfs /tmp --name home2l-showcase --hostname home2l-showcase \
	  -e DISPLAY=$$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix --device /dev/snd \
	  gkiefer/home2l





############################## Main targets ####################################

# The following targets are equivalent to the targets of subprojects and
# build all subprojects for all architectures.


# Export directories and other global settings ...
export HOME2L_BUILD
export HOME2L_INSTALL
export BUILD_VERSION := $(BUILD_VERSION)


# Note: In the 'build' target, we must pre-generate all directories inside the 'build'
#   tree. Otherwise, files shared be different modules will unnecessarily by re-compiled,
#   ultimately resulting in compilations during 'install' (perhaps as root) even after
#   a full successful build run!
#
#   Example:
#     1. Mod 'tools' compiles '../common/base.C' to 'build/tools/../common/base.o'.
#     2. Mod 'resources' checks 'build/resources/../common/base.o', which will be an identical
#        file. However, the subdir 'resources' does not exist yet, so the .o file is not found!
#     3. It is now (unnecessarily) re-compiled, leading to an unnecessary re-linking

build:
	@for A in $(ARCHS); do \
	  for P in $(MODS) $(PREP_DRVS); do \
	    mkdir -p $(HOME2L_BUILD)/$$A/$$P; \
	  done; \
	done
	@for P in $(MODS) $(PREP_DRVS); do \
	  echo -e "\n\n\n############################################################"; \
	  echo -e "#\n#       $$P\n#\n############################################################"; \
	  echo -e "\n\n#### Building '$$P' - common files...\n"; \
	  $(MAKE) -C $$P build-indep || exit 1; \
	  echo; \
	  for A in $(ARCHS); do \
	    echo -e "\n\n#### Building '$$P' for '$$A'...\n"; \
	    $(MAKE) -C $$P HOME2L_MOD=$$P ARCH=$$A build-arch || exit 1; \
	    echo; \
	  done; \
	done


install:
	echo ##### BUILD_VERSION=$(BUILD_VERSION) BUILD=$(BUILD)
	@mkdir -p $(HOME2L_INSTALL); \
	for P in $(MODS) $(PREP_DRVS); do \
	  echo -e "\n\n\n############################################################"; \
	  echo -e "#\n#       $$P\n#\n############################################################"; \
	  echo -e "\n\n#### Installing '$$P' - common files...\n"; \
	  $(MAKE) -C $$P install-indep || exit 1; \
	  echo; \
	  for A in $(ARCHS); do \
	    echo -e "\n\n#### Installing '$$P' for '$$A'...\n"; \
	    $(MAKE) -C $$P HOME2L_MOD=$$P ARCH=$$A install-arch || exit 1; \
	  done; \
	done


install-clean:
	cd $(HOME2L_INSTALL); \
	rm -fr README VERSION env.sh bin/ lib/ share/ install/ locale/ doc/


clean: clean-build
	@for P in $(MODS) $(PREP_DRVS); do       \
		echo -e "\n\n#### Cleaning '$$P'...\n"; \
		$(MAKE) -C $$P HOME2L_MOD=$$P clean || exit 1; \
		echo; \
	done


veryclean:
	@for P in $(MODS) $(PREP_DRVS); do       \
		echo -e "\n\n#### Cleaning '$$P' very thoroughly ...\n"; \
		$(MAKE) -C $$P HOME2L_MOD=$$P veryclean || exit 1; \
		echo; \
	done
#~ 	$(MAKE) -C doc veryclean


uninstall:
	rm -fr $(HOME2L_INSTALL)

