#ifndef _BOOT_H_
#define _BOOT_H_

typedef struct sTWLHeader {
	char gameTitle[12];			//!< 12 characters for the game title.
	char gameCode[4];			//!< 4 characters for the game code.
	char makercode[2];			//!< identifies the (commercial) developer.
	u8 unitCode;				//!< identifies the required hardware.
	u8 deviceType;				//!< type of device in the game card
	u8 deviceSize;				//!< capacity of the device (1 << n Mbit)
	u8 reserved1[9];
	u8 romversion;				//!< version of the ROM.
	u8 flags;					//!< bit 2: auto-boot flag.

	u32 arm9romOffset;			//!< offset of the arm9 binary in the nds file.
	u32 arm9executeAddress;		//!< adress that should be executed after the binary has been copied.
	u32 arm9destination;		//!< destination address to where the arm9 binary should be copied.
	u32 arm9binarySize;			//!< size of the arm9 binary.

	u32 arm7romOffset;			//!< offset of the arm7 binary in the nds file.
	u32 arm7executeAddress;		//!< adress that should be executed after the binary has been copied.
	u32 arm7destination;		//!< destination address to where the arm7 binary should be copied.
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


#define resetMemory2_ARM9_size 0x400
#define startBinary_ARM9_size 0x100
// #define setSCFG_ARM9_size 0x400
#define ARM9_START_FLAG (*(vu8*)0x02FFFDFB)

void __attribute__ ((long_call)) __attribute__((naked)) __attribute__((noreturn)) resetMemory2_ARM9();
// void __attribute__ ((long_call)) __attribute__((naked)) __attribute__((noreturn)) setSCFG_ARM9();
void __attribute__ ((long_call)) __attribute__((noreturn)) __attribute__((naked)) startBinary_ARM9();

#endif // _BOOT_H_

