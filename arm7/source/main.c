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
#include <nds/arm7/input.h>
#include <nds/system.h>

#include <maxmod7.h>

#include "resetslot.h"
#include "launch_engine_arm7.h"

void VcountHandler() {
	inputGetAndSend();
}

void VblankHandler(void) {
}

int main(void) {

	irqInit();
	fifoInit();

	// read User Settings from firmware
	readUserSettings();

	// Start the RTC tracking IRQ
	initClockIRQ();
	
	mmInstall(FIFO_MAXMOD);

	SetYtrigger(80);

	installSoundFIFO();
	installSystemFIFO();
	
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT);
	
	// Make sure Arm9 had a chance to check slot status
	fifoWaitValue32(FIFO_USER_01);
	// If Arm9 reported slot is powered off, have Arm7 wait for Arm9 to be ready before card reset. This makes sure arm7 doesn't try card reset too early.
	if(fifoCheckValue32(FIFO_USER_02)) { 
		if(fifoCheckValue32(FIFO_USER_07)) { TWL_ResetSlot1(); } else { PowerOnSlot(); }
	}
	fifoSendValue32(FIFO_USER_03, 1);
	
	while (1) {
		runLaunchEngineCheck();
		swiWaitForVBlank();
	}
}

