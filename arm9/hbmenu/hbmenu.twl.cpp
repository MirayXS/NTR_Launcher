/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>

#include "args.h"
#include "file_browse.h"
#include "font.h"
#include "hbmenu_consolebg.h"
#include "iconTitle.h"
#include "nds_loader_arm9.h"
#include "launcherData.h"
#include "../source/launch_engine.h"
#include "read_card.h"
#include "nds_card.h"
#include "tonccpy.h"

using namespace std;

static char gameTitle[13] = {0};

static ALIGN(4) u8* fileBuffer;

static const int FileListSize = 13;
static const u32 FileMaxSize = 0x500000;

static const char* NitroSourceFileList[FileListSize] = {
	"nitro:/NTR_Launcher/Acekard2i.nds",		// 0
	"nitro:/NTR_Launcher/ActionReplayDS.nds",	// 1
	"nitro:/NTR_Launcher/CycloDS.nds",			// 2
	"nitro:/NTR_Launcher/DSONEi.nds",			// 3
	"nitro:/NTR_Launcher/DSTwo.nds",			// 4
	"nitro:/NTR_Launcher/EZFlashV.nds",			// 5
	"nitro:/NTR_Launcher/NCARD.nds",			// 6
	"nitro:/NTR_Launcher/R4DS.nds",				// 7
	"nitro:/NTR_Launcher/R4DS_Ultra.nds",		// 8
	"nitro:/NTR_Launcher/R4i_SDHC_AVRJ.nds",	// 9
	"nitro:/NTR_Launcher/R4iGold_Launcher.nds",	// 10
	"nitro:/NTR_Launcher/R4iSDHC_Demon.nds",	// 11
	"nitro:/NTR_Launcher/TTDS.nds"				// 12
};

static const char* NitroDestFileList[FileListSize] = {
	"sd:/NTR_Launcher/Acekard2i.nds",			// 0
	"sd:/NTR_Launcher/ActionReplayDS.nds",		// 1
	"sd:/NTR_Launcher/CycloDS.nds",				// 2
	"sd:/NTR_Launcher/DSONEi.nds",				// 3
	"sd:/NTR_Launcher/DSTwo.nds",				// 4
	"sd:/NTR_Launcher/EZFlashV.nds",			// 5
	"sd:/NTR_Launcher/NCARD.nds",				// 6
	"sd:/NTR_Launcher/R4DS.nds",				// 7
	"sd:/NTR_Launcher/R4DS_Ultra.nds",			// 8
	"sd:/NTR_Launcher/R4i_SDHC_AVRJ.nds",		// 9
	"sd:/NTR_Launcher/R4iGold_Launcher.nds",	// 10
	"sd:/NTR_Launcher/R4iSDHC_Demon.nds",		// 11
	"sd:/NTR_Launcher/TTDS.nds"					// 12
};

static void DoWait(int waitTime = 30) { for (int i = 0; i < waitTime; i++)swiWaitForVBlank(); };

static void DoCardInit() {
	sysSetCardOwner(BUS_OWNER_ARM9);
	switch (REG_SCFG_MC) {
		case 0x10: { enableSlot1(); DoWait(15);	}break;
		case 0x11: { enableSlot1();	DoWait(15);	}break;
	}
	// Do cart init stuff to wake cart up. DLDI init may fail otherwise!
	cardInit(&ntrHeader);
	while(REG_ROMCTRL & CARD_BUSY)swiWaitForVBlank();
	// CardReset(true);
	// cardReadHeader((u8*)&ntrHeader);
	CardReset(false);
	tonccpy(gameTitle, ntrHeader.gameTitle, 12);
	while(REG_ROMCTRL & CARD_BUSY)swiWaitForVBlank();
}


static void InitGUI(void) {
	iconTitleInit();
	videoSetModeSub(MODE_4_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	int bgSub = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
	PrintConsole *console = consoleInit(0, 0, BgType_Text4bpp, BgSize_T_256x256, 4, 6, false, false);
	dmaCopy(hbmenu_consolebgBitmap, bgGetGfxPtr(bgSub), 256*256);
	ConsoleFont font;
	font.gfx = (u16*)fontTiles;
	font.pal = (u16*)fontPal;
	font.numChars = 95;
	font.numColors = (fontPalLen / 2);
	font.bpp = 4;
	font.asciiOffset = 32;
	font.convertSingleColor = true;
	consoleSetFont(console, &font);
	dmaCopy(hbmenu_consolebgPal, BG_PALETTE_SUB, 256*2);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	keysSetRepeat(25,5);
	consoleSetWindow(console, 1, 1, 30, 22);
}

static int stop(void) {
	while (keysDown()) { swiWaitForVBlank(); scanKeys(); }
	while (!keysDown()) { swiWaitForVBlank(); scanKeys(); }
	return 0;
}

static void CheckFolder() {
	bool copyNeeded = false;
	int i = 0;
	for (i = 0; i < FileListSize; i++) {
		if (access(NitroDestFileList[i], F_OK) != 0) {
			copyNeeded = true;
			break;
		}
	}
	if (!copyNeeded)return;
	
	printf("\n\n\n\n\n\n\n\n\n   Setting up Stage2 folder");
	printf("\n\n        Please Wait...\n");
	
	fileBuffer = (u8*)malloc(FileMaxSize);
	for (i = 0; i < FileListSize; i++) {
		if (access(NitroDestFileList[i], F_OK) != 0) {
			FILE *src = fopen(NitroSourceFileList[i], "rb");
			if (src) {
				fseek(src, 0, SEEK_END);
				u32 fSize = ftell(src);
				fseek(src, 0, SEEK_SET);
				if (fSize <= FileMaxSize) {
					FILE *dest = fopen(NitroDestFileList[i], "wb");
					if (dest) {
						fread((u8*)fileBuffer, 1, fSize, src);
						fwrite((u8*)fileBuffer, fSize, 1, dest);
						fclose(dest);
					}
				}
				fclose(src);
			}
		}
	}
}


static int BrowserUI(tLauncherSettings launchdata) {
	consoleClear();
	vector<string> extensionList = argsGetExtensionList();
	chdir("sd:/NTR_Launcher");
	while(1) {
		string filename = browseForFile(extensionList, launchdata);
		if (cartSelected)break;
		// Construct a command line
		vector<string> argarray;
		if (!argsFillArray(filename, argarray)) {
			printf("Invalid NDS or arg file selected\n");
		} else {
			iprintf("Running %s with %d parameters\n", argarray[0].c_str(), argarray.size());
			// Make a copy of argarray using C strings, for the sake of runNdsFile
			vector<const char*> c_args;
			for (const auto& arg: argarray) { c_args.push_back(arg.c_str()); }
			// Try to run the NDS file with the given arguments
			if (access(c_args[0], F_OK) == 0) {
				FILE *src = fopen(c_args[0], "rb");
				if (src) {
					fread((u8*)0x02FFE000, 1, 0x200, src);
					if (((tNDSHeader*)0x02FFE000)->unitCode & BIT(1))launchdata.isTWLSRL = 0xFF;
					fclose(src);
				}
			}
			// tonccpy((void*)0x02FFDFF0, (void*)LAUNCH_DATA, 0x10);
			launchdata.cachedChipID = *(vu32*)InitialCartChipID;
			int err = runNdsFile(c_args[0], c_args.size(), &c_args[0], launchdata);
			iprintf("Start failed. Error %i\n", err);
			break;
		}
		argarray.clear();
	}
	if (cartSelected) {
		// DS-Xtreme does not like running in TWL clock speeds.
		// (write function likely goes too fast and semi-bricks hidden sector region randomly when using official launcher)
		if (!memcmp(gameTitle, "D!S!XTREME", 10)) {
			launchdata.scfgUnlock = 0x00;
			launchdata.twlMode = 0x00;
			launchdata.twlCLK = 0x00;
			launchdata.twlVRAM = 0x00;
			launchdata.twlRAM = 0x00;
			launchdata.isTWLSRL = 0x00;
			// cardInit(&ntrHeader);
		}
		if (cartInsertedOnBoot) {
			DoCardInit(); // Currently required for bootlaoder to succeed with card init.
		} else {
			DoWait(29);	// Give launch soundfx time to finish if card Init already occured.
		}
		if (ntrHeader.unitCode & BIT(1))launchdata.isTWLSRL = 0x01;
		launchdata.cachedChipID = *(vu32*)InitialCartChipID;
		runLaunchEngine(launchdata);
	}
	return stop();
}

void StartFileBrowser(tLauncherSettings launchdata, bool nitroFSMounted) {
	InitGUI();
	CheckFolder();
	BrowserUI(launchdata);
}

