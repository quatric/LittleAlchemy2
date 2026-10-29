# Nintendo 3DS build (devkitARM + libctru): make -C ports/3ds  ->  LittleAlchemy2.3dsx
# Put the .3dsx and the icon/audio packs in sdmc:/3ds/LittleAlchemy2/.
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO, e.g. export DEVKITPRO=/opt/devkitpro)
endif
TOP	:=	$(abspath ../..)
BUILD	:=	build
CC	:=	$(DEVKITPRO)/devkitARM/bin/arm-none-eabi-gcc
CTRU	:=	$(DEVKITPRO)/libctru
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS	:=	-O2 -ffunction-sections -Wall -Wno-misleading-indentation $(ARCH) -D__3DS__ -I$(CTRU)/include
LDFLAGS	:=	-specs=3dsx.specs $(ARCH) -Wl,-Map,$(BUILD)/map.txt
LIBS	:=	-L$(CTRU)/lib -lctru -lm
.DEFAULT_GOAL := all
include $(TOP)/ports/common/common.mk

OBJS	:=	$(COMMON_OBJS) $(BUILD)/ports/3ds/main_3ds.o
TARGET	:=	LittleAlchemy2
VERSION	:=	$(shell sed -n 's:.*<version>\(.*\)</version>.*:\1:p' $(TOP)/apps/LittleAlchemy2/meta.xml)
export PATH := $(DEVKITPRO)/tools/bin:$(PATH)

all: $(TARGET).3dsx
$(BUILD)/$(TARGET).elf: $(OBJS)
	@$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
$(BUILD)/$(TARGET).smdh: icon.png
	smdhtool --create "Little Alchemy 2" "Combine elements, discover the world" "quatric" icon.png $@
$(TARGET).3dsx: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).smdh
	3dsxtool $< $@ --smdh=$(BUILD)/$(TARGET).smdh
clean:
	rm -rf $(BUILD) $(TARGET).3dsx
.PHONY: all clean
