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

//---------------------------------------------------------------------------------
void NDSTouchscreenMode() {
//---------------------------------------------------------------------------------
	u8 volLevel;
	
	volLevel = 0xA7;

	volLevel += 0x13;

	readTSCReg(0);
	writeTSCReg(0,0);
	writeTSCReg(0x3a,0);
	readTSCReg(0x51);
	writeTSCReg(3,0);
	readTSCReg(2);
	writeTSCReg(0,0);
	readTSCReg(0x3f);
	writeTSCReg(0,1);
	readTSCReg(0x38);
	readTSCReg(0x2a);
	readTSCReg(0x2E);
	writeTSCReg(0,0);
	writeTSCReg(0x52,0x80);
	writeTSCReg(0x40,0xC);
	writeTSCReg(0,1);
	writeTSCReg(0x24,0xff);
	writeTSCReg(0x25,0xff);
	writeTSCReg(0x26,0x7f);
	writeTSCReg(0x27,0x7f);
	writeTSCReg(0x28,0x4a);
	writeTSCReg(0x29,0x4a);
	writeTSCReg(0x2a,0x10);
	writeTSCReg(0x2b,0x10);
	writeTSCReg(0,0);
	writeTSCReg(0x51,0);
	writeTSCReg(0,3);
	readTSCReg(2);
	writeTSCReg(2,0x98);
	writeTSCReg(0,1);
	writeTSCReg(0x23,0);
	writeTSCReg(0x1f,0x14);
	writeTSCReg(0x20,0x14);
	writeTSCReg(0,0);
	writeTSCReg(0x3f,0);
	readTSCReg(0x0b);
	writeTSCReg(0x5,0);
	writeTSCReg(0xb,0x1);
	writeTSCReg(0xc,0x2);
	writeTSCReg(0x12,0x1);
	writeTSCReg(0x13,0x2);
	writeTSCReg(0,1);
	writeTSCReg(0x2E,0x00);
	writeTSCReg(0,0);
	writeTSCReg(0x3A,0x60);
	writeTSCReg(0x01,01);
	writeTSCReg(0x9,0x66);
	writeTSCReg(0,1);
	readTSCReg(0x20);
	writeTSCReg(0x20,0x10);
	writeTSCReg(0,0);
	writeTSCReg( 04,00);
	writeTSCReg( 0x12,0x81);
	writeTSCReg( 0x13,0x82);
	writeTSCReg( 0x51,0x82);
	writeTSCReg( 0x51,0x00);
	writeTSCReg( 0x04,0x03);
	writeTSCReg( 0x05,0xA1);
	writeTSCReg( 0x06,0x15);
	writeTSCReg( 0x0B,0x87);
	writeTSCReg( 0x0C,0x83);
	writeTSCReg( 0x12,0x87);
	writeTSCReg( 0x13,0x83);
	writeTSCReg(0,3);
	readTSCReg(0x10);
	writeTSCReg(0x10,0x08);
	writeTSCReg(0,4);
	writeTSCReg(0x08,0x7F);
	writeTSCReg(0x09,0xE1);
	writeTSCReg(0xa,0x80);
	writeTSCReg(0xb,0x1F);
	writeTSCReg(0xc,0x7F);
	writeTSCReg(0xd,0xC1);
	writeTSCReg(0,0);
	writeTSCReg( 0x41, 0x08);
	writeTSCReg( 0x42, 0x08);
	writeTSCReg( 0x3A, 0x00);
	writeTSCReg(0,4);
	writeTSCReg(0x08,0x7F);
	writeTSCReg(0x09,0xE1);
	writeTSCReg(0xa,0x80);
	writeTSCReg(0xb,0x1F);
	writeTSCReg(0xc,0x7F);
	writeTSCReg(0xd,0xC1);
	writeTSCReg(0,1);
	writeTSCReg(0x2F, 0x2B);
	writeTSCReg(0x30, 0x40);
	writeTSCReg(0x31, 0x40);
	writeTSCReg(0x32, 0x60);
	writeTSCReg(0,0);
	readTSCReg( 0x74);
	writeTSCReg( 0x74, 0x02);
	readTSCReg( 0x74);
	writeTSCReg( 0x74, 0x10);
	readTSCReg( 0x74);
	writeTSCReg( 0x74, 0x40);
	writeTSCReg(0,1);
	writeTSCReg( 0x21, 0x20);
	writeTSCReg( 0x22, 0xF0);
	writeTSCReg(0,0);
	readTSCReg( 0x51);
	readTSCReg( 0x3f);
	writeTSCReg( 0x3f, 0xd4);
	writeTSCReg(0,1);
	writeTSCReg(0x23,0x44);
	writeTSCReg(0x1F,0xD4);
	writeTSCReg(0x28,0x4e);
	writeTSCReg(0x29,0x4e);
	writeTSCReg(0x24,0x9e);
	writeTSCReg(0x24,0x9e);
	writeTSCReg(0x20,0xD4);
	writeTSCReg(0x2a,0x14);
	writeTSCReg(0x2b,0x14);
	writeTSCReg(0x26,volLevel);
	writeTSCReg(0x27,volLevel);
	writeTSCReg(0,0);
	writeTSCReg(0x40,0);
	writeTSCReg(0x3a,0x60);
	writeTSCReg(0,1);
	writeTSCReg(0x26,volLevel);
	writeTSCReg(0x27,volLevel);
	writeTSCReg(0x2e,0x03);
	writeTSCReg(0,3);
	writeTSCReg(3,0);
	writeTSCReg(0,1);
	writeTSCReg(0x21,0x20);
	writeTSCReg(0x22,0xF0);
	readTSCReg(0x22);
	writeTSCReg(0x22,0xF0);
	writeTSCReg(0,0);
	writeTSCReg(0x52,0x80);
	writeTSCReg(0x51,0x00);
	writeTSCReg(0,3);
	readTSCReg(0x02);
	writeTSCReg(2,0x98);
	writeTSCReg(0,0xff);
	writeTSCReg(5,0);

	writePowerManagement(0x00,0x0D);
}

void fifocheck (void)
{

	if(*((vu32*)0x027FFE24) == (u32)0x027FFE04)
	{
		if(fifoCheckValue32(FIFO_USER_08)) { NDSTouchscreenMode(); }
		if(fifoCheckValue32(FIFO_USER_04)) {
			if(fifoCheckValue32(FIFO_USER_05)) { REG_SCFG_CLK = 0x0181; } else { REG_SCFG_CLK = 0x0180; }
		}
		if(fifoCheckValue32(FIFO_USER_06)) { /*Do Nothing*/ } else { REG_SCFG_ROM = 0x703; }
		if(fifoCheckValue32(FIFO_USER_05)) { REG_SCFG_EXT = 0x93A53000; } else { REG_SCFG_EXT = 0x12A03000; }
		irqDisable (IRQ_ALL);
		*((vu32*)0x027FFE34) = (u32)0x06000000;

		swiSoftReset();
	} 
}

