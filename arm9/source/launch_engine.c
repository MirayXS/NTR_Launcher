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

#include <string.h>
#include <nds.h>

#include "load_bin.h"
#include "launch_engine.h"
#include "debugConsole.h"

#define LCDC_BANK_D (u16*)0x06860000

#define TMP_DATA 0x027FC000

typedef struct sLauncherSettings {
	u8 language;
	u8 scfgUnlock;
	u8 twlMode;
	u8 twlCLK;
	u8 twlVRAM;
	u8 debugMode;
} tLauncherSettings;

void vramcpy (void* dst, const void* src, int len) {
	u16* dst16 = (u16*)dst;
	u16* src16 = (u16*)src;
	
	for ( ; len > 0; len -= 2) { *dst16++ = *src16++; }
}	

extern void InitConsole();

extern bool ConsoleInit;

void runLaunchEngine (int language, bool scfgUnlock, bool TWLMODE, bool TWLCLK, bool TWLVRAM, bool debugMode) {
	
	// Always init console so bootloader's new console can display error codes if needed.
	if (!debugMode || !ConsoleInit) {  InitConsole();  } else { consoleClear(); }
	
	irqDisable(IRQ_ALL);
	// Direct CPU access to VRAM bank D
	VRAM_D_CR = VRAM_ENABLE | VRAM_D_LCD;
	
	// Clear VRAM
	memset (LCDC_BANK_D, 0x00, 128 * 1024);
	
	// Load the loader/patcher into the correct address
	vramcpy (LCDC_BANK_D, load_bin, load_bin_size);

	// Give the VRAM to the ARM7
	// nocashMessage("Give the VRAM to the ARM7");
	VRAM_D_CR = VRAM_ENABLE | VRAM_D_ARM7_0x06020000;
	
	// Reset into a passme loop
	nocashMessage("Reset into a passme loop");
	REG_EXMEMCNT |= ARM7_OWNS_ROM | ARM7_OWNS_CARD;

	REG_SCFG_EXT=0x83002000;
	
	// for (int i = 0; i < 10; i++) { swiWaitForVBlank(); }
	
	tLauncherSettings* tmpData = (tLauncherSettings*)TMP_DATA;
	
	tmpData->language = 0xFF;
	if (language != -1)tmpData->language = language;
	if (scfgUnlock)tmpData->scfgUnlock = 0x01;
	if (TWLMODE)tmpData->twlMode = 0x01;
	if (TWLCLK)tmpData->twlCLK = 0x01;
	if (TWLVRAM)tmpData->twlVRAM = 0x01;
	if (debugMode)tmpData->debugMode = 0x01;

	// Return to passme loop
	*(vu32*)0x027FFFFC = 0;
	*(vu32*)0x027FFE04 = (u32)0xE59FF018; // ldr pc, 0x02FFFE24
	*(vu32*)0x027FFE24 = (u32)0x02FFFE04;  // Set ARM9 Loop address --> resetARM9(0x027FFE04);	
	// Reset ARM7
	// nocashMessage("resetARM7");
	resetARM7(0x06020000);

	// swi soft reset
	// nocashMessage("swiSoftReset");
	swiSoftReset();
}

