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

void fifocheck (void)
{
	if(*((vu32*)0x027FFE24) == (u32)0x027FFE04)
	{
		REG_SCFG_ROM = 0x703;

		if(fifoCheckValue32(FIFO_USER_01)) { REG_SCFG_CLK = 0x0180; } else { REG_SCFG_CLK = 0x0187; }
		
		REG_MBK6=0x09403900;
		REG_MBK7=0x09803940;
		REG_MBK8=0x09C03980;
		REG_MBK9=0xFCFFFF0F;

		REG_SCFG_EXT = 0x12A03000;

		irqDisable (IRQ_ALL);
		*((vu32*)0x027FFE34) = (u32)0x06000000;

		swiSoftReset();
	} 
}

