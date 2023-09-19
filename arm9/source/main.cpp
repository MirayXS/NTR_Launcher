/*
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

#include <nds.h>
#include <fat.h>
#include <nds/fifocommon.h>

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <list>

#include "inifile.h"
#include "bootsplash.h"
#include "bootsplash2.h"
#include "launch_engine.h"
#include "crc.h"
#include "tonccpy.h"
#include "read_card.h"

sNDSHeaderExt ndsHeader;
char gameTitle[13] = {0};
char gameCode[7] = {0};

const char* PROGVERSION = "2.6";

off_t getFileSize(const char *fileName) {
    FILE* fp = fopen(fileName, "rb");
    off_t fsize = 0;
    if (fp) {
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);			// Get source file's size
		fseek(fp, 0, SEEK_SET);
	}
	fclose(fp);
	return fsize;
}

void DisplayText(const char* text, bool clear = false, bool clearOnly = false){
	if (clear | clearOnly) {
		consoleClear();
		printf("--------------------------------\n");
		printf("----[NTR Launcher Debug Mode]---\n");
		printf("----------[Version: 2.6]--------\n");
		printf("--------------------------------\n\n");
		if (clearOnly) return;
	}
	printf(text);
}

void DoWait(int waitTime = 30){
	for (int i = 0; i < waitTime; i++) swiWaitForVBlank();
};

void DoCardInit(bool DebugMode) {
	if (DebugMode){ 
		DisplayText("CLEARONLY", true);
		DisplayText("Loading Cart details.\nPlease Wait...\n", true);
	}
	// Do cart init stuff to wake cart up. DLDI init may fail otherwise!
	cardInit(&ndsHeader);
	tonccpy(gameTitle, ndsHeader.gameTitle, 12);
	tonccpy(gameCode, ndsHeader.gameCode, 6);
	DoWait(60);
	if (DebugMode) {
		DisplayText("CLEARONLY", true, true);
		iprintf("Detected Cart Name: %12s \n", gameTitle);
		iprintf("Detected Cart Game ID: %6s \n\n", gameCode);
		DisplayText("Press any button to continue...");
		do { swiWaitForVBlank(); scanKeys(); } while (!keysDown());
	}
}

void ResetSlot1() {
	if (REG_SCFG_MC == 0x11) return;
	disableSlot1();
	DoWait();
	enableSlot1();
}

void DoSlotCheck(bool DebugMode) {
	if (REG_SCFG_MC == 0x11) {
		if (DebugMode) DisplayText("Please insert a cartridge...\n", true);
		REDO:
		swiWaitForVBlank();
		do { 
			if (!DebugMode) CartridgePrompt();
		} while (REG_SCFG_MC != 0x10);
		enableSlot1();
		DoWait(60);
		if (REG_SCFG_MC != 0x18) goto REDO;
		// fifoSendValue32(FIFO_USER_02, 1);
	}
}

int main() {
	defaultExceptionHandler();
	
	// bool consoleInited = false;
	bool scfgUnlock = false;
	bool TWLMODE = false;
	bool TWLEXTRAM = false;
	bool TWLCLK = false;	// false == NTR, true == TWL
	bool TWLVRAM = false;
	bool soundFreq = false;
	bool EnableSD = false;
	// bool slot1Init = false;
	bool LegacyMode = false;
	
	bool UseAnimatedSplash = false;
	bool UseNTRSplash = true;
	bool HealthAndSafety_MSG = true;
	
	int language = -1;
	
	bool DebugMode = false;
		
	if (fatInitDefault()) {
		CIniFile ntrlauncher_config( "sd:/NDS/NTR_Launcher.ini" );
		
		TWLCLK = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLCLOCK",0);
		TWLVRAM = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLVRAM",0);
		TWLEXTRAM = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLEXTRAM",0);
		TWLMODE = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLMODE",0);
		soundFreq = ntrlauncher_config.GetInt("NTRLAUNCHER","SOUNDFREQ",0);
		// EnableSD = ntrlauncher_config.GetInt("NTRLAUNCHER","SDACCESS",0);
		EnableSD = true;
		scfgUnlock = ntrlauncher_config.GetInt("NTRLAUNCHER","SCFGUNLOCK",0);
		// cardInit() does a slot reset anyways so this feature will be deprecated.
		// slot1Init = ntrlauncher_config.GetInt("NTRLAUNCHER","RESETSLOT1",0);
		UseAnimatedSplash = ntrlauncher_config.GetInt("NTRLAUNCHER","ANIMATEDSPLASH",0);
		UseNTRSplash = ntrlauncher_config.GetInt("NTRLAUNCHER","NTRSPLASH",0);
		HealthAndSafety_MSG = ntrlauncher_config.GetInt("NTRLAUNCHER","HEALTHSAFETYSPLASH",0);
		
		DebugMode = ntrlauncher_config.GetInt("NTRLAUNCHER","DEBUGMODE",0);
		LegacyMode = ntrlauncher_config.GetInt("NTRLAUNCHER", "LEGACYMODE", 0);
		language = ntrlauncher_config.GetInt("NTRLAUNCHER", "LANGUAGE", -1);
		
		/*if(slot1Init) {
			ResetSlot1();
		}*/
	}/* else {
		ResetSlot1();
	}*/
	
	if (DebugMode) UseAnimatedSplash = false;
	
	if (!UseAnimatedSplash) { 
		BootSplashInit(DebugMode);
		DoSlotCheck(DebugMode);
	} else {
		if (DebugMode) {
			DoSlotCheck(DebugMode);
		} else {
			char *p = (char*)PersonalData->name;
			for (int i = 0; i < 10; i++) {
				if (p[i*2] == 0x00) {
					p[i*2/2] = 0;
				} else {
					p[i*2/2] = p[i*2];
				}
			}
			if (language == -1) language = (PersonalData->language);
			BootSplashInit2(UseNTRSplash, HealthAndSafety_MSG, language, false);
		}
	}
	
	if (LegacyMode && !TWLCLK) { 
		fifoSendValue32(FIFO_USER_01, 1); 
		fifoWaitValue32(FIFO_USER_02);
	}
	DoWait();
	sysSetCardOwner (BUS_OWNER_ARM9);
	
	DoCardInit(DebugMode);
		
	// Force disable Legacy Mode if a TWL cart is detected. Old cart loader doesn't support TWL carts.
	if ((ndsHeader.unitCode == 0x03) | (ndsHeader.unitCode == 0x02)) { 
		LegacyMode = false; 
		// Force specific settings needed for proper support for retail TWL carts
		if (ndsHeader.unitCode == 0x03) {
			scfgUnlock = false;
			TWLMODE = true;
			TWLEXTRAM = true;
			TWLCLK = true;	// false == NTR, true == TWL
			TWLVRAM = true;
			soundFreq = true;
			if (!memcmp(gameCode, "DSYE", 4))LegacyMode = true;
		} else {
			TWLCLK = true;
		}
	} else if (!memcmp(gameCode, "ASMA", 4) | !memcmp(gameCode, "ACEK", 4) ) { 
		//Original R4 doesn't like new bootloader
		LegacyMode = true;
		DoWait(60);
	} else if (!memcmp(gameTitle, "D!S!XTREME", 9)) {
		// DS-Xtreme does not like running in TWL clock speeds. (write function likely goes too fast and semi-bricks hidden sector region randomly)
		TWLCLK = false;
	};
	
	while(1) {
		// If SCFG_MC is returning as zero/null, this means SCFG_EXT registers are locked on arm9 or user attempted to run this while in NTR mode.
		if((REG_SCFG_MC == 0x00) | (REG_SCFG_MC == 0x11) | (REG_SCFG_MC == 0x10)) {
			if (UseAnimatedSplash) {
				BootSplashInit2(false, false, 0, true);
			} else {
				if (!DebugMode) {
					ErrorScreen(DebugMode);
				} else {
					DisplayText("Error has occured.!\nEither card ejected late,\nor NTR mode detected!\nPress any button to exit...", true);
				}
				do { swiWaitForVBlank(); scanKeys(); } while (!keysDown());
			}
			break;
		} else {
			runLaunchEngine (LegacyMode, EnableSD, language, scfgUnlock, TWLMODE, TWLCLK, TWLVRAM, soundFreq, TWLEXTRAM, DebugMode);
		}
	}
	return 0;
}

