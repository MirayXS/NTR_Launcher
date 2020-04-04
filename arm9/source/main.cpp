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
#include "launch_engine.h"
#include "crc.h"
#include "version.h" 

sNDSHeader ndsHeader;

off_t getFileSize(const char *fileName)
{
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

bool consoleOn = false;

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
	bool slot1Init = false;
	
	int language = -1;	
	
	u32 ndsHeader[0x80];

	BootSplashInit();

	if (REG_SCFG_MC == 0x11) {
		do { CartridgePrompt(); }
		while (REG_SCFG_MC == 0x11);
		fifoSendValue32(FIFO_USER_02, 1);
		for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
	}

	if (fatInitDefault()) {
		CIniFile ntrlauncher_config( "sd:/nds/NTR_Launcher.ini" );
		
		TWLCLK = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLCLOCK",0);
		TWLVRAM = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLVRAM",0);
		TWLEXTRAM = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLEXTRAM",0);
		TWLMODE = ntrlauncher_config.GetInt("NTRLAUNCHER","TWLMODE",0);		
		soundFreq = ntrlauncher_config.GetInt("NTRLAUNCHER","SOUNDFREQ",0);
		scfgUnlock = ntrlauncher_config.GetInt("NTRLAUNCHER","SCFGUNLOCK",0);
		language = ntrlauncher_config.GetInt("NTRLAUNCHER", "LANGUAGE", -1);
		slot1Init = ntrlauncher_config.GetInt("NTRLAUNCHER","RESETSLOT1",0);
				
		if(slot1Init) {
			fifoSendValue32(FIFO_USER_04, 1);
			// disableSlot1();
			for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
			// enableSlot1();
		}
	} else {
		fifoSendValue32(FIFO_USER_04, 1);
	}
	
	// If card is inserted but slot is powered off, turn slot-1 back on. This can happen with certain flashcarts that do not show up
	// in DSi's System Menu. The console will always boot with the slot powered off for these type of cards.
	// This is not an issue on 3DS however. TWL_FIRM doesn't care and will still power slot-1 as long as some kind of valid cart is
	// inserted.
	if(REG_SCFG_MC == 0x10) { fifoSendValue32(FIFO_USER_02, 1); }
	
	if( TWLCLK == false )  { 
		REG_SCFG_CLK = 0x80;
		swiWaitForVBlank();
	}
	
	fifoSendValue32(FIFO_USER_01, 1);
	fifoWaitValue32(FIFO_USER_03);
	
	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }
	
	sysSetCardOwner (BUS_OWNER_ARM9);
	
	cardReadHeader((uint8*)&ndsHeader);
	
	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }
	
	// memcpy (gameid, ((const char*)ndsHeader) + 12, 4);	
	// for (int i = 0; i < 15; i++) { swiWaitForVBlank(); }

	while(1) {
		// If SCFG_MC is returning as zero/null, this means SCFG_EXT registers are locked on arm9 or user attempted to run this while in NTR mode.
		if((REG_SCFG_MC == 0x00) | (REG_SCFG_MC == 0x11) | (REG_SCFG_MC == 0x10)) {
			ErrorScreen();
			for (int i = 0; i < 300; i++) { swiWaitForVBlank(); }
			break;
		} else {
			runLaunchEngine (EnableSD, language, scfgUnlock, TWLMODE, TWLCLK, TWLVRAM, soundFreq, TWLEXTRAM);
		}
	}
	return 0;
}

