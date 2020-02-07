# This file is part of the Home2L project.
#
# (C) 2019-2020 Gundolf Kiefer
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


# This file defines the set of brownie variants - the Brownie Family.
# Custom variants can be added below. Variants are defined in a device-independent
# way (though some devices may not support certain features due to pin limitations,
# for example).
#
# The variable BROWNIE_FAMILY defines all variant/device combinations to be
# built.





#################### List of all Brownies of the family ########################

# The following variable defines all Brownies built with the "all" target.

BROWNIE_FAMILY = init.t85 init.t84 init.t861 \
                 ahub.t85 bhub.t85 \
                 win.t84 win2.t84 \
                 mat4x8.t861 \
                 gpio4.t84 gpio12i.t861






#################### Initialization  ###########################################


# init: Maintenances system, for initializing new devices ...
ifeq ($(BROWNIE_VARIANT),init)
	BROWNIE_CFG = IS_MAINTENANCE=1
	BROWNIE_BASE = 0x0040
endif





#################### Hubs ######################################################


# ahub: Primary hub, not notifying, for immediate connection to a Linux host ...
ifeq ($(BROWNIE_VARIANT),ahub)
	BROWNIE_CFG = WITH_TWIHUB=1 TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x1 GPIO_OUT_PRESET=0x0
endif


# bhub: Intermediate hub, notifying, for connection to a Brownie TWI master ...
ifeq ($(BROWNIE_VARIANT),bhub)
	BROWNIE_CFG = WITH_TWIHUB=1 TWI_SL_NOTIFY=1 GPIO_OUT_PRESENCE=0x1 GPIO_OUT_PRESET=0x0
endif





#################### Generic/GPIO ##############################################


# gpio4: (Testing) Generic GPIO node ...
ifeq ($(BROWNIE_VARIANT),gpio4)
	BROWNIE_CFG ?= GPIO_OUT_PRESENCE=0x03 GPIO_OUT_PRESET=0x02 GPIO_IN_PRESENCE=0x0c GPIO_IN_PULLUP=0x0f
endif





#################### Matrix ####################################################


# matrix4x8: Matrix Brownie for up to 32 (window) sensors ...
ifeq ($(BROWNIE_VARIANT),mat4x8)
	BROWNIE_CFG ?= WITH_TEMP_ZACWIRE=1 MATRIX_ROWS=4 MATRIX_COLS=8
endif





#################### Window/Shades #############################################


# win: Window brownie; supports temperature sensor and single shades ...
ifeq ($(BROWNIE_VARIANT),win)
	BROWNIE_CFG ?= WITH_TEMP_ZACWIRE=1 SHADES_PORTS=1
	  # SHADES_TIMEOUT=0 TWI_SL_NOTIFY=0
endif


# win2: Dual window brownie; supports temperature sensor and two shades ...
ifeq ($(BROWNIE_VARIANT),win2)
	BROWNIE_CFG ?= WITH_TEMP_ZACWIRE=1 SHADES_PORTS=2
	  # SHADES_TIMEOUT=0
endif





#################### Testing ###################################################


# Testing ...
ifeq ($(BROWNIE),test.t85)
	BROWNIE_CFG ?= WITH_TWIHUB=1 TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x1 GPIO_OUT_PRESET=0x0
endif

ifeq ($(BROWNIE),test.t84)
	BROWNIE_CFG ?= WITH_TWIHUB=0 TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x00 GPIO_OUT_PRESET=0x00 WITH_TEMP_ZACWIRE=1 SHADES_PORTS=2 SHADES_TIMEOUT=0 SHADES_PERSISTENCE=0
	BROWNIE_BASE = 0x0000
endif

ifeq ($(BROWNIE),test.t861)
	BROWNIE_CFG ?= TWI_SL_NOTIFY=0 WITH_TEMP_ZACWIRE=0 MATRIX_ROWS=4 MATRIX_COLS=8
#~ 	BROWNIE_CFG ?= TWI_SL_NOTIFY=0  WITH_TEMP_ZACWIRE=0 \
#~ 	               GPIO_OUT_PRESENCE=0x333 GPIO_OUT_PRESET=0x211 \
#~ 	               GPIO_IN_PRESENCE=0xccc GPIO_IN_PULLUP=0xccc
	BROWNIE_BASE = 0x0000
endif
