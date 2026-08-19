##############################################################################
BUILD = build
BIN = indxboot

##############################################################################
.PHONY: all directory clean size $(BUILD)/version.o amalgamation

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

BOOT2 ?= 
APP_OFFSET ?= 0x4000

ifeq ($(BOOT2),)
	BOOT2_OFFSET = 
else
	BOOT2 = $(shell realpath $(BOOT2))
	BOOT2_OFFSET = $(shell ./scripts/get_run_offset $(BOOT2))
endif

ifeq ($(OS), Windows_NT)
  MKDIR = gmkdir
else
  MKDIR = mkdir
endif

CFLAGS += -W -Wall --std=gnu99 -Os -g3
CFLAGS += -Wno-address-of-packed-member
CFLAGS += -fno-diagnostics-show-caret
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -funsigned-char -funsigned-bitfields
CFLAGS += -mcpu=cortex-m0plus -mthumb
CFLAGS += -MD -MP -MT $(BUILD)/$(*F).o -MF $(BUILD)/$(@F).d

LDFLAGS += -mcpu=cortex-m0plus -mthumb -nostdlib -lgcc -lstdc++_nano
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,--script=./linker/same51j19a.ld

INCLUDES += \
  -I./include \
  -I./include/cmsis/ \
  -I.

SRCS += \
  ./bootloader.c \
  ./usb_descriptors.c

DEFINES += \
  -D__SAME51J19A__ \
  -DDONT_USE_CMSIS_INIT \
	-DAPP_OFFSET=$(APP_OFFSET) \
	-DBOOT2_OFFSET=$(BOOT2_OFFSET)

CFLAGS += $(INCLUDES) $(DEFINES)
ASFLAGS += $(CFLAGS)

OBJS = $(addprefix $(BUILD)/, $(notdir %/$(subst .c,.o, $(SRCS))))
OBJS += $(BUILD)/startup.o $(BUILD)/lib1funcs.o $(BUILD)/version.o

all: directory $(BUILD)/$(BIN).elf $(BUILD)/$(BIN).hex $(BUILD)/$(BIN).bin size

$(BUILD)/$(BIN).elf: $(OBJS)
	@echo LD $@
	@$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

$(BUILD)/$(BIN).hex: $(BUILD)/$(BIN).elf
	@echo OBJCOPY $@
	@$(OBJCOPY) -O ihex $^ $@

$(BUILD)/$(BIN).bin: $(BUILD)/$(BIN).elf
	@echo OBJCOPY $@
	@$(OBJCOPY) -O binary $^ $@
	@./scripts/crc_append --fix-crc-pointer 0x0 $@

$(BUILD)/version.o:
	@VERSION="$$(git describe --tags --dirty)"; \
		echo "const char bootloader_version[] = \"indxboot $${VERSION}\";" > $(BUILD)/version.c
	@$(CC) $(CFLAGS) $(BUILD)/version.c -c -o $@

%.o:
	@echo CC $@
	@$(CC) $(CFLAGS) $(filter %/$(subst .o,.c,$(notdir $@)), $(SRCS)) -c -o $@

$(BUILD)/startup.o: ./startup.s
	@echo CC $@
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

$(BUILD)/lib1funcs.o: ./lib1funcs.S
	@echo CC $@
	@$(CC) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

directory:
	@$(MKDIR) -p $(BUILD)

size: $(BUILD)/$(BIN).elf
	@echo size:
	@$(SIZE) -t $^

clean:
	@echo clean
	@-rm -rf $(BUILD)

ifneq ($(BOOT2),)
amalgamation: all
	@./scripts/build_amalgamation $(BUILD)/$(BIN).elf $(BOOT2) $(BUILD)/amalgamation.bin
	@./scripts/crc_append 0x0 $(BUILD)/amalgamation.bin
	@$(OBJCOPY) -O ihex -I binary $(BUILD)/amalgamation.bin $(BUILD)/amalgamation.hex
	@echo Amalgamation built at $(BUILD)/amalgamation.bin

else
amalgamation:
	$(error Can only build amalgamation when BOOT2= variable is set to a chainload target ELF file)

endif
 
-include $(wildcard $(BUILD)/*.d)

