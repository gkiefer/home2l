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


AUTHOR := "Gundolf Kiefer"
PACKAGE := Home2L
DOMAIN := home2l

#~ HOME2L_INSTALL := ./tmp			# testing





######################### Setup and local targets ##############################


# Identify myself inside the Home2L source tree ...
#   (values may be overwritten by some custom build system)
HOME2L_SRC ?= ..
HOME2L_MOD ?= locales


# Setup...
include $(HOME2L_SRC)/Setup.mk


SRC = ../common/*.C ../wallclock/*.C

.PHONY: all
all: de.mo


de.po: $(SRC)
	@echo "LC: $@ (update)"
	@xgettext --from-code=utf-8 -k_ -kN_ -cTRANSLATORS -i -d $(DOMAIN) -o $(DOMAIN).pot \
		--package-name=$(PACKAGE) --copyright-holder=$(AUTHOR) --msgid-bugs-address=$(AUTHOR) \
		$(SRC)
	@msgmerge -N -i -U --backup=simple de.po home2l.pot 2> /dev/null

#~ 		$(shell for F in `(cd ../wallclock; make --no-print-directory -s print-sources`; do realpath -e --relative-to=$$PWD $$F; done | sort)


de.mo: de.po
	@echo "LC: $@ (compile)"
	@msgfmt --check --statistics -o de.mo de.po


.PHONY: template
template:



.PHONY: init
init:
	msginit -i home2l.pot -l de_DE





############################ Build system targets ##############################


clean:
	rm -f home2l.pot de.mo core *~


build-indep: de.mo


install-indep: de.mo
	install -Dp -m 644 de.mo $(HOME2L_INSTALL)/locale/de_DE/LC_MESSAGES/home2l.mo
