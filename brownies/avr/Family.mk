# This file is part of the Home2L project.
#
# (C) 2015-2024 Gundolf Kiefer
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
                 win.t84 win2.t84 wins.t84 \
                 gpio4.t84 \
                 mat4x8.t861 mat1x7.t861 mat2x6.t861 \
                 mat4x8t.t861 mat1x7t.t861 mat2x6t.t861 \
                 gpio5uart.t84 \
                 gatekeeper.t861 \
                 garage.t861





#################### Device Initialization  ####################################


# init: Maintenances system, for initializing new devices ...
ifeq ($(BROWNIE_VARIANT),init)
	BROWNIE_CFG = IS_MAINTENANCE=1
	BROWNIE_BASE = 0x0040
endif





#################### Hubs ######################################################


# ahub: Primary hub, not notifying, for immediate connection to a Linux host ...
#   - used in circuit: 'hubcard'
ifeq ($(BROWNIE_VARIANT),ahub)
	BROWNIE_CFG = WITH_TWIHUB=1 TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x1 GPIO_OUT_PRESET=0x0
endif


# bhub: Intermediate hub, notifying, for connection to a Brownie TWI master ...
#   - used in circuit: 'matrix4x8'
ifeq ($(BROWNIE_VARIANT),bhub)
	BROWNIE_CFG = WITH_TWIHUB=1 TWI_SL_NOTIFY=1 GPIO_OUT_PRESENCE=0x1 GPIO_OUT_PRESET=0x0
endif





#################### Generic/GPIO ##############################################


# gpio4: (Testing) Generic GPIO node ...
ifeq ($(BROWNIE_VARIANT),gpio4)
	BROWNIE_CFG ?= GPIO_OUT_PRESENCE=0x03 GPIO_OUT_PRESET=0x02 GPIO_IN_PRESENCE=0x0c GPIO_IN_PULLUP=0x0f
endif





#################### Window and Shades/Blinds ##################################


# win: Window brownie; supports temperature sensor and single shades ...
#   - circuit: 'window'
ifeq ($(BROWNIE_VARIANT),win)
	BROWNIE_CFG ?= WITH_TEMP_ZACWIRE=1 SHADES_PORTS=1
	  # SHADES_TIMEOUT=0 TWI_SL_NOTIFY=0
endif


# win2: Dual window brownie; supports temperature sensor and two shades ...
#   - circuit: 'window_dual'
ifeq ($(BROWNIE_VARIANT),win2)
	BROWNIE_CFG ?= WITH_TEMP_ZACWIRE=1 SHADES_PORTS=2
	  # SHADES_TIMEOUT=0
endif


# wins: Safe window brownie; for single window with shades (#0) and opener (#1);
#       opener auto-closes on network failure; supports temperature sensor ...
#   - circuit: 'window_dual'
ifeq ($(BROWNIE_VARIANT),wins)
	BROWNIE_CFG ?= WITH_TEMP_ZACWIRE=1 SHADES_PORTS=2 SHADES_1_RINT_FAILSAFE=0
	  # SHADES_TIMEOUT=0
endif





#################### Matrix ####################################################


# mat1x7: Matrix brownie for up to 7 (window) sensors ...
#   - used in circuit: 'matrix1x7'
#   - columns map to GPIO0..GPIO6 (PA0..PA6 on t861)
ifeq ($(BROWNIE_VARIANT),mat1x7)
	BROWNIE_CFG ?= MATRIX_ROWS=1 MATRIX_COLS=7 MATRIX_COLS_GSHIFT=0
endif


# mat1x7t: Matrix brownie for up to 7 (window) sensors with temperature sensor ...
#   - used in circuit: 'matrix1x7'
#   - columns map to GPIO0..GPIO6 (PA0..PA6 on t861)
ifeq ($(BROWNIE_VARIANT),mat1x7t)
	BROWNIE_CFG ?= MATRIX_ROWS=1 MATRIX_COLS=7 MATRIX_COLS_GSHIFT=0 WITH_TEMP_ZACWIRE=1
endif


# mat2x6: Matrix brownie for up to 12 (window) sensors ...
#   - used in circuit: 'matrix2x6' without temperature sensor
#   - rows map to GPIO6..GPIO7 (PA6..PA7 on t861)
#   - columns map to GPIO0..GPIO5 (PA0..PA5 on t861)
ifeq ($(BROWNIE_VARIANT),mat2x6)
	BROWNIE_CFG ?= MATRIX_ROWS=2 MATRIX_COLS=6 MATRIX_COLS_GSHIFT=0 MATRIX_ROWS_GSHIFT=6
endif


# mat2x6t: Matrix brownie for up to 12 (window) sensors with temperature sensor ...
#   - used in circuit: 'matrix2x6' equipped with temperature sensor
ifeq ($(BROWNIE_VARIANT),mat2x6t)
	BROWNIE_CFG ?= MATRIX_ROWS=2 MATRIX_COLS=6 MATRIX_COLS_GSHIFT=0 MATRIX_ROWS_GSHIFT=6 WITH_TEMP_ZACWIRE=1
endif


# mat4x8: Matrix brownie for up to 32 (window) sensors ...
#   - used in circuit: 'matrix4x8' without temperature sensor
#   - rows map to GPIO8..GPIO11 (PB3..PB6 on t861)
#   - columns map to GPIO0..GPIO7 (PA0..PA7 on t861)
ifeq ($(BROWNIE_VARIANT),mat4x8)
	BROWNIE_CFG ?= MATRIX_ROWS=4 MATRIX_COLS=8
endif


# mat4x8t: Matrix brownie for up to 32 (window) sensors with temperature sensor ...
#   - used in circuit: 'matrix4x8' equipped with temperature sensor
ifeq ($(BROWNIE_VARIANT),mat4x8t)
	BROWNIE_CFG ?= MATRIX_ROWS=4 MATRIX_COLS=8 WITH_TEMP_ZACWIRE=1
endif




#################### Multi-Purpose #############################################


# gpio5uart: 5 GPIOs and UART for RS485 ...
#   - used in circuit: 'relais_rs485'
ifeq ($(BROWNIE_VARIANT),gpio5uart)
	BROWNIE_CFG ?= GPIO_OUT_PRESENCE=0x0f GPIO_OUT_PRESET=0x00 GPIO_IN_PRESENCE=0x80 GPIO_IN_PULLUP=0x00 \
	               WITH_UART=1 UART_BAUDRATE=9600
# (Debug) For UART sample timing calibration: Set GPIO7 as output ...
#~ 	BROWNIE_CFG ?= GPIO_OUT_PRESENCE=0x8f GPIO_OUT_PRESET=0x00 GPIO_IN_PRESENCE=0x00 GPIO_IN_PULLUP=0x00 \
#~ 	               WITH_UART=1 UART_BAUDRATE=9600
endif


# gatekeeper (t861): ADC for an optical mail sensor, 8 GPIs, 2 GPOs, temperature support ...
ifeq ($(BROWNIE_VARIANT),gatekeeper)
	BROWNIE_CFG ?= GPIO_IN_PRESENCE=0x0ff GPIO_IN_PULLUP=0x000 GPIO_OUT_PRESENCE=0x300 GPIO_OUT_PRESET=0x000 \
	               ADC_PORTS=1 ADC_PERIOD=1024 P_ADC_0_STROBE=P_B6 ADC_0_STROBE_TICKS=1 \
	               WITH_TEMP_ZACWIRE=1
# (Debug)
#~ 	BROWNIE_CFG ?= GPIO_IN_PRESENCE=0x03f GPIO_IN_PULLUP=0x000 GPIO_OUT_PRESENCE=0x300 GPIO_OUT_PRESET=0x000 \
#~ 	               ADC_PORTS=2 ADC_PERIOD=1024 P_ADC_0_STROBE=P_A6 ADC_0_STROBE_TICKS=128 P_ADC_1_STROBE=P_A7 ADC_1_STROBE_TICKS=256 ADC_1_STROBE_VALUE=0 \
#~ 	               WITH_TEMP_ZACWIRE=1
#~ 	BROWNIE_CFG ?= GPIO_IN_PRESENCE=0x03f GPIO_IN_PULLUP=0x000 GPIO_OUT_PRESENCE=0x300 GPIO_OUT_PRESET=0x000 \
#~ 	               ADC_PORTS=2 ADC_PERIOD=0 \
#~ 	               WITH_TEMP_ZACWIRE=1
endif


# garage (t861): General-purpose IOs with temperature support
ifeq ($(BROWNIE_VARIANT),garage)
	BROWNIE_CFG ?= GPIO_IN_PRESENCE=0xee0 GPIO_IN_PULLUP=0x000 GPIO_OUT_PRESENCE=0x00f GPIO_OUT_PRESET=0x000 \
	               WITH_TEMP_ZACWIRE=1
endif





#################### Testing ###################################################


# Testing ...
ifeq ($(BROWNIE),test.t85)
	BROWNIE_CFG ?= WITH_TWIHUB=1 TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x1 GPIO_OUT_PRESET=0x0
endif

ifeq ($(BROWNIE),test.t84)
	BROWNIE_CFG ?= TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x02 GPIO_OUT_PRESET=0x00 WITH_TEMP_ZACWIRE=0 WITH_UART=1
#~ 	BROWNIE_CFG ?= WITH_TWIHUB=0 TWI_SL_NOTIFY=0 GPIO_OUT_PRESENCE=0x00 GPIO_OUT_PRESET=0x00 WITH_TEMP_ZACWIRE=1 SHADES_PORTS=2 SHADES_TIMEOUT=0 SHADES_PERSISTENCE=0
	BROWNIE_BASE = 0x0000
endif

ifeq ($(BROWNIE),test.t861)
	BROWNIE_CFG ?= TWI_SL_NOTIFY=0 WITH_TEMP_ZACWIRE=0 MATRIX_ROWS=4 MATRIX_COLS=8
#~ 	BROWNIE_CFG ?= TWI_SL_NOTIFY=0  WITH_TEMP_ZACWIRE=0 \
#~ 	               GPIO_OUT_PRESENCE=0x333 GPIO_OUT_PRESET=0x211 \
#~ 	               GPIO_IN_PRESENCE=0xccc GPIO_IN_PULLUP=0xccc
	BROWNIE_BASE = 0x0000
endif
