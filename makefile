ifneq ($(lastword a b),b)
$(error This Makefile require make 3.81 or newer)
endif

# Detect whether shell style is windows or not
# https://stackoverflow.com/questions/714100/os-detecting-makefile/52062069#52062069
ifeq '$(findstring ;,$(PATH))' ';'
CMDEXE := 1
endif

# Set TOP to be the path to get from the current directory (where make was
# invoked) to the top of the tree. $(lastword $(MAKEFILE_LIST)) returns
# the name of this makefile relative to where make was invoked.

THIS_MAKEFILE := $(lastword $(MAKEFILE_LIST))
TOP := $(patsubst %makefile,%,$(THIS_MAKEFILE))

ifeq ($(CMDEXE),1)
TOP := $(subst \,/,$(shell for %%i in ( $(TOP) ) do echo %%~fi))
else
TOP := $(shell realpath $(TOP))
endif
# $(info Top directory is $(TOP))

ifeq ($(CMDEXE),1)
CURRENT_PATH := $(subst ,,$(subst \,/,$(shell echo %CD%)))
else
CURRENT_PATH := $(shell realpath --relative-to=$(TOP) `pwd`)
endif
# $(info Path from top is $(CURRENT_PATH))

# ---------------------------------------
# Common make definition for all examples
# ---------------------------------------

# Build directory
ifeq ($(CMDEXE),1)
$(shell if exist build\$(BOARD) rd build\$(BOARD) /s /q)
endif

BUILD := build/$(BOARD)

PROJECT := $(notdir $(CURDIR))
BIN := _bin/$(BOARD)/$(notdir $(CURDIR))


# Handy check parameter function
check_defined = \
    $(strip $(foreach 1,$1, \
    $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
    $(error Undefined make flag: $1$(if $2, ($2))))

#-------------- Select the board to build for. ------------    

# Board without family
ifneq ($(wildcard devices/$(BOARD)/device.mk),)
BOARD_PATH := devices/$(BOARD)
FAMILY :=
endif

# Board within family
ifeq ($(BOARD_PATH),)
  BOARD_PATH := $(subst ,,$(wildcard devices/*/$(BOARD)))
  FAMILY := $(word 3, $(subst /, ,$(BOARD_PATH)))
  FAMILY_PATH = device/$(FAMILY)
endif

ifeq ($(BOARD_PATH),)
  $(info You must provide a BOARD parameter with 'BOARD=')
  $(error Invalid BOARD specified)
endif

ifeq ($(FAMILY),)
  include devices/$(BOARD)/device.mk
else
  # Include Family and Board specific defs
  include $(FAMILY_PATH)/family.mk

  SRC_C += $(subst ,,$(wildcard $(FAMILY_PATH)/*.c))
  SRC_CPP += $(subst ,,$(wildcard $(FAMILY_PATH)/*.cpp))
endif

# Fetch submodules depended by family
fetch_submodule_if_empty = $(if $(wildcard $1/*),,$(info $(shell git -C $(TOP) submodule update --init $1)))
ifdef DEPS_SUBMODULES
  $(foreach s,$(DEPS_SUBMODULES),$(call fetch_submodule_if_empty,$(s)))
endif

#-------------- Cross Compiler  ------------
# Can be set by board, default to ARM GCC
CROSS_COMPILE ?= arm-none-eabi-

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
GDB = $(CROSS_COMPILE)gdb
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE = $(CROSS_COMPILE)size
MKDIR = mkdir

ifeq ($(CMDEXE),1)
  CP = copy
  RM = del
else
  SED = sed
  CP = cp
  RM = rm
endif

#-------------- Source files and compiler flags --------------

# Include all source C in family & board folder
SRC_C += $(subst ,,$(wildcard $(BOARD_PATH)/*.c))
SRC_CPP += $(subst ,,$(wildcard $(BOARD_PATH)/*.cpp))

INC   += $(FAMILY_PATH)

# Compiler Flags
CFLAGS += \
	-std=c++17	
#   -ggdb \
#   -fdata-sections \
#   -ffunction-sections \
#   -fsingle-precision-constant \
#   -fno-strict-aliasing \
#   -Wdouble-promotion \
#   -Wstrict-prototypes \
#   -Wstrict-overflow \
#   -Wall \
#   -Wextra \
#   -Werror \
#   -Wfatal-errors \
#   -Werror-implicit-function-declaration \
#   -Wfloat-equal \
#   -Wundef \
#   -Wshadow \
#   -Wwrite-strings \
#   -Wsign-compare \
#   -Wmissing-format-attribute \
#   -Wunreachable-code \
#   -Wcast-align \
#   -Wcast-function-type

# Debugging/Optimization
ifeq ($(DEBUG), 1)
  CFLAGS += -Og
else
  CFLAGS += -Os
endif

# # Log level is mapped to TUSB DEBUG option
# ifneq ($(LOG),)
#   CMAKE_DEFSYM +=	-DLOG=$(LOG)
#   CFLAGS += -DCFG_TUSB_DEBUG=$(LOG)
# endif

# # Logger: default is uart, can be set to rtt or swo
# ifneq ($(LOGGER),)
# 	CMAKE_DEFSYM +=	-DLOGGER=$(LOGGER)
# endif

# ifeq ($(LOGGER),rtt)
#   CFLAGS += -DLOGGER_RTT -DSEGGER_RTT_MODE_DEFAULT=SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
#   RTT_SRC = lib/SEGGER_RTT
#   INC   += $(RTT_SRC)/RTT
#   SRC_C += $(RTT_SRC)/RTT/SEGGER_RTT.c
# else ifeq ($(LOGGER),swo)
#   CFLAGS += -DLOGGER_SWO
# endif

# Code source
SRC_C += $(wildcard src/*.c src/*/*.c src/*/*/*.c src/*/*/*/*.c src/*/*/*/*/*.c) #Lazy solution for recursive find. I gived up tring to find something cleaner
SRC_CPP += $(wildcard src/*.cpp src/*/*.cpp src/*/*/*.cpp src/*/*/*/*.cpp src/*/*/*/*/*.cpp) #Same as above

# Code Include
INC += src/

# ---------------------------------------
# Common make rules for all examples
# ---------------------------------------

# Set all as default goal
.DEFAULT_GOAL := all

# ESP32-SX and RP2040 has its own CMake build system
ifneq ($(FAMILY),esp32s2)
ifneq ($(FAMILY),esp32s3)
ifneq ($(FAMILY),rp2040)
# ---------------------------------------
# GNU Make build system
# ---------------------------------------

# libc
LIBS += -lgcc -lm -lnosys

ifneq ($(BOARD), spresense)
LIBS += -lc
endif

CFLAGS += $(addprefix -I,$(INC))

LDFLAGS += $(CFLAGS) -Wl,-T,$(LD_FILE) -Wl,-Map=$@.map -Wl,-cref -Wl,-gc-sections
ifneq ($(SKIP_NANOLIB), 1)
LDFLAGS += -specs=nosys.specs -specs=nano.specs
endif

ASFLAGS += $(CFLAGS)

# Assembly files can be name with upper case .S, convert it to .s
SRC_S := $(SRC_S:.S=.s)

# Due to GCC LTO bug https://bugs.launchpad.net/gcc-arm-embedded/+bug/1747966
# assembly file should be placed first in linking order
# '_asm' suffix is added to object of assembly file

# $(info $(SRC_C))

OBJ += $(addprefix $(BUILD)/obj/, $(SRC_S:.s=_asm.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/obj/, $(SRC_CPP:.cpp=.o))

$(info $(OBJ))



# Verbose mode
ifeq ("$(V)","1")
$(info CFLAGS  $(CFLAGS) ) $(info )
$(info LDFLAGS $(LDFLAGS)) $(info )
$(info ASFLAGS $(ASFLAGS)) $(info )
endif

all: $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex size

uf2: $(BUILD)/$(PROJECT).uf2

OBJ_DIRS = $(sort $(dir $(OBJ)))
$(OBJ): | $(OBJ_DIRS)
$(OBJ_DIRS):
ifeq ($(CMDEXE),1)
	@$(MKDIR) $(subst /,\,$@)
else
	@$(MKDIR) -p $@
endif

$(BUILD)/$(PROJECT).elf: $(OBJ)
	@echo LINK $@
	@$(CC) -o $@ $(LDFLAGS) $^ -Wl,--start-group $(LIBS) -Wl,--end-group

$(BUILD)/$(PROJECT).bin: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O binary $^ $@

$(BUILD)/$(PROJECT).hex: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O ihex $^ $@

# UF2 generation, iMXRT need to strip to text only before conversion
ifeq ($(FAMILY),imxrt)
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).elf
	@echo CREATE $@
	@$(OBJCOPY) -O ihex -R .flash_config -R .ivt $^ $(BUILD)/$(PROJECT)-textonly.hex
	$(PYTHON) tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -c -o $@ $(BUILD)/$(PROJECT)-textonly.hex
else
$(BUILD)/$(PROJECT).uf2: $(BUILD)/$(PROJECT).hex
	@echo CREATE $@
	$(PYTHON) tools/uf2/utils/uf2conv.py -f $(UF2_FAMILY_ID) -c -o $@ $^
endif

copy-artifact: $(BUILD)/$(PROJECT).bin $(BUILD)/$(PROJECT).hex $(BUILD)/$(PROJECT).uf2

# We set vpath to point to the top of the tree so that the source files
# can be located. By following this scheme, it allows a single build rule
# to be used to compile all .c files.
vpath %.c . $(TOP)
$(BUILD)/obj/%.o: %.c
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -MD -o $@ $<

# ASM sources .cpp
vpath %.cpp . $(TOP)
$(BUILD)/obj/%.o: %.cpp
	@echo CC $(notdir $@)
	@$(CC) $(CFLAGS) -c -MD -o $@ $<

# ASM sources lower case .s
vpath %.s . $(TOP)
$(BUILD)/obj/%_asm.o: %.s
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

# ASM sources upper case .S
vpath %.S . $(TOP)
$(BUILD)/obj/%_asm.o: %.S
	@echo AS $(notdir $@)
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

size: $(BUILD)/$(PROJECT).elf
	-@echo ''
	@$(SIZE) $<
	-@echo ''

.PHONY: clean
clean:
ifeq ($(CMDEXE),1)
	rd /S /Q $(subst /,\,$(BUILD))
else
	$(RM) -rf $(BUILD)
endif

endif
endif
endif # GNU Make

# ---------------------------------------
# Flash Targets
# ---------------------------------------

# Flash binary using Jlink
ifeq ($(OS),Windows_NT)
  JLINKEXE = JLink.exe
else
  JLINKEXE = JLinkExe
endif

JLINK_IF ?= swd

# Flash using jlink
flash-jlink: $(BUILD)/$(PROJECT).hex
	@echo halt > $(BUILD)/$(BOARD).jlink
	@echo r > $(BUILD)/$(BOARD).jlink
	@echo loadfile $^ >> $(BUILD)/$(BOARD).jlink
	@echo r >> $(BUILD)/$(BOARD).jlink
	@echo go >> $(BUILD)/$(BOARD).jlink
	@echo exit >> $(BUILD)/$(BOARD).jlink
	$(JLINKEXE) -device $(JLINK_DEVICE) -if $(JLINK_IF) -JTAGConf -1,-1 -speed auto -CommandFile $(BUILD)/$(BOARD).jlink

# flash STM32 MCU using stlink with STM32 Cube Programmer CLI
flash-stlink: $(BUILD)/$(PROJECT).elf
	STM32_Programmer_CLI --connect port=swd --write $< --go

# flash with pyocd
flash-pyocd: $(BUILD)/$(PROJECT).hex
	pyocd flash -t $(PYOCD_TARGET) $<
	pyocd reset -t $(PYOCD_TARGET)

# flash with Black Magic Probe

# This symlink is created by https://github.com/blacksphere/blackmagic/blob/master/driver/99-blackmagic.rules
BMP ?= /dev/ttyBmpGdb

flash-bmp: $(BUILD)/$(PROJECT).elf
	$(GDB) --batch -ex 'target extended-remote $(BMP)' -ex 'monitor swdp_scan' -ex 'attach 1' -ex load  $<

debug-bmp: $(BUILD)/$(PROJECT).elf
	$(GDB) -ex 'target extended-remote $(BMP)' -ex 'monitor swdp_scan' -ex 'attach 1' $<

#-------------- Artifacts --------------

# Create binary directory
$(BIN):
	@$(MKDIR) -p $@

# Copy binaries .elf, .bin, .hex, .uf2 to BIN for upload
# due to large size of combined artifacts, only uf2 is uploaded for now
copy-artifact: $(BIN)
	@$(CP) $(BUILD)/$(PROJECT).uf2 $(BIN)
	#@$(CP) $(BUILD)/$(PROJECT).bin $(BIN)
	#@$(CP) $(BUILD)/$(PROJECT).hex $(BIN)
	#@$(CP) $(BUILD)/$(PROJECT).elf $(BIN)

# Print out the value of a make variable.
# https://stackoverflow.com/questions/16467718/how-to-print-out-a-variable-in-makefile
print-%:
	@echo $* = $($*)