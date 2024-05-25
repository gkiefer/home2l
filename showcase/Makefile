# This file is part of the Home2L project.
#
# (C) 2024 Gundolf Kiefer
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





######################### Setup and local targets ##############################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ..
HOME2L_MOD ?= showcase


# Setup...
include $(HOME2L_SRC)/Setup.mk


.PHONY: all
all: build-indep





######################### Tools and Scripts ####################################


SCRIPTS := home2l-showhouse home2l-demo





######################### Initial etc/ directory ###############################


etc/floorplan.fpo:
	$(HOME2L_SRC)/wallclock/home2l-fpc etc/floorplan.svg


.PHONY: install-etc
install-etc: etc/floorplan.fpo
	@if [ ! -e $(HOME2L_INSTALL)/etc ]; then \
	  echo "Installing initial 'etc' directory: $(HOME2L_INSTALL)/etc ..."; \
	  find etc/ -type f -exec install -Dpm 644 "{}" "$(HOME2L_INSTALL)/{}" \; ; \
	  chmod a+x $(HOME2L_INSTALL)/etc/rules-* ; \
	else \
	  echo "Leaving existing 'etc' directory untouched: $(HOME2L_INSTALL)/etc"; \
	fi





######################### VirtualBox VM ########################################

# The VM image is not installed, but included and checked in as a source file.
# This allows new users to just download it from the source repo to get started.


VM_VBOX=../home2l-showcase-vbox.tar.gz


$(VM_VBOX):
	@echo CREATE $@
	@mkdir -p $(HOME2L_BUILD) && \
	rm -fr $(HOME2L_BUILD)/home2l-showcase-vbox && \
	cp -a vm_vbox $(HOME2L_BUILD)/home2l-showcase-vbox && \
	gunzip $(HOME2L_BUILD)/home2l-showcase-vbox/system.vdi.gz && \
	tar czf $@ -C $(HOME2L_BUILD) home2l-showcase-vbox && \
	rm -fr $(HOME2L_BUILD)/home2l-showcase-vbox





####################### Build system targets ###################################

# The 'clean' target cleans up this directory.
#
# The 'veryclean' target removes all generated files from .. (the root folder),
# which are checked into the repo.


clean:
	rm -fr etc/floorplan.fpo


veryclean: clean
	rm -fr $(VM_VBOX)


build-indep: $(VM_VBOX) etc/floorplan.fpo


install-indep: install-etc
	install -Dp -m 755 -t $(HOME2L_INSTALL)/bin $(SCRIPTS)
