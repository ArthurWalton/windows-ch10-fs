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

NTSTATUS
FsdFileSystemControl (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    NTSTATUS Status;

    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == ICX) &&
           (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

    switch (IrpContext->MinorFunction)
    {
    case IRP_MN_USER_FS_REQUEST:
		DbgPrint((DRIVER_NAME ": IRP_MN_USER_FS_REQUEST\n"));
        Status = FsdUserFsRequest(IrpContext);
        break;

    case IRP_MN_MOUNT_VOLUME:
		DbgPrint((DRIVER_NAME ": IRP_MN_MOUNT_VOLUME\n"));
        Status = FsdMountVolume(IrpContext);
		break;

    case IRP_MN_VERIFY_VOLUME:
		DbgPrint((DRIVER_NAME ": IRP_MN_VERIFY_VOLUME\n"));
        Status = FsdVerifyVolume(IrpContext);
        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
        IrpContext->Irp->IoStatus.Status = Status;
        FsdCompleteRequest(IrpContext->Irp, IO_NO_INCREMENT);
        FsdFreeIrpContext(IrpContext);
    }

    return Status;
}

NTSTATUS
FsdUserFsRequest (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PIRP                Irp;
    PIO_STACK_LOCATION  IrpSp;
    ULONG               FsControlCode;
    NTSTATUS            Status;

    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == ICX) &&
           (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

    Irp = IrpContext->Irp;

    IrpSp = IoGetCurrentIrpStackLocation(Irp);

#ifndef _GNU_NTIFS_
    FsControlCode =
        IrpSp->Parameters.FileSystemControl.FsControlCode;
#else
    FsControlCode = ((PEXTENDED_IO_STACK_LOCATION)
        IrpSp)->Parameters.FileSystemControl.FsControlCode;
#endif

    switch (FsControlCode)
    {
    case FSCTL_LOCK_VOLUME:
        Status = FsdLockVolume(IrpContext);
        break;

    case FSCTL_UNLOCK_VOLUME:
        Status = FsdUnlockVolume(IrpContext);
        break;

    case FSCTL_DISMOUNT_VOLUME:
        Status = FsdDismountVolume(IrpContext);
        break;

    case FSCTL_IS_VOLUME_MOUNTED:
        Status = FsdIsVolumeMounted(IrpContext);
        break;

    default:
        Status = STATUS_INVALID_DEVICE_REQUEST;
        IrpContext->Irp->IoStatus.Status = Status;
        FsdCompleteRequest(IrpContext->Irp, IO_NO_INCREMENT);
        FsdFreeIrpContext(IrpContext);
    }

    return Status;
}

#pragma code_seg(FSD_PAGED_CODE)

NTSTATUS
FsdLockVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PDEVICE_OBJECT  DeviceObject;
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;
    PFSD_VCB        Vcb;
#if (VER_PRODUCTBUILD >= 2195)
    BOOLEAN         VolumeLockNotifyed = FALSE;
#endif
    BOOLEAN         VcbResourceAcquired = FALSE;

	PAGED_CODE();

    __try
    {
        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == ICX) &&
               (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		KeEnterCriticalRegion();

        DeviceObject = IrpContext->DeviceObject;

        if (DeviceObject == FsdGlobalData.DeviceObject)
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
            __leave;
        }

        Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

        ASSERT(Vcb != NULL);

        ASSERT((Vcb->Identifier.Type == VCB) &&
               (Vcb->Identifier.Size == sizeof(FSD_VCB)));

#if (VER_PRODUCTBUILD >= 2195)

        FsRtlNotifyVolumeEvent(IrpContext->FileObject, FSRTL_VOLUME_LOCK);

        VolumeLockNotifyed = TRUE;

#endif

        ExAcquireResourceSharedLite(
             &Vcb->MainResource,
             TRUE
             );

        VcbResourceAcquired = TRUE;

        if (FlagOn(Vcb->Flags, VCB_VOLUME_LOCKED))
        {
            KdPrint((DRIVER_NAME ": *** Volume is already locked ***\n"));

            Status = STATUS_ACCESS_DENIED;

            __leave;
        }

        if (Vcb->OpenFileHandleCount)
        {
            KdPrint((DRIVER_NAME ": *** Open files exists ***\n"));

            Status = STATUS_ACCESS_DENIED;

            __leave;
        }

        ExReleaseResourceForThreadLite(
            &Vcb->MainResource,
            ExGetCurrentResourceThread()
            );

        VcbResourceAcquired = FALSE;

        FsdPurgeVolume(Vcb, TRUE);

        ExAcquireResourceExclusiveLite(
             &Vcb->MainResource,
             TRUE
             );

        VcbResourceAcquired = TRUE;

        if (Vcb->ReferenceCount > 1)
        {
            KdPrint((DRIVER_NAME ": *** Could not purge cached files ***\n"));

            Status = STATUS_ACCESS_DENIED;

            __leave;
        }

        SetFlag(Vcb->Flags, VCB_VOLUME_LOCKED);

        FsdSetVpbFlag(Vcb->Vpb, VPB_LOCKED);

        KdPrint((DRIVER_NAME ": Volume locked\n"));

        Status = STATUS_SUCCESS;
    }
    __finally
    {
        if (VcbResourceAcquired)
        {
            ExReleaseResourceForThreadLite(
                &Vcb->MainResource,
                ExGetCurrentResourceThread()
                );
        }

		KeLeaveCriticalRegion();

#if (VER_PRODUCTBUILD >= 2195)

        if (!NT_SUCCESS(Status) && VolumeLockNotifyed)
        {
            FsRtlNotifyVolumeEvent(IrpContext->FileObject, FSRTL_VOLUME_LOCK_FAILED);
        }

#endif

        if (!AbnormalTermination())
        {
            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest(
                IrpContext->Irp,
                (CCHAR)
                (NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
                );

            FsdFreeIrpContext(IrpContext);
        }
    }

    return Status;
}

NTSTATUS
FsdUnlockVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PDEVICE_OBJECT  DeviceObject;
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;
    PFSD_VCB        Vcb;
    BOOLEAN         VcbResourceAcquired = FALSE;

	PAGED_CODE();

    __try
    {
        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == ICX) &&
               (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		KeEnterCriticalRegion();

        DeviceObject = IrpContext->DeviceObject;

        if (DeviceObject == FsdGlobalData.DeviceObject)
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
            __leave;
        }

        Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

        ASSERT(Vcb != NULL);

        ASSERT((Vcb->Identifier.Type == VCB) &&
               (Vcb->Identifier.Size == sizeof(FSD_VCB)));

        ExAcquireResourceExclusiveLite(
             &Vcb->MainResource,
             TRUE
             );

        VcbResourceAcquired = TRUE;

        if (!FlagOn(Vcb->Flags, VCB_VOLUME_LOCKED))
        {
            KdPrint((DRIVER_NAME ": *** Volume is not locked ***\n"));

            Status = STATUS_ACCESS_DENIED;

            __leave;
        }

        ClearFlag(Vcb->Flags, VCB_VOLUME_LOCKED);

        FsdClearVpbFlag(Vcb->Vpb, VPB_LOCKED);

        KdPrint((DRIVER_NAME ": Volume unlocked\n"));

        Status = STATUS_SUCCESS;
    }
    __finally
    {
        if (VcbResourceAcquired)
        {
            ExReleaseResourceForThreadLite(
                &Vcb->MainResource,
                ExGetCurrentResourceThread()
                );
        }

		KeLeaveCriticalRegion();

#if (VER_PRODUCTBUILD >= 2195)

        if (NT_SUCCESS(Status))
        {
            FsRtlNotifyVolumeEvent(IrpContext->FileObject, FSRTL_VOLUME_UNLOCK);
        }

#endif

        if (!AbnormalTermination())
        {
            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest(
                IrpContext->Irp,
                (CCHAR)
                (NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
                );

            FsdFreeIrpContext(IrpContext);
        }
    }

    return Status;
}

NTSTATUS
FsdDismountVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PDEVICE_OBJECT  DeviceObject;
    NTSTATUS        Status = STATUS_UNSUCCESSFUL;
    PFSD_VCB        Vcb;
#if (VER_PRODUCTBUILD >= 2195)
    BOOLEAN         VolumeDismountNotifyed = FALSE;
#endif
    BOOLEAN         VcbResourceAcquired = FALSE;

	PAGED_CODE();

    __try
    {
        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == ICX) &&
               (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		KeEnterCriticalRegion();

        DeviceObject = IrpContext->DeviceObject;

        if (DeviceObject == FsdGlobalData.DeviceObject)
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
            __leave;
        }

        Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

        ASSERT(Vcb != NULL);

        ASSERT((Vcb->Identifier.Type == VCB) &&
               (Vcb->Identifier.Size == sizeof(FSD_VCB)));

#if (VER_PRODUCTBUILD >= 2195)

        FsRtlNotifyVolumeEvent(IrpContext->FileObject, FSRTL_VOLUME_DISMOUNT);

        VolumeDismountNotifyed = TRUE;

#endif

        ExAcquireResourceExclusiveLite(
             &Vcb->MainResource,
             TRUE
             );

        VcbResourceAcquired = TRUE;

        if (!FlagOn(Vcb->Flags, VCB_VOLUME_LOCKED))
        {
            KdPrint((DRIVER_NAME ": *** Volume is not locked ***\n"));

            Status = STATUS_ACCESS_DENIED;

            __leave;
        }

        SetFlag(Vcb->Flags, VCB_DISMOUNT_PENDING);

        KdPrint((DRIVER_NAME ": Volume dismount pending\n"));

        Status = STATUS_SUCCESS;
    }
    __finally
    {
        if (VcbResourceAcquired)
        {
            ExReleaseResourceForThreadLite(
                &Vcb->MainResource,
                ExGetCurrentResourceThread()
                );
        }

		KeLeaveCriticalRegion();

#if (VER_PRODUCTBUILD >= 2195)

        if (!NT_SUCCESS(Status) && VolumeDismountNotifyed)
        {
            FsRtlNotifyVolumeEvent(IrpContext->FileObject, FSRTL_VOLUME_DISMOUNT_FAILED);
        }

#endif

        if (!AbnormalTermination())
        {
            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest(
                IrpContext->Irp,
                (CCHAR)
                (NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
                );

            FsdFreeIrpContext(IrpContext);
        }
    }

    return Status;
}

#pragma code_seg() // end FSD_PAGED_CODE

NTSTATUS
FsdIsVolumeMounted (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    ASSERT(IrpContext);

    ASSERT((IrpContext->Identifier.Type == ICX) &&
           (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

    return FsdVerifyVolume(IrpContext);
}

#pragma code_seg(FSD_PAGED_CODE)

NTSTATUS
FsdMountVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PDEVICE_OBJECT              MainDeviceObject;
    BOOLEAN                     GlobalDataResourceAcquired = FALSE;
    PIRP                        Irp;
    PIO_STACK_LOCATION          IrpSp;
    PDEVICE_OBJECT              TargetDeviceObject;
    NTSTATUS                    Status = STATUS_UNSUCCESSFUL;
    PDEVICE_OBJECT              VolumeDeviceObject = NULL;
    PFSD_VCB                    Vcb;
    BOOLEAN                     VcbResourceInitialized = FALSE;
    BOOLEAN                     NotifySyncInitialized = FALSE;
    USHORT                      VolumeLabelLength;
    LARGE_INTEGER               Offset;
    ULONG                       IoctlSize;

	PAGED_CODE();

    __try
    {
        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == ICX) &&
               (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		KeEnterCriticalRegion();

        MainDeviceObject = IrpContext->DeviceObject;

        if (MainDeviceObject != FsdGlobalData.DeviceObject)
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
            __leave;
        }

        ExAcquireResourceExclusiveLite(
            &FsdGlobalData.Resource,
            TRUE
            );

        GlobalDataResourceAcquired = TRUE;

        if (FlagOn(FsdGlobalData.Flags, FSD_UNLOAD_PENDING))
        {
            Status = STATUS_UNRECOGNIZED_VOLUME;
            __leave;
        }

        Irp = IrpContext->Irp;

        IrpSp = IoGetCurrentIrpStackLocation(Irp);

        TargetDeviceObject = IrpSp->Parameters.MountVolume.DeviceObject;

        Status = FsdIsDeviceCh10fs(TargetDeviceObject);

        if (!NT_SUCCESS(Status))
        {
			DbgPrint((DRIVER_NAME ": DEVICE is NOT Ch10!!!!\n"));
            __leave;
        }
		
		DbgPrint((DRIVER_NAME ": DEVICE is Ch10!!!!\n"));

        Status = IoCreateDevice(
            MainDeviceObject->DriverObject,
            sizeof(FSD_VCB),
            NULL,
            FILE_DEVICE_DISK_FILE_SYSTEM,
            0,
            FALSE,
            &VolumeDeviceObject
            );

        if (!NT_SUCCESS(Status))
        {
            __leave;
        }

        VolumeDeviceObject->StackSize = TargetDeviceObject->StackSize;

        (IrpSp->Parameters.MountVolume.Vpb)->DeviceObject = VolumeDeviceObject;

        Vcb = (PFSD_VCB) VolumeDeviceObject->DeviceExtension;

        RtlZeroMemory(Vcb, sizeof(FSD_VCB));

        Vcb->Identifier.Type = VCB;
        Vcb->Identifier.Size = sizeof(FSD_VCB);

        ExInitializeResourceLite(&Vcb->MainResource);
        ExInitializeResourceLite(&Vcb->PagingIoResource);

        VcbResourceInitialized = TRUE;

        Vcb->Vpb = IrpSp->Parameters.MountVolume.Vpb;

        InitializeListHead(&Vcb->FcbList);

        InitializeListHead(&Vcb->NotifyList);

        FsRtlNotifyInitializeSync(&Vcb->NotifySync);

        NotifySyncInitialized = TRUE;

        Vcb->DeviceObject = VolumeDeviceObject;

        Vcb->TargetDeviceObject = TargetDeviceObject;

        Vcb->OpenFileHandleCount = 0;

        Vcb->ReferenceCount = 0;

        Vcb->Flags = 0;

        Offset.QuadPart = CH10_MAGIC_OFFSET;

        Status = FsdReadBlockDevice(
            TargetDeviceObject,
            &Offset,
            sizeof(struct ch10_dir_block) * CH10_MAX_DIR_BLOCKS,
            &Vcb->dirblocks
            );

        if (!NT_SUCCESS(Status))
        {
            __leave;
        }

        VolumeLabelLength = (USHORT) strnlen(
            Vcb->dirblocks[0].volName, 32);

        if (VolumeLabelLength > MAXIMUM_VOLUME_LABEL_LENGTH / 2)
        {
            VolumeLabelLength = MAXIMUM_VOLUME_LABEL_LENGTH / 2;
        }

        Vcb->Vpb->VolumeLabelLength = VolumeLabelLength * 2;

        FsdCharToWchar(
            Vcb->Vpb->VolumeLabel,
            Vcb->dirblocks[0].volName,
            VolumeLabelLength
            );

        Vcb->Vpb->SerialNumber = 0x1234;

        IoctlSize = sizeof(DISK_GEOMETRY);

        Status = FsdBlockDeviceIoControl(
            TargetDeviceObject,
            IOCTL_DISK_GET_DRIVE_GEOMETRY,
            NULL,
            0,
            &Vcb->DiskGeometry,
            &IoctlSize
            );

        if (!NT_SUCCESS(Status))
        {
            __leave;
        }

        IoctlSize = sizeof(PARTITION_INFORMATION);

        Status = FsdBlockDeviceIoControl(
            TargetDeviceObject,
            IOCTL_DISK_GET_PARTITION_INFO,
            NULL,
            0,
            &Vcb->PartitionInformation,
            &IoctlSize
            );

        if (!NT_SUCCESS(Status))
        {
            Vcb->PartitionInformation.StartingOffset.QuadPart = 0;

            Vcb->PartitionInformation.PartitionLength.QuadPart =
                Vcb->DiskGeometry.Cylinders.QuadPart *
                Vcb->DiskGeometry.TracksPerCylinder *
                Vcb->DiskGeometry.SectorsPerTrack *
                Vcb->DiskGeometry.BytesPerSector;

            Status = STATUS_SUCCESS;
        }

        InsertTailList(&FsdGlobalData.VcbList, &Vcb->Next);
    }
    __finally
    {
        if (GlobalDataResourceAcquired)
        {
            ExReleaseResourceForThreadLite(
                &FsdGlobalData.Resource,
                ExGetCurrentResourceThread()
                );
        }

		KeLeaveCriticalRegion();

        if (!NT_SUCCESS(Status))
        {
            if (NotifySyncInitialized)
            {
                FsRtlNotifyUninitializeSync(&Vcb->NotifySync);
            }

            if (VcbResourceInitialized)
            {
                ExDeleteResourceLite(&Vcb->MainResource);
                ExDeleteResourceLite(&Vcb->PagingIoResource);
            }

            if (VolumeDeviceObject)
            {
                IoDeleteDevice(VolumeDeviceObject);
            }
        }

        if (!AbnormalTermination())
        {
            if (NT_SUCCESS(Status))
            {
                ClearFlag(VolumeDeviceObject->Flags, DO_DEVICE_INITIALIZING);
            }

            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest(
                IrpContext->Irp,
                (CCHAR)
                (NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
                );

            FsdFreeIrpContext(IrpContext);
        }
    }

    return Status;
}

#pragma code_seg() // end FSD_PAGED_CODE

NTSTATUS
FsdVerifyVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PDEVICE_OBJECT          DeviceObject;
    NTSTATUS                Status = STATUS_UNSUCCESSFUL;
    PFSD_VCB                Vcb;
    BOOLEAN                 VcbResourceAcquired = FALSE;
    PIRP                    Irp;

    __try
    {
        ASSERT(IrpContext != NULL);

        ASSERT((IrpContext->Identifier.Type == ICX) &&
               (IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		KeEnterCriticalRegion();

        DeviceObject = IrpContext->DeviceObject;

        if (DeviceObject == FsdGlobalData.DeviceObject)
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
            __leave;
        }

        Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

        ASSERT(Vcb != NULL);

        ASSERT((Vcb->Identifier.Type == VCB) &&
               (Vcb->Identifier.Size == sizeof(FSD_VCB)));

        ExAcquireResourceExclusiveLite(
            &Vcb->MainResource,
            TRUE
            );

        VcbResourceAcquired = TRUE;

        if (!FlagOn(Vcb->TargetDeviceObject->Flags, DO_VERIFY_VOLUME))
        {
            Status = STATUS_SUCCESS;
            __leave;
        }

        Irp = IrpContext->Irp;

		
        Status = FsdIsDeviceSameCh10fs(
            Vcb->TargetDeviceObject,
            0
            );

        if (NT_SUCCESS(Status))
        {
            ClearFlag(Vcb->TargetDeviceObject->Flags, DO_VERIFY_VOLUME);

            KdPrint((DRIVER_NAME ": Volume verify succeeded\n"));

            __leave;
        }
        else
        {
            ExReleaseResourceForThreadLite(
                &Vcb->MainResource,
                ExGetCurrentResourceThread()
                );

            VcbResourceAcquired = FALSE;

            FsdPurgeVolume(Vcb, FALSE);

            ExAcquireResourceExclusiveLite(
                &Vcb->MainResource,
                TRUE
                );

            VcbResourceAcquired = TRUE;

            SetFlag(Vcb->Flags, VCB_DISMOUNT_PENDING);

            ClearFlag(Vcb->TargetDeviceObject->Flags, DO_VERIFY_VOLUME);

            KdPrint((DRIVER_NAME ": Volume verify failed\n"));

            __leave;
        }
    }
    __finally
    {
        if (VcbResourceAcquired)
        {
            ExReleaseResourceForThreadLite(
                &Vcb->MainResource,
                ExGetCurrentResourceThread()
                );
        }

		KeLeaveCriticalRegion();

        if (!AbnormalTermination())
        {
            IrpContext->Irp->IoStatus.Status = Status;

            FsdCompleteRequest(
                IrpContext->Irp,
                (CCHAR)
                (NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
                );

            FsdFreeIrpContext(IrpContext);
        }
    }

    return Status;
}

#pragma code_seg(FSD_PAGED_CODE)

typedef struct _FCB_LIST_ENTRY {
    PFSD_FCB    Fcb;
    LIST_ENTRY  Next;
} FCB_LIST_ENTRY, *PFCB_LIST_ENTRY;

VOID
FsdPurgeVolume (
    IN PFSD_VCB Vcb,
    IN BOOLEAN  FlushBeforePurge
    )
{
    BOOLEAN         VcbResourceAcquired = FALSE;
    PFSD_FCB        Fcb;
    LIST_ENTRY      FcbList;
    PLIST_ENTRY     ListEntry;
    PFCB_LIST_ENTRY FcbListEntry;

	PAGED_CODE();

    __try
    {
        ASSERT(Vcb != NULL);

        ASSERT((Vcb->Identifier.Type == VCB) &&
               (Vcb->Identifier.Size == sizeof(FSD_VCB)));

		KeEnterCriticalRegion();

        ExAcquireResourceSharedLite(
            &Vcb->MainResource,
            TRUE
            );

        VcbResourceAcquired = TRUE;

#ifndef FSD_RO
        if (FlagOn(Vcb->Flags, VCB_READ_ONLY))
        {
            FlushBeforePurge = FALSE;
        }
#endif

        InitializeListHead(&FcbList);

        for (
            ListEntry = Vcb->FcbList.Flink;
            ListEntry != &Vcb->FcbList;
            ListEntry = ListEntry->Flink
            )
        {
            Fcb = CONTAINING_RECORD(ListEntry, FSD_FCB, Next);

            ExAcquireResourceExclusiveLite(
                &Fcb->MainResource,
                TRUE
                );

            Fcb->ReferenceCount++;

            ExReleaseResourceForThreadLite(
                &Fcb->MainResource,
                ExGetCurrentResourceThread()
                );

            FcbListEntry = FsdAllocatePool(
                NonPagedPool,
                sizeof(FCB_LIST_ENTRY),
                '1mTR'
                );

            FcbListEntry->Fcb = Fcb;

            InsertTailList(&FcbList, &FcbListEntry->Next);
        }

        ExReleaseResourceForThreadLite(
            &Vcb->MainResource,
            ExGetCurrentResourceThread()
            );

        VcbResourceAcquired = FALSE;

        while (!IsListEmpty(&FcbList))
        {
            ListEntry = RemoveHeadList(&FcbList);

            FcbListEntry = CONTAINING_RECORD(ListEntry, FCB_LIST_ENTRY, Next);

            Fcb = FcbListEntry->Fcb;
			ASSERT(Fcb != NULL);

            FsdPurgeFile(Fcb, FlushBeforePurge);

			ASSERT(Fcb != NULL);
            Fcb->ReferenceCount--;

            if (!Fcb->ReferenceCount)
            {
                KdPrint(("FsdFreeFcb %s\n", Fcb->AnsiFileName.Buffer));
                FsdFreeFcb(Fcb);
            }

            FsdFreePool(FcbListEntry);
        }

        KdPrint((DRIVER_NAME ": Volume flushed and purged\n"));
    }
    __finally
    {
        if (VcbResourceAcquired)
        {
            ExReleaseResourceForThreadLite(
                &Vcb->MainResource,
                ExGetCurrentResourceThread()
                );
        }

		KeLeaveCriticalRegion();
    }
}

VOID
FsdPurgeFile (
    IN PFSD_FCB Fcb,
    IN BOOLEAN  FlushBeforePurge
    )
{
#ifndef FSD_RO
    IO_STATUS_BLOCK IoStatus;
#endif

	PAGED_CODE();

    ASSERT(Fcb != NULL);
        
    ASSERT((Fcb->Identifier.Type == FCB) &&
           (Fcb->Identifier.Size == sizeof(FSD_FCB)));

#ifndef FSD_RO
    if (FlushBeforePurge)
    {
        KdPrint(("CcFlushCache on %s\n", Fcb->AnsiFileName.Buffer));

        CcFlushCache(&Fcb->SectionObject, NULL, 0, &IoStatus);
    }
#endif
    
    if (Fcb->SectionObject.ImageSectionObject)
    {
        KdPrint(("MmFlushImageSection on %s\n", Fcb->AnsiFileName.Buffer));

        MmFlushImageSection(&Fcb->SectionObject, MmFlushForWrite);
    }
    
    if (Fcb->SectionObject.DataSectionObject)
    {
        KdPrint(("CcPurgeCacheSection on %s\n", Fcb->AnsiFileName.Buffer));

        CcPurgeCacheSection(&Fcb->SectionObject, NULL, 0, FALSE);
    }
}

#pragma code_seg() // end FSD_PAGED_CODE

VOID
FsdSetVpbFlag (
    IN PVPB     Vpb,
    IN USHORT   Flag
    )
{
    KIRQL OldIrql;

    IoAcquireVpbSpinLock(&OldIrql);

    Vpb->Flags |= Flag;

    IoReleaseVpbSpinLock(OldIrql);
}

VOID
FsdClearVpbFlag (
    IN PVPB     Vpb,
    IN USHORT   Flag
    )
{
    KIRQL OldIrql;

    IoAcquireVpbSpinLock(&OldIrql);

    Vpb->Flags &= ~Flag;

    IoReleaseVpbSpinLock(OldIrql);
}
