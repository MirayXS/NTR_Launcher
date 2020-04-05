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

void VcountHandler() { inputGetAndSend(); }
void VblankHandler(void) { }

int PowerOnSlot() {
    REG_SCFG_MC = 0x04;    // set state=1
    while(REG_SCFG_MC&1);
    
    REG_SCFG_MC = 0x08;    // set state=2      
    while(REG_SCFG_MC&1);
    
    REG_ROMCTRL = 0x20000000; // set ROMCTRL=20000000h
    return 0;
}

int PowerOffSlot() {
    if(REG_SCFG_MC&1) return 1; 
    
    REG_SCFG_MC = 0x0C; // set state=3 
    while(REG_SCFG_MC&1);
    return 0;
}

int TWL_ResetSlot1() {
	PowerOffSlot();
	for (int i = 0; i < 30; i++) { swiWaitForVBlank(); }
	PowerOnSlot();
	return 0;
}

int main(void) {

	// read User Settings from firmware
	readUserSettings();
	irqInit();

	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	
	mmInstall(FIFO_MAXMOD);

	SetYtrigger(80);

	installSoundFIFO();
	installSystemFIFO();
	
	irqSet(IRQ_VCOUNT, VcountHandler);
	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT);
	
	i2cWriteRegister(0x4A, 0x12, 0x00);	// Press power-button for auto-reset
	i2cWriteRegister(0x4A, 0x70, 0x01);	// Bootflag = Warmboot/SkipHealthSafety

	// Make sure Arm9 had a chance to check slot status
	fifoWaitValue32(FIFO_USER_01);
	
	if(fifoCheckValue32(FIFO_USER_02)) { 
		if(fifoCheckValue32(FIFO_USER_04)) { TWL_ResetSlot1(); } else { PowerOnSlot(); }
	}	
	
	fifoSendValue32(FIFO_USER_03, 1);
	
	while (1) { swiWaitForVBlank(); }
}

