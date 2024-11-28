/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"
	Michael "mtheall" Theall

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

#include <nds.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#include "args.h"
#include "hbmenu_banner.h"
#include "hbmenu_banner_cartSelected.h"
#include "hbmenu_banner_noCart.h"
#include "font6x8.h"
#include "tonccpy.h"
#include "read_card.h"
#include "launcherData.h"

#define TITLE_POS_X	(13*8)
#define TITLE_POS_Y	(10*4)

#define CARTTITLE_OFFSET_Y 12

#define ICON_POS_X	26
#define ICON_POS_Y	40

#define ICON2_POS_X	26
#define ICON2_POS_Y	132

#define TEXT_WIDTH	((22-4)*8/6)

typedef struct sNDSBannerTest {
	u16 version;			//!< version of the banner.
	u16 crc;				//!< 16 bit crc/checksum of the banner.
	u8 reserved[28];
	u8 iconData[2080];
} tNDSBannerTest;


static int bg2, bg3;
static u16 *sprite;
static u16 *sprite2;

extern tNDSBanner dsCardDefault_bin;
extern tNDSBanner dsCardInvalid_bin;
extern tNDSBanner hbNoIcon_bin;

static tNDSBanner banner;
static tNDSBanner* cartBanner;

static u32 CartBannerBuffer[2304];

bool cartSelected = false;

static inline void writecharRS (int row, int col, u16 car) {
	// get map pointer
	u16 *gfx   = bgGetMapPtr(bg2);
	// get old pair of values from VRAM
	u16 oldval = gfx[row*(512/8/2)+(col/2)];

	// clear the half we will update
	oldval &= (col%2) ? 0x00FF : 0xFF00;
	// apply the updated half
	oldval |= (col%2) ? (car<<8) : car;

	// write back to VRAM
	gfx[row*(512/8/2)+col/2] = oldval;
}

static inline void writeRow (int rownum, const char* text, bool isCartBannerText) {
	int row = rownum;
	if (isCartBannerText)row = (rownum + CARTTITLE_OFFSET_Y);
	int i,len,p=0;
	len=strlen(text);

	if (len>TEXT_WIDTH)len=TEXT_WIDTH;

	// clear left part
	for (i=0;i<(TEXT_WIDTH-len)/2;i++)writecharRS (row, i, 0);

	// write centered text
	for (i=(TEXT_WIDTH-len)/2;i<((TEXT_WIDTH-len)/2+len);i++)writecharRS (row, i, text[p++]-' ');

	// clear right part
	for (i=((TEXT_WIDTH-len)/2+len);i<TEXT_WIDTH;i++)writecharRS (row, i, 0);
}

static inline void clearIcon (void) { dmaFillHalfWords(0, sprite, sizeof(banner.icon)); }

void ToggleBackground (bool noCart) {
	DC_FlushAll();
	if (cartSelected) {
		decompress(hbmenu_banner_cartSelectedBitmap, bgGetGfxPtr(bg3), LZ77Vram);
	} else {
		if (noCart) {
			decompress(hbmenu_banner_noCartBitmap, bgGetGfxPtr(bg3), LZ77Vram);
		} else {
			decompress(hbmenu_bannerBitmap, bgGetGfxPtr(bg3), LZ77Vram);
		}
	}
}

void clearCartIcon(bool clearBannerText) { 
	if (clearBannerText) {
		for (int i = 0; i < 4; i++)writeRow (i, "", true);
	}
	dmaFillHalfWords(0, sprite2, sizeof(banner.icon)); 
}

static bool checkBannerCRC(u8* banner) {
	return (((tNDSBannerTest*)banner)->crc == swiCRC16(0xFFFF, ((tNDSBannerTest*)banner)->iconData, 0x820));
}


void iconTitleInit (void) {
	// initialize video mode
	videoSetMode(MODE_4_2D);

	// initialize VRAM banks
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_MAIN_SPRITE);

	// initialize bg2 as a rotation background and bg3 as a bmp background
	// http://mtheall.com/vram.html#T2=3&RNT2=96&MB2=3&TB2=0&S2=2&T3=6&MB3=1&S3=1
	bg2 = bgInit(2, BgType_Rotation, BgSize_R_512x512,   3, 0);
	bg3 = bgInit(3, BgType_Bmp16,    BgSize_B16_256x256, 1, 0);

	// initialize rotate, scale, and scroll
	bgSetRotateScale(bg3, 0, 1<<8, 1<<8);
	bgSetScroll(bg3, 0, 0);
	bgSetRotateScale(bg2, 0, 8*(1<<8)/6, 1<<8);
	bgSetScroll(bg2, -TITLE_POS_X, -TITLE_POS_Y);

	// clear bg2's map: 512x512 pixels is 64x64 tiles is 4KB
	dmaFillHalfWords(0, bgGetMapPtr(bg2), 4096);
	// load compressed font into bg2's tile data
	decompress(font6x8Tiles, bgGetGfxPtr(bg2), LZ77Vram);

	// load compressed bitmap into bg3
	decompress(hbmenu_banner_noCartBitmap, bgGetGfxPtr(bg3), LZ77Vram);

	// load font palette
	dmaCopy(font6x8Pal, BG_PALETTE, font6x8PalLen);

	// apply the bg changes
	bgUpdate();

	// initialize OAM
	oamInit(&oamMain, SpriteMapping_1D_128, false);
	sprite = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_16Color);
	sprite2 = oamAllocateGfx(&oamMain, SpriteSize_32x32, SpriteColorFormat_16Color);
	dmaFillHalfWords(0, sprite, sizeof(banner.icon));
	dmaFillHalfWords(0, sprite2, sizeof(banner.icon));
	oamSet(&oamMain, 0, ICON_POS_X, ICON_POS_Y, 0, 0, SpriteSize_32x32, SpriteColorFormat_16Color, sprite, -1, 0, 0, 0, 0, 0);
	oamSet(&oamMain, 1, ICON2_POS_X, ICON2_POS_Y, 0, 0, SpriteSize_32x32, SpriteColorFormat_16Color, sprite2, -1, 0, 0, 0, 0, 0);
	oamSetPalette(&oamMain, 1, 1);

	// oam can only be updated during vblank
	swiWaitForVBlank();
	oamUpdate(&oamMain);

	toncset32(CartBannerBuffer, 0, 0x900);
	
	cartBanner = (tNDSBanner*)CartBannerBuffer;

	// Load Default Icons.
	DC_FlushAll();
	dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
	dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
	dmaCopy(dsCardDefault_bin.icon, sprite2, 512);
	dmaCopy(dsCardDefault_bin.palette, (u16*)((u32)SPRITE_PALETTE + 0x20), 0x20);

	// everything's ready :)
	writeRow (2,"     Loading ...     ", false);
}


void iconTitleUpdate (int isdir, const std::string& name) {
	writeRow (0, name.c_str(), false);
	writeRow (1, "", false);
	writeRow (2, "", false);
	writeRow (3, "", false);

	if (isdir) {
		// text
		writeRow (2, "[directory]", false);
		// icon
		clearIcon();
		DC_FlushAll();
		dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
		dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
	} else {
		std::string ndsPath;
		if (!argsNdsPath(name, ndsPath)) {
			writeRow(2, "(invalid argv or NDS file!)", false);
			clearIcon();
			return;
		}

		unsigned int Icon_title_offset;

		// open file for reading info
		FILE *fp = fopen (ndsPath.c_str(), "rb");

		if (!fp) {
			// text
			writeRow (2,"(can't open file!)", false);
			// icon
			clearIcon();
			DC_FlushAll();
			dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
			dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
			fclose (fp);
			return;
		}

		if (fseek (fp, offsetof(tNDSHeader, bannerOffset), SEEK_SET) != 0 ||
				fread (&Icon_title_offset, sizeof(int), 1, fp) != 1) {
			// text
			writeRow (2, "(can't read file!)", false);
			// icon
			clearIcon();
			DC_FlushAll();
			dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
			dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
			fclose (fp);
			return;
		}

		if (Icon_title_offset == 0) {
			// text
			writeRow (2, "(no title/icon)", false);
			// icon
			clearIcon();
			DC_FlushAll();
			dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
			dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
			fclose (fp);
			return;
		}

		if (fseek (fp, Icon_title_offset, SEEK_SET) != 0 || fread (&banner, sizeof(banner), 1, fp) != 1) {
			// text
			writeRow (2,"(can't read icon/title!)", false);
			// icon
			clearIcon();
			DC_FlushAll();
			dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
			dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
			fclose (fp);
			return;
		}

		if (!checkBannerCRC((u8*)&banner)) {
			// text
			writeRow (2,"(invalid icon/title!)", false);
			// icon
			clearIcon();
			DC_FlushAll();
			dmaCopy(hbNoIcon_bin.icon,    sprite,         sizeof(hbNoIcon_bin.icon));
			dmaCopy(hbNoIcon_bin.palette, SPRITE_PALETTE, sizeof(hbNoIcon_bin.palette));
			fclose (fp);
			return;
		}

		// close file!
		fclose (fp);
		
		// turn unicode into ascii (kind of)
		// and convert 0x0A into 0x00
		char *p = (char*)banner.titles[1];
		int rowOffset = 1;
		int lineReturns = 0;
		for (size_t i = 0; i < sizeof(banner.titles[1]); i = i+2) {
			if ((p[i] == 0x0A) || (p[i] == 0xFF)) {
				p[i/2] = 0;
				lineReturns++;
			} else {
				p[i/2] = p[i];
			}
		}
		
		if (lineReturns < 2)rowOffset = 2; // Recenter if bennar has less 2 or less rows of text maintaining empty row gap between nds file name and nds banner.

		// text
		for (size_t i = 0; i < 3; ++i) {
			writeRow(i+rowOffset, p, false);
			p += strlen(p) + 1;
		}

		// icon
		DC_FlushAll();
		dmaCopy(banner.icon,    sprite,         sizeof(banner.icon));
		dmaCopy(banner.palette, SPRITE_PALETTE, sizeof(banner.palette));
	}
}


void cartIconUpdate (u32 BannerOffset, bool readExistingBanner) {	
	toncset32(CartBannerBuffer, 0, 0x900);
	if(readExistingBanner) {
		cardReadAlt(*(u32*)InitialCartBannerOffset, (u32*)CartBannerBuffer, 0x2400);
	} else {
		cardReadAlt(BannerOffset, (u32*)CartBannerBuffer, 0x2400);
	}
	switch (cartBanner->crc) {
		case 0x0000: {
			clearCartIcon(false);
			writeRow (1,"(invalid icon/title!)", true);
			DC_FlushAll();
			dmaCopy(dsCardInvalid_bin.icon, sprite2, 512);
			dmaCopy(dsCardInvalid_bin.palette, (u16*)((u32)SPRITE_PALETTE + 0x20), 0x20);
			return;
		} break;
		case 0xFFFF: {
			clearCartIcon(false);
			dmaCopy(dsCardInvalid_bin.icon, sprite2, 512);
			dmaCopy(dsCardInvalid_bin.palette, (u16*)((u32)SPRITE_PALETTE + 0x20), 0x20);
			writeRow (1,"(invalid icon/title!)", true);
			return;
		}break;
		default: {
			if (!checkBannerCRC((u8*)cartBanner)) {
				clearCartIcon(false);
				dmaCopy(dsCardInvalid_bin.icon, sprite2, 512);
				dmaCopy(dsCardInvalid_bin.palette, (u16*)((u32)SPRITE_PALETTE + 0x20), 0x20);
				writeRow (1,"(invalid icon/title!)", true);
				return;
			}
			
			clearCartIcon(false);
			// turn unicode into ascii (kind of)
			// and convert 0x0A into 0x00
			char *p = (char*)cartBanner->titles[1];
			int rowOffset = 0;
			int lineReturns = 0;
			for (size_t i = 0; i < sizeof(cartBanner->titles[1]); i = i+2) {
				if ((p[i] == 0x0A) || (p[i] == 0xFF)) {
					p[i/2] = 0;
					lineReturns++;
				} else {
					p[i/2] = p[i];
				}
			}
			
			// Recenter text to center row if less then 2 rows of text.
			// Default offset 0 instead of 1 since no NDS file name for this to account for.
			if (lineReturns < 2 && lineReturns != 1) { rowOffset = 1; } else if (lineReturns == 1) { rowOffset = 0; }
			
			// text
			for (size_t i = 0; i < 3; ++i) {
				if ((i > 0) && (lineReturns == 1))rowOffset = 1;
				writeRow(i+rowOffset, p, true);
				p += strlen(p) + 1;
			}
		}break;
	}
	DC_FlushAll();
	dmaCopy(&cartBanner->icon, sprite2, 512);
	dmaCopy(&cartBanner->palette, (u16*)((u32)SPRITE_PALETTE + 0x20), 0x20);
}

