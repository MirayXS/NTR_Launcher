/*
 main.arm7.c

 By Michael Chisholm (Chishm)

 All resetMemory and startBinary functions are based
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

 License:
    NitroHax -- Cheat tool for the Nintendo DS
    Copyright (C) 2008  Michael "Chishm" Chisholm

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef ARM7
# define ARM7
#endif
#include <nds/ndstypes.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#include <nds/dma.h>
#include <nds/arm7/audio.h>
#include <nds/arm7/codec.h>
#include <nds/memory.h>
#include <nds/ipc.h>
#include <string.h>

#ifndef NULL
#define NULL 0
#endif


#include "common.h"
#include "../../arm9/common/launcherData.h"

#include "read_card.h"
#include "tonccpy.h"

#include "crypto.h"
#include "f_xy.h"
#include "dsi.h"
#include "u128_math.h"

/*-------------------------------------------------------------------------
External functions
--------------------------------------------------------------------------*/
extern void arm7_clearmem (void* loc, size_t len);
extern void arm7_reset (void);

volatile u16 useTwlCfg = 0;
volatile int twlCfgLang = 0;
// volatile bool useShortInit = false;

// ALIGN(4) char modcrypt_shared_key[8] = "Nintendo";

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define NDS_HEADER			0x02FFFE00
#define NDS_HEADER_POKEMON	0x02FFF000
#define TWL_HEADER			0x02FFE000
#define TMP_HEADER			0x02FFC000
tNDSHeader* ndsHeader;
tTWLHeader* twlHeader;

#define REG_GPIO_WIFI *(vu16*)0x4004C04

// ALIGN(4) u32 chipID = 0;

// static void memset_addrs_arm7(u32 start, u32 end) { toncset((u32*)start, 0, ((int)end - (int)start)); }

static tNDSHeader* loadHeader(tDSiHeader* twlHeaderTemp) {
	tNDSHeader* ntrHeader = (tNDSHeader*)NDS_HEADER;
	*ntrHeader = twlHeaderTemp->ndshdr;
	if (ntrHeader->unitCode > 0) {
		tDSiHeader* dsiHeader = (tDSiHeader*)TWL_HEADER; // __DSiHeader
		*dsiHeader = *twlHeaderTemp;
	}
	return ntrHeader;
}

static void decrypt_modcrypt_area(dsi_context* ctx, u8 *buffer, unsigned int size) {
	uint32_t len = size / 0x10;
	u8 block[0x10];

	while (len>0) {
		toncset(block, 0, 0x10);
		dsi_crypt_ctr_block(ctx, buffer, block);
		tonccpy(buffer, block, 0x10);
		buffer+=0x10;
		len--;
	}
}

static void NDSTouchscreenMode(void) {

	u16 specialSetting = 0;
	u8 volLevel;
	
	static const char list[][4] = {
		"ABX",	// NTR-ABXE Bomberman Land Touch!
		"YO9",	// NTR-YO9J Bokura no TV Game Kentei - Pikotto! Udedameshi
		"ALH",	// NTR-ALHE Flushed Away
		"ACC",	// NTR-ACCE Cooking Mama
		"YCQ",	// NTR-YCQE Cooking Mama 2 - Dinner with Friends
		"YYK",	// NTR-YYKE Trauma Center - Under the Knife 2
		"AZW",	// NTR-AZWE WarioWare - Touched!
		"AKA",	// NTR-AKAE Rub Rabbits!, The
		"AN9",	// NTR-AN9E Little Mermaid - Ariel's Undersea Adventure, The
		"AKE",	// NTR-AKEJ Keroro Gunsou - Enshuu da Yo! Zenin Shuugou Part 2
		"YFS",	// NTR-YFSJ Frogman Show - DS Datte, Shouganaijanai, The
		"YG8",	// NTR-YG8E Yu-Gi-Oh! World Championship 2008
		"AY7",	// NTR-AY7E Yu-Gi-Oh! World Championship 2007
		"YON",	// NTR-YONJ Minna no DS Seminar - Kantan Ongakuryoku
		"A5H",	// NTR-A5HE Interactive Storybook DS - Series 2
		"A5I",	// NTR-A5IE Interactive Storybook DS - Series 3
		"AMH",	// NTR-AMHE Metroid Prime Hunters
		"A3T",	// NTR-A3TE Tak - The Great Juju Challenge
		"YBO",	// NTR-YBOE Boogie
		"ADA",	// NTR-ADAE PKMN Diamond
		"APA",	// NTR-APAE PKMN Pearl
		"CPU",	// NTR-CPUE PKMN Platinum
		"APY",	// NTR-APYE Puyo Pop Fever
		"AWH",	// NTR-AWHE Bubble Bobble Double Shot
		"AXB",	// NTR-AXBJ Daigassou! Band Brothers DX
		"A4U",	// NTR-A4UJ Wi-Fi Taiou - Morita Shogi
		"A8N",	// NTR-A8NE Planet Puzzle League
		"ABJ",	// NTR-ABJE Harvest Moon DS - Island of Happiness
		"ABN",	// NTR-ABNE Bomberman Story DS
		"ACL",	// NTR-ACLE Custom Robo Arena
		"ART",	// NTR-ARTJ Shin Lucky Star Moe Drill - Tabidachi
		"AVT",	// NTR-AVTJ Kou Rate Ura Mahjong Retsuden Mukoubuchi - Goburei, Shuuryou desu ne
		"AWY",	// NTR-AWYJ Wi-Fi Taiou - Gensen Table Game DS
		"AXJ",	// NTR-AXJE Dungeon Explorer - Warriors of Ancient Arts
		"AYK",	// NTR-AYKJ Wi-Fi Taiou - Yakuman DS
		"YB2",	// NTR-YB2E Bomberman Land Touch! 2
		"YB3",	// NTR-YB3E Harvest Moon DS - Sunshine Islands
		"YCH",	// NTR-YCHJ Kousoku Card Battle - Card Hero
		"YFE",	// NTR-YFEE Fire Emblem - Shadow Dragon
		"YGD",	// NTR-YGDE Diary Girl
		"YKR",	// NTR-YKRJ Culdcept DS
		"YRM",	// NTR-YRME My Secret World by Imagine
		"YW2",	// NTR-YW2E Advance Wars - Days of Ruin
		"AJU",	// NTR-AJUJ Jump! Ultimate Stars
		"ACZ",	// NTR-ACZE Cars
		"AHD",	// NTR-AHDE Jam Sessions
		"ANR",	// NTR-ANRE Naruto - Saikyou Ninja Daikesshu 3
		"YT3",	// NTR-YT3E Tamagotchi Connection - Corner Shop 3
		"AVI",	// NTR-AVIJ Kodomo no Tame no Yomi Kikase - Ehon de Asobou 1-Kan
		"AV2",	// NTR-AV2J Kodomo no Tame no Yomi Kikase - Ehon de Asobou 2-Kan
		"AV3",	// NTR-AV3J Kodomo no Tame no Yomi Kikase - Ehon de Asobou 3-Kan
		"AV4",	// NTR-AV4J Kodomo no Tame no Yomi Kikase - Ehon de Asobou 4-Kan
		"AV5",	// NTR-AV5J Kodomo no Tame no Yomi Kikase - Ehon de Asobou 5-Kan
		"AV6",	// NTR-AV6J Kodomo no Tame no Yomi Kikase - Ehon de Asobou 6-Kan
		"YNZ",	// NTR-YNZE Petz - Dogz Fashion
	};

	for (unsigned int i = 0; i < sizeof(list) / sizeof(list[0]); i++) {
		if (memcmp(ndsHeader->gameCode, list[i], 3) == 0) {
			// Found a match.
			specialSetting = 0xFFFF; // Special setting (when found special gamecode)
			break;
		}
	}

	if (specialSetting > 0) {
		// special setting (when found special gamecode)
		volLevel = 0xAC;
	} else {
		// normal setting (for any other gamecodes)
		volLevel = 0xA7;
	}
	

	// Touchscreen
	cdcReadReg (0x63, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x00);
	cdcReadReg (CDC_CONTROL, 0x51);
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcReadReg (CDC_CONTROL, 0x3F);
	cdcReadReg (CDC_SOUND, 0x28);
	cdcReadReg (CDC_SOUND, 0x2A);
	cdcReadReg (CDC_SOUND, 0x2E);
	cdcWriteReg(CDC_CONTROL, 0x52, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x40, 0x0C);
	cdcWriteReg(CDC_SOUND, 0x24, 0xFF);
	cdcWriteReg(CDC_SOUND, 0x25, 0xFF);
	cdcWriteReg(CDC_SOUND, 0x26, 0x7F);
	cdcWriteReg(CDC_SOUND, 0x27, 0x7F);
	cdcWriteReg(CDC_SOUND, 0x28, 0x4A);
	cdcWriteReg(CDC_SOUND, 0x29, 0x4A);
	cdcWriteReg(CDC_SOUND, 0x2A, 0x10);
	cdcWriteReg(CDC_SOUND, 0x2B, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcWriteReg(CDC_TOUCHCNT, 0x02, 0x98);
	cdcWriteReg(CDC_SOUND, 0x23, 0x00);
	cdcWriteReg(CDC_SOUND, 0x1F, 0x14);
	cdcWriteReg(CDC_SOUND, 0x20, 0x14);
	cdcWriteReg(CDC_CONTROL, 0x3F, 0x00);
	cdcReadReg (CDC_CONTROL, 0x0B);
	cdcWriteReg(CDC_CONTROL, 0x05, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x0B, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x0C, 0x02);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x02);
	cdcWriteReg(CDC_SOUND, 0x2E, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x60);
	cdcWriteReg(CDC_CONTROL, 0x01, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x39, 0x66);
	cdcReadReg (CDC_SOUND, 0x20);
	cdcWriteReg(CDC_SOUND, 0x20, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x04, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x81);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x82);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x82);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x04, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x05, 0xA1);
	cdcWriteReg(CDC_CONTROL, 0x06, 0x15);
	cdcWriteReg(CDC_CONTROL, 0x0B, 0x87);
	cdcWriteReg(CDC_CONTROL, 0x0C, 0x83);
	cdcWriteReg(CDC_CONTROL, 0x12, 0x87);
	cdcWriteReg(CDC_CONTROL, 0x13, 0x83);
	cdcReadReg (CDC_TOUCHCNT, 0x10);
	cdcWriteReg(CDC_TOUCHCNT, 0x10, 0x08);
	cdcWriteReg(0x04, 0x08, 0x7F);
	cdcWriteReg(0x04, 0x09, 0xE1);
	cdcWriteReg(0x04, 0x0A, 0x80);
	cdcWriteReg(0x04, 0x0B, 0x1F);
	cdcWriteReg(0x04, 0x0C, 0x7F);
	cdcWriteReg(0x04, 0x0D, 0xC1);
	cdcWriteReg(CDC_CONTROL, 0x41, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x42, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x00);
	cdcWriteReg(0x04, 0x08, 0x7F);
	cdcWriteReg(0x04, 0x09, 0xE1);
	cdcWriteReg(0x04, 0x0A, 0x80);
	cdcWriteReg(0x04, 0x0B, 0x1F);
	cdcWriteReg(0x04, 0x0C, 0x7F);
	cdcWriteReg(0x04, 0x0D, 0xC1);
	cdcWriteReg(CDC_SOUND, 0x2F, 0x2B);
	cdcWriteReg(CDC_SOUND, 0x30, 0x40);
	cdcWriteReg(CDC_SOUND, 0x31, 0x40);
	cdcWriteReg(CDC_SOUND, 0x32, 0x60);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x02);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x10);
	cdcReadReg (CDC_CONTROL, 0x74);
	cdcWriteReg(CDC_CONTROL, 0x74, 0x40);
	cdcWriteReg(CDC_SOUND, 0x21, 0x20);
	cdcWriteReg(CDC_SOUND, 0x22, 0xF0);
	cdcReadReg (CDC_CONTROL, 0x51);
	cdcReadReg (CDC_CONTROL, 0x3F);
	cdcWriteReg(CDC_CONTROL, 0x3F, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x23, 0x44);
	cdcWriteReg(CDC_SOUND, 0x1F, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x28, 0x4E);
	cdcWriteReg(CDC_SOUND, 0x29, 0x4E);
	cdcWriteReg(CDC_SOUND, 0x24, 0x9E);
	cdcWriteReg(CDC_SOUND, 0x25, 0x9E);
	cdcWriteReg(CDC_SOUND, 0x20, 0xD4);
	cdcWriteReg(CDC_SOUND, 0x2A, 0x14);
	cdcWriteReg(CDC_SOUND, 0x2B, 0x14);
	cdcWriteReg(CDC_SOUND, 0x26, 0xA7);
	cdcWriteReg(CDC_SOUND, 0x27, 0xA7);
	cdcWriteReg(CDC_CONTROL, 0x40, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x3A, 0x60);
	cdcWriteReg(CDC_SOUND, 0x26, volLevel);
	cdcWriteReg(CDC_SOUND, 0x27, volLevel);
	cdcWriteReg(CDC_SOUND, 0x2E, 0x03);
	cdcWriteReg(CDC_TOUCHCNT, 0x03, 0x00);
	cdcWriteReg(CDC_SOUND, 0x21, 0x20);
	cdcWriteReg(CDC_SOUND, 0x22, 0xF0);
	cdcReadReg (CDC_SOUND, 0x22);
	cdcWriteReg(CDC_SOUND, 0x22, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x52, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x51, 0x00);
	
	// Set remaining values
	cdcWriteReg(CDC_CONTROL, 0x03, 0x44);
	cdcWriteReg(CDC_CONTROL, 0x0D, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x0E, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x0F, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x10, 0x08);
	cdcWriteReg(CDC_CONTROL, 0x14, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x15, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x16, 0x04);
	cdcWriteReg(CDC_CONTROL, 0x1A, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x1E, 0x01);
	cdcWriteReg(CDC_CONTROL, 0x24, 0x80);
	cdcWriteReg(CDC_CONTROL, 0x33, 0x34);
	cdcWriteReg(CDC_CONTROL, 0x34, 0x32);
	cdcWriteReg(CDC_CONTROL, 0x35, 0x12);
	cdcWriteReg(CDC_CONTROL, 0x36, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x37, 0x02);
	cdcWriteReg(CDC_CONTROL, 0x38, 0x03);
	cdcWriteReg(CDC_CONTROL, 0x3C, 0x19);
	cdcWriteReg(CDC_CONTROL, 0x3D, 0x05);
	cdcWriteReg(CDC_CONTROL, 0x44, 0x0F);
	cdcWriteReg(CDC_CONTROL, 0x45, 0x38);
	cdcWriteReg(CDC_CONTROL, 0x49, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x4A, 0x00);
	cdcWriteReg(CDC_CONTROL, 0x4B, 0xEE);
	cdcWriteReg(CDC_CONTROL, 0x4C, 0x10);
	cdcWriteReg(CDC_CONTROL, 0x4D, 0xD8);
	cdcWriteReg(CDC_CONTROL, 0x4E, 0x7E);
	cdcWriteReg(CDC_CONTROL, 0x4F, 0xE3);
	cdcWriteReg(CDC_CONTROL, 0x58, 0x7F);
	cdcWriteReg(CDC_CONTROL, 0x74, 0xD2);
	cdcWriteReg(CDC_CONTROL, 0x75, 0x2C);
	cdcWriteReg(CDC_SOUND, 0x22, 0x70);
	cdcWriteReg(CDC_SOUND, 0x2C, 0x20);

	// Finish up!
	cdcReadReg (CDC_TOUCHCNT, 0x02);
	cdcWriteReg(CDC_TOUCHCNT, 0x02, 0x98);
	cdcWriteReg(0xFF, 0x05, 0x00); //writeTSC(0x00, 0xFF);

	// Power management
	writePowerManagement(PM_READ_REGISTER, 0x00); //*(unsigned char*)0x40001C2 = 0x80, 0x00; // read PWR[0]   ;<-- also part of TSC !
	writePowerManagement(PM_CONTROL_REG, 0x0D); //*(unsigned char*)0x40001C2 = 0x00, 0x0D; // PWR[0]=0Dh    ;<-- also part of TSC !
}

const char* getRomTid(const tNDSHeader* ndsHeader) {
	static char romTid[5];
	strncpy(romTid, ndsHeader->gameCode, 4);
	romTid[4] = '\0';
	return romTid;
}

ALIGN(4) static void errorOutput (u16 code, u16 isError) {
	arm9_errorCode = code;
	if (isError > 0) { 
		ipcSendState(ARM7_ERR);
		while(1); // Stop
	}
}

static void arm7_readFirmware(tNDSHeader* ndsHeader) {
	PERSONAL_DATA slot1;
	PERSONAL_DATA slot2;

	short slot1count, slot2count; //u8
	short slot1CRC, slot2CRC;

	u32 userSettingsBase;

	// Get settings location
	readFirmware(0x20, &userSettingsBase, 2);

	u32 slot1Address = userSettingsBase * 8;
	u32 slot2Address = userSettingsBase * 8 + 0x100;

	// Reload DS Firmware settings
	readFirmware(slot1Address, &slot1, sizeof(PERSONAL_DATA)); //readFirmware(slot1Address, personalData, 0x70);
	readFirmware(slot2Address, &slot2, sizeof(PERSONAL_DATA)); //readFirmware(slot2Address, personalData, 0x70);
	readFirmware(slot1Address + 0x70, &slot1count, 2); //readFirmware(slot1Address + 0x70, &slot1count, 1);
	readFirmware(slot2Address + 0x70, &slot2count, 2); //readFirmware(slot1Address + 0x70, &slot2count, 1);
	readFirmware(slot1Address + 0x72, &slot1CRC, 2);
	readFirmware(slot2Address + 0x72, &slot2CRC, 2);

	// Default to slot 1 user settings
	void *currentSettings = &slot1;

	short calc1CRC = swiCRC16(0xFFFF, &slot1, sizeof(PERSONAL_DATA));
	short calc2CRC = swiCRC16(0xFFFF, &slot2, sizeof(PERSONAL_DATA));

	// Bail out if neither slot is valid
	if (calc1CRC != slot1CRC && calc2CRC != slot2CRC) { return; }

	// If both slots are valid pick the most recent
	if (calc1CRC == slot1CRC && calc2CRC == slot2CRC) { 
		currentSettings = (slot2count == ((slot1count + 1) & 0x7f) ? &slot2 : &slot1); //if ((slot1count & 0x7F) == ((slot2count + 1) & 0x7F)) {
	} else {
		if (calc2CRC == slot2CRC) { currentSettings = &slot2; }
	}

	PERSONAL_DATA* personalData = (PERSONAL_DATA*)((u32)__NDSHeader - (u32)ndsHeader + (u32)PersonalData); //(u8*)((u32)ndsHeader - 0x180)

	tonccpy(PersonalData, currentSettings, sizeof(PERSONAL_DATA));

	if ((useTwlCfg > 0) && (language == 0xFF)) { language = twlCfgLang; }

	if ((language != 0) && (language <= 7)) {
		// Change language
		personalData->language = (unsigned int)language; //*(u8*)((u32)ndsHeader - 0x11C) = language;
	}

	if ((unsigned int)personalData->language != 6 && ndsHeader->reserved1[8] == 0x80) {
		ndsHeader->reserved1[8] = 0;	// Patch iQue game to be region-free
		ndsHeader->headerCRC16 = swiCRC16(0xFFFF, ndsHeader, 0x015E);	// Fix CRC
	}
}

static void arm7_resetMemory (void) {
	int i, reg;

	REG_IME = 0;

	for (i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}
	
	REG_SOUNDCNT = 0;

	// Clear out ARM7 DMA channels and timers
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
		for(reg=0; reg<0x1c; reg+=4)*((u32*)(0x04004104 + ((i*0x1c)+reg))) = 0; //Reset NDMA.
	}

	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	// clear IWRAM - 037F:8000 to 0380:FFFF, total 96KiB
	// arm7_clearmem ((void*)0x037F8000, 96*1024);

	// clear most of EXRAM - except after 0x022FD800, which has the ARM9 code
	// Skip 0x0200000 region if fastBoot enabled. (cart header copy stored here)
	/*if (useShortInit) {
		arm7_clearmem ((void*)0x02000200, 0x002FD600);
	} else {
		arm7_clearmem ((void*)0x02000000, 0x002FD800);
	}*/
	
	// arm7_clearmem ((void*)0x02000000, 0x002FD000);
	
	// clear last part of EXRAM, skipping the ARM9's section
	// arm7_clearmem ((void*)0x023FF800, 0x800);
	
	// Clear tmp header region previously used by custom struct
	// arm7_clearmem ((void*)TMP_HEADER, 0x160);
	
	/*if ((REG_SNDEXTCNT != 0) && (REG_SCFG_EXT & BIT(31)) && (REG_SCFG_ROM != 0x703)) {
		memset_addrs_arm7(0x03000000, 0x0380FFC0);
		memset_addrs_arm7(0x0380FFD0, 0x03800000 + 0x10000);
	} else {
		memset_addrs_arm7(0x03800000 - 0x8000, 0x03800000 + 0x10000);
	}*/
	// memset_addrs_arm7(0x03000000, 0x0380FFC0);
	// memset_addrs_arm7(0x0380FFD0, 0x03800000 + 0x10000);
	arm7_clearmem((void*)0x03000000, 0x80FFC0);
	arm7_clearmem((void*)0x0380FFD0, 0x30);
	
	// clear most of EXRAM
	arm7_clearmem((void*)0x02000000, 0x3FD000); // clear more of EXRAM, skipping the arm9 temp area used by bootloader
	// memset_addrs_arm7(0x02000000, 0x023FD000); // clear more of EXRAM, skipping the arm9 temp area used by bootloader
	// memset_addrs_arm7(0x023FD000, 0x023FD7BC); // Leave eMMC CID Mirror intact
	// memset_addrs_arm7(0x023FD7CC, 0x02400000);
	
	arm7_clearmem((void*)0x023FE000, 0x2000);
	
	if (twlRAM > 0) {
		arm7_clearmem((void*)0x02FFE000, 0x2000);
		// memset_addrs_arm7(0x02400000, 0x02800000); // Clear the rest of EXRAM
		// clear last part of EXRAM
		// memset_addrs_arm7(0x02800000, 0x02FFD000); // Leave arm9 temp area intact (used by bootloader)
		// memset_addrs_arm7(0x02FFD000, 0x02FFD7BC); // Leave eMMC CID intact
		// memset_addrs_arm7(0x02FFD7CC, 0x03000000);
		// memset_addrs_arm7(0x02FFE000, 0x03000000);
	}

	REG_IE = 0;
	REG_IF = ~0;
	REG_AUXIE = 0;
	REG_AUXIF = ~0;
	REG_POWERCNT = 1;  //turn off power to stuffs
	
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
}

static void setMemoryAddress(const tNDSHeader* ndsHeader, u32 ChipID) {
	if (ndsHeader->unitCode & BIT(1)) {
		copyLoop((u32*)0x02FFFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header

		*(vu32*)(0x02FFA680) = 0x02FD4D80;
		*(vu32*)(0x02FFA684) = 0x00000000;
		*(vu32*)(0x02FFA688) = 0x00001980;

		*(vu32*)(0x02FFF00C) = 0x0000007F;
		*(vu32*)(0x02FFF010) = 0x550E25B8;
		*(vu32*)(0x02FFF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(vu8*)(0x02FFFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(vu8*)(0x02FFFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(vu8*)(0x02FFFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(vu8*)(0x02FFFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(vu8*)(0x02FFFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(vu8*)(0x02FFFD70) = 5;
		}
	}
	
    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((vu32*)0x02FFF800) = ChipID;					// CurrentCardID
	// *((u32*)0x02FFF804) = ChipID;					// Command10CardID // This does not result in correct v
	*((vu16*)0x02FFF808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((vu16*)0x02FFF80A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((vu16*)0x02FFF850) = 0x5835;
	// Copies of above
	*((vu32*)0x02FFFC00) = ChipID;					// CurrentCardID
	// *((u32*)0x02FFFC04) = ChipID;					// Command10CardID
	*((vu16*)0x02FFFC08) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((vu16*)0x02FFFC0A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((vu16*)0x02FFFC10) = 0x5835;
	*((vu16*)0x02FFFC40) = 0x01;						// Boot Indicator -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
}

static void setMemoryAddressTWL(const tNDSHeader* ndsHeader, u32 ChipID) {
	if (ndsHeader->unitCode & BIT(1)) {
		copyLoop((u32*)0x027FFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header

		*(vu32*)(0x027FA680) = 0x02FD4D80;
		*(vu32*)(0x027FA684) = 0x00000000;
		*(vu32*)(0x027FA688) = 0x00001980;

		*(vu32*)(0x027FF00C) = 0x0000007F;
		*(vu32*)(0x027FF010) = 0x550E25B8;
		*(vu32*)(0x027FF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(vu8*)(0x027FFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(vu8*)(0x027FFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(vu8*)(0x027FFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(vu8*)(0x027FFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(vu8*)(0x027FFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(vu8*)(0x027FFD70) = 5;
		}
	}
	
    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((vu32*)0x027FF800) = ChipID;					// CurrentCardID
	// *((u32*)0x027FF804) = ChipID;					// Command10CardID
	*((vu16*)0x027FF808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((vu16*)0x027FF80A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((vu16*)0x027FF850) = 0x5835;
	// Copies of above
	*((vu32*)0x027FFC00) = ChipID;					// CurrentCardID
	// *((u32*)0x027FFC04) = ChipID;					// Command10CardID
	*((vu16*)0x027FFC08) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((vu16*)0x027FFC0A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((vu16*)0x027FFC10) = 0x5835;
	*((vu16*)0x027FFC40) = 0x01;						// Boot Indicator -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
	
	tonccpy((u32*)0x027FC000, (u32*)TMP_HEADER, 0x1000);
	tonccpy((u32*)0x027FF000, (u32*)NDS_HEADER_POKEMON, 0x170);
	tonccpy((u32*)0x027FFE00, (u32*)NDS_HEADER, 0x160);
	tonccpy((u32*)0x027FE000, (u32*)TWL_HEADER, 0x1000);
		
	tonccpy((u32*)0x027FF830, (u32*)0x02FFF830, 0x20);
	tonccpy((u32*)0x027FFC80, (u32*)0x02FFFC80, 0x70);
	tonccpy((u32*)0x027FFD80, (u32*)0x02FFFD80, 0x70);
}

static void arm7_loadExtendedBinary(sNDSHeaderExt* ndsHeader) {
	twlHeader = (tTWLHeader*)TWL_HEADER;
	
	*(vu32*)REG_MBK1 = *(u32*)0x02FFE180;
	*(vu32*)REG_MBK2 = *(u32*)0x02FFE184;
	*(vu32*)REG_MBK3 = *(u32*)0x02FFE188;
	*(vu32*)REG_MBK4 = *(u32*)0x02FFE18C;
	*(vu32*)REG_MBK5 = *(u32*)0x02FFE190;
	REG_MBK6 = *(u32*)0x02FFE1A0;
	REG_MBK7 = *(u32*)0x02FFE1A4;
	REG_MBK8 = *(u32*)0x02FFE1A8;
	REG_MBK9 = *(u32*)0x02FFE1AC;
	/*REG_MBK9 = twlHeader->arm9MBKMaster;
	REG_MBK6 = twlHeader->arm7MBK6;
	REG_MBK7 = twlHeader->arm7MBK7;
	REG_MBK8 = twlHeader->arm7MBK8;*/
	
	cardRead(ndsHeader, twlHeader->arm9iromOffset, (u32*)twlHeader->arm9idestination, twlHeader->arm9ibinarySize);
	cardRead(ndsHeader, twlHeader->arm7iromOffset, (u32*)twlHeader->arm7idestination, twlHeader->arm7ibinarySize);
	
	if (twlHeader->twlHeaderSettings & BIT(1)) {
		char modcrypt_shared_key[9] = { 'N', 'i', 'n', 't', 'e', 'n', 'd', 'o', '\0' };
		int i;
		u8 key[16];
		u8 keyp[16];
		
		for (i = 0; i < 16; i++) {
			key[i] = 0;
			keyp[i] = 0;
		}
		
		if (twlHeader->twlHeaderSettings & BIT(3)) {
			// Debug Key
			tonccpy(key, (u8*)twlHeader, 16);
		} else {
			u8 *target = (u8*)twlHeader;
			tonccpy(keyp, modcrypt_shared_key, 8);
			for (int i=0;i<4;i++) { keyp[8+i] = target[0x0c+i]; keyp[15-i] = target[0x0c+i]; }
			tonccpy(key, target+0x350, 16);
			u128_xor(key, keyp);
			u128_add(key, DSi_KEY_MAGIC);
			u128_lrot(key, 42);
		}
		
		u32 rk[4];
		tonccpy(rk, key, 16);
		
		dsi_context ctx;
		dsi_set_key(&ctx, key);
		dsi_set_ctr(&ctx, twlHeader->hmac_arm9);
		if (twlHeader->modcrypt1Size != 0) { decrypt_modcrypt_area(&ctx, (u8*)twlHeader->arm9idestination, twlHeader->modcrypt1Size); }

		dsi_set_key(&ctx, key);
		dsi_set_ctr(&ctx, twlHeader->hmac_arm7);
		if (twlHeader->modcrypt2Size != 0) { decrypt_modcrypt_area(&ctx, (u8*)twlHeader->arm7idestination, twlHeader->modcrypt2Size); }
	}
}

static u16 arm7_loadBinary (void) {
	u16 errorCode;
	
	tDSiHeader* twlHeaderTemp = (tDSiHeader*)TMP_HEADER; // Use same region cheat engine goes. Cheat engine will replace this later when it's not needed.

	// Init card
	/*if (useShortInit) {
		errorCode = cardInitShort((sNDSHeaderExt*)twlHeaderTemp, &chipID);
		arm7_clearmem ((void*)0x02000200, 0x200); // clear temp header data
	} else {
		errorCode = cardInit((sNDSHeaderExt*)twlHeaderTemp, &chipID);
	}*/
	
	errorCode = cardInit((sNDSHeaderExt*)twlHeaderTemp, (vu32*)InitialCartChipID);
	
	if (errorCode)return errorCode;

	ndsHeader = loadHeader(twlHeaderTemp); // copy twlHeaderTemp to ndsHeader location
	
	cardRead((sNDSHeaderExt*)twlHeaderTemp, ndsHeader->arm9romOffset, (u32*)ndsHeader->arm9destination, ndsHeader->arm9binarySize);
	cardRead((sNDSHeaderExt*)twlHeaderTemp, ndsHeader->arm7romOffset, (u32*)ndsHeader->arm7destination, ndsHeader->arm7binarySize);
	
	if ((ndsHeader->unitCode & BIT(1)) && (twlMode > 0) && (twlRAM > 0))arm7_loadExtendedBinary((sNDSHeaderExt*)twlHeaderTemp);
	
	// Fix Pokemon games needing header data.
	copyLoop((u32*)NDS_HEADER_POKEMON, (u32*)NDS_HEADER, 0x170);

	char* romTid = (char*)NDS_HEADER_POKEMON+0xC;
	if (   memcpy(romTid, "ADA", 3) == 0 // Diamond
		|| memcmp(romTid, "APA", 3) == 0 // Pearl
		|| memcmp(romTid, "CPU", 3) == 0 // Platinum
		|| memcmp(romTid, "IPK", 3) == 0 // HG
		|| memcmp(romTid, "IPG", 3) == 0 // SS
	) {
		// Make the Pokemon game code ADAJ.
		const char gameCodePokemon[] = { 'A', 'D', 'A', 'J' };
		memcpy((char*)NDS_HEADER_POKEMON+0xC, gameCodePokemon, 4);
	}
	
	return ERR_NONE;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function
void arm7_main (void) {

	u16 errorCode;

	// Synchronise start
	while (ipcRecvState() != ARM9_START);
	ipcSendState(ARM7_START);

	// Wait until ARM9 is ready
	while (ipcRecvState() != ARM9_READY);

	if ((twlMode == 0) || (twlRAM == 0) || (isTWLSRL == 0)) {
		REG_MBK9=0xFCFFFF0F;
		REG_MBK6=0x09403900;
		REG_MBK7=0x09803940;
		REG_MBK8=0x09C03980;
	}

	errorOutput(ERR_STS_CLR_MEM, 0);

	ipcSendState(ARM7_MEMCLR);
		
	// Get ARM7 to clear RAM
	arm7_resetMemory();
	
	if (twlMode == 0)REG_SCFG_ROM = 0x703;

	errorOutput(ERR_STS_LOAD_BIN, 0);
	
	ipcSendState(ARM7_LOADBIN);

	// Load the NDS file
	errorCode = arm7_loadBinary();
	if (errorCode)errorOutput(errorCode, 0xFFFF);
	
	errorOutput(ERR_STS_STARTBIN, 0);
	
	if (twlRAM == 0)tonccpy((u32*)0x023FF000, (u32*)0x02FFF000, 0x1000);
	
	arm7_readFirmware(ndsHeader); // Header has to be loaded first
	
	if (twlMode > 0) {
		REG_SCFG_CLK = 0x187;
		// REG_SCFG_ROM = 0x501;
		// REG_SCFG_EXT = 0x92FBFB06;
		// REG_SCFG_EXT = 0x9307F100;
		REG_SCFG_EXT = 0x93FFFB06;
	} else { 
		REG_SCFG_CLK = 0x107;
		if (cdcReadReg(CDC_SOUND, 0x22) == 0xF0) {
			// Switch touch mode to NTR
			*(u16*)0x4004700 = 0x800F;
			NDSTouchscreenMode();
			*(u16*)0x4000500 = 0x807F;
		}
		REG_GPIO_WIFI |= BIT(8);	// Old NDS-Wifi mode
		REG_SCFG_EXT = 0x92A40000;
	}

	// if (twlCLK > 0) { REG_SCFG_CLK = 0x187; } else { REG_SCFG_CLK = 0x107; }
	if (scfgUnlock == 0)REG_SCFG_EXT &= ~(1UL << 31); /// REG_SCFG_EXT |= BIT(18);
	
	setMemoryAddress(ndsHeader, *(vu32*)InitialCartChipID);
	
	if ((twlRAM > 0) && ((twlMode == 0) || (isTWLSRL == 0) || ((isTWLSRL == 0) && (twlMode > 0))))setMemoryAddressTWL(ndsHeader, *(vu32*)InitialCartChipID);

	ipcSendState(ARM7_BOOTBIN);
		
	arm7_reset();
}

