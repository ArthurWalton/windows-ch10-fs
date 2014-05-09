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
FsdQueryVolumeInformation (
    IN PFSD_IRP_CONTEXT IrpContext
    )
{
    PDEVICE_OBJECT          DeviceObject;
    NTSTATUS                Status = STATUS_UNSUCCESSFUL;
    PFSD_VCB                Vcb;
    PIRP                    Irp;
    PIO_STACK_LOCATION      IrpSp;
    FS_INFORMATION_CLASS    FsInformationClass;
    ULONG                   Length;
    PVOID                   SystemBuffer;
    BOOLEAN                 VcbResourceAcquired = FALSE;

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

        if (!ExAcquireResourceSharedLite(
                 &Vcb->MainResource,
                 IrpContext->IsSynchronous
                 ))
        {
            Status = STATUS_PENDING;
            __leave;
        }

        VcbResourceAcquired = TRUE;

        Irp = IrpContext->Irp;

        IrpSp = IoGetCurrentIrpStackLocation(Irp);

        FsInformationClass = IrpSp->Parameters.QueryVolume.FsInformationClass;

        Length = IrpSp->Parameters.QueryVolume.Length;

        SystemBuffer = Irp->AssociatedIrp.SystemBuffer;

        RtlZeroMemory(SystemBuffer, Length);

        switch (FsInformationClass)
        {
        case FileFsVolumeInformation:
            {
                PFILE_FS_VOLUME_INFORMATION Buffer;
                ULONG                       VolumeLabelLength;
                ULONG                       RequiredLength;

                if (Length < sizeof(FILE_FS_VOLUME_INFORMATION))
                {
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    __leave;
                }

                Buffer = (PFILE_FS_VOLUME_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_FS_VOLUME_INFORMATION {
                    LARGE_INTEGER   VolumeCreationTime;
                    ULONG           VolumeSerialNumber;
                    ULONG           VolumeLabelLength;
                    BOOLEAN         SupportsObjects;
                    WCHAR           VolumeLabel[1];
                } FILE_FS_VOLUME_INFORMATION, *PFILE_FS_VOLUME_INFORMATION;
*/

                Buffer->VolumeCreationTime.QuadPart = 0;

                Buffer->VolumeSerialNumber = 4242;

                VolumeLabelLength = 7;

                Buffer->VolumeLabelLength = VolumeLabelLength * 2;

                // I don't know what this means.
                Buffer->SupportsObjects = FALSE;

                RequiredLength = sizeof(FILE_FS_VOLUME_INFORMATION)
                    + VolumeLabelLength * 2 - sizeof(WCHAR);

                if (Length < RequiredLength)
                {
                    Irp->IoStatus.Information =
                        sizeof(FILE_FS_VOLUME_INFORMATION);
                    Status = STATUS_BUFFER_OVERFLOW;
                    __leave;
                }

                FsdCopyWchar(
                    Buffer->VolumeLabel,
                    L"testvol",
                    VolumeLabelLength
                    );

                Irp->IoStatus.Information = RequiredLength;
                Status = STATUS_SUCCESS;
                __leave;
            }

        case FileFsSizeInformation:
            {
                PFILE_FS_SIZE_INFORMATION Buffer;

                if (Length < sizeof(FILE_FS_SIZE_INFORMATION))
                {
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    __leave;
                }

                Buffer = (PFILE_FS_SIZE_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_FS_SIZE_INFORMATION {
                    LARGE_INTEGER   TotalAllocationUnits;
                    LARGE_INTEGER   AvailableAllocationUnits;
                    ULONG           SectorsPerAllocationUnit;
                    ULONG           BytesPerSector;
                } FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;
*/

#ifndef FSD_RO
                if (!FlagOn(Vcb->Flags, VCB_READ_ONLY))
                {
                    Buffer->TotalAllocationUnits.QuadPart =
                        Vcb->PartitionInformation.PartitionLength.QuadPart /
                        CH10_BLOCK_SIZE;

                    Buffer->AvailableAllocationUnits.QuadPart =
                        (Vcb->PartitionInformation.PartitionLength.QuadPart -
                        FsdCh10PartitionSize(&Vcb->dirblocks[0]) / CH10_BLOCK_SIZE;
                }
                else
#endif // !FSD_RO
                {
                    // On a readonly filesystem total size is the size of the
                    // contents and available size is zero

                    Buffer->TotalAllocationUnits.QuadPart =
                        FsdCh10PartitionSize(&Vcb->dirblocks[0]) / CH10_BLOCK_SIZE;

                    Buffer->AvailableAllocationUnits.QuadPart =
                        0;
                }

                Buffer->SectorsPerAllocationUnit =
                    CH10_BLOCK_SIZE / Vcb->DiskGeometry.BytesPerSector;

                Buffer->BytesPerSector = Vcb->DiskGeometry.BytesPerSector;

                Irp->IoStatus.Information = sizeof(FILE_FS_SIZE_INFORMATION);
                Status = STATUS_SUCCESS;
                __leave;
            }

        case FileFsDeviceInformation:
            {
                PFILE_FS_DEVICE_INFORMATION Buffer;

                if (Length < sizeof(FILE_FS_DEVICE_INFORMATION))
                {
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    __leave;
                }

                Buffer = (PFILE_FS_DEVICE_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_FS_DEVICE_INFORMATION {
                    DEVICE_TYPE DeviceType;
                    ULONG       Characteristics;
                } FILE_FS_DEVICE_INFORMATION, *PFILE_FS_DEVICE_INFORMATION;
*/

                Buffer->DeviceType = Vcb->TargetDeviceObject->DeviceType;

                Buffer->Characteristics =
                    Vcb->TargetDeviceObject->Characteristics;

#ifndef FSD_RO
                if (FlagOn(Vcb->Flags, VCB_READ_ONLY))
#endif
                {
                    SetFlag(
                        Buffer->Characteristics,
                        FILE_READ_ONLY_DEVICE
                        );
                }

                Irp->IoStatus.Information = sizeof(FILE_FS_DEVICE_INFORMATION);
                Status = STATUS_SUCCESS;
                __leave;
            }

        case FileFsAttributeInformation:
            {
                PFILE_FS_ATTRIBUTE_INFORMATION  Buffer;
                ULONG                           RequiredLength;

                if (Length < sizeof(FILE_FS_ATTRIBUTE_INFORMATION))
                {
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    __leave;
                }

                Buffer = (PFILE_FS_ATTRIBUTE_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_FS_ATTRIBUTE_INFORMATION {
                    ULONG   FileSystemAttributes;
                    ULONG   MaximumComponentNameLength;
                    ULONG   FileSystemNameLength;
                    WCHAR   FileSystemName[1];
                } FILE_FS_ATTRIBUTE_INFORMATION, *PFILE_FS_ATTRIBUTE_INFORMATION;
*/

                Buffer->FileSystemAttributes =
                    FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES;

                Buffer->MaximumComponentNameLength = CH10_MAXFN;

                Buffer->FileSystemNameLength = sizeof(DRIVER_NAME) * 2;

                RequiredLength = sizeof(FILE_FS_ATTRIBUTE_INFORMATION) +
                    sizeof(DRIVER_NAME) * 2 - sizeof(WCHAR);

                if (Length < RequiredLength)
                {
                    Irp->IoStatus.Information =
                        sizeof(FILE_FS_ATTRIBUTE_INFORMATION);
                    Status = STATUS_BUFFER_OVERFLOW;
                    __leave;
                }

                FsdCharToWchar(
                    Buffer->FileSystemName,
                    DRIVER_NAME,
                    sizeof(DRIVER_NAME)
                    );

                Irp->IoStatus.Information = RequiredLength;
                Status = STATUS_SUCCESS;
                __leave;
            }

#if (VER_PRODUCTBUILD >= 2195)

        case FileFsFullSizeInformation:
            {
                PFILE_FS_FULL_SIZE_INFORMATION Buffer;

                if (Length < sizeof(FILE_FS_FULL_SIZE_INFORMATION))
                {
                    Status = STATUS_INFO_LENGTH_MISMATCH;
                    __leave;
                }

                Buffer = (PFILE_FS_FULL_SIZE_INFORMATION) SystemBuffer;

/*
                typedef struct _FILE_FS_FULL_SIZE_INFORMATION {
                    LARGE_INTEGER   TotalAllocationUnits;
                    LARGE_INTEGER   CallerAvailableAllocationUnits;
                    LARGE_INTEGER   ActualAvailableAllocationUnits;
                    ULONG           SectorsPerAllocationUnit;
                    ULONG           BytesPerSector;
                } FILE_FS_FULL_SIZE_INFORMATION, *PFILE_FS_FULL_SIZE_INFORMATION;
*/

#ifndef FSD_RO
                if (!FlagOn(Vcb->Flags, VCB_READ_ONLY))
                {
                    Buffer->TotalAllocationUnits.QuadPart =
                        Vcb->PartitionInformation.PartitionLength.QuadPart /
                        CH10_BLOCK_SIZE;

                    Buffer->CallerAvailableAllocationUnits.QuadPart =
                    Buffer->ActualAvailableAllocationUnits.QuadPart =
                        (Vcb->PartitionInformation.PartitionLength.QuadPart -
                        FsdCh10PartitionSize(&Vcb->dirblocks[0])) / CH10_BLOCK_SIZE;
                }
                else
#endif // !FSD_RO
                {
                    // On a readonly filesystem total size is the size of the
                    // contents and available size is zero

                    Buffer->TotalAllocationUnits.QuadPart =
                        FsdCh10PartitionSize(&Vcb->dirblocks[0]) / CH10_BLOCK_SIZE;

                    Buffer->CallerAvailableAllocationUnits.QuadPart =
                    Buffer->ActualAvailableAllocationUnits.QuadPart =
                        0;
                }

                Buffer->SectorsPerAllocationUnit =
                    CH10_BLOCK_SIZE / Vcb->DiskGeometry.BytesPerSector;

                Buffer->BytesPerSector = Vcb->DiskGeometry.BytesPerSector;

                Irp->IoStatus.Information = sizeof(FILE_FS_FULL_SIZE_INFORMATION);
                Status = STATUS_SUCCESS;
                __leave;
            }

#endif // (VER_PRODUCTBUILD >= 2195)

        default:
            Status = STATUS_INVALID_INFO_CLASS;
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
            if (Status == STATUS_PENDING)
            {
                FsdQueueRequest(IrpContext);
            }
            else
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
    }

    return Status;
}

#pragma code_seg() // end FSD_PAGED_CODE
