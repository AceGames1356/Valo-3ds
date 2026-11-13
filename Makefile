#---------------------------------------------------------------------------------
# Valorant 3DS - devkitARM / libctru build (standard devkitPro 3DS template layout)
#   make          -> valorant3ds.3dsx (+ .smdh)         runs on real 3DS via Homebrew Launcher, Citra/Azahar
#   make 3ds      -> valorant3ds.3ds  (needs makerom + bannertool, see README)
#   make cia      -> valorant3ds.cia  (needs makerom + bannertool, see README)
#   make test     -> host-side tests (gcc only, no devkitPro needed)
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := valorant3ds
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    := source

APP_TITLE       := Valorant 3DS
APP_DESCRIPTION := Offline 5v5 tactical shooter
APP_AUTHOR      := Valorant3DS project
ICON            := assets/icon.png

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

# which C-Stick API does the installed libctru provide?
CSTICK_DEF := $(shell sh $(TOPDIR)/tools/probe_cstick.sh 2>/dev/null)

CFLAGS  := -g -Wall -O2 -mword-relocations -fomit-frame-pointer -ffunction-sections $(ARCH)
CFLAGS  += $(INCLUDE) -D__3DS__ -DPLATFORM_3DS $(CSTICK_DEF) -std=gnu99
CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11
ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS    := -lctru -lm
LIBDIRS := $(CTRULIB)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)
export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES  := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))

export LD := $(CC)
export OFILES_SOURCES := $(CFILES:.c=.o)
export OFILES := $(OFILES_SOURCES)
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export APP_ICON := $(TOPDIR)/$(ICON)
export _3DSXDEPS := $(OUTPUT).smdh
export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: $(BUILD) all clean 3ds cia test

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

3ds cia: all
	@sh tools/package.sh $@ $(TARGET)

test:
	@sh tools/run_tests.sh

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).3ds $(TARGET).cia

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf  : $(OFILES)

$(OUTPUT).smdh : $(APP_ICON) $(MAKEFILE_LIST)
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@
	@echo built ... $(notdir $@)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
