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
#ifndef _ROM_FS_
#define _ROM_FS_

#define SECTOR_SIZE         512

#define CH10_MAGIC         "FORTYtwo"

#define CH10_MAGIC_OFFSET  512

//
// Types used by Linux
//
#include "ltypes.h"

//
// Use 1 byte packing of on-disk structures
//
#include <pshpack1.h>

//
// The following is a subset of linux/include/linux/ch10fs_fs.h from
// version 2.2.14
//

/* The basic structures of the ch10fs filesystem */
#define CH10_BLOCK_SIZE    512

#define CH10_MAXFN 48



#define MAX_FILES_PER_DIR 4
#define CH10_MAX_DIR_BLOCKS 64

/*
 * Ch10 Directory Entry
 */
struct ch10_dir_entry {
  __u8 name[56];       // name of the directory entry
  __u64 blockNum;      // block number that the entry starts at
  __u64 numBlocks;     // length of the entry in blocks
  __u64 size;          // length of the entry in bytes
  __u8 createDate[8];  // date entry was created
  __u8 createTime[8];  // time entry was created
  __u8 timeType;       // time system the previous date and time were stored in
  __u8 reserved[7];    // currently unused, reserved for future use
  __u8 closeTime[8];   // time this entry was finished being written
};

/*
 * Ch10 Directory Block
 */
struct ch10_dir_block {
  __u8 magicNumAscii[8];  // Identifies this as being a directory block, always set to FORTYtwo
  __u8 revNum;            // revision number of the data recording standard in use
  __u8 shutdown;          // flag to indicate filesystem was not properly shutdown while writing to this directory
  __u16 numEntries;       // number of directory entries/files that are in this block
  __u32 bytesPerBlock;    // number of bytes per block
  __u8 volName[32];       // name of this directory block
  __u64 forwardLink:64;   // block address of next directory block
  __u64 reverseLink:64;   // block address of previous directory block
  struct ch10_dir_entry dirEntries[MAX_FILES_PER_DIR]; // all entries/files in the block
};

#include <poppack.h>

#endif
