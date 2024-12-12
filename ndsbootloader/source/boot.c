/*-----------------------------------------------------------------
 boot.c

 BootLoader
 Loads a file into memory and runs it

 All resetMemory and startBinary functions are based
 on the MultiNDS loader by Darkain.
 Original source available at:
 http://cvs.sourceforge.net/viewcvs.py/ndslib/ndslib/examples/loader/boot/main.cpp

License:
 Copyright (C) 2005  Michael "Chishm" Chisholm

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

 If you use this code, please give due credit and email me about your
 project at chishm@hotmail.com

Helpful information:
 This code runs from VRAM bank C on ARM7
------------------------------------------------------------------*/

#include <nds/ndstypes.h>
#include <nds/dma.h>
#include <nds/system.h>
#include <nds/interrupts.h>
#include <nds/timers.h>
#include <nds/memory.h>
#include <nds/arm7/audio.h>
#include <nds/ipc.h>
#include <string.h>
#include "fat.h"
#include "card.h"
#include "boot.h"
#include "sdmmc.h"
#include "tonccpy.h"

#include "../../arm9/common/launcherData.h"

void arm7clearRAM();

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Important things
#define TEMP_MEM			0x02FFD000
#define TEMP_LAUNCHMEM		0x02FFDFF0
#define TWL_HEAD			0x02FFE000
#define NDS_HEAD			0x02FFFE00
#define NDS_HEAD_POKEMON	0x02FFF000

#define TEMP_ARM9_START_ADDRESS (*(vu32*)0x02FFFFF4)
tTWLHeader* ntrHeader;
tTWLHeader* twlHeader;

const char* bootName = "BOOT.NDS";

extern unsigned long _start;
extern unsigned long storedFileCluster;
extern unsigned long initDisc;
extern unsigned long wantToPatchDLDI;
extern unsigned long argStart;
extern unsigned long argSize;
extern unsigned long dsiSD;
extern unsigned long dsiMode;

volatile u16 scfgunlock = 0;
volatile u16 twlmode = 0;
volatile u16 twlclk = 0;
volatile u16 twlvram = 0;
volatile u16 twlram = 0;
volatile u32 chipID = 0;

static volatile u32 ROM_TID = 0;

static void setTWLMBK() {
	*(vu32*)REG_MBK1 = *(u32*)0x02FFE180;
	*(vu32*)REG_MBK2 = *(u32*)0x02FFE184;
	*(vu32*)REG_MBK3 = *(u32*)0x02FFE188;
	*(vu32*)REG_MBK4 = *(u32*)0x02FFE18C;
	*(vu32*)REG_MBK5 = *(u32*)0x02FFE190;
	REG_MBK6 = *(u32*)0x02FFE1A0;
	REG_MBK7 = *(u32*)0x02FFE1A4;
	REG_MBK8 = *(u32*)0x02FFE1A8;
	REG_MBK9 = *(u32*)0x02FFE1AC;
}


const char* getRomTid(const tNDSHeader* ndsHeader) {
	static char romTid[5];
	strncpy(romTid, ndsHeader->gameCode, 4);
	romTid[4] = '\0';
	return romTid;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff
#define FW_READ        0x03

void mpu_reset();
void mpu_reset_end();

static char boot_nds[] = "fat:/boot.nds";
static unsigned long argbuf[4];

#ifndef NO_SDMMC
int sdmmc_sd_readsectors(u32 sector_no, u32 numsectors, void *out);
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Main function
bool sdmmc_inserted() {	return true; }

bool sdmmc_startup() {
	sdmmc_controller_init(true);
	return sdmmc_sdcard_init() == 0;
}

bool sdmmc_readsectors(u32 sector_no, u32 numsectors, void *out) { return sdmmc_sdcard_readsectors(sector_no, numsectors, out) == 0; }
#endif


/*-------------------------------------------------------------------------
passArgs_ARM7
Copies the command line arguments to the end of the ARM9 binary,
then sets a flag in memory for the loaded NDS to use
--------------------------------------------------------------------------*/
void passArgs_ARM7 (void) {
	u32 ARM9_DST = *((u32*)(NDS_HEAD + 0x028));
	u32 ARM9_LEN = *((u32*)(NDS_HEAD + 0x02C));
	u32* argSrc;
	u32* argDst;

	if (!argStart || !argSize) {
		char *arg = boot_nds;
		argSize = __builtin_strlen(boot_nds);
		if (dsiSD) {
			arg++;
			arg[0] = 's';
			arg[1] = 'd';
		}
		__builtin_memcpy(argbuf,arg,argSize+1);
		argSrc = argbuf;
	} else {
		argSrc = (u32*)(argStart + (int)&_start);
	}

	if ( ARM9_DST == 0 && ARM9_LEN == 0) {
		ARM9_DST = *((u32*)(NDS_HEAD + 0x038));
		ARM9_LEN = *((u32*)(NDS_HEAD + 0x03C));
	}

	argDst = (u32*)((ARM9_DST + ARM9_LEN + 3) & ~3);		// Word aligned

	if ((twlmode > 0) && dsiMode && (*(u8*)(NDS_HEAD + 0x012) & BIT(1)))	{
		u32 ARM9i_DST = *((u32*)(TWL_HEAD + 0x1C8));
		u32 ARM9i_LEN = *((u32*)(TWL_HEAD + 0x1CC));
		if (ARM9i_LEN) {
			u32* argDst2 = (u32*)((ARM9i_DST + ARM9i_LEN + 3) & ~3);		// Word aligned
			if (argDst2 > argDst)argDst = argDst2;
		}
	}

	// copyLoop(argDst, argSrc, argSize);
	tonccpy(argDst, argSrc, argSize);

	__system_argv->argvMagic = ARGV_MAGIC;
	__system_argv->commandLine = (char*)argDst;
	__system_argv->length = argSize;
}

/*-------------------------------------------------------------------------
resetMemory_ARM7
Clears all of the NDS's RAM that is visible to the ARM7
Written by Darkain.
Modified by Chishm:
 * Added STMIA clear mem loop
--------------------------------------------------------------------------*/
void resetMemory_ARM7(void) {
	int i, reg;

	REG_IME = 0;

	for (i=0; i<16; i++) {
		SCHANNEL_CR(i) = 0;
		SCHANNEL_TIMER(i) = 0;
		SCHANNEL_SOURCE(i) = 0;
		SCHANNEL_LENGTH(i) = 0;
	}

	REG_SOUNDCNT = 0;
	REG_SNDCAP0CNT = 0;
	REG_SNDCAP1CNT = 0;

	REG_SNDCAP0DAD = 0;
	REG_SNDCAP0LEN = 0;
	REG_SNDCAP1DAD = 0;
	REG_SNDCAP1LEN = 0;

	// Clear out ARM7 DMA channels and timers
	for (i=0; i<4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
		if ((twlmode > 0) || dsiMode) {
			for (reg=0; reg<0x1c; reg+=4)*((u32*)(0x04004104 + ((i*0x1c)+reg))) = 0; //Reset NDMA.
		}
	}

	REG_RCNT = 0;

	// Clear out FIFO
	REG_IPC_SYNC = 0;
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;
	REG_IPC_FIFO_CR = 0;

	arm7clearRAM();

	// clear most of EXRAM
	toncset((u8*)0x02000000, 0, 0x3FC000); // clear more of EXRAM, skipping the arm9 temp area used by bootloader

	REG_IE = 0;
	REG_IF = ~0;
	REG_AUXIE = 0;
	REG_AUXIF = ~0;
	(*(vu32*)(0x04000000-4)) = 0;  //IRQ_HANDLER ARM7 version
	(*(vu32*)(0x04000000-8)) = ~0; //VBLANK_INTR_WAIT_FLAGS, ARM7 version
	REG_POWERCNT = 1;  //turn off power to stuffs
}

static void XMenuFix(const tNDSHeader* ndsHeader) {
	*((vu8*)0x02FFFF70) = 0x91;
	if ((twlram > 0) && !(ndsHeader->unitCode & BIT(1)))*((vu8*)0x027FFF70) = 0x91;
}

static void setMemoryAddressTWL(const tNDSHeader* ndsHeader) {
	if (ndsHeader->unitCode & BIT(1)) {
		// copyLoop((u32*)0x027FFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header
		tonccpy((u32*)0x027FFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header

		*(u32*)(0x027FA680) = 0x02FD4D80;
		*(u32*)(0x027FA684) = 0x00000000;
		*(u32*)(0x027FA688) = 0x00001980;

		*(u32*)(0x027FF00C) = 0x0000007F;
		*(u32*)(0x027FF010) = 0x550E25B8;
		*(u32*)(0x027FF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(u8*)(0x027FFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(u8*)(0x027FFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(u8*)(0x027FFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(u8*)(0x027FFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(u8*)(0x027FFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(u8*)(0x027FFD70) = 5;
		}
	}
	
    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)0x027FF800) = *(u32*)0x02FFFC00;					// CurrentCardID
	// *((u32*)0x027FF804) = *(u32*)0x02FFFC00;					// Command10CardID
	*((u16*)0x027FF808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027FF80A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027FF850) = 0x5835;
	// Copies of above
	*((u32*)0x027FFC00) = *(u32*)0x02FFFC00;					// CurrentCardID
	// *((u32*)0x027FFC04) = *(u32*)0x02FFFC00;					// Command10CardID
	*((u16*)0x027FFC08) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x027FFC0A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x027FFC10) = 0x5835;
	*((u16*)0x027FFC40) = 0x01;						// Boot Indicator -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
	
	tonccpy((u32*)0x027FC000, (u32*)0x02FFC000, 0x1000);
	tonccpy((u32*)0x027FF000, (u32*)NDS_HEAD_POKEMON, 0x170);
	tonccpy((u32*)0x027FFE00, (u32*)NDS_HEAD, 0x160);
	tonccpy((u32*)0x027FE000, (u32*)TWL_HEAD, 0x1000);
		
	tonccpy((u32*)0x027FF830, (u32*)0x02FFF830, 0x20);
	tonccpy((u32*)0x027FFC80, (u32*)0x02FFFC80, 0x70);
	tonccpy((u32*)0x027FFD80, (u32*)0x02FFFD80, 0x70);
}

static void setMemoryAddress(const tNDSHeader* ndsHeader) {
	if (ndsHeader->unitCode & BIT(1)) {
		// copyLoop((u32*)0x02FFFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header
		tonccpy((u32*)0x02FFFA80, (u32*)ndsHeader, 0x160);	// Make a duplicate of DS header

		*(u32*)(0x02FFA680) = 0x02FD4D80;
		*(u32*)(0x02FFA684) = 0x00000000;
		*(u32*)(0x02FFA688) = 0x00001980;

		*(u32*)(0x02FFF00C) = 0x0000007F;
		*(u32*)(0x02FFF010) = 0x550E25B8;
		*(u32*)(0x02FFF014) = 0x02FF4000;

		// Set region flag
		if (strncmp(getRomTid(ndsHeader)+3, "J", 1) == 0) {
			*(u8*)(0x02FFFD70) = 0;
		} else if (strncmp(getRomTid(ndsHeader)+3, "E", 1) == 0) {
			*(u8*)(0x02FFFD70) = 1;
		} else if (strncmp(getRomTid(ndsHeader)+3, "P", 1) == 0) {
			*(u8*)(0x02FFFD70) = 2;
		} else if (strncmp(getRomTid(ndsHeader)+3, "U", 1) == 0) {
			*(u8*)(0x02FFFD70) = 3;
		} else if (strncmp(getRomTid(ndsHeader)+3, "C", 1) == 0) {
			*(u8*)(0x02FFFD70) = 4;
		} else if (strncmp(getRomTid(ndsHeader)+3, "K", 1) == 0) {
			*(u8*)(0x02FFFD70) = 5;
		}
	}
	
	// Fix Pokemon games needing header data.
	// copyLoop((u32*)NDS_HEAD_POKEMON, (u32*)NDS_HEAD, 0x170);
	tonccpy((u32*)NDS_HEAD_POKEMON, (u32*)NDS_HEAD, 0x170);
	
    // Set memory values expected by loaded NDS
    // from NitroHax, thanks to Chism
	*((u32*)0x02FFF800) = chipID;					// CurrentCardID
	// *((u32*)0x02FFF804) = chipID;					// Command10CardID
	*((u16*)0x02FFF808) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x02FFF80A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x02FFF850) = 0x5835;
	// Copies of above
	*((u32*)0x02FFFC00) = chipID;					// CurrentCardID
	// *((u32*)0x02FFFC04) = chipID;					// Command10CardID
	*((u16*)0x02FFFC08) = ndsHeader->headerCRC16;	// Header Checksum, CRC-16 of [000h-15Dh]
	*((u16*)0x02FFFC0A) = ndsHeader->secureCRC16;	// Secure Area Checksum, CRC-16 of [ [20h]..7FFFh]
	*((u16*)0x02FFFC10) = 0x5835;
	*((u16*)0x02FFFC40) = 0x01;						// Boot Indicator -- EXTREMELY IMPORTANT!!! Thanks to cReDiAr
	
	switch (*((vu16*)0x02FFFF5E)) {
		case 0xF63D: XMenuFix(ndsHeader); break;
		case 0x0695: XMenuFix(ndsHeader); break;
		case 0xE4C4: XMenuFix(ndsHeader); break;
		case 0x918C: XMenuFix(ndsHeader); break;
	}
	
	if ((twlram > 0) && !(ndsHeader->unitCode & BIT(1)))setMemoryAddressTWL(ndsHeader);
}

static u8 readwriteSPI(u8 data) {
	REG_SPIDATA = data;
	SerialWaitBusy();
	return REG_SPIDATA;
}

//---------------------------------------------------------------------------------
void readFirmware(u32 address, void * destination, u32 size) {
//---------------------------------------------------------------------------------
	int oldIME=enterCriticalSection();
	u8 *buffer = destination;

	// Read command
	REG_SPICNT = SPI_ENABLE | SPI_BYTE_MODE | SPI_CONTINUOUS | SPI_DEVICE_FIRMWARE;
	readwriteSPI(FIRMWARE_READ);

	// Set the address
	readwriteSPI((address>>16) & 0xFF);
	readwriteSPI((address>> 8) & 0xFF);
	readwriteSPI((address) & 0xFF);

	u32 i;

	// Read the data
	for(i=0;i<size;i++) {
		buffer[i] = readwriteSPI(0);
	}

	REG_SPICNT = 0;
	leaveCriticalSection(oldIME);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Firmware stuff
static void my_readUserSettings(tNDSHeader* ndsHeader) {
	PERSONAL_DATA slot1;
	PERSONAL_DATA slot2;

	short slot1count, slot2count; //u8
	short slot1CRC, slot2CRC;

	u32 userSettingsBase;

	// Get settings location
	readFirmware(0x20, &userSettingsBase, 2);

	u32 slot1Address = userSettingsBase * 8;
	u32 slot2Address = userSettingsBase * 8 + 0x100;

	// Reload DS Firmware settings
	readFirmware(slot1Address, &slot1, sizeof(PERSONAL_DATA)); //readFirmware(slot1Address, personalData, 0x70);
	readFirmware(slot2Address, &slot2, sizeof(PERSONAL_DATA)); //readFirmware(slot2Address, personalData, 0x70);
	readFirmware(slot1Address + 0x70, &slot1count, 2); //readFirmware(slot1Address + 0x70, &slot1count, 1);
	readFirmware(slot2Address + 0x70, &slot2count, 2); //readFirmware(slot1Address + 0x70, &slot2count, 1);
	readFirmware(slot1Address + 0x72, &slot1CRC, 2);
	readFirmware(slot2Address + 0x72, &slot2CRC, 2);

	// Default to slot 1 user settings
	void *currentSettings = &slot1;

	short calc1CRC = swiCRC16(0xFFFF, &slot1, sizeof(PERSONAL_DATA));
	short calc2CRC = swiCRC16(0xFFFF, &slot2, sizeof(PERSONAL_DATA));

	// Bail out if neither slot is valid
	if (calc1CRC != slot1CRC && calc2CRC != slot2CRC)return;

	// If both slots are valid pick the most recent
	if (calc1CRC == slot1CRC && calc2CRC == slot2CRC) { 
		currentSettings = (slot2count == ((slot1count + 1) & 0x7f) ? &slot2 : &slot1); //if ((slot1count & 0x7F) == ((slot2count + 1) & 0x7F)) {
	} else {
		if (calc2CRC == slot2CRC)currentSettings = &slot2;
	}

	PERSONAL_DATA* personalData = (PERSONAL_DATA*)((u32)__NDSHeader - (u32)ndsHeader + (u32)PersonalData); //(u8*)((u32)ndsHeader - 0x180)

	tonccpy(PersonalData, currentSettings, sizeof(PERSONAL_DATA));

	if (personalData->language != 6 && ndsHeader->reserved1[8] == 0x80) {
		ndsHeader->reserved1[8] = 0;	// Patch iQue game to be region-free
		ndsHeader->headerCRC16 = swiCRC16(0xFFFF, ndsHeader, 0x15E);	// Fix CRC
	}
}

void loadBinary_ARM7 (u32 fileCluster) {
	// read NDS header
	fileRead ((char*)NDS_HEAD, fileCluster, 0, 0x170);
	
	// Load binaries into memory
	fileRead((char*)ntrHeader->arm9destination, fileCluster, ntrHeader->arm9romOffset, ntrHeader->arm9binarySize);
	fileRead((char*)ntrHeader->arm7destination, fileCluster, ntrHeader->arm7romOffset, ntrHeader->arm7binarySize);

	// first copy the header to its proper location, excluding
	// the ARM9 start address, so as not to start it
	TEMP_ARM9_START_ADDRESS = ntrHeader->arm9executeAddress;		// Store for later
	*(u32*)ntrHeader->arm9executeAddress = 0;
	dmaCopyWords(3, (void*)ntrHeader, (void*)NDS_HEAD, 0x170);
	
	ROM_TID = *(u32*)0x02FFFE0C;
	
	setMemoryAddress((tNDSHeader*)ntrHeader);
	
	if ((twlram > 0) && (ntrHeader->unitCode & BIT(1))) {
		// Read full TWL header
		fileRead((char*)TWL_HEAD, fileCluster, 0, 0x1000);
		tonccpy((void*)0x02FFC000, (void*)TWL_HEAD, 0x1000);
		
		if (twlHeader->arm9ibinarySize > 0)fileRead((char*)twlHeader->arm9idestination, fileCluster, twlHeader->arm9iromOffset, twlHeader->arm9ibinarySize);
		if (twlHeader->arm7ibinarySize > 0)fileRead((char*)twlHeader->arm7idestination, fileCluster, twlHeader->arm7iromOffset, twlHeader->arm7ibinarySize);
		
		if (twlmode > 0) {
			setTWLMBK();
			if (twlHeader->unitCode == 0x02)scfgunlock = 0;
		}
	}

	my_readUserSettings((tNDSHeader*)ntrHeader);
}

/*-------------------------------------------------------------------------
startBinary_ARM7
Jumps to the ARM7 NDS binary in sync with the display and ARM9
Written by Darkain.
Modified by Chishm:
 * Removed MultiNDS specific stuff
--------------------------------------------------------------------------*/
void startBinary_ARM7 (void) {
	REG_IME=0;
	while(REG_VCOUNT!=191);
	while(REG_VCOUNT==191);
	// copy NDS ARM9 start address into the header, starting ARM9
	*((vu32*)0x02FFFE24) = TEMP_ARM9_START_ADDRESS;
	ARM9_START_FLAG = 1;
	// Start ARM7
	VoidFn arm7code = *(VoidFn*)(0x2FFFE34);
	arm7code();
}

int main (void) {
	ntrHeader = (tTWLHeader*)NDS_HEAD;
	twlHeader = (tTWLHeader*)TWL_HEAD;
		
	tLauncherSettings* tmpData = (tLauncherSettings*)LAUNCH_DATA;
	
	if (tmpData->scfgUnlock > 0x00)scfgunlock = 0xFFFF;
	if (tmpData->twlMode > 0x00)twlmode = 0xFFFF;
	if (tmpData->twlCLK > 0x00)twlclk = 0xFFFF;
	if (tmpData->twlRAM > 0x00)twlram = 0xFFFF;
	if (tmpData->twlVRAM > 0x00)twlvram = 0xFFFF;
	if (tmpData->cachedChipID > 0x00)chipID = tmpData->cachedChipID;
	
	if ((twlmode == 0) && (twlram == 0)) {
		REG_MBK9=0xFCFFFF0F;
		REG_MBK6=0x09403900;
		REG_MBK7=0x09803940;
		REG_MBK8=0x09C03980;
	}
	
#ifdef NO_DLDI
	dsiSD = true;
	dsiMode = true;
#endif
#ifndef NO_SDMMC
	if (dsiSD && dsiMode) {
		_io_dldi.fn_readSectors = sdmmc_readsectors;
		_io_dldi.fn_isInserted = sdmmc_inserted;
		_io_dldi.fn_startup = sdmmc_startup;
	}
#endif
	u32 fileCluster = storedFileCluster;
	// Init card
	if(!FAT_InitFiles(initDisc))return -1;
	/* Invalid file cluster specified */
	if ((fileCluster < CLUSTER_FIRST) || (fileCluster >= CLUSTER_EOF))fileCluster = getBootFileCluster(bootName);
	if (fileCluster == CLUSTER_FREE)return -1;

	// ARM9 clears its memory part 2
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	tonccpy((void*)TEMP_MEM, (void*)resetMemory2_ARM9, resetMemory2_ARM9_size);
	(*(vu32*)0x02FFFE24) = (u32)TEMP_MEM;	// Make ARM9 jump to the function
	// Wait until the ARM9 has completed its task
	while ((*(vu32*)0x02FFFE24) == (u32)TEMP_MEM);

	// ARM9 sets up mpu
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	tonccpy((void*)TEMP_MEM, (void*)mpu_reset, mpu_reset_end - mpu_reset);
	(*(vu32*)0x02FFFE24) = (u32)TEMP_MEM;	// Make ARM9 jump to the function
	
	// Wait until the ARM9 has completed its task
	while ((*(vu32*)0x02FFFE24) == (u32)TEMP_MEM);

	// Get ARM7 to clear RAM
	resetMemory_ARM7();

	// ARM9 enters a wait loop
	// copy ARM9 function to RAM, and make the ARM9 jump to it
	tonccpy((void*)TEMP_MEM, (void*)startBinary_ARM9, startBinary_ARM9_size);
	(*(vu32*)0x02FFFE24) = (u32)TEMP_MEM;	// Make ARM9 jump to the function
		
	// Load the NDS file
	loadBinary_ARM7(fileCluster);
	
	// Fix for Pictochat and DLP
	if (ROM_TID == 0x41444E48 || ROM_TID == 0x41454E48
	 || ROM_TID == 0x43444E48 || ROM_TID == 0x43454E48
	 || ROM_TID == 0x4B444E48 || ROM_TID == 0x4B454E48) {
		(*(vu16*)0x02FFFCFA) = 0x1041;	// NoCash: channel ch1+7+13
	}
		
#ifndef NO_SDMMC
	if (dsiSD && dsiMode) {
		sdmmc_controller_init(true);
		*(vu16*)(SDMMC_BASE + REG_SDDATACTL32) &= 0xFFFDu;
		*(vu16*)(SDMMC_BASE + REG_SDDATACTL) &= 0xFFDDu;
		*(vu16*)(SDMMC_BASE + REG_SDBLKLEN32) = 0;
	}
#endif
	// Pass command line arguments to loaded program
	passArgs_ARM7();

	if (twlmode > 0) { 
		// REG_SCFG_EXT = 0x92FBFB06;
		// REG_SCFG_EXT = 0x8307F100;
		REG_SCFG_EXT = 0x93FFFB06;
	} else { 
		REG_SCFG_EXT = 0x92A40000;  // REG_SCFG_EXT |= BIT(18);
		REG_SCFG_ROM = 0x703;
	}
	if (twlclk > 0) { REG_SCFG_CLK = 0x187; } else { REG_SCFG_CLK = 0x107; }
	if (scfgunlock == 0)REG_SCFG_EXT &= ~(1UL << 31);

	startBinary_ARM7();
	return 0;
}

