/*
    This is a File System Recognizer for IRIG 106 Ch10 Filesystem
	Copyright (C) 2014 Arthur Walton.

	Heavily derived from the File System Recognizer for RomFs
    Copyright (C) 2001 Bo Brantén.
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

#include "ntifs.h"
#include "fsrec.h"
#include "ch10_fs.h"

#pragma code_seg(FSD_PAGED_CODE)

NTSTATUS
FsrIsDeviceCh10fs (
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PUCHAR          Buffer;
    LARGE_INTEGER   Offset;
    NTSTATUS        Status;
	int i;

    ASSERT(DeviceObject != NULL);

    Buffer = ExAllocatePool(NonPagedPoolCacheAligned, SECTOR_SIZE);

    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


	Offset.QuadPart = CH10_MAGIC_OFFSET;

	Status = FsrReadBlockDevice(
		DeviceObject,
		&Offset,
		SECTOR_SIZE,
		Buffer
		);
	
    if (!NT_SUCCESS(Status) ||
        RtlCompareMemory(Buffer, CH10_MAGIC, sizeof(CH10_MAGIC) - 1) != (sizeof(CH10_MAGIC) - 1)
        )
    {
        Status = STATUS_UNRECOGNIZED_VOLUME;
    }

    ExFreePool(Buffer);

    return Status;
}

NTSTATUS
FsdIsDeviceSameCh10fs (
    IN PDEVICE_OBJECT   DeviceObject,
    IN ULONG            CheckSum
    )
{
	return STATUS_SUCCESS;
}

#pragma code_seg() // end FSD_PAGED_CODE
