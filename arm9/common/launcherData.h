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

#ifndef LAUNCHERDATA_H
#define LAUNCHERDATA_H

#ifdef __cplusplus
extern "C" {
#endif

#define LAUNCH_DATA 0x020007F0
#define CartHeaderCopy 0x02000000
#define CartChipIDCopy 0x02000180

#define CartBannerBuffer 0x02500000
// #define InitialCartBannerOffset 0x02FFC068

#define InitialCartHeaderTWL 0x02FFC000 // System Menu keeps cart's header here (if cart is present) on initial boot of any DSiWare!
#define InitialCartHeader 0x02FFFA80 // System Menu keeps cart's header here (if cart is present) on initial boot of any DSiWare!
#define InitialCartChipID 0x02FFFC00 // System Menu keeps cart's chip id here (if cart is present) on initial boot of any DSiWare!
#define InitialCartBannerOffset 0x02FFFAE8 

typedef struct sLauncherSettings {
	u8 language;
	u8 scfgUnlock;
	u8 twlMode;
	u8 twlCLK;
	u8 twlVRAM;
	u8 debugMode;
	u8 fastBoot;
	u8 unused2;
} tLauncherSettings;


#ifdef __cplusplus
}
#endif

#endif // LAUNCHERDATA_H

