# This file is part of the Home2L project.
#
# (C) 2015-2025 Gundolf Kiefer
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


# Web browser and PDF viewer for the 'view_*' targets...
BROWSER ?= firefox --new-window
PDFVIEWER ?= evince


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ..
HOME2L_MOD ?= doc


# Setup...
include $(HOME2L_SRC)/Setup.mk


BOOK := home2l-book
API_C := home2l-api_c
API_PYTHON := home2l-api_python


.PHONY: all
all: build-indep


.PHONY: readme
readme: ../README.pdf


.PHONY: book
book: $(BOOK).pdf


.PHONY: api
api: $(API_C) $(API_PYTHON)


.PHONY: view_book
view_book: book
	$(PDFVIEWER) $(BOOK).pdf


.PHONY: view_api
view_api: api
	$(BROWSER) $(API_C)/index.html $(API_PYTHON)/index.html &


.PHONY: check
check: check_api check_readme check_book





######################### README & Checked-in Top-Level Doc ####################

# The targets in this section generate doc files which are checked into the repository,
# even if they are binary files. These files are removed by the 'veryclean' target.
#
# The update policy is as follows:
#
# 1. The normal 'build-indep' target generates/updates:
#      README.pdf
#      home2l-figs/*
#      home2l-showcase-vbox.tar.gz
#
# 2. The following files/dirs consume much disk space and would be updated very frequently
#    on small code changes. Hence, they are NOT updated with 'build*', but only explicitly
#    with the 'commit' target:
#
#      home2l-book.pdf
#      home2l-api_c/
#      home2l-api_python/


../README.pdf: ../README.md ../home2l-figs/readme-title-web.png
	@echo PANDOC $@
	@cd .. && \
	pandoc README.md -f gfm -V fontfamily=arev -V fontsize=12pt -V geometry:margin=2.5cm -V urlcolor=blue -o README.pdf \
		# In Debian (Buster), '-V fontfamily=arev' requires the package 'texlive-fonts-extra' to be installed.

../home2l-figs/home2l-icon-web.png: figs/home2l-icon.svg
	@echo INKSCAPE $@
	@mkdir -p ../home2l-figs && \
	inkscape -C $< -b white -w 96 -h 96 -o $@ >/dev/null 2>&1 && \
	mogrify $@ -colors 256 # -quality 90

../home2l-figs/readme-title-web.png: figs/readme-title.svg  figs/wallclock-floorplan-3.png figs/wallclock-doorman-1.png figs/wallclock-doorman-2.png figs/wallclock-music.png figs/wallclock-calendar.png figs/wallclock-home-3.png
	@echo INKSCAPE $@
	@mkdir -p ../home2l-figs && \
	inkscape -C $< -b white -w 1024 -o $@ 2>&1 >/dev/null 2>&1 && \
	mogrify $@ -colors 256 # -quality 90


.PHONY: commit
commit:  $(API_C) $(API_PYTHON) $(BOOK).pdf
	rm -fr ../$(API_C) ../$(API_PYTHON)
	mv $(API_C) $(API_PYTHON) $(BOOK).pdf ..





######################### Demo: VirtualBox VM ##################################


.PHONY: showcase-vbox
showcase-vbox: ../home2l-showcase-vbox.tar.gz


../home2l-showcase-vbox.tar.gz:
	@echo CREATE $@
	@mkdir -p $(HOME2L_BUILD) && \
	rm -fr $(HOME2L_BUILD)/home2l-showcase-vbox && \
	cp -a showcase/vmachine $(HOME2L_BUILD)/home2l-showcase-vbox && \
	gunzip $(HOME2L_BUILD)/home2l-showcase-vbox/system.vdi.gz && \
	tar czf $@ -C $(HOME2L_BUILD) home2l-showcase-vbox && \
	rm -fr $(HOME2L_BUILD)/home2l-showcase-vbox





######################### Home2L Book: Automated parts #########################

# The source files for the following sub-projects are set and must be
# updated manually!


# Module 'common'...
ref_env_common.tex: ../common/base.C ../common/env.C ../common/phone.C
	@echo EXCODE $@
	@./excode.py e debug:home2l:sys:net:location:phone $^ > $@ || (rm $@; exit 7)


# Module 'resources'...
ref_env_resources.tex: ../resources/rc_core.C ../resources/resources.C ../resources/rc_drivers.C \
	       ../resources/home2l-server.C ../resources/home2l-shell.C
	@echo EXCODE $@
	@./excode.py e rc:drv:shell $^ > $@ || (rm $@; exit 7)

ref_rc_resources.tex: ../resources/rc_drivers.C
	@echo EXCODE $@
	@./excode.py r : $^ > $@ || (rm $@; exit 7)


# Module 'tools'...
ref_env_daemon.tex: ../tools/home2l-daemon.C
	@echo EXCODE $@
	@./excode.py e daemon $^ > $@ || (rm $@; exit 7)


# Module 'brownies'...
#   includes the resources of 'home2l-drv-brownies', which may be documented
#   in the "Drivers" (not "Brownies") chapter.
ref_env_brownies.tex: ../brownies/brownies.C ../brownies/home2l-brownie2l.C
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)

ref_rc_brownies.tex: ../brownies/brownies.C
	@echo EXCODE $@
	@./excode.py r : $^ > $@ || (rm $@; exit 7)


# Module 'wallclock'...
ref_env_wallclock.tex: ../wallclock/system.C \
	       ../wallclock/ui_base.C ../wallclock/ui_widgets.C ../wallclock/ui_screen.C \
	       ../wallclock/floorplan.C \
	       ../wallclock/alarmclock.C ../wallclock/apps.C ../wallclock/alarmclock.C ../wallclock/app_home.C ../wallclock/home2l-wallclock.C \
	       ../wallclock/app_phone.C ../wallclock/app_calendar.C ../wallclock/app_music.C
	@echo EXCODE $@
	@./excode.py e ui:floorplan:alarm:var.alarm:phone:calendar:music:var.music $^ > $@ || (rm $@; exit 7)

ref_rc_wallclock.tex: ../wallclock/system.C
	@echo EXCODE $@
	@./excode.py r : $^ > $@ || (rm $@; exit 7)


# Module 'doorman'...
ref_env_doorman.tex: ../doorman/home2l-doorman.C
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)

ref_rc_doorman.tex: ../doorman/home2l-doorman.C
	@echo EXCODE $@
	@./excode.py r : $^ > $@ || (rm $@; exit 7)


# Drivers ...
ref_env_drv_gpio.tex: ../drivers/gpio/home2l-drv-gpio.C
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)

ref_env_drv_mqtt.tex: ../drivers/mqtt/home2l-drv-mqtt.C
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)

ref_env_drv_enocean.tex: ../drivers/enocean/enocean.C ../drivers/enocean/home2l-drv-enocean.C
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)

ref_env_drv_weather.tex: ../drivers/weather/home2l-drv-weather
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)
ref_rc_drv_weather.tex: ../drivers/weather/home2l-drv-weather
	@echo EXCODE $@
	@./excode.py r : $^ > $@ || (rm $@; exit 7)

ref_env_drv_fronius.tex: ../drivers/fronius/home2l-drv-fronius
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)

ref_env_drv_fingerprint.tex: ../drivers/fingerprint/home2l-drv-fingerprint
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)
ref_rc_drv_fingerprint.tex: ../drivers/fingerprint/home2l-drv-fingerprint
	@echo EXCODE $@
	@./excode.py r : $^ > $@ || (rm $@; exit 7)

ref_env_drv_mpd.tex: ../drivers/mpd/home2l-drv-mpd.C
	@echo EXCODE $@
	@./excode.py e : $^ > $@ || (rm $@; exit 7)





######################### Home2L Book: Main document ###########################


REFS_ENV := ref_env_common.tex ref_env_resources.tex ref_env_daemon.tex \
						ref_env_brownies.tex \
	          ref_env_wallclock.tex ref_env_doorman.tex \
	          ref_env_drv_gpio.tex ref_env_drv_mqtt.tex ref_env_drv_enocean.tex \
	          ref_env_drv_weather.tex ref_env_drv_fronius.tex ref_env_drv_mpd.tex ref_env_drv_fingerprint.tex \


REFS_RC  := ref_rc_resources.tex  \
						ref_rc_brownies.tex \
	          ref_rc_wallclock.tex ref_rc_doorman.tex \
	          ref_rc_drv_weather.tex ref_rc_drv_fingerprint.tex


$(BOOK).pdf: $(REFS_ENV) $(REFS_RC) $(BOOK).tex
	@echo "PDFLaTeX: $(BOOK).pdf (1st run - draft)"
	@set -o pipefail; pdflatex -draftmode -file-line-error --shell-escape -interaction=nonstopmode "\def\version{$(BUILD_VERSION)} \input{$(BOOK).tex}" 2>&1 | (grep "\.tex:.*:" || true)
	@echo "PDFLaTeX: $(BOOK).pdf (2nd run - draft)"
	@set -o pipefail; pdflatex -draftmode -file-line-error --shell-escape -interaction=nonstopmode "\def\version{$(BUILD_VERSION)} \input{$(BOOK).tex}" 2>&1 | (grep "\.tex:.*:" || true)
	@echo "makeindex $(BOOK).idx"
	@makeindex $(BOOK).idx >/dev/null 2>&1
	@echo "PDFLaTeX: $(BOOK).pdf (3rd run - final)"
	@set -o pipefail; pdflatex -file-line-error --shell-escape -interaction=nonstopmode "\def\version{$(BUILD_VERSION)} \input{$(BOOK).tex}" 2>&1 | (grep "\.tex:.*:" || true)


.PHONY: draft
draft:
	TEXINPUTS=.:./c_api/latex: pdflatex -synctex=1 -shell-escape -interaction=nonstopmode "\def\pub{0} \input{$(BOOK).tex}"





#######################  Doxygen: C API, Python API ##########################


# How to improve layout:
# - see https://stackoverflow.com/questions/19831837/documenting-with-doxygen-put-details-on-top
# - generate layout file template: doxygen -l layoutFileName.xml


API_C_SRC = $(wildcard api_c.H \
	../common/*.H \
	../resources/resources.H \
	../brownies/brownies.H ../brownies/avr/interface.h  ../brownies/avr/configure.h ../brownies/avr/base.h ../brownies/avr/core.h ../brownies/avr/twi.h \
	../wallclock/*.H \
	)

$(API_C): api_c.doxygen ../home2l-figs/home2l-icon-web.png $(API_C_SRC)
	@echo DOXY $@
	@rm -fr $@
	@( cat api_c.doxygen ; echo 'PROJECT_NUMBER="$(BUILD_VERSION) ($(BUILD_DATE))"' ; echo 'INPUT=$(API_C_SRC)' ) | doxygen - >/dev/null
	@mv api_c/html $@
	@rm -fr api_c



API_PYTHON_SRC = ../common/env.H ../resources/resources.H ../resources/home2l-python.i

HOST_ARCH := $(shell dpkg --print-architecture)

SWIG_PY := $(HOME2L_BUILD)/$(HOST_ARCH)/resources/home2l.py


$(SWIG_PY): $(API_PYTHON_SRC)
	@$(MAKE) -C ../resources ARCH=$(HOST_ARCH) $(SWIG_PY)


$(API_PYTHON): api_python.doxygen ../home2l-figs/home2l-icon-web.png api_python.py $(SWIG_PY)
	@echo DOXY $@
	@rm -fr $@
	@cp -a $(SWIG_PY) home2l.py
	@( cat api_python.doxygen ; echo 'PROJECT_NUMBER="$(BUILD_VERSION) ($(BUILD_DATE))"' ) | doxygen - >/dev/null
	@mv api_python/html $@
	@rm -fr api_python home2l.py





####################### Checking ###############################################

# Check all documentation for broken links.
# This target is meant to be called manually.


.PHONY: check_api
check_api: $(API_C) $(API_PYTHON) $(BOOK).pdf
	@rm -f brownies && ln -s ../brownies .
	@echo -e "\n#################### Checking C/C++ API ####################\n"
	@linkchecker --check-extern home2l-api_c/index.html
	@echo -e "\n#################### Checking Python API ####################\n"
	@linkchecker --check-extern home2l-api_python/index.html
	@rm brownies


.PHONY: check_readme
check_readme: ../README.pdf
	@echo -e "\n#################### Checking README.pdf ####################\n"
	@./check_pdf_links ../README.pdf ..


.PHONY: check_book
check_book: $(API_C) $(API_PYTHON) $(BOOK).pdf ../README.pdf
	@echo -e "\n#################### Checking Home2L Book ####################\n"
	@./check_pdf_links home2l-book.pdf ..





####################### Build system targets ###################################

# The 'clean' target cleans up this directory.
#
# The 'veryclean' target removes all generated files from .. (the root folder),
# which are checked into the repo.


clean:
	rm -fr api_c api_python svg-inkscape texput.log home2l.py
	rm -fr $(BOOK).aux $(BOOK).idx $(BOOK).ilg $(BOOK).ind $(BOOK).log $(BOOK).out $(BOOK).toc $(BOOK).tex.bak
	rm -fr _markdown_* $(BOOK).markdown.*
	rm -fr $(BOOK).pdf $(API_C) $(API_C).pdf $(API_PYTHON) $(API_PYTHON).pdf
	rm -fr ref_*.tex


veryclean: clean
	rm -fr ../README.pdf ../home2l-*


build-indep: $(API_C) $(API_PYTHON) $(BOOK).pdf ../README.pdf ../home2l-showcase-vbox.tar.gz


install-indep:
	install -Dp -m 644 -t $(HOME2L_INSTALL)/doc/ home2l-book.pdf
	install -Dp -m 644 -t $(HOME2L_INSTALL)/doc/home2l-api_c/ $(API_C)/*.*
	install -Dp -m 644 -t $(HOME2L_INSTALL)/doc/home2l-api_c/search/ $(API_C)/search/*.*
	install -Dp -m 644 -t $(HOME2L_INSTALL)/doc/home2l-api_python/ $(API_PYTHON)/*.*
	install -Dp -m 644 -t $(HOME2L_INSTALL)/doc/home2l-api_python/search/ $(API_PYTHON)/search/*.*
