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


MYDIR := $(dir $(lastword $(MAKEFILE_LIST)))

# Use these sources...
CFLAGS += -I$(MYDIR)usr/$(ARCH)/include
LDFLAGS += -L$(MYDIR)usr/$(ARCH)/lib \
	-lpjsua -lpjsip-simple -lpjsip-ua -lpjsip -lpjmedia-codec -lpjmedia -lpjmedia-videodev -lpjmedia-audiodev \
	-lpjmedia -lpjnath -lpjlib-util -lsrtp -lresample -lspeex -lyuv -lpj \
	-lavformat -lavcodec -lswscale -lavutil -lv4l2 \
	-lasound -luuid \
	-lgsmcodec -lilbccodec -lopencore-amrnb -lopencore-amrwb -lvo-amrwbenc 

	# Lines 1 & 2: PJ libs
	# Line 3: ffmpeg libs
	# Line 4: system lib(s)
	# Line 5: useless libs (-> should try to eliminate them)

	
	# Unused PJ libs:
	#  -lg7221codec
	#
	# Unused system/external libs:
	#  -lrt
