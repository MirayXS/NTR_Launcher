#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
.SECONDARY:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/ds_rules

export TARGET		:=	NTR_Launcher
export TOPDIR		:=	$(CURDIR)

export VERSION_MAJOR	:= 3
export VERSION_MINOR	:= 2
export VERSTRING	:=	$(VERSION_MAJOR).$(VERSION_MINOR)

# specify a directory which contains the nitro filesystem
# this is relative to the Makefile
NITRO_FILES := CartFiles

.PHONY: bootloader ndsbootloader clean arm7/$(TARGET).elf arm9/$(TARGET).elf

#---------------------------------------------------------------------------------
# main targets
#			-g KKGP 01 "NTR Launcher" -z 80040000 -u 00030004 -a 00000138 -p 0001 \
#---------------------------------------------------------------------------------
all: bootloader ndsbootloader $(TARGET).nds

dist:	all
	@mkdir -p debug
	@cp $(TARGET).arm7.elf debug/$(TARGET).arm7.elf
	@cp $(TARGET).arm9.elf debug/$(TARGET).arm9.elf

$(TARGET).nds:	$(TARGET).arm7 $(TARGET).arm9
	ndstool	-c $(TARGET).nds -7 $(TARGET).arm7.elf -9 $(TARGET).arm9.elf \
			-b $(CURDIR)/icon.bmp "NTR Launcher v$(VERSTRING);Slot-1 Launcher;Apache Thunder & RocketRobz" \
			-g KKGP 01 "NTR Launcher" -z 80040407 -u 00030004 -a 00000138 -p 0001 \
			-d $(NITRO_FILES)
	@cp $(TARGET).nds 00000000.app

$(TARGET).arm7	: arm7/$(TARGET).elf
	cp arm7/$(TARGET).elf $(TARGET).arm7.elf
$(TARGET).arm9	: arm9/$(TARGET).elf
	cp arm9/$(TARGET).elf $(TARGET).arm9.elf

#---------------------------------------------------------------------------------
arm7/$(TARGET).elf:
	$(MAKE) -C arm7
	
#---------------------------------------------------------------------------------
arm9/$(TARGET).elf:
	$(MAKE) -C arm9

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr debug
	@rm -fr data
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).nds.orig.nds
	@rm -fr $(TARGET).arm7
	@rm -fr $(TARGET).arm9
	@rm -fr $(TARGET).arm7.elf
	@rm -fr $(TARGET).arm9.elf
	@rm -fr 00000000.app
	@rm -fr $(TARGET).cia
	@$(MAKE) -C bootloader clean
	@$(MAKE) -C ndsbootloader clean
	@$(MAKE) -C arm9 clean
	@$(MAKE) -C arm7 clean

data:
	@mkdir -p data

bootloader: data
	@$(MAKE) -C bootloader
	
ndsbootloader: data
	@$(MAKE) -C ndsbootloader

