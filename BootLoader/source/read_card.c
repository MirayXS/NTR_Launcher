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

#include "read_card.h"

#include <assert.h>
#include <nds.h>
#include <string.h>
#include <stdlib.h>

#include "encryption.h"
#include "common.h"
#include "tonccpy.h"

#define BASE_DELAY (100)

/*#define ARM7OWNSCARD BIT(11)
#define ARM7OWNSROM  BIT(7)*/

typedef union { char title[4]; u32 key; } GameCode;

/*void SetCardOwner(bool arm9) {
	REG_EXMEMSTAT = (REG_EXMEMSTAT & ~ARM7OWNSCARD) | (arm9 ? 0 : ARM7OWNSCARD);
}*/

static u32 twlBlowfish = 0;
static u32 normalChip = 0;
static u32 portFlags = 0;
static u32 secureAreaData[CARD_SECURE_AREA_SIZE/sizeof(u32)];
static ALIGN(4) u32 twlSecureAreaData[CARD_SECURE_AREA_SIZE/sizeof(u32)];
// static bool shortInit = false;
// static u32 iCardId;

// u32 cardGetId() { return iCardId; }

static const u8 cardSeedBytes[] = { 0xE8, 0x4D, 0x5A, 0xB1, 0x17, 0x8F, 0x99, 0xD5 };

void EnableSlot1() {
	int oldIME = enterCriticalSection();

	while((REG_SCFG_MC & 0x0c) == 0x0c) swiDelay(1 * BASE_DELAY);

	if(!(REG_SCFG_MC & 0x0c)) {
		REG_SCFG_MC = (REG_SCFG_MC & ~0x0c) | 4;
		swiDelay(20 * BASE_DELAY);
		REG_SCFG_MC = (REG_SCFG_MC & ~0x0c) | 8;
		swiDelay(20 * BASE_DELAY);
	}
	
	/*REG_ROMCTRL = 0x20000000; // wait 27ms, then set ROMCTRL=20000000h
	while((REG_ROMCTRL & 0x8000000))swiDelay(1 * BASE_DELAY);*/
	
	leaveCriticalSection(oldIME);
}

void DisableSlot1() {
	int oldIME = enterCriticalSection();

	while((REG_SCFG_MC & 0x0c) == 0x0c) swiDelay(1 * BASE_DELAY);

	if((REG_SCFG_MC & 0x0c) == 8) {
		REG_SCFG_MC = (REG_SCFG_MC & ~0x0c) | 0x0c;
		while((REG_SCFG_MC & 0x0c) != 0) swiDelay(1 * BASE_DELAY);
	}

	leaveCriticalSection(oldIME);
}


// chosen by fair dice roll.
// guaranteed to be random.
// static u32 getRandomNumber(void) { return 4; }
// static u32 getRandomNumber(void) { return rand(); } // make this actually random lol. :P
static u32 getRandomNumber(void) { return 0xDDAC86F5; }

static void decryptSecureArea (u32 gameCode, u32* secureArea, int iCardDevice) {
	init_keycode (gameCode, 2, 8, iCardDevice);
	crypt_64bit_down (secureArea);
	init_keycode (gameCode, 3, 8, iCardDevice);
	for (int i = 0; i < 0x200; i+= 2)crypt_64bit_down (secureArea + i);
}

static struct {
	unsigned int iii;
	unsigned int jjj;
	unsigned int kkkkk;
	unsigned int llll;
	unsigned int mmm;
	unsigned int nnn;
} key1data;


static void initKey1Encryption (u8* cmdData, int iCardDevice) {
	key1data.iii = getRandomNumber() & 0x00000fff;
	key1data.jjj = getRandomNumber() & 0x00000fff;
	key1data.kkkkk = getRandomNumber() & 0x000fffff;
	key1data.llll = getRandomNumber() & 0x0000ffff;
	key1data.mmm = getRandomNumber() & 0x00000fff;
	key1data.nnn = getRandomNumber() & 0x00000fff;

    if (iCardDevice) //DSi
      cmdData[7]=0x3D;	// CARD_CMD_ACTIVATE_BF2
    else
      cmdData[7]=CARD_CMD_ACTIVATE_BF;

	cmdData[6] = (u8) (key1data.iii >> 4);
	cmdData[5] = (u8) ((key1data.iii << 4) | (key1data.jjj >> 8));
	cmdData[4] = (u8) key1data.jjj;
	cmdData[3] = (u8) (key1data.kkkkk >> 16);
	cmdData[2] = (u8) (key1data.kkkkk >> 8);
	cmdData[1] = (u8) key1data.kkkkk;
	cmdData[0] = (u8) getRandomNumber();
}

// Note: cmdData must be aligned on a word boundary
static void createEncryptedCommand (u8 command, u8* cmdData, u32 block) {
	unsigned long iii, jjj;

	if (command != CARD_CMD_SECURE_READ)block = key1data.llll;
	
	if (command == CARD_CMD_ACTIVATE_SEC) {
		iii = key1data.mmm;
		jjj = key1data.nnn;
	} else {
		iii = key1data.iii;
		jjj = key1data.jjj;
	}

	cmdData[7] = (u8) (command | (block >> 12));
	cmdData[6] = (u8) (block >> 4);
	cmdData[5] = (u8) ((block << 4) | (iii >> 8));
	cmdData[4] = (u8) iii;
	cmdData[3] = (u8) (jjj >> 4);
	cmdData[2] = (u8) ((jjj << 4) | (key1data.kkkkk >> 16));
	cmdData[1] = (u8) (key1data.kkkkk >> 8);
	cmdData[0] = (u8) key1data.kkkkk;

	crypt_64bit_up ((u32*)cmdData);

	key1data.kkkkk += 1;
}

static void cardDelay (u16 readTimeout) {
	/* Using a while loop to check the timeout,
	   so we have to wait until one before overflow.
	   This also requires an extra 1 for the timer data.
	   See GBATek for the normal formula used for card timeout.
	*/
	TIMER_DATA(0) = 0 - (((readTimeout & 0x3FFF) + 3));
	TIMER_CR(0)   = TIMER_DIV_256 | TIMER_ENABLE;
	while (TIMER_DATA(0) != 0xFFFF);

	// Clear out the timer registers
	TIMER_CR(0)   = 0;
	TIMER_DATA(0) = 0;
}

static void switchToTwlBlowfish(sNDSHeaderExt* ndsHeader) {
	if ((twlBlowfish == 0) || ndsHeader->unitCode == 0) return;

	// Used for dumping the DSi arm9i/7i binaries

	u32 portFlagsKey1, portFlagsSecRead;
	int secureBlockNumber;
	int i;
	u8 cmdData[8] __attribute__ ((aligned));
	GameCode* gameCode;

	if (REG_SCFG_EXT & BIT(31)) {
		// int oldIME = 0;
		// Reset card slot
		DisableSlot1();
		// oldIME = enterCriticalSection();
		swiDelay(40 * BASE_DELAY);
		// leaveCriticalSection(oldIME);
		EnableSlot1();
		// oldIME = enterCriticalSection();
		swiDelay(30 * BASE_DELAY);
		// leaveCriticalSection(oldIME);
	}

	// Dummy command sent after card reset
	cardParamCommand (CARD_CMD_DUMMY, 0, CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F), NULL, 0);

	//int iCardDevice = 1;

	// Initialise blowfish encryption for KEY1 commands and decrypting the secure area
	gameCode = (GameCode*)ndsHeader->gameCode;
	init_keycode (gameCode->key, 1, 8, 1);

	// Port 40001A4h setting for normal reads (command B7)
	portFlags = ndsHeader->cardControl13 & ~CARD_BLK_SIZE(7);
	// Port 40001A4h setting for KEY1 commands   (usually 001808F8h)
	portFlagsKey1 = CARD_ACTIVATE | CARD_nRESET | (ndsHeader->cardControl13 & (CARD_WR|CARD_CLK_SLOW)) |
		((ndsHeader->cardControlBF & (CARD_CLK_SLOW|CARD_DELAY1(0x1FFF))) + ((ndsHeader->cardControlBF & CARD_DELAY2(0x3F)) >> 16));

	// Adjust card transfer method depending on the most significant bit of the chip ID
	if (normalChip == 0)portFlagsKey1 |= CARD_SEC_LARGE;

	// 3Ciiijjj xkkkkkxx - Activate KEY1 Encryption Mode
	initKey1Encryption(cmdData, 1);
	cardPolledTransfer((ndsHeader->cardControl13 & (CARD_WR|CARD_nRESET|CARD_CLK_SLOW)) | CARD_ACTIVATE, NULL, 0, cmdData);

	// 4llllmmm nnnkkkkk - Activate KEY2 Encryption Mode
	createEncryptedCommand (CARD_CMD_ACTIVATE_SEC, cmdData, 0);

	if (normalChip > 0) {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
		cardDelay(ndsHeader->readTimeout);
	}
	cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);

	// Set the KEY2 encryption registers
	REG_ROMCTRL = 0;
	REG_CARD_1B0 = cardSeedBytes[ndsHeader->deviceType & 0x07] | (key1data.nnn << 15) | (key1data.mmm << 27) | 0x6000;
	REG_CARD_1B4 = 0x879b9b05;
	REG_CARD_1B8 = key1data.mmm >> 5;
	REG_CARD_1BA = 0x5c;
	REG_ROMCTRL = CARD_nRESET | CARD_SEC_SEED | CARD_SEC_EN | CARD_SEC_DAT;

	// Update the DS card flags to suit KEY2 encryption
	portFlagsKey1 |= CARD_SEC_EN | CARD_SEC_DAT;

	// 1lllliii jjjkkkkk - 2nd Get ROM Chip ID / Get KEY2 Stream
	createEncryptedCommand (CARD_CMD_SECURE_CHIPID, cmdData, 0);

	if (normalChip > 0) {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
		cardDelay(ndsHeader->readTimeout);
	}
	cardPolledTransfer(portFlagsKey1 | CARD_BLK_SIZE(7), NULL, 0, cmdData);

	// 2bbbbiii jjjkkkkk - Get Secure Area Block
	portFlagsSecRead = (ndsHeader->cardControlBF & (CARD_CLK_SLOW|CARD_DELAY1(0x1FFF)|CARD_DELAY2(0x3F)))
		| CARD_ACTIVATE | CARD_nRESET | CARD_SEC_EN | CARD_SEC_DAT;

    int secureAreaOffset = 0;
	for (secureBlockNumber = 4; secureBlockNumber < 8; secureBlockNumber++) {
		createEncryptedCommand (CARD_CMD_SECURE_READ, cmdData, secureBlockNumber);

		if (normalChip > 0) {
			cardPolledTransfer(portFlagsSecRead, NULL, 0, cmdData);
			cardDelay(ndsHeader->readTimeout);
			for (i = 8; i > 0; i--) {
				cardPolledTransfer(portFlagsSecRead | CARD_BLK_SIZE(1), twlSecureAreaData + secureAreaOffset, 0x200, cmdData);
				secureAreaOffset += 0x200/sizeof(u32);
			}
		} else {
			cardPolledTransfer(portFlagsSecRead | CARD_BLK_SIZE(4) | CARD_SEC_LARGE, twlSecureAreaData + secureAreaOffset, 0x1000, cmdData);
			secureAreaOffset += 0x1000/sizeof(u32);
		}
	}

	// Alllliii jjjkkkkk - Enter Main Data Mode
	createEncryptedCommand (CARD_CMD_DATA_MODE, cmdData, 0);

	if (normalChip > 0) {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
		cardDelay(ndsHeader->readTimeout);
    }
	cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);

	twlBlowfish = 0x01;
}


u16 cardInit (sNDSHeaderExt* ndsHeader, u32 *chipID) {
	u32 portFlagsKey1, portFlagsSecRead;
	normalChip = 0;	// As defined by GBAtek, normal chip secure area is accessed in blocks of 0x200, other chip in blocks of 0x1000
	u32* secureArea;
	int secureBlockNumber;
	int i;
	u8 cmdData[8] __attribute__ ((aligned));
	GameCode* gameCode;
	// shortInit = false;
	
	// SetCardOwner(false);
	
	// Dummy command sent after card reset
	cardParamCommand (CARD_CMD_DUMMY, 0, CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F), NULL, 0);

	// Verify that the ndsHeader is packed correctly, now that it's no longer __packed__
	static_assert(sizeof(tNDSHeader) == 0x160, "tNDSHeader not packed properly");

	// Read the header
	cardReadHeader((u8*)ndsHeader);
	// cardParamCommand (CARD_CMD_HEADER_READ, 0, CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F), (u32*)ndsHeader, sizeof(tNDSHeader));
	// while (REG_ROMCTRL & CARD_BUSY);
	
	// Check header CRC
	if (ndsHeader->headerCRC16 != swiCRC16(0xFFFF, (void*)ndsHeader, 0x15E))return ERR_HEAD_CRC;
	
	// 1st Get ROM Chip ID
	cardParamCommand (CARD_CMD_HEADER_CHIPID, 0, (ndsHeader->cardControl13 & (CARD_WR|CARD_nRESET|CARD_CLK_SLOW)) | CARD_ACTIVATE | CARD_BLK_SIZE(7), chipID, sizeof(u32));
	// iCardId = cardReadID((ndsHeader->cardControl13 & (CARD_WR|CARD_nRESET|CARD_CLK_SLOW)) | CARD_ACTIVATE | CARD_BLK_SIZE(7));
	// iCardId = cardReadID(CARD_CLK_SLOW);
	// chipID = cardReadID(CARD_CLK_SLOW);
	while (REG_ROMCTRL & CARD_BUSY);
		
	// *chipID = iCardId;
	
	// Initialise blowfish encryption for KEY1 commands and decrypting the secure area
	gameCode = (GameCode*)ndsHeader->gameCode;
	init_keycode (gameCode->key, 2, 8, 0);

	// Port 40001A4h setting for normal reads (command B7)
	portFlags = ndsHeader->cardControl13 & ~CARD_BLK_SIZE(7);
	// Port 40001A4h setting for KEY1 commands   (usually 001808F8h)
	portFlagsKey1 = CARD_ACTIVATE | CARD_nRESET | (ndsHeader->cardControl13 & (CARD_WR|CARD_CLK_SLOW)) | ((ndsHeader->cardControlBF & (CARD_CLK_SLOW|CARD_DELAY1(0x1FFF))) + ((ndsHeader->cardControlBF & CARD_DELAY2(0x3F)) >> 16));
	
	// Adjust card transfer method depending on the most significant bit of the chip ID
	if(((*chipID) & 0x80000000) != 0)normalChip = 0xFFFF;		// ROM chip ID MSB
	if (normalChip == 0)portFlagsKey1 |= CARD_SEC_LARGE;

	// 3Ciiijjj xkkkkkxx - Activate KEY1 Encryption Mode
	initKey1Encryption (cmdData, 0);
	cardPolledTransfer((ndsHeader->cardControl13 & (CARD_WR|CARD_nRESET|CARD_CLK_SLOW)) | CARD_ACTIVATE, NULL, 0, cmdData);

	// 4llllmmm nnnkkkkk - Activate KEY2 Encryption Mode
	createEncryptedCommand (CARD_CMD_ACTIVATE_SEC, cmdData, 0);

	if (normalChip > 0) {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
		cardDelay(ndsHeader->readTimeout);
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
	} else {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
	}

	// Set the KEY2 encryption registers
	REG_ROMCTRL = 0;
	REG_CARD_1B0 = cardSeedBytes[ndsHeader->deviceType & 0x07] | (key1data.nnn << 15) | (key1data.mmm << 27) | 0x6000;
	REG_CARD_1B4 = 0x879B9B05;
	REG_CARD_1B8 = key1data.mmm >> 5;
	REG_CARD_1BA = 0x5C;
	REG_ROMCTRL = CARD_nRESET | CARD_SEC_SEED | CARD_SEC_EN | CARD_SEC_DAT;

	// Update the DS card flags to suit KEY2 encryption
	portFlagsKey1 |= CARD_SEC_EN | CARD_SEC_DAT;

	// 1lllliii jjjkkkkk - 2nd Get ROM Chip ID / Get KEY2 Stream
	createEncryptedCommand (CARD_CMD_SECURE_CHIPID, cmdData, 0);

	if (normalChip > 0) {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
		cardDelay(ndsHeader->readTimeout);
		cardPolledTransfer(portFlagsKey1 | CARD_BLK_SIZE(7), NULL, 0, cmdData);
	} else {
		cardPolledTransfer(portFlagsKey1 | CARD_BLK_SIZE(7), NULL, 0, cmdData);
	}

	// 2bbbbiii jjjkkkkk - Get Secure Area Block
	secureArea = secureAreaData;
	portFlagsSecRead = (ndsHeader->cardControlBF & (CARD_CLK_SLOW|CARD_DELAY1(0x1FFF)|CARD_DELAY2(0x3F))) | CARD_ACTIVATE | CARD_nRESET | CARD_SEC_EN | CARD_SEC_DAT;
	
	for (secureBlockNumber = 4; secureBlockNumber < 8; secureBlockNumber++) {
		createEncryptedCommand (CARD_CMD_SECURE_READ, cmdData, secureBlockNumber);
		if (normalChip > 0) {
			cardPolledTransfer(portFlagsSecRead, NULL, 0, cmdData);
			cardDelay(ndsHeader->readTimeout);
			for (i = 8; i > 0; i--) {
				cardPolledTransfer(portFlagsSecRead | CARD_BLK_SIZE(1), secureArea, 0x200, cmdData);
				secureArea += 0x200/sizeof(u32);
			}
		} else {
			cardPolledTransfer(portFlagsSecRead | CARD_BLK_SIZE(4) | CARD_SEC_LARGE, secureArea, 0x1000, cmdData);
			secureArea += 0x1000/sizeof(u32);
		}
	}

	// Alllliii jjjkkkkk - Enter Main Data Mode
	createEncryptedCommand (CARD_CMD_DATA_MODE, cmdData, 0);

	if (normalChip > 0) {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
		cardDelay(ndsHeader->readTimeout);
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
    } else {
		cardPolledTransfer(portFlagsKey1, NULL, 0, cmdData);
	}

	// Now deal with secure area decryption and verification
	decryptSecureArea (gameCode->key, secureAreaData, 0);

	secureArea = secureAreaData;
	if (secureArea[0] == 0x72636e65 /*'encr'*/ && secureArea[1] == 0x6a624f79 /*'yObj'*/) {
		// Secure area exists, so just clear the tag
		secureArea[0] = 0xe7ffdeff;
		secureArea[1] = 0xe7ffdeff;
	}

	return ERR_NONE;
}

void cardRead(sNDSHeaderExt* ndsHeader, u32 src, u32* dest, size_t size) {
	size_t readSize;

	if ((twlBlowfish == 0) && (ndsHeader->unitCode & BIT(1)) && (src > ndsHeader->romSize))switchToTwlBlowfish(ndsHeader);

	if (src < CARD_SECURE_AREA_OFFSET) {
		return;
	} else if (src < CARD_DATA_OFFSET) {
		// Read data from secure area
		readSize = src + size < CARD_DATA_OFFSET ? size : CARD_DATA_OFFSET - src;
		tonccpy (dest, (u8*)secureAreaData + src - CARD_SECURE_AREA_OFFSET, readSize);
		src += readSize;
		dest += readSize/sizeof(*dest);
		size -= readSize;
	} else if ((ndsHeader->unitCode & BIT(1)) && (twlBlowfish > 0) && (src >= ndsHeader->arm9iromOffset) && (src < ndsHeader->arm9iromOffset+CARD_SECURE_AREA_SIZE)) {
		// Read data from secure area
		readSize = src + size < (ndsHeader->arm9iromOffset+CARD_SECURE_AREA_SIZE) ? size : (ndsHeader->arm9iromOffset+CARD_SECURE_AREA_SIZE) - src;
		tonccpy (dest, (u8*)twlSecureAreaData + src - ndsHeader->arm9iromOffset, readSize);
		src += readSize;
		dest += readSize/sizeof(*dest);
		size -= readSize;
		if (size <= 0)return;
	}
	
	while (size > 0) {
		readSize = size < CARD_DATA_BLOCK_SIZE ? size : CARD_DATA_BLOCK_SIZE;
		cardParamCommand (CARD_CMD_DATA_READ, src, portFlags | CARD_ACTIVATE | CARD_nRESET | CARD_BLK_SIZE(1), dest, readSize);
		src += readSize;
		dest += readSize/sizeof(*dest);
		size -= readSize;
	}
}


// If booted from DSi System Menu short cart init with no card reads or pokes to rom ctrl registers can be done.
// System Menu is nice enough to do this for you. :P
// (also is the case for booting from DS Download Play. ;) )
/*u32 cardInitShort (sNDSHeaderExt* ndsHeader, u32* chipID) {
	bool normalChip;	// As defined by GBAtek, normal chip secure area is accessed in blocks of 0x200, other chip in blocks of 0x1000
	u32* secureArea;
	GameCode* gameCode;
	shortInit = true;

	// Verify that the ndsHeader is packed correctly, now that it's no longer __packed__
	static_assert(sizeof(tNDSHeader) == 0x160, "tNDSHeader not packed properly");

	// Read the header
	tonccpy(ndsHeader, (u32*)CartHeaderCopy, 0x180);

	// Check header CRC
	if (ndsHeader->headerCRC16 != swiCRC16(0xFFFF, (void*)ndsHeader, 0x15E))return ERR_HEAD_CRC;

	// Initialise blowfish encryption for KEY1 commands and decrypting the secure area
	gameCode = (GameCode*)ndsHeader->gameCode;
	init_keycode (gameCode->key, 2, 8, 0);

	// Port 40001A4h setting for normal reads (command B7)
	portFlags = ndsHeader->cardControl13 & ~CARD_BLK_SIZE(7);

	// 1st Get ROM Chip ID
	// chipID = *(u32*)0x02FFFC00; // This location contains cart's chipID when booting DSiWare that has Slot-1 access.
	// chipID = *(u32*)0x027FF800; // This location contains cart's chipID for DS Download Play users
	tonccpy(chipID, (u32*)CartChipIDCopy, 0x08); // This location contains cart's chipID when booting DSiWare that has Slot-1 access.
	
	// Adjust card transfer method depending on the most significant bit of the chip ID
	normalChip = ((*chipID) & 0x80000000) != 0;		// ROM chip ID MSB
	
	if (normalChip)cardDelay(ndsHeader->readTimeout); 

	// 2bbbbiii jjjkkkkk - Get Secure Area Block
	secureArea = secureAreaData;
	shortInit = false;
	u32 secureBlockSize = 0x800;
	if (!normalChip)secureBlockSize = 0x1000;
	cardRead (ndsHeader->arm9romOffset, secureArea, secureBlockSize);

	// Now deal with secure area decryption and verification
	decryptSecureArea (gameCode->key, secureAreaData, 0);

	secureArea = secureAreaData;
	if (secureArea[0] == 0x72636e65 && secureArea[1] == 0x6a624f79) {
		// Secure area exists, so just clear the tag
		secureArea[0] = 0xe7ffdeff;
		secureArea[1] = 0xe7ffdeff;
	}

	return ERR_NONE;
}*/

