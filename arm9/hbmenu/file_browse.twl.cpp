/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2017
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

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

#include "file_browse.h"
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <dirent.h>

#include <nds.h>

#include "hbmenu.h"
#include "iconTitle.h"
#include "read_card.h"
#include "audio.h"
#include "tonccpy.h"
#include "launcherData.h"

#define SCREEN_COLS 30
#define ENTRIES_PER_SCREEN 20
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

using namespace std;

ALIGN(4) sNDSHeaderExt ntrHeader;

struct DirEntry {
	string name;
	bool isDirectory;
};


static bool cardInserted = false;
static bool cardLoaded = false;
static bool initialLoad = true;

extern bool cartSelected;

bool cartInsertedOnBoot = false;


static void cartCheck(tLauncherSettings launchdata) {
	switch (REG_SCFG_MC) {
		case 0x10: {
			if (!cardInserted)cardInserted = true; 
			initialLoad = false;
			if (cartInsertedOnBoot) {
				cartInsertedOnBoot = false;
				launchdata.cachedChipID = 0x00000000;
			}
		}break;
		case 0x11: { 
			cardInserted = false; 
			initialLoad = false;
			if (cartSelected)cartSelected = false;
			if (cartInsertedOnBoot) {
				cartInsertedOnBoot = false;
				launchdata.cachedChipID = 0x00000000;
			}
		}break;
		case 0x18: {
			cardInserted = true;
		}break;
	}
	if (!cardInserted) {
		if (cardLoaded) {
			clearCartIcon(true);
			cardLoaded = false;
			if (cartSelected)cartSelected = false;
		}
		if (cartInsertedOnBoot)cartInsertedOnBoot = false;
		if (cartSelected)cartSelected = false;
		launchdata.cachedChipID = 0x00000000;
		ToggleBackground(true);
		return;
	}
	if (cardInserted && initialLoad) {
		launchdata.cachedChipID = *(u32*)0x02FFFC00;
		cardInitShort(&ntrHeader);
		cartIconUpdate(0, initialLoad);
		initialLoad = false;
		cardLoaded = true;
		cartInsertedOnBoot = true;
		ToggleBackground(false);
	} else if (cardInserted && !cardLoaded){
		cardInit(&ntrHeader);
		while(REG_ROMCTRL & CARD_BUSY)swiWaitForVBlank();
		launchdata.cachedChipID = cardGetId();
		for (int i = 0; i < 30; i++)swiWaitForVBlank();
		cartIconUpdate(ntrHeader.bannerOffset, false);
		ToggleBackground(false);
		cardLoaded = true;
	}
}

bool nameEndsWith (const string& name, const vector<string> extensionList) {

	if (name.size() == 0) return false;
	if (name.front() == '.') return false;

	if (extensionList.size() == 0) return true;

	for (int i = 0; i < (int)extensionList.size(); i++) {
		const string ext = extensionList.at(i);
		if ( strcasecmp (name.c_str() + name.size() - ext.size(), ext.c_str()) == 0) return true;
	}
	return false;
}

bool dirEntryPredicate (const DirEntry& lhs, const DirEntry& rhs) {
	if (!lhs.isDirectory && rhs.isDirectory)return false;
	if (lhs.isDirectory && !rhs.isDirectory)return true;
	return strcasecmp(lhs.name.c_str(), rhs.name.c_str()) < 0;
}

void getDirectoryContents (vector<DirEntry>& dirContents, const vector<string> extensionList) {
	struct stat st;

	dirContents.clear();

	DIR *pdir = opendir (".");

	if (pdir == NULL) {
		iprintf ("Unable to open the directory.\n");
	} else {

		while(true) {
			DirEntry dirEntry;

			struct dirent* pent = readdir(pdir);
			if(pent == NULL) break;

			stat(pent->d_name, &st);
			dirEntry.name = pent->d_name;
			dirEntry.isDirectory = (st.st_mode & S_IFDIR) ? true : false;

			if (dirEntry.name.compare(".") != 0 && (dirEntry.isDirectory || nameEndsWith(dirEntry.name, extensionList))) {
				dirContents.push_back (dirEntry);
			}

		}

		closedir(pdir);
	}

	sort(dirContents.begin(), dirContents.end(), dirEntryPredicate);
}

void getDirectoryContents (vector<DirEntry>& dirContents) {
	vector<string> extensionList;
	getDirectoryContents (dirContents, extensionList);
}

void showDirectoryContents (const vector<DirEntry>& dirContents, int startRow) {
	char path[PATH_MAX];

	getcwd(path, PATH_MAX);

	// Clear the screen
	consoleClear();
	
	// Print the path
	if (strlen(path) < SCREEN_COLS) {
		iprintf ("%s", path);
	} else {
		iprintf ("%s", path + strlen(path) - SCREEN_COLS);
	}

	// Move to 2nd row
	iprintf ("\x1b[1;0H");
	// Print line of dashes
	iprintf ("------------------------------");

	// Print directory listing
	for (int i = 0; i < ((int)dirContents.size() - startRow) && i < ENTRIES_PER_SCREEN; i++) {
		const DirEntry* entry = &dirContents.at(i + startRow);
		char entryName[SCREEN_COLS + 1];

		// Set row
		iprintf ("\x1b[%d;0H", i + ENTRIES_START_ROW);

		if (entry->isDirectory) {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 3] = '\0';
			iprintf (" [%s]", entryName);
		} else {
			strncpy (entryName, entry->name.c_str(), SCREEN_COLS);
			entryName[SCREEN_COLS - 1] = '\0';
			iprintf (" %s", entryName);
		}
	}
}

string browseForFile (const vector<string>& extensionList, tLauncherSettings launchdata) {
	int pressed = 0;
	int screenOffset = 0;
	int fileOffset = 0;
	vector<DirEntry> dirContents;

	getDirectoryContents (dirContents, extensionList);
	showDirectoryContents (dirContents, screenOffset);

	while (true) {
		// Clear old cursors
		for (int i = ENTRIES_START_ROW; i < ENTRIES_PER_SCREEN + ENTRIES_START_ROW; i++) {
			iprintf ("\x1b[%d;0H ", i);
		}
		// Show cursor
		iprintf ("\x1b[%d;0H*", fileOffset - screenOffset + ENTRIES_START_ROW);

		iconTitleUpdate (dirContents.at(fileOffset).isDirectory, dirContents.at(fileOffset).name);

		// Power saving loop. Only poll the keys once per frame and sleep the CPU if there is nothing else to do
		do {
			cartCheck(launchdata);
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!pressed);
		if (!cartSelected) {
			if (pressed & KEY_UP) {		fileOffset -= 1; PlaySelectSFX(); }
			if (pressed & KEY_DOWN) { 	fileOffset += 1; PlaySelectSFX(); }
			if (pressed & KEY_LEFT) { 	fileOffset -= ENTRY_PAGE_LENGTH; PlaySelectSFX(); }
			if (pressed & KEY_RIGHT) {	fileOffset += ENTRY_PAGE_LENGTH; PlaySelectSFX(); }
		
			if (fileOffset < 0)fileOffset = dirContents.size() - 1;		// Wrap around to bottom of list
			if (fileOffset > ((int)dirContents.size() - 1))		fileOffset = 0;		// Wrap around to top of list
	
			// Scroll screen if needed
			if (fileOffset < screenOffset) {
				screenOffset = fileOffset;
				showDirectoryContents (dirContents, screenOffset);
			}
			if (fileOffset > screenOffset + ENTRIES_PER_SCREEN - 1) {
				screenOffset = fileOffset - ENTRIES_PER_SCREEN + 1;
				showDirectoryContents (dirContents, screenOffset);
			}
		}
		if ((pressed & KEY_X)) {
			if (cardLoaded) {
				PlaySelectSFX();
				if (cartSelected) {
					cartSelected = false;
					ToggleBackground(false);
				} else if (cardLoaded) {
					cartSelected = true;
					ToggleBackground(false);
				}
			} else {
				PlayWrongSFX();
			}
		}
		
		if (cartSelected) {
			if (pressed & KEY_UP)PlayWrongSFX();
			if (pressed & KEY_DOWN)PlayWrongSFX();
			if (pressed & KEY_LEFT)PlayWrongSFX();
			if (pressed & KEY_RIGHT)PlayWrongSFX();
				
			if ((pressed & KEY_A)) {
				if (cardLoaded) {
					PlayLaunchSFX();
					// Clear the screen
					consoleClear();
					return "CARTBOOT";
				} else {
					PlayWrongSFX();
				}
			}
		} else {
			if (pressed & KEY_A) {
				DirEntry* entry = &dirContents.at(fileOffset);
				if (entry->isDirectory) {
					PlaySelectSFX();
					iprintf("Entering directory\n");
					// Enter selected directory
					chdir (entry->name.c_str());
					getDirectoryContents (dirContents, extensionList);
					screenOffset = 0;
					fileOffset = 0;
					showDirectoryContents (dirContents, screenOffset);
				} else {
					PlayLaunchSFX();
					// Clear the screen
					consoleClear();
					for (int i = 0; i < 32; i++)swiWaitForVBlank(); // Give Launch sfx time to finish.
					// Return the chosen file
					return entry->name;
				}
			}
	
			if (pressed & KEY_B) {
				if (cartSelected) { PlayWrongSFX(); } else { PlayBackSFX(); }
				// Go up a directory
				chdir ("..");
				getDirectoryContents (dirContents, extensionList);
				screenOffset = 0;
				fileOffset = 0;
				showDirectoryContents (dirContents, screenOffset);
			}
		}
	}
}

