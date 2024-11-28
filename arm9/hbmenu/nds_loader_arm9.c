/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2010
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy

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
#include <string.h>
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <sys/stat.h>
#include <limits.h>

#include <unistd.h>
#include <fat.h>

#include "load2_bin.h"

#include "nds_loader_arm9.h"
#include "launcherData.h"

#define LCDC_BANK_D (u16*)0x06860000
#define STORED_FILE_CLUSTER (*(((u32*)LCDC_BANK_D) + 1))
#define INIT_DISC (*(((u32*)LCDC_BANK_D) + 2))
#define WANT_TO_PATCH_DLDI (*(((u32*)LCDC_BANK_D) + 3))

#define STORED_FILE_CLUSTER_OFFSET 4 
#define INIT_DISC_OFFSET 8
#define WANT_TO_PATCH_DLDI_OFFSET 12
#define ARG_START_OFFSET 16
#define ARG_SIZE_OFFSET 20
#define HAVE_DSISD_OFFSET 28
#define DSIMODE_OFFSET 32

typedef signed int addr_t;
typedef unsigned char data_t;

#define FIX_ALL	0x01
#define FIX_GLUE	0x02
#define FIX_GOT	0x04
#define FIX_BSS	0x08

/*enum DldiOffsets {
	DO_magicString = 0x00,			// "\xED\xA5\x8D\xBF Chishm"
	DO_magicToken = 0x00,			// 0xBF8DA5ED
	DO_magicShortString = 0x04,		// " Chishm"
	DO_version = 0x0C,
	DO_driverSize = 0x0D,
	DO_fixSections = 0x0E,
	DO_allocatedSpace = 0x0F,

	DO_friendlyName = 0x10,

	DO_text_start = 0x40,			// Data start
	DO_data_end = 0x44,				// Data end
	DO_glue_start = 0x48,			// Interworking glue start	-- Needs address fixing
	DO_glue_end = 0x4C,				// Interworking glue end
	DO_got_start = 0x50,			// GOT start					-- Needs address fixing
	DO_got_end = 0x54,				// GOT end
	DO_bss_start = 0x58,			// bss start					-- Needs setting to zero
	DO_bss_end = 0x5C,				// bss end

	// IO_INTERFACE data
	DO_ioType = 0x60,
	DO_features = 0x64,
	DO_startup = 0x68,	
	DO_isInserted = 0x6C,	
	DO_readSectors = 0x70,	
	DO_writeSectors = 0x74,
	DO_clearStatus = 0x78,
	DO_shutdown = 0x7C,
	DO_code = 0x80
};*/

static addr_t readAddr (data_t *mem, addr_t offset) { return ((addr_t*)mem)[offset/sizeof(addr_t)]; }

static void writeAddr (data_t *mem, addr_t offset, addr_t value) { ((addr_t*)mem)[offset/sizeof(addr_t)] = value; }

static void vramcpy (void* dst, const void* src, int len) {
	u16* dst16 = (u16*)dst;
	u16* src16 = (u16*)src;
	
	for ( ; len > 0; len -= 2) { *dst16++ = *src16++; }
}	

/* static addr_t quickFind (const data_t* data, const data_t* search, size_t dataLen, size_t searchLen) {
	const int* dataChunk = (const int*) data;
	int searchChunk = ((const int*)search)[0];
	addr_t i;
	addr_t dataChunkEnd = (addr_t)(dataLen / sizeof(int));

	for ( i = 0; i < dataChunkEnd; i++) {
		if (dataChunk[i] == searchChunk) {
			if ((i*sizeof(int) + searchLen) > dataLen)return -1;
			if (memcmp (&data[i*sizeof(int)], search, searchLen) == 0)return i*sizeof(int);
		}
	}
	return -1;
}*/

// Normal DLDI uses "\xED\xA5\x8D\xBF Chishm"
// Bootloader string is different to avoid being patched
// static const data_t dldiMagicLoaderString[] = "\xEE\xA5\x8D\xBF Chishm";	// Different to a normal DLDI file

/*#define DEVICE_TYPE_DLDI 0x49444C44

static bool dldiPatchLoader (data_t *binData, u32 binSize, bool clearBSS) {
	addr_t memOffset;			// Offset of DLDI after the file is loaded into memory
	addr_t patchOffset;			// Position of patch destination in the file
	addr_t relocationOffset;	// Value added to all offsets within the patch to fix it properly
	addr_t ddmemOffset;			// Original offset used in the DLDI file
	addr_t ddmemStart;			// Start of range that offsets can be in the DLDI file
	addr_t ddmemEnd;			// End of range that offsets can be in the DLDI file
	addr_t ddmemSize;			// Size of range that offsets can be in the DLDI file

	addr_t addrIter;

	data_t *pDH;
	data_t *pAH;

	size_t dldiFileSize = 0;
	
	// Find the DLDI reserved space in the file
	patchOffset = quickFind (binData, dldiMagicLoaderString, binSize, sizeof(dldiMagicLoaderString));

	if (patchOffset < 0)return false; // does not have a DLDI section

	pDH = (data_t*)(io_dldi_data);
	
	pAH = &(binData[patchOffset]);

	if (*((u32*)(pDH + DO_ioType)) == DEVICE_TYPE_DLDI)return false; // No DLDI patch

	if (pDH[DO_driverSize] > pAH[DO_allocatedSpace])return false; // Not enough space for patch
	
	dldiFileSize = 1 << pDH[DO_driverSize];

	memOffset = readAddr (pAH, DO_text_start);
	if (memOffset == 0)memOffset = readAddr (pAH, DO_startup) - DO_code;
	ddmemOffset = readAddr (pDH, DO_text_start);
	relocationOffset = memOffset - ddmemOffset;

	ddmemStart = readAddr (pDH, DO_text_start);
	ddmemSize = (1 << pDH[DO_driverSize]);
	ddmemEnd = ddmemStart + ddmemSize;

	// Remember how much space is actually reserved
	pDH[DO_allocatedSpace] = pAH[DO_allocatedSpace];
	// Copy the DLDI patch into the application
	vramcpy (pAH, pDH, dldiFileSize);

	// Fix the section pointers in the header
	writeAddr (pAH, DO_text_start, readAddr (pAH, DO_text_start) + relocationOffset);
	writeAddr (pAH, DO_data_end, readAddr (pAH, DO_data_end) + relocationOffset);
	writeAddr (pAH, DO_glue_start, readAddr (pAH, DO_glue_start) + relocationOffset);
	writeAddr (pAH, DO_glue_end, readAddr (pAH, DO_glue_end) + relocationOffset);
	writeAddr (pAH, DO_got_start, readAddr (pAH, DO_got_start) + relocationOffset);
	writeAddr (pAH, DO_got_end, readAddr (pAH, DO_got_end) + relocationOffset);
	writeAddr (pAH, DO_bss_start, readAddr (pAH, DO_bss_start) + relocationOffset);
	writeAddr (pAH, DO_bss_end, readAddr (pAH, DO_bss_end) + relocationOffset);
	// Fix the function pointers in the header
	writeAddr (pAH, DO_startup, readAddr (pAH, DO_startup) + relocationOffset);
	writeAddr (pAH, DO_isInserted, readAddr (pAH, DO_isInserted) + relocationOffset);
	writeAddr (pAH, DO_readSectors, readAddr (pAH, DO_readSectors) + relocationOffset);
	writeAddr (pAH, DO_writeSectors, readAddr (pAH, DO_writeSectors) + relocationOffset);
	writeAddr (pAH, DO_clearStatus, readAddr (pAH, DO_clearStatus) + relocationOffset);
	writeAddr (pAH, DO_shutdown, readAddr (pAH, DO_shutdown) + relocationOffset);

	if (pDH[DO_fixSections] & FIX_ALL) { 
		// Search through and fix pointers within the data section of the file
		for (addrIter = (readAddr(pDH, DO_text_start) - ddmemStart); addrIter < (readAddr(pDH, DO_data_end) - ddmemStart); addrIter++) {
			if ((ddmemStart <= readAddr(pAH, addrIter)) && (readAddr(pAH, addrIter) < ddmemEnd)) {
				writeAddr (pAH, addrIter, readAddr(pAH, addrIter) + relocationOffset);
			}
		}
	}

	if (pDH[DO_fixSections] & FIX_GLUE) { 
		// Search through and fix pointers within the glue section of the file
		for (addrIter = (readAddr(pDH, DO_glue_start) - ddmemStart); addrIter < (readAddr(pDH, DO_glue_end) - ddmemStart); addrIter++) {
			if ((ddmemStart <= readAddr(pAH, addrIter)) && (readAddr(pAH, addrIter) < ddmemEnd)) {
				writeAddr (pAH, addrIter, readAddr(pAH, addrIter) + relocationOffset);
			}
		}
	}

	if (pDH[DO_fixSections] & FIX_GOT) { 
		// Search through and fix pointers within the Global Offset Table section of the file
		for (addrIter = (readAddr(pDH, DO_got_start) - ddmemStart); addrIter < (readAddr(pDH, DO_got_end) - ddmemStart); addrIter++) {
			if ((ddmemStart <= readAddr(pAH, addrIter)) && (readAddr(pAH, addrIter) < ddmemEnd)) {
				writeAddr (pAH, addrIter, readAddr(pAH, addrIter) + relocationOffset);
			}
		}
	}

	if (clearBSS && (pDH[DO_fixSections] & FIX_BSS)) { 
		// Initialise the BSS to 0, only if the disc is being re-inited
		memset (&pAH[readAddr(pDH, DO_bss_start) - ddmemStart] , 0, readAddr(pDH, DO_bss_end) - readAddr(pDH, DO_bss_start));
	}

	return true;
}*/

ITCM_CODE static void SetSCFG(tLauncherSettings launchData) {
	/*if (launchData->twlMode > 0) {
		*((vu32*)REG_MBK1)=0x8D898581;
		*((vu32*)REG_MBK2)=0x8C888480;
		*((vu32*)REG_MBK3)=0x9C989490;
		*((vu32*)REG_MBK4)=0x8C888480;
		*((vu32*)REG_MBK5)=0x9C989490;
		REG_MBK6=0x00000000;
		REG_MBK7=0x07C03740;
		REG_MBK8=0x07403700;
	} else if ((launchData->twlMode == 0) && (launchData->twlRAM == 0)) {
		// MBK settings for NTR mode games
		*((vu32*)REG_MBK1)=0x8D898581;
		*((vu32*)REG_MBK2)=0x91898581;
		*((vu32*)REG_MBK3)=0x91999591;
		*((vu32*)REG_MBK4)=0x91898581;
		*((vu32*)REG_MBK5)=0x91999591;
		REG_MBK6 = 0x00003000;
		REG_MBK7 = 0x00003000;
		REG_MBK8 = 0x00003000;
	}*/

	if (launchData.twlCLK == 0) { REG_SCFG_CLK = 0x80; } else { REG_SCFG_CLK = 0x87; };
	
	if (launchData.twlMode > 0) {
		REG_SCFG_EXT = 0x8307F100;
		REG_SCFG_RST = 1;
	} else {
		REG_SCFG_EXT = 0x8300E000;
	}
		
	if (launchData.twlVRAM == 0)REG_SCFG_EXT &= ~(1UL << 13);
	if (launchData.twlRAM == 0) {
		REG_SCFG_EXT &= ~(1UL << 14);
		REG_SCFG_EXT &= ~(1UL << 15);
	}

	if ((launchData.isTWLSRL > 0) && (launchData.twlRAM > 0) && (launchData.twlMode > 0)) {
		*(vu32*)REG_MBK1 = *(u32*)0x02FFE180;
		*(vu32*)REG_MBK2 = *(u32*)0x02FFE184;
		*(vu32*)REG_MBK3 = *(u32*)0x02FFE188;
		*(vu32*)REG_MBK4 = *(u32*)0x02FFE18C;
		*(vu32*)REG_MBK5 = *(u32*)0x02FFE190;
		REG_MBK6 = *(u32*)0x02FFE194;
		REG_MBK7 = *(u32*)0x02FFE198;
		REG_MBK8 = *(u32*)0x02FFE19C;
		REG_MBK9 = *(u32*)0x02FFE1AC;
		WRAM_CR  =  *(u8*)0x02FFE1AF;
	} else {
		*((vu32*)REG_MBK1)=0x8D898581;
		*((vu32*)REG_MBK2)=0x91898581;
		*((vu32*)REG_MBK3)=0x91999591;
		*((vu32*)REG_MBK4)=0x91898581;
		*((vu32*)REG_MBK5)=0x91999591;
		REG_MBK6 = 0x00003000;
		REG_MBK7 = 0x00003000;
		REG_MBK8 = 0x00003000;
		if (launchData.scfgUnlock == 0)REG_SCFG_EXT &= ~(1UL << 31);
	}
	
	for (int i = 0; i < 10; i++) { while(REG_VCOUNT!=191); while(REG_VCOUNT==191); }
}


eRunNdsRetCode runNds (const void* loader, u32 loaderSize, u32 cluster, bool initDisc, bool dldiPatchNds, int argc, const char** argv, tLauncherSettings launchdata) {
	char* argStart;
	u16* argData;
	u16 argTempVal = 0;
	int argSize;
	const char* argChar;
		
	if (launchdata.twlMode == 0) {
		fifoSendValue32(FIFO_USER_01, 1);
		fifoWaitValue32(FIFO_USER_02);
	}

	irqDisable(IRQ_ALL);

	SetSCFG(launchdata);

	// Direct CPU access to VRAM bank C
	VRAM_D_CR = (VRAM_ENABLE | VRAM_D_LCD);
	// Load the loader/patcher into the correct address
	vramcpy (LCDC_BANK_D, loader, loaderSize);

	// Set the parameters for the loader
	// STORED_FILE_CLUSTER = cluster;
	writeAddr ((data_t*) LCDC_BANK_D, STORED_FILE_CLUSTER_OFFSET, cluster);
	// INIT_DISC = initDisc;
	writeAddr ((data_t*) LCDC_BANK_D, INIT_DISC_OFFSET, initDisc);

	writeAddr ((data_t*) LCDC_BANK_D, DSIMODE_OFFSET, isDSiMode());
	// if(argv[0][0]=='s' && argv[0][1]=='d') {
	dldiPatchNds = false;
	writeAddr ((data_t*) LCDC_BANK_D, HAVE_DSISD_OFFSET, 1);
	// }

	// WANT_TO_PATCH_DLDI = dldiPatchNds;
	// writeAddr ((data_t*) LCDC_BANK_D, WANT_TO_PATCH_DLDI_OFFSET, dldiPatchNds);
	writeAddr ((data_t*) LCDC_BANK_D, WANT_TO_PATCH_DLDI_OFFSET, false);
	// Give arguments to loader
	argStart = (char*)LCDC_BANK_D + readAddr((data_t*)LCDC_BANK_D, ARG_START_OFFSET);
	argStart = (char*)(((int)argStart + 3) & ~3);	// Align to word
	argData = (u16*)argStart;
	argSize = 0;
	
	for (; argc > 0 && *argv; ++argv, --argc)  {
		for (argChar = *argv; *argChar != 0; ++argChar, ++argSize)  {
			if (argSize & 1)  {
				argTempVal |= (*argChar) << 8;
				*argData = argTempVal;
				++argData;
			} else {
				argTempVal = *argChar;
			}
		}
		if (argSize & 1) { *argData = argTempVal; ++argData; }
		argTempVal = 0;
		++argSize;
	}
	
	*argData = argTempVal;
	
	writeAddr ((data_t*) LCDC_BANK_D, ARG_START_OFFSET, (addr_t)argStart - (addr_t)LCDC_BANK_D);
	writeAddr ((data_t*) LCDC_BANK_D, ARG_SIZE_OFFSET, argSize);

	/*if(dldiPatchNds) {
		// Patch the loader with a DLDI for the card
		if (!dldiPatchLoader ((data_t*)LCDC_BANK_D, loaderSize, initDisc))return RUN_NDS_PATCH_DLDI_FAILED;
	}*/

	irqDisable(IRQ_ALL);
	
	*(tLauncherSettings*)LAUNCH_DATA = launchdata;

	// Give the VRAM to the ARM7
	VRAM_D_CR = (VRAM_ENABLE | VRAM_D_ARM7_0x06020000);
	// Reset into a passme loop
	REG_EXMEMCNT |= (ARM7_OWNS_ROM | ARM7_OWNS_CARD);
	*((vu32*)0x02FFFFFC) = 0;
	*((vu32*)0x02FFFE04) = (u32)0xE59FF018;
	*((vu32*)0x02FFFE24) = (u32)0x02FFFE04;

	resetARM7(0x06020000);

	swiSoftReset(); 
	return RUN_NDS_OK;
}

eRunNdsRetCode runNdsFile (const char* filename, int argc, const char** argv, tLauncherSettings launchData)  {
	struct stat st;
	char filePath[PATH_MAX];
	int pathLen;
	const char* args[1];

	
	if (stat (filename, &st) < 0)return RUN_NDS_STAT_FAILED;

	if (argc <= 0 || !argv) {
		// Construct a command line if we weren't supplied with one
		if (!getcwd (filePath, PATH_MAX))return RUN_NDS_GETCWD_FAILED;
		pathLen = strlen (filePath);
		strcpy (filePath + pathLen, filename);
		args[0] = filePath;
		argv = args;
	}

	// bool havedsiSD = false;

	// if(argv[0][0]=='s' && argv[0][1]=='d') havedsiSD = true;
	
	return runNds (load2_bin, load2_bin_size, st.st_ino, true, true, argc, argv, launchData);
}

