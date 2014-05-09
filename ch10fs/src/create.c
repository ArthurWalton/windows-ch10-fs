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
#include "ch10fs.h"
#include "border.h"

#pragma code_seg(FSD_PAGED_CODE)

NTSTATUS
FsdCreate (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	PDEVICE_OBJECT      DeviceObject;
	PIRP                Irp;
	PIO_STACK_LOCATION  IrpSp;

	PAGED_CODE();

	DeviceObject = IrpContext->DeviceObject;

	Irp = IrpContext->Irp;

	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	if (DeviceObject == FsdGlobalData.DeviceObject)
	{
		DbgPrint((DRIVER_NAME ": FsdCreateFs\n"));
		return FsdCreateFs(IrpContext);
	}
	else if (IrpSp->FileObject->FileName.Length == 0)
	{
		DbgPrint((DRIVER_NAME ": FsdCreateVolume\n"));
		return FsdCreateVolume(IrpContext);
	}
	else
	{
		DbgPrint((DRIVER_NAME ": FsdCreateFile\n"));
		return FsdCreateFile(IrpContext);
	}
}

NTSTATUS
FsdCreateFs (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	PAGED_CODE();
	IrpContext->Irp->IoStatus.Status = STATUS_SUCCESS;

	IrpContext->Irp->IoStatus.Information = FILE_OPENED;
	
	FsdCompleteRequest(IrpContext->Irp, IO_NO_INCREMENT);
	
	FsdFreeIrpContext(IrpContext);
	
	return STATUS_SUCCESS;
}

NTSTATUS
FsdCreateVolume (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	PDEVICE_OBJECT      DeviceObject;
	PFSD_VCB            Vcb;
	PFILE_OBJECT        FileObject;

	PAGED_CODE();

	DeviceObject = IrpContext->DeviceObject;

	Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

	ASSERT(Vcb != NULL);

	ASSERT((Vcb->Identifier.Type == VCB) &&
	(Vcb->Identifier.Size == sizeof(FSD_VCB)));

	FileObject = IrpContext->FileObject;

	FileObject->FsContext = Vcb;

	KeEnterCriticalRegion();

	ExAcquireResourceExclusiveLite(
	&Vcb->MainResource,
	TRUE
	);

	Vcb->ReferenceCount++;

	ExReleaseResourceForThreadLite(
	&Vcb->MainResource,
	ExGetCurrentResourceThread()
	);

	KeLeaveCriticalRegion();

	IrpContext->Irp->IoStatus.Status = STATUS_SUCCESS;

	IrpContext->Irp->IoStatus.Information = FILE_OPENED;
	
	FsdCompleteRequest(IrpContext->Irp, IO_NO_INCREMENT);
	
	FsdFreeIrpContext(IrpContext);
	
	return STATUS_SUCCESS;
}

NTSTATUS
FsdCreateFile (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	PDEVICE_OBJECT      	DeviceObject;
	PIRP                	Irp;
	PIO_STACK_LOCATION  	IrpSp;
	NTSTATUS            	Status = STATUS_UNSUCCESSFUL;
	PFSD_VCB            	Vcb = NULL;
	PFSD_FCB            	Fcb;
	PFSD_CCB            	Ccb;
	ULONG               	found_index = 0;
	struct ch10_dir_entry* 	Inode = NULL;
	BOOLEAN            	 	VcbResourceAcquired = FALSE;

	PAGED_CODE();
	
	DeviceObject = IrpContext->DeviceObject;
	
	Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;
	
	Irp = IrpContext->Irp;
	
	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	
	__try
	{
		KeEnterCriticalRegion();
		ExAcquireResourceExclusiveLite(
		&Vcb->MainResource,
		TRUE
		);
		
		VcbResourceAcquired = TRUE;
		
		Fcb = FsdLookupFcbByFileName(
		Vcb,
		&IrpSp->FileObject->FileName
		);
		
		if (!Fcb)
		{
			Inode = FsdAllocatePool(
			NonPagedPool,
			sizeof(struct ch10_dir_entry),
			'3cFR'
			);
			
			if (Inode == NULL)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				__leave;
			}
			
			Status = FsdLookupFileName(
			Vcb,
			&IrpSp->FileObject->FileName,
			&found_index,
			Inode
			);
			
			if (!NT_SUCCESS(Status))
			{
				KdPrint((
				DRIVER_NAME ": STATUS_OBJECT_NAME_NOT_FOUND: %.*S\n",
				IrpSp->FileObject->FileName.Length / 2,
				IrpSp->FileObject->FileName.Buffer
				));

				Status = STATUS_OBJECT_NAME_NOT_FOUND;
				__leave;
			}

			Fcb = FsdAllocateFcb(
			Vcb,
			&IrpSp->FileObject->FileName,
			found_index,
			Inode
			);
			
			if (Fcb == NULL)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				__leave;
			}
			
			KdPrint((
			DRIVER_NAME ": Allocated a new FCB for %s\n",
			Fcb->AnsiFileName.Buffer
			));
		}

		if (Fcb->OpenHandleCount >= 1)
		{
			Status = IoCheckShareAccess(
			IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
			IrpSp->Parameters.Create.ShareAccess,
			IrpSp->FileObject,
			&Fcb->ShareAccess,
			FALSE
			);

			if (!NT_SUCCESS(Status))
			{
				__leave;
			}
		}

		Ccb = FsdAllocateCcb();

		if (Ccb == NULL)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

		Fcb->OpenHandleCount++;
		Vcb->OpenFileHandleCount++;
		Fcb->ReferenceCount++;
		Vcb->ReferenceCount++;

		//if (!FlagOn(be32_to_cpu(Fcb->ch10_direntry->next), ROMFH_DIR))
		//{
		//    Fcb->CommonFCBHeader.IsFastIoPossible = FastIoIsPossible;
		//}
		
		IrpSp->FileObject->FsContext = (void*) Fcb;
		IrpSp->FileObject->FsContext2 = (void*) Ccb;
		IrpSp->FileObject->PrivateCacheMap = NULL;
		IrpSp->FileObject->SectionObjectPointer = &(Fcb->SectionObject);
		IrpSp->FileObject->Vpb = Vcb->Vpb;

		if (Fcb->OpenHandleCount == 1)
		{
			IoSetShareAccess(
			IrpSp->Parameters.Create.SecurityContext->DesiredAccess,
			IrpSp->Parameters.Create.ShareAccess,
			IrpSp->FileObject,
			&Fcb->ShareAccess
			);
		}
		else
		{
			IoUpdateShareAccess(IrpSp->FileObject, &Fcb->ShareAccess);
		}

		Irp->IoStatus.Information = FILE_OPENED;
		Status = STATUS_SUCCESS;
		
		KdPrint((
		DRIVER_NAME ": %s OpenHandleCount: %u ReferenceCount: %u\n",
		Fcb->AnsiFileName.Buffer,
		Fcb->OpenHandleCount,
		Fcb->ReferenceCount
		));
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

		if (!NT_SUCCESS(Status) && Inode)
		{
			FsdFreePool(Inode);
		}
		
		if (!AbnormalTermination())
		{
			IrpContext->Irp->IoStatus.Status = Status;
			
			FsdCompleteRequest(
			IrpContext->Irp,
			(CCHAR)
			(NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
			);
			
			FsdFreeIrpContext(IrpContext);
			
			if (Vcb &&
					FlagOn(Vcb->Flags, VCB_DISMOUNT_PENDING) &&
					!Vcb->ReferenceCount
					)
			{
				FsdFreeVcb(Vcb);
			}
		}
	}
	
	return Status;
}

NTSTATUS
FsdLookupFileName (
IN PFSD_VCB                 	Vcb,
IN PUNICODE_STRING          	FullFileName,
IN OUT PULONG               	Index,
IN OUT struct ch10_dir_entry*  	Inode
)
{
	UNICODE_STRING  InodeFileName;
	USHORT   		InodeFileNameLength;
	USHORT   		FileCount;
	DWORD64   FileSize = 424242;
	struct ch10_dir_entry *CurrentDirEntry;
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;
	UNICODE_STRING FileName;
	INT32 colonIndex;

	PAGED_CODE();
	
	DbgPrint(DRIVER_NAME ": Looking for file %wZ\n", FullFileName);
	FileName = *FullFileName;

	//colonIndex = RtlIndexOfCharInUnicodeString(&FileName, ':');
	//if(colonIndex > 0) {
	////	DbgPrint(DRIVER_NAME ": Setting colon index to %d\n", colonIndex);
	//	FileName.Length = colonIndex * 2;
	//	DbgPrint(DRIVER_NAME ": filname string is now %wZ\n", &FileName);
	//}

    FileName.Buffer++;
    FileName.Length -= sizeof(WCHAR);
	DbgPrint((DRIVER_NAME ": FsdLookupFileName\n"));
	
	if (FullFileName->Length == 0)
	{
		DbgPrint((DRIVER_NAME ": FullFileName Length is Zero\n"));
		*Index = 0;
		return STATUS_OBJECT_NAME_NOT_FOUND;
	}

	if (FullFileName->Length == sizeof(WCHAR) && FullFileName->Buffer[0] == L'\\')
	{
		DbgPrint((DRIVER_NAME ": Allocating root file FCB!!!!!!!!!!!!!!!!\n"));

		
		RtlCopyMemory(
		Inode,
		&Vcb->dirblocks[0].dirEntries[0],
		sizeof(struct ch10_dir_entry)
		);

		*Index = 0;

		return STATUS_SUCCESS;
	}
	
	__try
	{
		FileCount = FsdCh10GetFileCount(Vcb->dirblocks);
		
		//DbgPrint(DRIVER_NAME ": File Count %u\n", FileCount);
		for(*Index = 0; *Index < FileCount; *Index = *Index + 1) {
			CurrentDirEntry = GetDirEntryAtIndex(Vcb->dirblocks, *Index);
			InodeFileNameLength = ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN);
			if(InodeFileNameLength == 0) continue;
			//DbgPrint(DRIVER_NAME ": Looking at file %s\n", CurrentDirEntry->name);
			
			InodeFileName.Length =
            InodeFileName.MaximumLength =
                InodeFileNameLength * 2;
				
			InodeFileName.Buffer = FsdAllocatePool(
				NonPagedPool,
				InodeFileName.MaximumLength,
				'1rCR'
			);
			
			FsdCharToWchar(
				InodeFileName.Buffer,
				CurrentDirEntry->name,
				InodeFileNameLength
			);
			
			DbgPrint(DRIVER_NAME ": Comparing String '%wZ' to '%wZ' with result %d\n", &FileName, &InodeFileName, 
			RtlCompareUnicodeString(
				&FileName,
				&InodeFileName,
				TRUE
				));
			
			if (!RtlCompareUnicodeString(
				&FileName,
				&InodeFileName,
				TRUE
				))
			{
				DbgPrint((DRIVER_NAME ": Allocating actual CH10FS File ##################################3\n"));
				FileSize = be64_to_cpu(CurrentDirEntry->size);
				DbgPrint(DRIVER_NAME ": New file has size of %I64d\n", FileSize);
				RtlCopyMemory(
					Inode,
					CurrentDirEntry,
					sizeof(struct ch10_dir_entry)
				);
				
				DbgPrint((DRIVER_NAME ": test10\n"));
				Status = STATUS_SUCCESS;
				__leave;
			}
			
			if (InodeFileName.Buffer != NULL)
            {
                FsdFreePool(InodeFileName.Buffer);
                InodeFileName.Buffer = NULL;
            }
		}
		
		*Index = 0;
		Status = STATUS_NO_SUCH_FILE;
	}
	__finally
	{
		if (InodeFileName.Buffer != NULL)
		{
			FsdFreePool(InodeFileName.Buffer);
		}
	}
	
	DbgPrint(DRIVER_NAME ": End FsdLookupFileName\n");
	return Status;
}

PFSD_FCB
FsdLookupFcbByFileName (
IN PFSD_VCB         Vcb,
IN PUNICODE_STRING  FullFileName
)
{
	PLIST_ENTRY ListEntry;
	PFSD_FCB    Fcb;

	PAGED_CODE();

	ListEntry = Vcb->FcbList.Flink;

	while (ListEntry != &Vcb->FcbList)
	{
		Fcb = CONTAINING_RECORD(ListEntry, FSD_FCB, Next);

		if (!RtlCompareUnicodeString(
					&Fcb->FileName,
					FullFileName,
					TRUE
					))
		{
			KdPrint((
			DRIVER_NAME ": Found an allocated FCB for %s\n",
			Fcb->AnsiFileName.Buffer
			));

			return Fcb;
		}

		ListEntry = ListEntry->Flink;
	}

	return NULL;
}

#pragma code_seg() // end FSD_PAGED_CODE
