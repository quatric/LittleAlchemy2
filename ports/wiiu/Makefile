# Wii U build (devkitPPC + wut + SDL2): make -C ports/wiiu  ->  LittleAlchemy2.wuhb (Aroma) and .rpx (Tiramisu/HBL)
# Put the icon/audio packs in sd:/wiiu/apps/LittleAlchemy2/ next to the app.
ifeq ($(strip $(DEVKITPRO)),)
$(error Set DEVKITPRO, e.g. export DEVKITPRO=/opt/devkitpro)
endif
DEVKITPPC	?=	$(DEVKITPRO)/devkitPPC
TOP	:=	$(abspath ../..)
BUILD	:=	build
CC	:=	$(DEVKITPPC)/bin/powerpc-eabi-gcc
CXX	:=	$(DEVKITPPC)/bin/powerpc-eabi-g++
WUT	:=	$(DEVKITPRO)/wut
PORTLIBS:=	$(DEVKITPRO)/portlibs/wiiu
ARCH	:=	-DESPRESSO -mcpu=750 -meabi -mhard-float
CFLAGS	:=	-O2 -ffunction-sections -Wall -Wno-misleading-indentation $(ARCH) -D__WIIU__ -D__WUT__ \
		-I$(WUT)/include -I$(PORTLIBS)/include -I$(PORTLIBS)/include/SDL2
LDFLAGS	:=	$(ARCH) -specs=$(WUT)/share/wut.specs
LIBS	:=	-L$(WUT)/lib -L$(PORTLIBS)/lib -lSDL2 -lm -lwut
.DEFAULT_GOAL := all
include $(TOP)/ports/common/common.mk

OBJS	:=	$(COMMON_OBJS) $(BUILD)/ports/sdl/main_sdl.o
TARGET	:=	LittleAlchemy2
export PATH := $(DEVKITPRO)/tools/bin:$(PATH)

all: $(TARGET).wuhb
$(BUILD)/$(TARGET).elf: $(OBJS)
	@$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
$(TARGET).rpx: $(BUILD)/$(TARGET).elf
	elf2rpl $< $@
$(TARGET).wuhb: $(TARGET).rpx icon.png
	wuhbtool $< $@ --name="Little Alchemy 2" --short-name="Little Alchemy 2" --author=quatric --icon=icon.png
clean:
	rm -rf $(BUILD) $(TARGET).rpx $(TARGET).wuhb
.PHONY: all clean
