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

// ALIGN(4) sNDSHeaderExt NTRHeader;

static char gameTitle[13] = {0};
// static char gameCode[5] = {0};

static u8* fileBuffer;


static const char* NitroSourceFileList[7] = {
	"nitro:/NTR_Launcher/Acekard2i.nds",
	"nitro:/NTR_Launcher/ActionReplayDS.nds",
	"nitro:/NTR_Launcher/DSTwo.nds",
	"nitro:/NTR_Launcher/EZFlashV.nds",
	"nitro:/NTR_Launcher/R4iGold_Launcher.nds",
	"nitro:/NTR_Launcher/R4iSDHC_Demon.nds",
	"nitro:/NTR_Launcher/TTDS.nds"
};

static const char* NitroDestFileList[7] = {
	"sd:/NTR_Launcher/Acekard2i.nds",
	"sd:/NTR_Launcher/ActionReplayDS.nds",
	"sd:/NTR_Launcher/DSTwo.nds",
	"sd:/NTR_Launcher/EZFlashV.nds",
	"sd:/NTR_Launcher/R4iGold_Launcher.nds",
	"sd:/NTR_Launcher/R4iSDHC_Demon.nds",
	"sd:/NTR_Launcher/TTDS.nds"
};

static void DoWait(int waitTime = 30) { for (int i = 0; i < waitTime; i++)swiWaitForVBlank(); };

static void DoCardInit() {
	switch (REG_SCFG_MC) {
		case 0x10: { enableSlot1(); DoWait(10);	}break;
		case 0x11: { enableSlot1();	DoWait(10);	}break;
	}
	// Do cart init stuff to wake cart up. DLDI init may fail otherwise!
	CardReset(true);
	cardReadHeader((u8*)&ntrHeader);
	CardReset(false);
	tonccpy(gameTitle, ntrHeader.gameTitle, 12);
	// tonccpy(gameCode, ntrHeader.gameCode, 4);
	DoWait(25);
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
	while (1) {
		scanKeys();
		while (!keysDown())swiWaitForVBlank();
	}
	return 0;
}

static void CheckFolder() {
	if (sizeof(NitroSourceFileList) != sizeof(NitroDestFileList))return;
	bool copyNeeded = false;
	int listSize = 7;
	for (int i = 0; i < listSize; i++) {
		if (access(NitroDestFileList[i], F_OK) != 0) {
			copyNeeded = true;
			break;
		}
	}
	if (!copyNeeded)return;
	printf("\n\n\n\n\n\n\n\n\n   Setting up Stage2 folder");
	printf("\n\n        Please Wait...\n");
	
	u32 BufferSize = 0x40000;
	fileBuffer = (u8*)malloc(BufferSize);
	for (int i = 0; i < listSize; i++) {
		if (access(NitroDestFileList[i], F_OK) != 0) {
			FILE *src = fopen(NitroSourceFileList[i], "rb");
			if (src) {
				fseek(src, 0, SEEK_END);
				u32 fSize = ftell(src);
				// toncset((u8*)fileBuffer, 0xFF, fSize);
				fseek(src, 0, SEEK_SET);
				if (fSize <= BufferSize) {
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
		string filename = browseForFile(extensionList);
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
			int err = runNdsFile(c_args[0], c_args.size(), &c_args[0], launchdata);
			iprintf("Start failed. Error %i\n", err);
			break;
		}
		argarray.clear();
	}
	if (cartSelected) {
		if (cartInsertedOnBoot)DoCardInit(); // Currently required for bootlaoder to succeed with card init.
		// DS-Xtreme does not like running in TWL clock speeds.
		// (write function likely goes too fast and semi-bricks hidden sector region randomly when using official launcher)
		if (!memcmp(gameTitle, "D!S!XTREME", 9)) {
			launchdata.scfgUnlock = 0x00;
			launchdata.twlMode = 0x00;
			launchdata.twlCLK = 0x00;
			launchdata.twlVRAM = 0x00;
		}
		// Give launch soundfx time to finish if card Init already occured.
		if (!cartInsertedOnBoot)DoWait(29);
		runLaunchEngine(launchdata);
	}
	return stop();
}

void StartFileBrowser(tLauncherSettings launchdata, bool nitroFSMounted) {
	InitGUI();
	CheckFolder();
	BrowserUI(launchdata);
}

