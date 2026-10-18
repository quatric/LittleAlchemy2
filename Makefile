#---------------------------------------------------------------------------------
# Little Alchemy 2 for Wii (devkitPPC + libogc)
#---------------------------------------------------------------------------------
.SUFFIXES:
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules

TARGET		:=	LittleAlchemy2
BUILD		:=	build
SOURCES		:=	source libinput-wii/source
DATA		:=	data

CFLAGS	= -O2 -Wall -Wno-misleading-indentation $(MACHDEP) $(INCLUDE)
# Version for the update check: the release tag when CI builds one, else meta.xml
APP_VERSION	?=	$(if $(filter refs/tags/%,$(GITHUB_REF)),$(GITHUB_REF_NAME),$(shell sed -n 's:.*<version>\(.*\)</version>.*:\1:p' $(dir $(abspath $(firstword $(MAKEFILE_LIST))))apps/LittleAlchemy2/meta.xml))
CFLAGS	+=	-DAPP_VERSION=\"$(APP_VERSION)\"
LDFLAGS	=	$(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS	:=	-lfat -lvorbisidec -logg -lwiikeyboard -lwiiuse -lbte -lasnd -logc -lm
LIBS	:=	-lwiiupdate $(LIBS)
TOPDIR	:=	$(patsubst %/,%,$(dir $(abspath $(firstword $(MAKEFILE_LIST)))))
# portlibs/ppc: optional controller libraries; $(PORTLIBS): devkitPro's ppc-libvorbisidec
LIBDIRS	:=	$(TOPDIR)/portlibs/ppc $(PORTLIBS) $(TOPDIR)/libinput-wii
LIBDIRS	+=	$(TOPDIR)/libwiiupdate

# Wii U GamePad, Wii U Pro Controller and DualShock 3 support, on by default once
# scripts/build_controller_portlibs.sh has built their libraries into portlibs/
HAVE_PORTLIBS	:=	$(if $(wildcard $(TOPDIR)/portlibs/ppc/lib/libwupc.a),1,0)
WITH_WIIDRC	?=	$(HAVE_PORTLIBS)
WITH_WUPC	?=	$(HAVE_PORTLIBS)
WITH_SICKSAXIS	?=	$(HAVE_PORTLIBS)
ifeq ($(WITH_WIIDRC),1)
 CFLAGS += -DHAVE_WIIDRC
 LIBS := -lwiidrc $(LIBS)
endif
ifeq ($(WITH_WUPC),1)
 CFLAGS += -DHAVE_WUPC
 LDFLAGS += -Wl,-wrap,wiiuse_accept
 LIBS := -lwupc $(LIBS)
endif
ifeq ($(WITH_SICKSAXIS),1)
 CFLAGS += -DHAVE_SICKSAXIS
 LIBS := -lsicksaxis $(LIBS)
endif

# The .dol that goes into the Homebrew Channel package.
ifeq ($(NO_DOLXZ),1)
BOOT_DOL	=	$(TARGET).dol
else
BOOT_DOL	=	$(TARGET)-xz.dol
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
export LD	:=	$(CC)
export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE	:=	$(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS	:= -L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean elements package run

$(BUILD): $(SOURCES)/elements.h
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD): libwiiupdate-lib
.PHONY: libwiiupdate-lib
libwiiupdate-lib:
	@$(MAKE) --no-print-directory -C $(TOPDIR)/libwiiupdate

$(SOURCES)/elements.h: tools/elements.txt tools/gen_elements.py
	@python3 tools/gen_elements.py

package: $(BUILD)
	@cp $(BOOT_DOL) apps/LittleAlchemy2/boot.dol
	@echo packaged apps/LittleAlchemy2/boot.dol

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol $(OUTPUT)-xz.dol

run:
	wiiload $(TARGET).dol

else

DEPENDS	:=	$(OFILES:.o=.d)
#---------------------------------------------------------------------------------
# Self-extracting xz-compressed copy of the .dol (FIX94's dolxz, see
# scripts/dolxz.sh). It is the default target; NO_DOLXZ=1 skips it.
#---------------------------------------------------------------------------------
ifneq ($(NO_DOLXZ),1)
$(OUTPUT)-xz.dol: $(OUTPUT).dol
	@echo compressing ... $(notdir $@)
	@sh $(dir $(OUTPUT))scripts/dolxz.sh $< $@
endif

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SOURCES) : $(HFILES)

%.ttf.o	%_ttf.h :	%.ttf
	@echo $(notdir $<)
	$(bin2o)

# data/logo.rgba: u16 width, u16 height (big-endian), then RGBA pixels
%.rgba.o	%_rgba.h :	%.rgba
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)
endif
