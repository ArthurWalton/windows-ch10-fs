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

#pragma code_seg(FSR_INIT_CODE)

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    )
{
    UNICODE_STRING  DeviceName;
    NTSTATUS        Status;
    PDEVICE_OBJECT  DeviceObject;

    KdPrint((DRIVER_NAME ": Loading driver\n"));

    DriverObject->MajorFunction[IRP_MJ_FILE_SYSTEM_CONTROL] =
        FsrFileSystemControl;

    RtlInitUnicodeString(&DeviceName, DEVICE_NAME);

    Status = IoCreateDevice(
        DriverObject,
        0,
        &DeviceName,
        FILE_DEVICE_DISK_FILE_SYSTEM,
        0,
        FALSE,
        &DeviceObject
        );

    if (NT_SUCCESS(Status))
    {
        IoRegisterFileSystem(DeviceObject);
    }

    MmPageEntireDriver(DriverEntry);

    return Status;
}

#pragma code_seg(FSR_PAGED_CODE)

VOID
DriverUnload (
    IN PDRIVER_OBJECT DriverObject
    )
{
    KdPrint((DRIVER_NAME ": Unloading driver\n"));
}

NTSTATUS
FsrFileSystemControl (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    )
{
    PIO_STACK_LOCATION  IoStackLocation;
    NTSTATUS            Status;
    UNICODE_STRING      RegistryPath;

    PAGED_CODE();

    IoStackLocation = IoGetCurrentIrpStackLocation(Irp);

    switch (IoStackLocation->MinorFunction)
    {
    case IRP_MN_MOUNT_VOLUME:

        KdPrint((
            DRIVER_NAME ": IRP_MJ_FILE_SYSTEM_CONTROL: IRP_MN_MOUNT_VOLUME\n"
            ));

        Status = FsrIsDeviceCh10fs(
            IoStackLocation->Parameters.MountVolume.DeviceObject
            );

        if (NT_SUCCESS(Status))
        {
            KdPrint((DRIVER_NAME ": Found RomFs\n"));
            Status = STATUS_FS_DRIVER_REQUIRED;
        }

        Irp->IoStatus.Status = Status;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return Status;

    case IRP_MN_LOAD_FILE_SYSTEM:

        KdPrint((
            DRIVER_NAME
            ": IRP_MJ_FILE_SYSTEM_CONTROL: IRP_MN_LOAD_FILE_SYSTEM\n"
            ));

        RtlInitUnicodeString(&RegistryPath, FSD_REGISTRY_PATH);

        Status = ZwLoadDriver(&RegistryPath);

        if (!NT_SUCCESS(Status))
        {
            KdPrint((
                DRIVER_NAME ": ZwLoadDriver failed with error %#x\n",
                Status
                ));
        }

        Irp->IoStatus.Status = Status;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        IoUnregisterFileSystem(DeviceObject);

        DeviceObject->DriverObject->DriverUnload = DriverUnload;

        IoDeleteDevice(DeviceObject);

        RtlInitUnicodeString(&RegistryPath, FSR_REGISTRY_PATH);

        ZwUnloadDriver(&RegistryPath);

        return Status;

    default:

        KdPrint((
            DRIVER_NAME ": IRP_MJ_FILE_SYSTEM_CONTROL: "
            "Unknown minor function %#x\n",
            IoStackLocation->MinorFunction
            ));

        Status = STATUS_INVALID_DEVICE_REQUEST;

        Irp->IoStatus.Status = Status;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return Status;
    }
}

#pragma code_seg()  // end FSR_PAGED_CODE
