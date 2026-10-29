# Nintendo Switch build (devkitA64 + libnx + SDL2): make -C ports/switch  ->  LittleAlchemy2.nro
# Put the .nro and the icon/audio packs in sdmc:/switch/LittleAlchemy2/.
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO, e.g. export DEVKITPRO=/opt/devkitpro)
endif
TOP	:=	$(abspath ../..)
BUILD	:=	build
CC	:=	$(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-gcc
CXX	:=	$(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++
PORTLIBS:=	$(DEVKITPRO)/portlibs/switch
LIBNX	:=	$(DEVKITPRO)/libnx
ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS	:=	-O2 -ffunction-sections -Wall -Wno-misleading-indentation $(ARCH) -D__SWITCH__ \
		-I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2 -I$(LIBNX)/include
LDFLAGS	:=	-specs=$(LIBNX)/switch.specs $(ARCH) -Wl,-Map,$(BUILD)/map.txt
LIBS	:=	-L$(PORTLIBS)/lib -L$(LIBNX)/lib -lSDL2 -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx -lm
.DEFAULT_GOAL := all
include $(TOP)/ports/common/common.mk

OBJS	:=	$(COMMON_OBJS) $(BUILD)/ports/sdl/main_sdl.o
TARGET	:=	LittleAlchemy2
VERSION	:=	$(shell sed -n 's:.*<version>\(.*\)</version>.*:\1:p' $(TOP)/apps/LittleAlchemy2/meta.xml)
export PATH := $(DEVKITPRO)/tools/bin:$(PATH)

all: $(TARGET).nro
$(BUILD)/$(TARGET).elf: $(OBJS)
	@$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
$(BUILD)/$(TARGET).nacp:
	@mkdir -p $(BUILD)
	nacptool --create "Little Alchemy 2" "quatric" "$(VERSION)" $@
$(TARGET).nro: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).nacp
	elf2nro $< $@ --icon=$(TOP)/ports/switch/icon.jpg --nacp=$(BUILD)/$(TARGET).nacp
clean:
	rm -rf $(BUILD) $(TARGET).nro
.PHONY: all clean
