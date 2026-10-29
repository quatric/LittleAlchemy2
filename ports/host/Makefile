# Desktop build of the SDL2 port, for trying the game without a console:
#   make -C ports/host run     (reads icons.bin and audio/ from apps/LittleAlchemy2, or $LA2_DIR)
TOP	:=	$(abspath ../..)
BUILD	:=	build
CC	:=	cc
CFLAGS	:=	-O2 -Wall -Wno-misleading-indentation $(shell sdl2-config --cflags)
LIBS	:=	$(shell sdl2-config --libs) -lm
.DEFAULT_GOAL := all
include $(TOP)/ports/common/common.mk

OBJS	:=	$(COMMON_OBJS) $(BUILD)/ports/sdl/main_sdl.o
TARGET	:=	LittleAlchemy2-host

all: $(TARGET)
$(TARGET): $(OBJS)
	@$(CC) $(OBJS) $(LIBS) -o $@
run: $(TARGET)
	LA2_DIR=$(TOP)/apps/LittleAlchemy2 ./$(TARGET)
clean:
	rm -rf $(BUILD) $(TARGET)
.PHONY: all run clean
