/*
 main.arm9.c

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

#define ARM9
#undef ARM7
#include <nds/memory.h>
#include <nds/arm9/video.h>
#include <nds/arm9/input.h>
#include <nds/interrupts.h>
#include <nds/dma.h>
#include <nds/timers.h>
#include <nds/system.h>
#include <nds/ipc.h>

#include "common.h"
#include "miniconsole.h"

#include "../../arm9/common/launcherData.h"

volatile tLauncherSettings* launchData = (tLauncherSettings*)LAUNCH_DATA;

ALIGN(4) volatile int arm9_stateFlag = ARM9_BOOT;
ALIGN(4) volatile u16 arm9_errorCode = 0xFFFF;
ALIGN(4) volatile u16 consoleDebugMode = 0;
ALIGN(4) volatile u32 arm9_BLANK_RAM = 0;
ALIGN(4) volatile u32 defaultFontPalSlot = 0;
ALIGN(4) volatile u32 arm9_cachedChipID = 0xFFFFFFFF;

ALIGN(4) volatile u16 language = 0xFFFF;
ALIGN(4) volatile u16 scfgUnlock = 0;
ALIGN(4) volatile u16 twlMode = 0;
ALIGN(4) volatile u16 twlCLK = 0;
ALIGN(4) volatile u16 isTWLSRL = 0;
// ALIGN(4) volatile u16 twlVRAM = 0;
ALIGN(4) volatile u16 twlRAM = 0;
ALIGN(4) volatile u16 debugMode = 0;
ALIGN(4) volatile u16 consoleInit = 0;


ALIGN(4) const u16 redFont = 0x801B;
ALIGN(4) const u16 greenFont = 0x8360;

ALIGN(4) char ERRTXT_NONE[14] = "\nSTATUS: NONE";
ALIGN(4) char ERRTXT_STS_START[22] = "\nSTATUS: LOADER START";
ALIGN(4) char ERRTXT_STS_CLRMEM[22] = "\nSTATUS: CLEAR MEMORY";
ALIGN(4) char ERRTXT_STS_LOAD_BIN[19] = "\nSTATUS: LOAD CART";
ALIGN(4) char ERRTXT_STS_STARTBIN[21] = "\nSTATUS: START BINARY";
ALIGN(4) char ERRTXT_LOAD_NORM[20] = "\nERROR: LOAD NORMAL";
ALIGN(4) char ERRTXT_LOAD_OTHR[19] = "\nERROR: LOAD OTHER";
ALIGN(4) char ERRTXT_SEC_NORM[22] = "\nERROR: SECURE NORMAL";
ALIGN(4) char ERRTXT_SEC_OTHR[21] = "\nERROR: SECURE OTHER";
ALIGN(4) char ERRTXT_HEAD_CRC[19] = "\nERROR: HEADER CRC";
	
/*-------------------------------------------------------------------------
External functions
--------------------------------------------------------------------------*/
extern void arm9_clearCache (void);
extern void arm9_reset (void);
extern void Print(char *str);

static void waitForVBlank() { while (REG_VCOUNT!=191); while (REG_VCOUNT==191); }

/*-------------------------------------------------------------------------
arm9_errorOutput
Displays an error text on screen.
Rewritten by Apache Thunder.
--------------------------------------------------------------------------*/
static void arm9_errorOutput (u16 code) {
	switch (code) {
		case ERR_NONE: {
			BG_PALETTE_SUB[defaultFontPalSlot] = greenFont;
			Print((char*)ERRTXT_NONE);
		} break;
		case ERR_STS_START: {
			BG_PALETTE_SUB[defaultFontPalSlot] = greenFont;
			Print((char*)ERRTXT_STS_START);
		} break;
		case ERR_STS_CLR_MEM: {
			BG_PALETTE_SUB[defaultFontPalSlot] = greenFont;
			Print((char*)ERRTXT_STS_CLRMEM);
		} break;
		case ERR_STS_LOAD_BIN: {
			BG_PALETTE_SUB[defaultFontPalSlot] = greenFont;
			Print((char*)ERRTXT_STS_LOAD_BIN);
		} break;
		case ERR_STS_STARTBIN: {
			BG_PALETTE_SUB[defaultFontPalSlot] = greenFont;
			Print((char*)ERRTXT_STS_STARTBIN);
		} break;
		case ERR_LOAD_NORM: {
			BG_PALETTE_SUB[defaultFontPalSlot] = redFont;
			Print((char*)ERRTXT_LOAD_NORM);
		} break;
		case ERR_LOAD_OTHR: {
			BG_PALETTE_SUB[defaultFontPalSlot] = redFont;
			Print((char*)ERRTXT_LOAD_OTHR);
		} break;
		case ERR_SEC_NORM: {
			BG_PALETTE_SUB[defaultFontPalSlot] = redFont;
			Print((char*)ERRTXT_SEC_NORM);
		} break;
		case ERR_SEC_OTHR: {
			BG_PALETTE_SUB[defaultFontPalSlot] = redFont;
			Print((char*)ERRTXT_SEC_OTHR);
		} break;
		case ERR_HEAD_CRC: {
			BG_PALETTE_SUB[defaultFontPalSlot] = redFont;
			Print((char*)ERRTXT_HEAD_CRC);
		} break;
		default: {
			BG_PALETTE_SUB[defaultFontPalSlot] = greenFont;
			Print((char*)ERR_NONE);
		} break;
	}
}

/*-------------------------------------------------------------------------
arm9_main
Clears the ARM9's icahce and dcache
Clears the ARM9's DMA channels and resets video memory
Jumps to the ARM9 NDS binary in sync with the  ARM7
Written by Darkain, modified by Chishm
--------------------------------------------------------------------------*/
void arm9_main (void) {
	register int i;
	
	language = 0xFFFF;
	scfgUnlock = 0;
	twlMode = 0;
	twlCLK = 0;
	// twlVRAM = 0;
	twlRAM = 0;
	isTWLSRL = 0;
	debugMode = 0;
	consoleInit = 0;

	if (launchData->language != 0xFF)language = (u16)launchData->language;
	if (launchData->scfgUnlock > 0)scfgUnlock = 0xFFFF;
	if (launchData->twlMode > 0)twlMode = 0xFFFF;
	// if (launchData->twlVRAM > 0)twlVRAM = 0xFFFF;
	if (launchData->twlRAM > 0)twlRAM = 0xFFFF;
	if (launchData->twlCLK > 0)twlCLK = 0xFFFF;
	if (launchData->isTWLSRL > 0)isTWLSRL = 0xFFFF;
	if (launchData->debugMode > 0)debugMode = 0xFFFF;
	arm9_cachedChipID = launchData->cachedChipID;
	
	if ((isTWLSRL > 0) && (twlMode > 0) && (twlRAM > 0)) {
		*(vu32*)REG_MBK1 = *(u32*)0x02FFE180;
		*(vu32*)REG_MBK2 = *(u32*)0x02FFE184;
		*(vu32*)REG_MBK3 = *(u32*)0x02FFE188;
		*(vu32*)REG_MBK4 = *(u32*)0x02FFE18C;
		*(vu32*)REG_MBK5 = *(u32*)0x02FFE190;
		REG_MBK6 = *(u32*)0x02FFE194;
		REG_MBK7 = *(u32*)0x02FFE198;
		REG_MBK8 = *(u32*)0x02FFE19C;
		REG_MBK9 = *(u32*)0x02FFE1AC;
		WRAM_CR = *(u8*)0x02FFE1AF;
		scfgUnlock = 0x01;
	} else {
		// MBK settings for NTR mode games
		*((vu32*)REG_MBK1)=0x8D898581;
		*((vu32*)REG_MBK2)=0x91898581;
		*((vu32*)REG_MBK3)=0x91999591;
		*((vu32*)REG_MBK4)=0x91898581;
		*((vu32*)REG_MBK5)=0x91999591;
		REG_MBK6 = 0x00003000;
		REG_MBK7 = 0x00003000;
		REG_MBK8 = 0x00003000;
	}
	
	//set shared ram to ARM7
	WRAM_CR = 0x03;
	REG_EXMEMCNT = 0xE880;

	// Disable interrupts
	REG_IME = 0;
	REG_IE = 0;
	REG_IF = ~0;
	
	if (debugMode > 0)arm9_errorCode = ERR_STS_START;

	// Synchronise start
	ipcSendState(ARM9_START);
	while (ipcRecvState() != ARM7_START);

	ipcSendState(ARM9_MEMCLR);

	arm9_clearCache();

	for (i=0; i<16*1024; i+=4) {  //first 16KB
		(*(vu32*)(i+0x00000000)) = 0x00000000;      //clear ITCM
		(*(vu32*)(i+0x00800000)) = 0x00000000;      //clear DTCM
	}

	for (i=16*1024; i<32*1024; i+=4) {  //second 16KB
		(*(vu32*)(i+0x00000000)) = 0x00000000;      //clear ITCM
	}

	(*(vu32*)0x00803FFC) = 0;   //IRQ_HANDLER ARM9 version
	(*(vu32*)0x00803FF8) = ~0;  //VBLANK_INTR_WAIT_FLAGS ARM9 version

	// Clear out FIFO
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	// Blank out VRAM
	VRAM_A_CR = 0x80;
	VRAM_B_CR = 0x80;
// Don't mess with the VRAM used for execution
//	VRAM_D_CR = 0x80; 
	VRAM_E_CR = 0x80;
	VRAM_F_CR = 0x80;
	VRAM_G_CR = 0x80;
	VRAM_H_CR = 0x80;
	VRAM_I_CR = 0x80;

	// Clear out ARM9 DMA channels
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}
	
	VRAM_A_CR = 0;
	VRAM_B_CR = 0;
//	Don't mess with the VRAM used for execution
//	VRAM_D_CR = 0;
	VRAM_E_CR = 0;
	VRAM_F_CR = 0;
	VRAM_G_CR = 0;
	VRAM_H_CR = 0;
	VRAM_I_CR = 0;
	
	BG_PALETTE[0] = 0xFFFF;
	
	videoSetMode(0);
	
	dmaFill((void*)&arm9_BLANK_RAM, VRAM_A,  256*1024);		// Banks A, B
	dmaFill((void*)&arm9_BLANK_RAM, VRAM_E,  128*1024);		// Banks E, F, G, H, I
	
	// set ARM9 state to ready and wait for instructions from ARM7
	ipcSendState(ARM9_READY);
	while (ipcRecvState() != ARM7_BOOTBIN) {
		if (ipcRecvState() == ARM7_ERR) {
			if (!consoleInit) {
				BG_PALETTE_SUB[0] = RGB15(31,31,31);
				BG_PALETTE_SUB[255] = RGB15(0,0,0);
				defaultFontPalSlot = 31;
				miniconsoleSetWindow(5, 11, 24, 1); // Set console position for debug text if/when needed.
				consoleInit = true;
			}
			arm9_errorOutput (arm9_errorCode);
			// Halt after displaying error code
			while(1)waitForVBlank();
		} else if ((arm9_errorCode != ERR_NONE) && debugMode > 0) {
			if (!consoleInit) {
				BG_PALETTE_SUB[0] = RGB15(31,31,31);
				BG_PALETTE_SUB[255] = RGB15(0,0,0);
				defaultFontPalSlot = 31;
				miniconsoleSetWindow(5, 11, 24, 1); // Set console position for debug text if/when needed.
				consoleInit = true;
			}
			waitForVBlank();
			arm9_errorOutput (arm9_errorCode);
			arm9_errorCode = ERR_NONE;
		}
	}
	
	VRAM_C_CR = 0x80;
	// BG_PALETTE_SUB[0] = 0xFFFF;
	dmaFill((void*)&arm9_BLANK_RAM, BG_PALETTE+1, (2*1024)-2);
	dmaFill((void*)&arm9_BLANK_RAM, OAM,     2*1024);

	// Clear out display registers
	vu16 *mainregs = (vu16*)0x04000000;
	vu16 *subregs = (vu16*)0x04001000;
	for (i=0; i<43; i++) { mainregs[i] = 0; subregs[i] = 0; }
	REG_DISPSTAT = 0;
	VRAM_C_CR = 0;
	videoSetModeSub(0);
	REG_POWERCNT  = 0x820F;
	
	/*if ((twlRAM > 0) && ((twlMode == 0) || (isTWLSRL == 0))) {
		*((u32*)0x027FF800) = arm9_cachedChipID;	
		*((u32*)0x027FFC00) = arm9_cachedChipID;
	}
	*((u32*)0x02FFF800) = arm9_cachedChipID;	
	*((u32*)0x02FFFC00) = arm9_cachedChipID;*/
	
	if (twlCLK == 0) { REG_SCFG_CLK = 0x80; } else { REG_SCFG_CLK = 0x87; };
	if (scfgUnlock == 0)REG_SCFG_EXT &= ~(1UL << 31);

	arm9_reset();
}

