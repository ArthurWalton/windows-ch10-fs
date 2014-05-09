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

#include "ntifs.h"
#include "fsd.h"
#include "ch10_fs.h"
#include "border.h"
#include "ch10fs.h"

#pragma code_seg(FSD_PAGED_CODE)

NTSTATUS
FsdIsDeviceCh10fs (
    IN PDEVICE_OBJECT DeviceObject
    )
{
    PUCHAR          Buffer;
    LARGE_INTEGER   Offset;
    NTSTATUS        Status;
	int i;

	PAGED_CODE();

    ASSERT(DeviceObject != NULL);

    Buffer = FsdAllocatePool(NonPagedPool, SECTOR_SIZE, '1ceR');

    if (!Buffer)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }


	Offset.QuadPart = CH10_MAGIC_OFFSET;

	Status = FsdReadBlockDeviceOverrideVerify(
		DeviceObject,
		&Offset,
		SECTOR_SIZE,
		Buffer
		);

	//DbgPrint(DRIVER_NAME ": First Disk sector\n");
	//DbgPrintMem((char *)Buffer, SECTOR_SIZE);
	//DbgPrint("\n");
		
	///DbgPrint((DRIVER_NAME ": Magic buffer in memory\n"));
	//DbgPrint((DRIVER_NAME ": %s\n", CH10_MAGIC));
	
	//DbgPrint((DRIVER_NAME ": Magic buffer on disk\n"));
	//DbgPrint((DRIVER_NAME ": %s\n", Buffer));
	
	//DbgPrint(DRIVER_NAME ": Magic word size %d\n", sizeof(CH10_MAGIC) - 1);
	
    if (!NT_SUCCESS(Status) ||
        RtlCompareMemory(Buffer, CH10_MAGIC, sizeof(CH10_MAGIC) - 1) != (sizeof(CH10_MAGIC) - 1)
        )
    {
        Status = STATUS_UNRECOGNIZED_VOLUME;
    }

    FsdFreePool(Buffer);

    return Status;
}

NTSTATUS
FsdIsDeviceSameCh10fs (
    IN PDEVICE_OBJECT   DeviceObject,
    IN ULONG            CheckSum
    )
{
	PAGED_CODE();

	return STATUS_SUCCESS;
}

#pragma code_seg() // end FSD_PAGED_CODE
