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

#ifndef _CH10_FS_
#define _CH10_FS_
#include "ch10_fs.h"
#include "ntifs.h"
#include "fsd.h"

size_t ch10fs_strnlen(const char * s, size_t count);

void DbgPrintMem(char *buffer, __u32 size);

struct ch10_dir_entry *GetDirEntryAtIndex(struct ch10_dir_block dirblocks[], __u32 index);

__u32 GetDirEntryIndex(struct ch10_dir_block dirblocks[], struct ch10_dir_entry *entry);

__u32 FsdCh10GetFileCount(struct ch10_dir_block dirblocks[]);

__u32 FsdCh10PartitionSize(struct ch10_dir_block *DirBlocks);
#endif
