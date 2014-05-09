/*
    This is a IRIG 106 Chapter 10 file system driver for Windows XP/7.
	Copyright (C) 2014 Arthur Walton.

	Heavily derived from the RomFS filesystem driver 
    Copyright (C) 1999, 2000, 2001, 2002 Bo Brantén.
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ltypes.h"
#include "ch10_fs.h"
#include "ch10fs.h"
#include "ntifs.h"
#include "border.h"

void DbgPrintMem(char *buffer, __u32 size) {
	__u32 i;
	for(i = 0; i < size; i++) {
		if(size % 10 == 0) {
			if(size > 0) DbgPrint("\n");
			DbgPrint("0x%#x", size);
		}
		DbgPrint("%02x ", buffer[i]);
	}
}

struct ch10_dir_entry *GetDirEntryAtIndex(struct ch10_dir_block dirblocks[], __u32 index) {
	struct ch10_dir_block *dir_block = &dirblocks[index / MAX_FILES_PER_DIR];
	struct ch10_dir_entry *dir_entry = &dir_block->dirEntries[index % MAX_FILES_PER_DIR];
	return dir_entry;
}

__u32 GetDirEntryIndex(struct ch10_dir_block dirblocks[], struct ch10_dir_entry *entry) {
	int dirIndex, entryIndex;
	__u32 index = 0;
	for(dirIndex = 0; dirIndex < CH10_MAX_DIR_BLOCKS; dirIndex++) {
		struct ch10_dir_block *dir_block = &dirblocks[dirIndex];
		for(entryIndex = 0; entryIndex < dir_block->numEntries; entryIndex++) {
			struct ch10_dir_entry *dir_entry = &dir_block->dirEntries[entryIndex];
			if(dir_entry->blockNum == entry->blockNum) return index;
			index++;
		}
	}
	
	return index;
}

__u32 FsdCh10GetFileCount(struct ch10_dir_block dirblocks[]) {
	int dirIndex;
	__u32 count = 0;
	for(dirIndex = 0; dirIndex < CH10_MAX_DIR_BLOCKS; dirIndex++) {
		struct ch10_dir_block *dir_block = &dirblocks[dirIndex];
		count += be16_to_cpu(dir_block->numEntries);
	}
	return count;
}

__u32 FsdCh10PartitionSize(struct ch10_dir_block dirblocks[]) {
	int dirIndex, entryIndex;
	__u32 size = 0;
	for(dirIndex = 0; dirIndex < CH10_MAX_DIR_BLOCKS; dirIndex++) {
		struct ch10_dir_block *dir_block = &dirblocks[dirIndex];
		for(entryIndex = 0; entryIndex < MAX_FILES_PER_DIR; entryIndex++) {
			struct ch10_dir_entry *dir_entry = &dir_block->dirEntries[entryIndex];
			size += (__u32)dir_entry->size;
		}
	}
	return size;
}

