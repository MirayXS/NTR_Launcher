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

#include <maxmod9.h>
#include "soundbank.h"
#include "soundbank_bin.h"

#define SCREEN_COLS 30
#define ENTRIES_PER_SCREEN 20
#define ENTRIES_START_ROW 2
#define ENTRY_PAGE_LENGTH 10

using namespace std;

static ALIGN(4) sNDSHeaderExt ntrHeader;

struct DirEntry {
	string name;
	bool isDirectory;
};

static mm_sound_effect sfxBack;
static mm_sound_effect sfxLaunch;
static mm_sound_effect sfxSelect;
static mm_sound_effect sfxWrong;
static bool audioReady = false;

void InitAudio() {
	mmInitDefaultMem((mm_addr)soundbank_bin);
	
	mmLoadEffect( SFX_BACK );
	mmLoadEffect( SFX_LAUNCH );
	mmLoadEffect( SFX_SELECT );
	mmLoadEffect( SFX_WRONG );
	
	sfxBack = {
		{ SFX_BACK } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	
	sfxLaunch = {
		{ SFX_LAUNCH } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	
	sfxSelect = {
		{ SFX_SELECT } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	
	sfxWrong = {
		{ SFX_WRONG } ,			// id
		(int)(1.0f * (1<<10)),	// rate
		0,		// handle
		255,	// volume
		128,	// panning
	};
	audioReady = true;
}

static bool cardInserted = false;
static bool cardLoaded = false;
static bool initialLoad = true;

extern bool cartSelected;

bool cartInsertedOnBoot = false;


static void cartCheck() {
	switch (REG_SCFG_MC) {
		case 0x10: {
			if (!cardInserted)cardInserted = true; 
			initialLoad = false;
			if (cartInsertedOnBoot)cartInsertedOnBoot = false;
		}break;
		case 0x11: { 
			cardInserted = false; 
			initialLoad = false;
			if (cartSelected)cartSelected = false;
			if (cartInsertedOnBoot)cartInsertedOnBoot = false;
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
			ToggleBackground();
		}
		if (cartInsertedOnBoot)cartInsertedOnBoot = false;
		if (cartSelected)cartSelected = false;
		return;
	}
	if (cardInserted && initialLoad) {
		cardInitShort(&ntrHeader);
		cartIconUpdate(0, initialLoad);
		initialLoad = false;
		cardLoaded = true;
		cartInsertedOnBoot = true;
	} else if (cardInserted && !cardLoaded){
		cardInit(&ntrHeader);
		for (int i = 0; i < 25; i++)swiWaitForVBlank();
		cartIconUpdate(ntrHeader.bannerOffset, false);
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

string browseForFile (const vector<string>& extensionList) {
	if (!audioReady)InitAudio();
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
			cartCheck();
			scanKeys();
			pressed = keysDownRepeat();
			swiWaitForVBlank();
		} while (!pressed);
		if (!cartSelected) {
			if (pressed & KEY_UP) {		fileOffset -= 1; mmEffectEx(&sfxSelect); }
			if (pressed & KEY_DOWN) { 	fileOffset += 1; mmEffectEx(&sfxSelect); }
			if (pressed & KEY_LEFT) { 	fileOffset -= ENTRY_PAGE_LENGTH; mmEffectEx(&sfxSelect); }
			if (pressed & KEY_RIGHT) {	fileOffset += ENTRY_PAGE_LENGTH; mmEffectEx(&sfxSelect); }
		
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
				mmEffectEx(&sfxSelect);
				if (cartSelected) {
					cartSelected = false;
					ToggleBackground();
				} else if (cardLoaded) {
					cartSelected = true;
					ToggleBackground();
				}
			} else {
				mmEffectEx(&sfxWrong);
			}
		}
		
		if (cartSelected) {
			if (pressed & KEY_UP)mmEffectEx(&sfxWrong);
			if (pressed & KEY_DOWN)mmEffectEx(&sfxWrong);
			if (pressed & KEY_LEFT)mmEffectEx(&sfxWrong);
			if (pressed & KEY_RIGHT)mmEffectEx(&sfxWrong);
				
			if ((pressed & KEY_A)) {
				if (cardLoaded) {
					mmEffectEx(&sfxLaunch);
					// Clear the screen
					consoleClear();
					return "CARTBOOT";
				} else {
					mmEffectEx(&sfxWrong);
				}
			}
		} else {
			if (pressed & KEY_A) {
				DirEntry* entry = &dirContents.at(fileOffset);
				if (entry->isDirectory) {
					mmEffectEx(&sfxSelect);
					iprintf("Entering directory\n");
					// Enter selected directory
					chdir (entry->name.c_str());
					getDirectoryContents (dirContents, extensionList);
					screenOffset = 0;
					fileOffset = 0;
					showDirectoryContents (dirContents, screenOffset);
				} else {
					mmEffectEx(&sfxLaunch);
					// Clear the screen
					consoleClear();
					// Return the chosen file
					return entry->name;
				}
			}
	
			if (pressed & KEY_B) {
				mmEffectEx(&sfxBack);
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

