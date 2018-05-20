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
#include "nds_card.h"
#include "launch_engine.h"
#include "crc.h"
#include "version.h" 

int main() {

	defaultExceptionHandler();
	
	bool TWLCLOCK = false;
	
	// If slot is powered off, tell Arm7 slot power on is required.
	if(REG_SCFG_MC == 0x11) { enableSlot1(); }
	if(REG_SCFG_MC == 0x10) { enableSlot1(); }

	u32 ndsHeader[0x80];
	char gameid[4];

	BootSplashInit();

	if (fatInitDefault()) {
		CIniFile ntrlauncher_config( "sd:/nds/NTR_Launcher.ini" );
		
		if(ntrlauncher_config.GetInt("NTRLAUNCHER","RESETSLOT1",0) == 0) { /* */ } else {
			disableSlot1();
			for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
			enableSlot1();
		}

		if(ntrlauncher_config.GetInt("NTRLAUNCHER","TWLCLOCK",0) == 0) {
			fifoSendValue32(FIFO_USER_01, 1);
			REG_SCFG_CLK = 0x80;
			swiWaitForVBlank();
		} else { REG_SCFG_CLK = 0x85; TWLCLOCK = true; }
		
	} else {
		disableSlot1();
		for (int i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
	}

	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }
	
	sysSetCardOwner (BUS_OWNER_ARM9);

	getHeader (ndsHeader);
	
	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }
	
	memcpy (gameid, ((const char*)ndsHeader) + 12, 4);

	while(1) {
		if(REG_SCFG_MC == 0x11) {
			ErrorScreen();
			for (int i = 0; i < 300; i++) { swiWaitForVBlank(); }
			break;
		} else {
			runLaunchEngine(TWLCLOCK);
		}
	}
	return 0;
}

