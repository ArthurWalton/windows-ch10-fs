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

#pragma code_seg(FSR_PAGED_CODE)

NTSTATUS
FsrReadBlockDevice (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN OUT PVOID        Buffer
    )
{
    KEVENT          Event;
    PIRP            Irp;
    IO_STATUS_BLOCK IoStatus;
    NTSTATUS        Status;

    PAGED_CODE();

    ASSERT(DeviceObject != NULL);
    ASSERT(Offset != NULL);
    ASSERT(Buffer != NULL);

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildSynchronousFsdRequest(
        IRP_MJ_READ,
        DeviceObject,
        Buffer,
        Length,
        Offset,
        &Event,
        &IoStatus
        );

    if (!Irp)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Status = IoCallDriver(DeviceObject, Irp);

    if (Status == STATUS_PENDING)
    {
        KeWaitForSingleObject(
            &Event,
            Executive,
            KernelMode,
            FALSE,
            NULL
            );
        Status = IoStatus.Status;
    }

    return Status;
}

#pragma code_seg()  // end FSR_PAGED_CODE
