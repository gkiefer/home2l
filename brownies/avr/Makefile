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


# Default Brownie ...
MCU ?= t84
BROWNIE ?= test.$(MCU)


# Determine BROWNIE_VARIANT and BROWNIE_MCU ...
#   Allowed MCU types are:
#     t85 : ATTiny85  ( 8 Pins)
#     t84 : ATTiny84  (14 Pins)
#     t861: ATTiny861 (20 Pins)
VAR_MCU := $(subst ., ,$(BROWNIE))
BROWNIE_VARIANT := $(firstword $(VAR_MCU))
BROWNIE_MCU := $(lastword $(VAR_MCU))


# Clock frequency...
BROWNIE_CPU_FREQ ?= 1000000ul
  # CPU frequency as reported to GCC and the AVR libs.
  # This must be equal to 'BR_CPU_FREQ' defined in 'interface.h'.


# Flash memory layout...
BROWNIE_BASE ?= 0xa00
  # Actual base address, for which the code is compiled and linked.
  # This should match one of the 'BR_FLASH_BASE_*' constants defined in 'interface.h':
  #     BR_FLASH_BASE_MAINTENANCE (e.g. 0x0040) : maintenance/init
  #     BR_FLASH_BASE_OPERATIONAL (e.g. 0x0a00) : operational system
  #     0x0000: testing


# Family...
include Family.mk


# Sanity...
ifndef BROWNIE_CFG
$(error Missing brownie configuration: variant '$(BROWNIE_VARIANT)' undefined?)
endif





############################## Compiler/Tool Flags #############################


# Build directory...
BUILD_DIR ?= .


# Map short MCUs to GCC names...
ifeq ($(BROWNIE_MCU),t84)
  GCC_MCU = attiny84
else ifeq ($(BROWNIE_MCU),t85)
  GCC_MCU = attiny85
else ifeq ($(BROWNIE_MCU),t861)
  GCC_MCU = attiny861
else
  $(error Undefined MCU type: $(BROWNIE_CPU))
endif


# Common definitions...
DEFS = -DF_CPU=$(BROWNIE_CPU_FREQ) -DBROWNIE_BASE=$(BROWNIE_BASE) -DBROWNIE_FWNAME=\"$(BROWNIE_VARIANT)\" $(patsubst %,-D%, $(BROWNIE_CFG))


# Configure GCC C compiler, Assembler and Linker...
CC = avr-gcc

CFLAGS = -std=gnu99 -Wall -MMD -g -Os -mmcu=$(GCC_MCU) $(DEFS) -I$(BUILD_DIR)
CFLAGS += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
# -std=gnu99: enables Embedded-C extension (e.g. '__flash')
# TBD: Int size 8 bit? -> mint8 (not recommended)

ASFLAGS = -mmcu=$(GCC_MCU) $(DEFS)

LDFLAGS = -Wl,--section-start=.text=$(BROWNIE_BASE),--section-start=.boot=0x0000


# AVRDUDE options...
AVRDUDE_MCU = $(BROWNIE_MCU)
AVRDUDE_PROGRAMMER = avrisp2





############################## Sources #########################################


# Sources
SRC = boot.S interface.c core.c twi.c gpio.c matrix.c adc.c uart.c temperature.c shades.c main.c

# For TWI testing...
#~ SRC = twi.c interface.c


OBJ := $(patsubst %.S,%.$(BROWNIE).o,$(SRC:%.c=%.$(BROWNIE).o))





############################## Building ########################################


# Default target...
.PHONY: elf
elf: $(BUILD_DIR)/$(BROWNIE).elf


# Building all brownies (recursively) ...
.PHONY: all
all: $(BUILD_DIR)/version.h $(patsubst %,$(BUILD_DIR)/%.elf,$(BROWNIE_FAMILY))


$(BUILD_DIR)/%.elf: $(BUILD_DIR)/version.h configure.h $(SRC)
	@$(MAKE) --no-print-directory BUILD_VERSION=$(BUILD_VERSION) BUILD_DIR=$(BUILD_DIR) BROWNIE=$* $@


# Building the current brownie ...
$(BUILD_DIR)/$(BROWNIE).elf: $(BUILD_DIR)/version.h $(SRC)
	@echo CC+AS+LD $(BROWNIE)
	@$(CC) -o $@ $(CFLAGS) $(ASFLAGS) $(LDFLAGS) $(SRC)

# Note: The link command needs at least the -mmcu option to create proper startup code!


#~ $(BROWNIE).elf: $(BUILD_DIR)/version.h $(OBJ)
#~ 	@echo LD $(BROWNIE)
#~ 	@$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJ)

#~ # Building $(BROWNIE) ...
#~ $(BROWNIE).hex: $(BROWNIE).elf
#~ 	@echo HEX $(BROWNIE)
#~ 	@avr-objcopy -O ihex $< $@


# Automatic dependencies ...
#~ -include $(patsubst %,$(BUILD_DIR)/%.d,$(BROWNIE_FAMILY))
-include $(BUILD_DIR)/$(BROWNIE).d





############################## Inspection targets ##############################


.PHONY: view viewhex viewdata


view: $(BROWNIE).elf
	@avr-objdump -h $< | less

viewcode: $(BROWNIE).elf
	@avr-objdump -dS $< | less

viewhex: $(BROWNIE).elf
	@avr-objdump -s $< | less

viewdata: $(BROWNIE).elf
	@avr-objdump -t $< | sort # | grep ^008 | sort





############################## Programming targets #############################

# To initialize a new brownie, enter:
#
# $ make init MCU=<mcu>
#
# where <mcu> ::= t861 | t84 | t85


.PHONY: program init check info term


# Program the current ELF file (FLASH and optionally EEPROM) ...
program: $(BROWNIE).elf
	avrdude -c $(AVRDUDE_PROGRAMMER) -p $(AVRDUDE_MCU) -U flash:w:$(BROWNIE).elf
#~ 	avrdude -c $(AVRDUDE_PROGRAMMER) -p $(AVRDUDE_MCU) -U flash:w:$(BROWNIE).elf -U eeprom:w:$(BROWNIE).elf


# Init a device (requires BROWNIE to be a maintenance/init ELF) ...
init: init.$(BROWNIE_MCU).elf
	avrdude -c $(AVRDUDE_PROGRAMMER) -p $(AVRDUDE_MCU) \
					-U hfuse:w:init.$(BROWNIE_MCU).elf \
					-U efuse:w:init.$(BROWNIE_MCU).elf \
					-U eeprom:w:init.$(BROWNIE_MCU).elf \
					-U flash:w:init.$(BROWNIE_MCU).elf


# Check avrdude and device connection ...
check:
	avrdude -c $(AVRDUDE_PROGRAMMER) -p $(AVRDUDE_MCU)


# Call avrdude to print device info ...
info:
	echo "part" | avrdude -c $(AVRDUDE_PROGRAMMER) -p $(AVRDUDE_MCU) -t


# Start avrdude in shell mode ...
term:
	avrdude -c $(AVRDUDE_PROGRAMMER) -p $(AVRDUDE_MCU) -t





############################## Common rules & targets ##########################


# Version ...
$(BUILD_DIR)/version.h:
	@echo "Updating 'version.h'..."
	@./git2version.py $(BUILD_VERSION) > $(BUILD_DIR)/version-new.h; \
	diff -q $(BUILD_DIR)/version.h $(BUILD_DIR)/version-new.h > /dev/null 2>&1 \
	  || mv $(BUILD_DIR)/version-new.h $(BUILD_DIR)/version.h; \
	rm -f $(BUILD_DIR)/version-new.h


# Automatic dependencies...
-include $(OBJ:%.o=%.d)


# Compiling...
%.$(BROWNIE).o : %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) $< -o $@

%.$(BROWNIE).o : %.S
	@echo AS $<
	@$(CC) -c $(ASFLAGS) $< -o $@





############################## Build system targets ############################


clean:
	rm -fr *.o *.d core *.elf *.hex version.h test_crc
	rm -fr $(BUILD_DIR)/*.[od] $(BUILD_DIR)/*.elf $(BUILD_DIR)/version.h

