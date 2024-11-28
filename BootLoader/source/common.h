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

#ifndef _COMMON_H
#define _COMMON_H

#include <nds/dma.h>
#include <nds/ipc.h>
#include <stdlib.h>

#define resetCpu() \
		__asm volatile("swi 0x000000")

#include "../../arm9/common/launcherData.h"

ALIGN(4) extern volatile u16 arm9_errorCode;
ALIGN(4) extern volatile tLauncherSettings* launchData;
ALIGN(4) extern volatile u16 language;
ALIGN(4) extern volatile u16 scfgUnlock;
ALIGN(4) extern volatile u16 twlMode;
// ALIGN(4) extern volatile u16 twlVRAM;
ALIGN(4) extern volatile u16 twlRAM;
ALIGN(4) extern volatile u16 twlCLK;
ALIGN(4) extern volatile u16 isTWLSRL;
ALIGN(4) extern volatile u16 debugMode;

ALIGN(4) enum ERROR_CODES {
	ERR_NONE = (u16)0x00,
	ERR_STS_STARTBIN = (u16)0x01,
	ERR_STS_CLR_MEM = (u16)0x02,
	ERR_STS_LOAD_BIN = (u16)0x03,
	ERR_STS_START = (u16)0x04,
	// initCard error codes:
	ERR_LOAD_NORM = (u16)0x05,
	ERR_LOAD_OTHR = (u16)0x06,
	ERR_SEC_NORM = (u16)0x07,
	ERR_SEC_OTHR = (u16)0x08,
	ERR_HEAD_CRC = (u16)0x09,
	ERR_UNKNOWN = (u16)0x10,
	ERR_UNKNOWN2 = (u16)0x11
};

// Values fixed so they can be shared with ASM code
ALIGN(4) enum ARM9_STATE {
	ARM9_BOOT = 0,
	ARM9_START = 1,
	ARM9_RESET = 2,
	ARM9_READY = 3,
	ARM9_MEMCLR = 4
};

ALIGN(4) enum ARM7_STATE {
	ARM7_BOOT = 0,
	ARM7_START = 1,
	ARM7_RESET = 2,
	ARM7_READY = 3,
	ARM7_MEMCLR = 4,
	ARM7_LOADBIN = 5,
	ARM7_HOOKBIN = 6,
	ARM7_BOOTBIN = 7,
	ARM7_ERR = 8
};

ALIGN(4) static inline void dmaFill(const void* src, void* dest, uint32 size) {
	DMA_SRC(3)  = (uint32)src;
	DMA_DEST(3) = (uint32)dest;
	DMA_CR(3)   = DMA_COPY_WORDS | DMA_SRC_FIX | (size>>2);
	while(DMA_CR(3) & DMA_BUSY);
}

ALIGN(4) static inline void copyLoop (u32* dest, const u32* src, size_t size) {
	do { *dest++ = *src++; } while (size -= 4);
}

ALIGN(4) static inline void ipcSendState(uint8_t state) {
	REG_IPC_SYNC = (state & 0x0f) << 8;
}

ALIGN(4) static inline uint8_t ipcRecvState(void) {
	return (uint8_t)(REG_IPC_SYNC & 0x0f);
}

typedef struct sTWLHeader {
	char gameTitle[12];			//!< 12 characters for the game title.
	char gameCode[4];			//!< 4 characters for the game code.
	char makercode[2];			//!< identifies the (commercial) developer.
	u8 unitCode;				//!< identifies the required hardware.
	u8 deviceType;				//!< type of device in the game card
	u8 deviceSize;				//!< capacity of the device (1 << n Mbit)
	u8 reserved1[7];
	u8 twlHeaderSettings;
	u8 jumpSettings;
	u8 romversion;				//!< version of the ROM.
	u8 flags;					//!< bit 2: auto-boot flag.

	u32 arm9romOffset;			//!< offset of the arm9 binary in the nds file.
	void *arm9executeAddress;		//!< adress that should be executed after the binary has been copied.
	void *arm9destination;		//!< destination address to where the arm9 binary should be copied.
	u32 arm9binarySize;			//!< size of the arm9 binary.

	u32 arm7romOffset;			//!< offset of the arm7 binary in the nds file.
	void *arm7executeAddress;		//!< adress that should be executed after the binary has been copied.
	void *arm7destination;		//!< destination address to where the arm7 binary should be copied.
	u32 arm7binarySize;			//!< size of the arm7 binary.

	u32 filenameOffset;			//!< File Name Table (FNT) offset.
	u32 filenameSize;			//!< File Name Table (FNT) size.
	u32 fatOffset;				//!< File Allocation Table (FAT) offset.
	u32 fatSize;				//!< File Allocation Table (FAT) size.

	u32 arm9overlaySource;		//!< File arm9 overlay offset.
	u32 arm9overlaySize;		//!< File arm9 overlay size.
	u32 arm7overlaySource;		//!< File arm7 overlay offset.
	u32 arm7overlaySize;		//!< File arm7 overlay size.

	u32 cardControl13;			//!< Port 40001A4h setting for normal commands (used in modes 1 and 3)
	u32 cardControlBF;			//!< Port 40001A4h setting for KEY1 commands (used in mode 2)
	u32 bannerOffset;			//!< offset to the banner with icon and titles etc.

	u16 secureCRC16;			//!< Secure Area Checksum, CRC-16.

	u16 readTimeout;			//!< Secure Area Loading Timeout.

	u32 unknownRAM1;			//!< ARM9 Auto Load List RAM Address (?)
	u32 unknownRAM2;			//!< ARM7 Auto Load List RAM Address (?)

	u32 bfPrime1;				//!< Secure Area Disable part 1.
	u32 bfPrime2;				//!< Secure Area Disable part 2.
	u32 romSize;				//!< total size of the ROM.

	u32 headerSize;				//!< ROM header size.
	u32 zeros88[14];
	u8 gbaLogo[156];			//!< Nintendo logo needed for booting the game.
	u16 logoCRC16;				//!< Nintendo Logo Checksum, CRC-16.
	u16 headerCRC16;			//!< header checksum, CRC-16.
	
	u32 debugRomSource;			//!< debug ROM offset.
	u32 debugRomSize;			//!< debug size.
	u32 debugRomDestination;	//!< debug RAM destination.
	u32 offset_0x16C;			//reserved?

	u8 zero[0x10];
	
	u32 arm9MBK1;
	u32 arm9MBK2;
	u32 arm9MBK3;
	u32 arm9MBK4;
	u32 arm9MBK5;
	u32 arm9MBK6;
	u32 arm9MBK7;
	u32 arm9MBK8;
	u32 arm7MBK6;
	u32 arm7MBK7;
	u32 arm7MBK8;
	u32 arm9MBKMaster;
	
	u32 region;
	u32 accessControl;
	u32 arm7SCFGSettings;
	u16 dsi_unk1;
	u8 dsi_unk2;
	u8 dsi_flags;

	u32 arm9iromOffset;			//!< offset of the arm9 binary in the nds file.
	u32 arm9iexecuteAddress;
	u32 arm9idestination;		//!< destination address to where the arm9 binary should be copied.
	u32 arm9ibinarySize;		//!< size of the arm9 binary.

	u32 arm7iromOffset;			//!< offset of the arm7 binary in the nds file.
	u32 deviceListDestination;
	u32 arm7idestination;		//!< destination address to where the arm7 binary should be copied.
	u32 arm7ibinarySize;		//!< size of the arm7 binary.

	u8 zero2[0x20];

	// 0x200
	// TODO: More DSi-specific fields.
	u32 dsi1[0x10/4];
	u32 twlRomSize;
	u32 dsi_unk3;
	u32 dsi_unk4;
	u32 dsi_unk5;
	
	u32 modCrypt1Offset;
	u32 modcrypt1Size;
	u32 modcrypt2Offset;
	u32 modcrypt2Size;
	
	u32 dsi_tid;
	u32 dsi_tid2;
	u32 pubSavSize;
	u32 prvSavSize;
	
	u8 reserved3[176];
	u8 age_ratings[0x10];

	unsigned char hmac_arm9[16];
	unsigned char hmac_arm7[16];
	u8 hmac_digest_master[0x14];
	u8 hmac_icon_title[0x14];
	u8 hmac_arm9i[0x14];
	u8 hmac_arm7i[0x14];
	u8 reserved4[0x28];
	u8 hmac_arm9_no_secure[0x14];
	u8 reserved5[0xA4C];
	u8 debug_args[0x180];
	u8 rsa_signature[0x80];
} tTWLHeader;


#endif // _COMMON_H

