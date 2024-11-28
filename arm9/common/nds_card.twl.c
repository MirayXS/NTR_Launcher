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

#include <stddef.h>
#include <nds.h>
#include "nds_card.h"
#include "read_card.h"

void CardReset(bool properReset) {
	int i;
	sysSetCardOwner (BUS_OWNER_ARM9);	// Allow arm9 to access NDS cart
	if (isDSiMode()) { 
		// Reset card slot
		/*disableSlot1();
		for (i = 0; i < 25; i++) { swiWaitForVBlank(); }
		enableSlot1();
		for (i = 0; i < 15; i++) { swiWaitForVBlank(); }*/
		ResetSlot();

		if (!properReset) {
			// Dummy command sent after card reset
			cardParamCommand (CARD_CMD_DUMMY, 0,
				CARD_ACTIVATE | CARD_nRESET | CARD_CLK_SLOW | CARD_BLK_SIZE(1) | CARD_DELAY1(0x1FFF) | CARD_DELAY2(0x3F),
				NULL, 0);
		}
	}
	if (!isDSiMode() || properReset) {
		REG_ROMCTRL=0;
		REG_AUXSPICNT=0;
		for (i = 0; i < 10; i++) { swiWaitForVBlank(); }
		REG_AUXSPICNT=CARD_CR1_ENABLE|CARD_CR1_IRQ;
		REG_ROMCTRL=CARD_nRESET|CARD_SEC_SEED;
		while (REG_ROMCTRL&CARD_BUSY)swiWaitForVBlank();
		cardReset();
		while (REG_ROMCTRL&CARD_BUSY)swiWaitForVBlank();
	}
}


