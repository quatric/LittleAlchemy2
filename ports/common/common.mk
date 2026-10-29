# Shared by the Switch, Wii U, 3DS and desktop test builds: the game core and the portable audio.
# The includer sets TOP (repository root), BUILD (output folder) and CC; it gets back
# COMMON_OBJS and the generated data headers' folder in $(GEN).
GEN		:=	$(BUILD)/gen
COMMON_SRC	:=	$(TOP)/source/game.c $(TOP)/source/gfx.c $(TOP)/source/icons.c \
			$(TOP)/ports/common/audio_mixer.c $(TOP)/ports/common/stb_vorbis.c
COMMON_OBJS	:=	$(patsubst $(TOP)/%.c,$(BUILD)/%.o,$(COMMON_SRC))
COMMON_INC	:=	-I$(TOP)/source -I$(TOP)/ports/common -I$(GEN)

$(TOP)/source/elements.h: $(TOP)/tools/elements.txt $(TOP)/tools/gen_elements.py
	@python3 $(TOP)/tools/gen_elements.py

$(GEN)/font_ttf.h: $(TOP)/data/font.ttf $(TOP)/tools/bin2c.py
	@mkdir -p $(GEN)
	@python3 $(TOP)/tools/bin2c.py $< font_ttf $@
$(GEN)/logo_rgba.h: $(TOP)/data/logo.rgba $(TOP)/tools/bin2c.py
	@mkdir -p $(GEN)
	@python3 $(TOP)/tools/bin2c.py $< logo_rgba $@
GEN_HEADERS	:=	$(GEN)/font_ttf.h $(GEN)/logo_rgba.h $(TOP)/source/elements.h

$(BUILD)/%.o: $(TOP)/%.c $(GEN_HEADERS)
	@mkdir -p $(dir $@)
	@echo $(notdir $<)
	@$(CC) $(CFLAGS) $(COMMON_INC) -c $< -o $@
