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
#include <nds/arm9/console.h>
#include <nds/fifocommon.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <list>

#include "inifile.h"
#include "bootsplash.h"
#include "launch_engine.h"
#include "crc.h"
#include "tonccpy.h"
#include "nitrofs.h"
#include "nds_card.h"
#include "debugConsole.h"
#include "hbmenu.h"
#include "launcherData.h"
#include "read_card.h"

#define FILECOPYBUFFER 0x02000000

ALIGN(4) struct {
	sNDSHeader header;
	char padding[0x200 - sizeof(sNDSHeader)];
} ndsHeader;

static char gameTitle[13] = {0};
// static char gameCode[7] = {0};
static char gameCode[5] = {0};
static bool nitroFSMounted = false;
static ALIGN(4) sNDSHeaderExt ntrHeader;

const char* PROGVERSION = "3.0";

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

extern void InitConsole();
extern void StartFileBrowser(tLauncherSettings launchdata);

extern bool ConsoleInit;

void DisplayText(const char* text, bool clear = false, bool noText = false){
	if (!ConsoleInit)InitConsole();
	if (clear)consoleClear();
	printf("--------------------------------\n");
	printf("----[NTR Launcher Debug Mode]---\n");
	printf("----------[Version: 3.0]--------\n");
	printf("--------------------------------\n\n");
	if (!noText)printf(text);
}

void DoWait(int waitTime = 30){
	for (int i = 0; i < waitTime; i++)swiWaitForVBlank();
};


bool MountNitroFS() {
	if (nitroFSMounted)return nitroFSMounted;
	nitroFSMounted = nitroFSInit("sd:/title/00030004/4b4b4750/content/00000000.app");
	if (nitroFSMounted)return nitroFSMounted;
	nitroFSMounted = nitroFSInit("sd:/NTR_Launcher.nds");
	if (nitroFSMounted)return nitroFSMounted;
	nitroFSMounted = nitroFSInit("sd:/nds/NTR_Launcher.nds");
	if (nitroFSMounted)return nitroFSMounted;
	nitroFSMounted = nitroFSInit("sd:/homebrew/NTR_Launcher.nds");
	return nitroFSMounted;
}

void StartStage2Mode(tLauncherSettings Launchdata) {
	sysSetCardOwner (BUS_OWNER_ARM9);
	StartFileBrowser(Launchdata);
}


bool DoCardInit(bool DebugMode, bool fastBoot) {
	bool FastBoot = fastBoot;
	if (DebugMode){ 
		DisplayText("CLR", true, true);
		DisplayText("Loading Cart details.\nPlease Wait...\n", true);
	}
	switch (REG_SCFG_MC) {
		case 0x10: { enableSlot1(); DoWait(10);	FastBoot = false; }break;
		case 0x11: { enableSlot1();	DoWait(10);	FastBoot = false; }break;
	}
	// Do cart init stuff to wake cart up. DLDI init may fail otherwise!
	if (FastBoot) {
		cardInitShort(&ntrHeader);
		tonccpy(gameTitle, ntrHeader.gameTitle, 12);
		tonccpy(gameCode, ntrHeader.gameCode, 4);
	} else {
		CardReset(true);
		cardReadHeader((u8*)&ndsHeader);
		CardReset(false);
		tonccpy(gameTitle, ndsHeader.header.gameTitle, 12);
		tonccpy(gameCode, ndsHeader.header.gameCode, 4);
	}
	if (DebugMode) {
		DisplayText("CLR", true, true);
		iprintf("Detected Cart Name: %12s \n", gameTitle);
		iprintf("Detected Cart Game ID: %6s \n\n", gameCode);
		printf("Press any button to continue...");
		while (keysDown()) { swiWaitForVBlank(); scanKeys(); }
		while (!keysDown()) { swiWaitForVBlank(); scanKeys(); }
	} else {
		DoWait(25);
	}
	return FastBoot;
}

bool DoSlotCheck(bool DebugMode) {
	bool slotReset = false;
	switch (REG_SCFG_MC) {
		case 0x11: {
			if(DebugMode) { 
				DisplayText("Please insert a cartridge...\n", true);
			} else {
				if (!ConsoleInit)InitConsole();
				consoleClear();
				printf("\n\n\n\n\n\n\n\n\n\n\n  Please insert a cartridge...  ");
			}
			REDO:
			swiWaitForVBlank();
			while (REG_SCFG_MC != 0x10)swiWaitForVBlank();
			enableSlot1();
			DoWait(60);
			if (REG_SCFG_MC != 0x18) goto REDO;
			slotReset = true;	
		}break;
		case 0x10: {
			slotReset = true;
			enableSlot1();
		}break;
	}
	return slotReset;
}

int main() {
	defaultExceptionHandler();
	
	sysSetCardOwner(BUS_OWNER_ARM9);
		
	tLauncherSettings LaunchData = { 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF };

	// Create backup of Cart header (if present) saved by DSi System Menu. 
	// It will not survive memory reallocation after dropping to NTR ram spec during bootloader.
	tonccpy ((u32*)CartHeaderCopy, (u32*)InitialCartHeader, 0x200);
	
	int language = -1;
	bool scfgunlock = false;
	bool twlmode = false;
	bool twlclk = false;
	bool twlvram = false;
	bool debugmode = false;
	bool fastBoot = false;
	bool stage2Menu = false;

	bool useAnimatedSplash = false;
	bool useNTRSplash = true;
	bool healthAndSafety_MSG = true;

	if (fatMountSimple("sd", get_io_dsisd())) {
		if(access("sd:/NTR_Launcher", F_OK) != 0)mkdir("sd:/NTR_Launcher", 0777);
		if ((access("sd:/NTR_Launcher/NTR_Launcher.ini", F_OK) != 0) && MountNitroFS()) {
			FILE *src = fopen("nitro:/NTR_Launcher.ini", "rb");
			if (src) {
				FILE *dest = fopen("sd:/NTR_Launcher/NTR_Launcher.ini", "wb");
				if (dest) {
					fseek(src, 0, SEEK_END);
					u32 fSize = ftell(src);
					fseek(src, 0, SEEK_SET);
					fread((void*)FILECOPYBUFFER, 1, fSize, src);
					fwrite((void*)FILECOPYBUFFER, fSize, 1, dest);
					fclose(src);
					fclose(dest);
				}
			}
		}
		if (access("sd:/NTR_Launcher/NTR_Launcher.ini", F_OK) == 0) {
			CIniFile ntrlauncher_config( "sd:/NTR_Launcher/NTR_Launcher.ini" );
			
			twlclk = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLCLOCK",0);
			twlvram = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLVRAM",0);
			twlmode = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLMODE",0);
			scfgunlock = ntrlauncher_config.GetInt("NTRLAUNCHER","SCFGUNLOCK",0);
			useAnimatedSplash = ntrlauncher_config.GetInt("NTRLAUNCHER","ANIMATEDSPLASH",0);
			useNTRSplash = ntrlauncher_config.GetInt("NTRLAUNCHER","NTRSPLASH",0);
			healthAndSafety_MSG = ntrlauncher_config.GetInt("NTRLAUNCHER","HEALTHSAFETYSPLASH",0);
		
			debugmode = ntrlauncher_config.GetInt("NTRLAUNCHER","DEBUGMODE",0);
			language = ntrlauncher_config.GetInt("NTRLAUNCHER", "LANGUAGE", -1);
		}
	}

	scanKeys();
	swiWaitForVBlank();
	
	if (scfgunlock)LaunchData.scfgUnlock = 0x01;
	if (twlmode)LaunchData.twlMode = 0x01;
	if (twlclk)LaunchData.twlCLK = 0x01;
	if (twlvram)LaunchData.twlVRAM = 0x01;
	if (fastBoot)LaunchData.fastBoot = 0x01;
		
	if (((keysHeld() & KEY_L) && (keysHeld() & KEY_R)) || (REG_SCFG_MC == 0x11))stage2Menu = true;
	
	if ((REG_SCFG_MC == 0x10) && !stage2Menu) { sysSetCardOwner (BUS_OWNER_ARM9); DoCardInit(false, false); }
	
	if (keysDown() & KEY_B) {
		debugmode = true;
	} else if (keysDown() & KEY_L) {
		twlclk = false; twlvram = true; scfgunlock = false; twlmode = false;
	} else if (keysDown() & KEY_R) {
		twlclk = true; twlvram = true; scfgunlock = true; twlmode = true;
	}
	
	ConsoleInit = debugmode;
	if (stage2Menu) { ConsoleInit = false; debugmode = false; }
	if (debugmode)useAnimatedSplash = false;
	if (!useAnimatedSplash) {
		if (debugmode)InitConsole();
		// fastBoot = !DoSlotCheck(debugmode);
		if (!stage2Menu)DoSlotCheck(debugmode);
	} else {
		char *p = (char*)PersonalData->name;
		for (int i = 0; i < 10; i++) {
			if (p[i*2] == 0x00) { p[i*2/2] = 0; } else { p[i*2/2] = p[i*2]; }
		}
		if (language == -1)language = PersonalData->language;
		BootSplashInit(useNTRSplash, healthAndSafety_MSG, (int)language, false);
	}
	
	if (language != -1)LaunchData.language = (u8)language;
	if (scfgunlock)LaunchData.scfgUnlock = 0x01;
	if (twlmode)LaunchData.twlMode = 0x01;
	if (twlclk)LaunchData.twlCLK = 0x01;
	if (twlvram)LaunchData.twlVRAM = 0x01;
	if (debugmode)LaunchData.debugMode = 0x01;
	/*if (fastBoot) {
		LaunchData.fastBoot = 0x01;
	} else {
		LaunchData.fastBoot = 0x00;
	}*/
	
	if (stage2Menu || (REG_SCFG_MC == 0x11)) { 
		StartStage2Mode(LaunchData);
	} else {
		// DoCardInit(debugmode, fastBoot);
		DoCardInit(debugmode, false);
		// DS-Xtreme does not like running in TWL clock speeds.
		// (write function likely goes too fast and semi-bricks hidden sector region randomly when using official launcher)
		if (!memcmp(gameTitle, "D!S!XTREME", 9)) {
			scfgunlock = false;
			twlclk = false;
			twlvram = false;
			twlmode = false;
		}
		while(1) {
			// If SCFG_MC is returning as zero/null, this means SCFG_EXT registers are locked on arm9 or user attempted to run this while in NTR mode.
			if((REG_SCFG_MC == 0x00) || (REG_SCFG_MC == 0x11) || (REG_SCFG_MC == 0x10)) {
				if (useAnimatedSplash) {
					BootSplashInit(false, false, 0, true);
				} else {
					DisplayText("Error has occured.!\nEither card ejected late,\nor NTR mode detected!\nPress any button to exit...", true);
					do { swiWaitForVBlank(); scanKeys(); } while (!keysDown());
				}
				break;
			} else {
				runLaunchEngine(LaunchData);
				break;
			}
			swiWaitForVBlank();
		}
	}
	return 0;
}

