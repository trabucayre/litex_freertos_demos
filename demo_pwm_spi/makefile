# *****************************************************************************
# LiteX FreeRTOS Makefile
# *****************************************************************************

BUILD_DIR ?= ../build/olimex_gatemate_a1_evb

include $(BUILD_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

# User's application sources (*.c, *.cpp, *.s, *.S); add additional files here
APP_SRC = $(wildcard ./*.c) $(wildcard ./*.s) $(wildcard ./*.cpp) $(wildcard ./*.S)

# User's application include folders (don't forget the '-I' before each entry)
APP_INC = -I .

# User's application include folders - for assembly files only (don't forget the '-I' before each entry)
ASM_INC = -I .

# User flags for additional configuration (will be added to compiler flags)
USER_FLAGS =

# -----------------------------------------------------------------------------
# FreeRTOS
# -----------------------------------------------------------------------------

# FreeRTOS home folder
FREERTOS_HOME ?= ../FreeRTOS-Kernel

# Kernel
APP_SRC += $(wildcard $(FREERTOS_HOME)/*.c)
APP_INC += -I $(FREERTOS_HOME)/include

# RISC-V specifics
APP_SRC += $(wildcard  $(FREERTOS_HOME)/portable/GCC/RISC-V/*.c)
APP_SRC += $(FREERTOS_HOME)/portable/GCC/RISC-V/portASM.S
APP_INC += -I $(FREERTOS_HOME)/portable/GCC/RISC-V

# Heap management
APP_SRC += $(wildcard  $(FREERTOS_HOME)/portable/MemMang/heap_4.c)

# -----------------------------------------------------------------------------
# LiteX SoC
# -----------------------------------------------------------------------------

APP_INC += -I$(FREERTOS_HOME)/portable/GCC/RISC-V/chip_specific_extensions/RISCV_no_extensions

OBJECTS = crt0.o $(addsuffix .o,$(basename $(notdir $(APP_SRC))))
VPATH = $(sort $(dir $(APP_SRC))) $(BIOS_DIRECTORY) $(BIOS_DIRECTORY)/cmds $(CPU_DIRECTORY)

COMMA = ,
REGULAR_LIBS = $(filter-out $(ALWAYS_LINK_LIBS),$(LIBS))
ALWAYS_LINK_LIBFLAGS = $(if $(ALWAYS_LINK_LIBS),-Wl$(COMMA)--whole-archive $(ALWAYS_LINK_LIBS:lib%=-l%) -Wl$(COMMA)--no-whole-archive)

APP_ELF ?= demo.elf
APP_BIN ?= demo.bin
APP_ASM ?= demo.asm

all: $(APP_BIN)

$(APP_BIN): $(APP_ELF)
	$(OBJCOPY) -O binary $< $@
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

$(APP_ASM): $(APP_ELF)
	$(OBJDUMP) -d -S -z $< > $@

$(APP_ELF): $(OBJECTS)
	$(CC) $(LDFLAGS) -T linker.ld -N -o $@ \
		$(OBJECTS) \
		$(PACKAGES:%=-L$(BUILD_DIR)/software/%) \
		-Wl,--start-group \
		$(REGULAR_LIBS:lib%=-l%) \
		$(ALWAYS_LINK_LIBFLAGS) \
		-Wl,--end-group \
		-Wl,--gc-sections \
		-Wl,-Map,$@.map
ifneq ($(OS),Windows_NT)
	chmod -x $@
endif

-include $(OBJECTS:.o=.d)

%.o: %.c
	$(compile) $(APP_INC)

%.o: %.S
	$(assemble) $(APP_INC)

clean:
	$(RM) $(OBJECTS) $(OBJECTS:.o=.d) $(APP_ELF) $(APP_ELF).map $(APP_BIN) $(APP_ASM) .*~ *~

.PHONY: all clean
