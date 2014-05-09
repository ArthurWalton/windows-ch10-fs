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
FsdDirectoryControl (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	NTSTATUS Status;

	PAGED_CODE();

	ASSERT(IrpContext);

	ASSERT((IrpContext->Identifier.Type == ICX) &&
	(IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

	switch (IrpContext->MinorFunction)
	{
	case IRP_MN_QUERY_DIRECTORY:
		DbgPrint((DRIVER_NAME ": IRP_MN_QUERY_DIRECTORY\n"));
		Status = FsdQueryDirectory(IrpContext);
		break;

	case IRP_MN_NOTIFY_CHANGE_DIRECTORY:
		DbgPrint((DRIVER_NAME ": IRP_MN_NOTIFY_CHANGE_DIRECTORY\n"));
		Status = FsdNotifyChangeDirectory(IrpContext);
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
FsdQueryDirectory (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	PDEVICE_OBJECT          	DeviceObject;
	NTSTATUS                	Status = STATUS_UNSUCCESSFUL;
	PFSD_VCB                	Vcb;
	PFILE_OBJECT            	FileObject;
	PFSD_FCB                	Fcb;
	PFSD_CCB                	Ccb;
	PIRP                    	Irp;
	PIO_STACK_LOCATION      	IrpSp;
	FILE_INFORMATION_CLASS  	FileInformationClass;
	ULONG                   	Length;
	PUNICODE_STRING         	FileName;
	UNICODE_STRING          	UpcaseFileName;
	ULONG                   	FileIndex;
	BOOLEAN                 	RestartScan;
	BOOLEAN                 	ReturnSingleEntry;
	BOOLEAN                 	IndexSpecified;
	PUCHAR                  	UserBuffer;
	BOOLEAN                 	FirstQuery;
	struct ch10_dir_entry*     	Inode = NULL;
	BOOLEAN                 	FcbResourceAcquired = FALSE;
	ULONG                   	QueryBlockLength;
	ULONG                   	UsedLength = 0;
	USHORT                  	InodeFileNameLength;
	UNICODE_STRING          	InodeFileName;
	PULONG                  	NextEntryOffset = NULL;
	ULONG						CurrentDirEntryIndex;
	struct ch10_dir_entry *CurrentDirEntry;
	UpcaseFileName.Buffer = NULL;
	InodeFileName.Buffer = NULL;

	PAGED_CODE();

	__try
	{
		ASSERT(IrpContext);

		ASSERT((IrpContext->Identifier.Type == ICX) &&
		(IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		DeviceObject = IrpContext->DeviceObject;

		KeEnterCriticalRegion();

		if (DeviceObject == FsdGlobalData.DeviceObject)
		{
			Status = STATUS_INVALID_DEVICE_REQUEST;
			__leave;
		}

		Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

		ASSERT(Vcb != NULL);

		ASSERT((Vcb->Identifier.Type == VCB) &&
		(Vcb->Identifier.Size == sizeof(FSD_VCB)));

		FileObject = IrpContext->FileObject;

		Fcb = (PFSD_FCB) FileObject->FsContext;

		ASSERT(Fcb);

		if (Fcb->Identifier.Type == VCB)
		{
			DbgPrint((DRIVER_NAME ": FsdQueryDirectory file is volume\n"));
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		ASSERT((Fcb->Identifier.Type == FCB) &&
		(Fcb->Identifier.Size == sizeof(FSD_FCB)));

		//if (!FlagOn(Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
		//{
		//	DbgPrint((DRIVER_NAME ": FsdQueryDirectory file not directory\n"));
		//    Status = STATUS_INVALID_PARAMETER;
		//    __leave;
		// }

		Ccb = (PFSD_CCB) FileObject->FsContext2;

		ASSERT(Ccb);

		ASSERT((Ccb->Identifier.Type == CCB) &&
		(Ccb->Identifier.Size == sizeof(FSD_CCB)));

		Irp = IrpContext->Irp;

		IrpSp = IoGetCurrentIrpStackLocation(Irp);

#ifndef _GNU_NTIFS_

		FileInformationClass =
		IrpSp->Parameters.QueryDirectory.FileInformationClass;

		Length = IrpSp->Parameters.QueryDirectory.Length;

		FileName = IrpSp->Parameters.QueryDirectory.FileName;

		FileIndex = IrpSp->Parameters.QueryDirectory.FileIndex;

#else // _GNU_NTIFS_

		FileInformationClass = ((PEXTENDED_IO_STACK_LOCATION)
		IrpSp)->Parameters.QueryDirectory.FileInformationClass;

		Length = ((PEXTENDED_IO_STACK_LOCATION)
		IrpSp)->Parameters.QueryDirectory.Length;

		FileName = ((PEXTENDED_IO_STACK_LOCATION)
		IrpSp)->Parameters.QueryDirectory.FileName;

		FileIndex = ((PEXTENDED_IO_STACK_LOCATION)
		IrpSp)->Parameters.QueryDirectory.FileIndex;

#endif // _GNU_NTIFS_

		DbgPrint(DRIVER_NAME ": Length Parameter %d\n", Length);
		DbgPrint(DRIVER_NAME ": FileIndex Parameter %d\n", FileIndex);
		
		RestartScan = FlagOn(IrpSp->Flags, SL_RESTART_SCAN);
		ReturnSingleEntry = FlagOn(IrpSp->Flags, SL_RETURN_SINGLE_ENTRY);
		IndexSpecified = FlagOn(IrpSp->Flags, SL_INDEX_SPECIFIED);

		if (Irp->RequestorMode != KernelMode &&
				!Irp->MdlAddress &&
				Irp->UserBuffer)
		{
			ProbeForWrite(Irp->UserBuffer, Length, 1);
		}

		UserBuffer = FsdGetUserBuffer(Irp);

		if (UserBuffer == NULL)
		{
			Status = STATUS_INVALID_USER_BUFFER;
			__leave;
		}

		if (!IrpContext->IsSynchronous)
		{
			Status = STATUS_PENDING;
			__leave;
		}


		if (!ExAcquireResourceSharedLite(
					&Fcb->MainResource,
					IrpContext->IsSynchronous
					))
		{
			Status = STATUS_PENDING;
			__leave;
		}

		FcbResourceAcquired = TRUE;

		if (FileName != NULL)
		{
			UpcaseFileName.Length = UpcaseFileName.MaximumLength =
			FileName->Length;

			UpcaseFileName.Buffer =
			FsdAllocatePool(NonPagedPool, FileName->Length, '1iDR');

			if (UpcaseFileName.Buffer == NULL)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				__leave;
			}

			RtlUpcaseUnicodeString(
			&UpcaseFileName,
			FileName,
			FALSE
			);

			FileName = &UpcaseFileName;

			if (Ccb->DirectorySearchPattern.Buffer != NULL)
			{
				FirstQuery = FALSE;
			}
			else
			{
				FirstQuery = TRUE;

				Ccb->DirectorySearchPattern.Length =
				Ccb->DirectorySearchPattern.MaximumLength =
				FileName->Length;

				Ccb->DirectorySearchPattern.Buffer =
				FsdAllocatePool(NonPagedPool, FileName->Length, '2iDR');

				if (Ccb->DirectorySearchPattern.Buffer == NULL)
				{
					Status = STATUS_INSUFFICIENT_RESOURCES;
					__leave;
				}

				RtlCopyMemory(
				Ccb->DirectorySearchPattern.Buffer,
				FileName->Buffer,
				FileName->Length
				);
			}
		}
		else if (Ccb->DirectorySearchPattern.Buffer != NULL)
		{
			FirstQuery = FALSE;
			FileName = &Ccb->DirectorySearchPattern;
		}
		else
		{
			FirstQuery = TRUE;

			Ccb->DirectorySearchPattern.Length = sizeof(WCHAR);
			Ccb->DirectorySearchPattern.MaximumLength = sizeof(WCHAR);

			Ccb->DirectorySearchPattern.Buffer =
			FsdAllocatePool(NonPagedPool, sizeof(WCHAR), '3iDR');

			if (Ccb->DirectorySearchPattern.Buffer == NULL)
			{
				Status = STATUS_INSUFFICIENT_RESOURCES;
				__leave;
			}

			RtlCopyMemory(Ccb->DirectorySearchPattern.Buffer, L"*", sizeof(WCHAR));

			FileName = &Ccb->DirectorySearchPattern;
		}

		if (!IndexSpecified)
		{
			if (RestartScan || FirstQuery)
			{
				FileIndex = 0;
			}
			else
			{
				FileIndex = Ccb->CurrentByteOffset;
				DbgPrint(DRIVER_NAME ": Setting Fileindex to CurrentByteOffset of %u\n", FileIndex);
			}
		}

		Inode = (struct ch10_dir_entry*) FsdAllocatePool(
		NonPagedPool,
		sizeof(struct ch10_dir_entry),
		'4iDR'
		);

		if (Inode == NULL)
		{
			Status = STATUS_INSUFFICIENT_RESOURCES;
			__leave;
		}

		RtlZeroMemory(UserBuffer, Length);

		switch (FileInformationClass)
		{
		case FileDirectoryInformation:
			DbgPrint((DRIVER_NAME ": FileDirectoryInformation\n"));
			if (Length < sizeof(FILE_DIRECTORY_INFORMATION))
			{
				Status = STATUS_INFO_LENGTH_MISMATCH;
				__leave;
			}
			QueryBlockLength = sizeof(FILE_DIRECTORY_INFORMATION);
			break;

		case FileFullDirectoryInformation:
			DbgPrint((DRIVER_NAME ": FileFullDirectoryInformation\n"));
			if (Length < sizeof(FILE_FULL_DIR_INFORMATION))
			{
				Status = STATUS_INFO_LENGTH_MISMATCH;
				__leave;
			}
			QueryBlockLength = sizeof(FILE_FULL_DIR_INFORMATION);
			break;

		case FileBothDirectoryInformation:
			DbgPrint((DRIVER_NAME ": FileBothDirectoryInformation\n"));
			if (Length < sizeof(FILE_BOTH_DIR_INFORMATION))
			{
				Status = STATUS_INFO_LENGTH_MISMATCH;
				__leave;
			}
			QueryBlockLength = sizeof(FILE_BOTH_DIR_INFORMATION);
			break;

		case FileNamesInformation:
			DbgPrint((DRIVER_NAME ": FileNamesInformation\n"));
			if (Length < sizeof(FILE_NAMES_INFORMATION))
			{
				Status = STATUS_INFO_LENGTH_MISMATCH;
				__leave;
			}
			QueryBlockLength = sizeof(FILE_NAMES_INFORMATION);
			break;

#if (VER_PRODUCTBUILD >= 2600)

		case FileIdFullDirectoryInformation:
			DbgPrint((DRIVER_NAME ": FileIdFullDirectoryInformation\n"));
			if (Length < sizeof(FILE_ID_FULL_DIR_INFORMATION))
			{
				Status = STATUS_INFO_LENGTH_MISMATCH;
				__leave;
			}
			QueryBlockLength = sizeof(FILE_ID_FULL_DIR_INFORMATION);
			break;

		case FileIdBothDirectoryInformation:
			DbgPrint((DRIVER_NAME ": FileIdBothDirectoryInformation\n"));
			if (Length < sizeof(FILE_ID_BOTH_DIR_INFORMATION))
			{
				Status = STATUS_INFO_LENGTH_MISMATCH;
				__leave;
			}
			QueryBlockLength = sizeof(FILE_ID_BOTH_DIR_INFORMATION);
			break;

#endif // (VER_PRODUCTBUILD >= 2600)

		default:
			DbgPrint((DRIVER_NAME ": FsdQueryDirectory invalid file information class\n"));
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}
		
		DbgPrint(DRIVER_NAME ": Dir Entry Count %u\n", FsdCh10GetFileCount(Vcb->dirblocks));
		while (UsedLength < Length
		&& FileIndex < FsdCh10GetFileCount(Vcb->dirblocks))
		{
			CurrentDirEntry = GetDirEntryAtIndex(Vcb->dirblocks, FileIndex);
			InodeFileNameLength = ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN);
			
			DbgPrint(DRIVER_NAME ": Listing file at index %u\n", FileIndex);
			
			if (Length - UsedLength < QueryBlockLength +
					InodeFileNameLength * 2 - sizeof(WCHAR))
			{
				if (!UsedLength)
				{
					Status = STATUS_INFO_LENGTH_MISMATCH;
				}
				else
				{
					Status = STATUS_SUCCESS;
				}
				__leave;
			}		
			
			InodeFileName.Length =
			InodeFileName.MaximumLength =
			InodeFileNameLength * 2;

			InodeFileName.Buffer = FsdAllocatePool(
			NonPagedPool,
			InodeFileNameLength * 2,
			'5iDR'
			);

			FsdCharToWchar(
			InodeFileName.Buffer,
			CurrentDirEntry->name,
			InodeFileNameLength
			);
			
			if (FsRtlDoesNameContainWildCards(FileName) ?
					FsRtlIsNameInExpression(
						FileName,
						&InodeFileName,
						TRUE,
						NULL
						) :
					!RtlCompareUnicodeString(
						FileName,
						&InodeFileName,
						TRUE
						)
					)
			{
				switch (FileInformationClass)
				{
				case FileDirectoryInformation:
					DbgPrint((DRIVER_NAME ": FileDirectoryInformation Filler\n"));
					{
						//DbgPrint((DRIVER_NAME ": File Directory Information!\n"));
						PFILE_DIRECTORY_INFORMATION Buffer;

						Buffer = (PFILE_DIRECTORY_INFORMATION)(UserBuffer + UsedLength);
						
						Buffer->FileIndex = FileIndex;

						Buffer->CreationTime.QuadPart = 0;

						Buffer->LastAccessTime.QuadPart = 0;

						Buffer->LastWriteTime.QuadPart = 0;

						Buffer->ChangeTime.QuadPart = 0;

						Buffer->EndOfFile.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->AllocationSize.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

						SetFlag(
						Buffer->FileAttributes,
						FILE_ATTRIBUTE_READONLY
						);

						Buffer->FileNameLength = sizeof(WCHAR) * ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN);
						
						FsdCharToWchar(
						Buffer->FileName,
						CurrentDirEntry->name,
						ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN)
						);
						
						Buffer->NextEntryOffset = QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR) +
						UsedLength % 8;

						UsedLength += QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR);
						

						
						if (!ReturnSingleEntry) {
							Buffer->NextEntryOffset = QueryBlockLength +
							InodeFileNameLength * 2 - sizeof(WCHAR) +
							UsedLength % 8;
						}
						else
						{
							Buffer->NextEntryOffset = 0;
						}

						UsedLength += UsedLength % 8;
						DbgPrint(DRIVER_NAME ": Listing file %ws\n", Buffer->FileName);

						NextEntryOffset = &Buffer->NextEntryOffset;
					}
					break;

				case FileFullDirectoryInformation:
					DbgPrint((DRIVER_NAME ": FileFullDirectoryInformation Filler\n"));
					{
						//DbgPrint((DRIVER_NAME ": File Full Directory Information!\n"));
						PFILE_FULL_DIR_INFORMATION Buffer;

						Buffer = (PFILE_FULL_DIR_INFORMATION)(UserBuffer + UsedLength);

						Buffer->FileIndex = FileIndex;

						Buffer->CreationTime.QuadPart = 0;

						Buffer->LastAccessTime.QuadPart = 0;

						Buffer->LastWriteTime.QuadPart = 0;

						Buffer->ChangeTime.QuadPart = 0;

						Buffer->EndOfFile.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->AllocationSize.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

						SetFlag(
						Buffer->FileAttributes,
						FILE_ATTRIBUTE_READONLY
						);

						Buffer->EaSize = 0;
						


						Buffer->FileNameLength = sizeof(WCHAR) * ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN);
						
						FsdCharToWchar(
						Buffer->FileName,
						CurrentDirEntry->name,
						ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN)
						);
						
						UsedLength += QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR);
						

						
						if (!ReturnSingleEntry) {
							Buffer->NextEntryOffset = QueryBlockLength +
							InodeFileNameLength * 2 - sizeof(WCHAR) +
							UsedLength % 8;
						}
						else
						{
							Buffer->NextEntryOffset = 0;
						}

						UsedLength += UsedLength % 8;
						DbgPrint(DRIVER_NAME ": Listing file %ws\n", Buffer->FileName);

						NextEntryOffset = &Buffer->NextEntryOffset;
					}
					break;

				case FileBothDirectoryInformation:
					DbgPrint((DRIVER_NAME ": FileBothDirectoryInformation Filler\n"));
					{
						//DbgPrint((DRIVER_NAME ": File Both Directory Information!\n"));
						PFILE_BOTH_DIR_INFORMATION Buffer;

						Buffer = (PFILE_BOTH_DIR_INFORMATION)(UserBuffer + UsedLength);

						Buffer->FileIndex = FileIndex;

						Buffer->CreationTime.QuadPart = 0;

						Buffer->LastAccessTime.QuadPart = 0;

						Buffer->LastWriteTime.QuadPart = 0;

						Buffer->ChangeTime.QuadPart = 0;

						Buffer->EndOfFile.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->AllocationSize.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

						SetFlag(
						Buffer->FileAttributes,
						FILE_ATTRIBUTE_READONLY
						);

						Buffer->FileNameLength = 2 * InodeFileNameLength;
						
						FsdCharToWchar(
						Buffer->FileName,
						CurrentDirEntry->name,
						InodeFileNameLength
						);
						
						DbgPrint(DRIVER_NAME ": Listing file %ws\n", Buffer->FileName);

						Buffer->EaSize = 0;
						
						UsedLength += QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR);
						
						if (!ReturnSingleEntry) {
							Buffer->NextEntryOffset = QueryBlockLength +
							InodeFileNameLength * 2 - sizeof(WCHAR) +
							UsedLength % 8;
							DbgPrint(DRIVER_NAME ": More entries to come\n");
						}
						else
						{
							DbgPrint(DRIVER_NAME ": Last Entry\n");
							Buffer->NextEntryOffset = 0;
						}
						
						UsedLength += UsedLength % 8;
						
						NextEntryOffset = &Buffer->NextEntryOffset;
					}
					break;

				case FileNamesInformation:
					DbgPrint((DRIVER_NAME ": FileNames=Information Filler\n"));
					{
						//DbgPrint((DRIVER_NAME ": File Names Information!\n"));
						PFILE_NAMES_INFORMATION Buffer;

						Buffer = (PFILE_NAMES_INFORMATION)(UserBuffer + UsedLength);

						Buffer->FileIndex = FileIndex;

						Buffer->FileNameLength = sizeof(WCHAR) * ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN);
						
						FsdCharToWchar(
						Buffer->FileName,
						CurrentDirEntry->name,
						ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN)
						);

						DbgPrint(DRIVER_NAME ": Listing file %ws\n", Buffer->FileName);
						
						UsedLength += QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR);
						
						
						
						if (!ReturnSingleEntry) {
							Buffer->NextEntryOffset = QueryBlockLength +
							InodeFileNameLength * 2 - sizeof(WCHAR) +
							UsedLength % 8;
						}
						else
						{
							Buffer->NextEntryOffset = 0;
						}

						UsedLength += UsedLength % 8;
						NextEntryOffset = &Buffer->NextEntryOffset;
					}
					break;

#if (VER_PRODUCTBUILD >= 2600)

				case FileIdFullDirectoryInformation:
					DbgPrint((DRIVER_NAME ": FileIdFullDirectoryInformation Filler\n"));
					{
						//DbgPrint((DRIVER_NAME ": File Id Full Directory Information!\n"));
						PFILE_ID_FULL_DIR_INFORMATION Buffer;

						Buffer = (PFILE_ID_FULL_DIR_INFORMATION)(UserBuffer + UsedLength);

						Buffer->FileIndex = FileIndex;

						Buffer->CreationTime.QuadPart = 0;

						Buffer->LastAccessTime.QuadPart = 0;

						Buffer->LastWriteTime.QuadPart = 0;

						Buffer->ChangeTime.QuadPart = 0;

						Buffer->EndOfFile.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->AllocationSize.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;


						SetFlag(
						Buffer->FileAttributes,
						FILE_ATTRIBUTE_READONLY
						);

						Buffer->FileNameLength = sizeof(WCHAR) * ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN);
						
						FsdCharToWchar(
						Buffer->FileName,
						CurrentDirEntry->name,
						ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN)
						);

						DbgPrint(DRIVER_NAME ": Listing file %ws\n", Buffer->FileName);

						Buffer->EaSize = 0;

						Buffer->FileId = Fcb->IndexNumber;
						
						UsedLength += QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR);
						

						
						if (!ReturnSingleEntry) {
							Buffer->NextEntryOffset = QueryBlockLength +
							InodeFileNameLength * 2 - sizeof(WCHAR) +
							UsedLength % 8;
						}
						else
						{
							Buffer->NextEntryOffset = 0;
						}

						UsedLength += UsedLength % 8;

						NextEntryOffset = &Buffer->NextEntryOffset;
					}
					break;

				case FileIdBothDirectoryInformation:
					DbgPrint((DRIVER_NAME ": FileIdBothDirectoryInformation Filler\n"));
					{
						//DbgPrint((DRIVER_NAME ": File Id Both Directory Information!\n"));
						PFILE_ID_BOTH_DIR_INFORMATION Buffer;

						Buffer = (PFILE_ID_BOTH_DIR_INFORMATION)(UserBuffer + UsedLength);

						Buffer->FileIndex = FileIndex;

						Buffer->CreationTime.QuadPart = 0;

						Buffer->LastAccessTime.QuadPart = 0;

						Buffer->LastWriteTime.QuadPart = 0;

						Buffer->ChangeTime.QuadPart = 0;

						Buffer->EndOfFile.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->AllocationSize.QuadPart = be64_to_cpu(CurrentDirEntry->size);

						Buffer->FileAttributes = FILE_ATTRIBUTE_NORMAL;

						SetFlag(
						Buffer->FileAttributes,
						FILE_ATTRIBUTE_READONLY
						);
						
						Buffer->FileNameLength = 2 * InodeFileNameLength;
						
						FsdCharToWchar(
							Buffer->FileName,
							CurrentDirEntry->name,
							ch10fs_strnlen(CurrentDirEntry->name, CH10_MAXFN)
						);

						DbgPrint(DRIVER_NAME ": Directory Listing test\n");
						DbgPrint(DRIVER_NAME ": SFilname string length %u #########################################\n", InodeFileNameLength);
						DbgPrint(DRIVER_NAME ": Listing file '%ws'\n", Buffer->FileName);
						DbgPrint(DRIVER_NAME ": moved UsedLength\n");

						Buffer->EaSize = 0;

						Buffer->FileId = Fcb->IndexNumber;
						
						UsedLength += QueryBlockLength +
						InodeFileNameLength * 2 - sizeof(WCHAR);
						
						if (!ReturnSingleEntry) {
							Buffer->NextEntryOffset = QueryBlockLength +
							InodeFileNameLength * 2 - sizeof(WCHAR) +
							UsedLength % 8;
						}
						else
						{
							Buffer->NextEntryOffset = 0;
						}

						UsedLength += UsedLength % 8;

						NextEntryOffset = &Buffer->NextEntryOffset;
					}
					break;

#endif // (VER_PRODUCTBUILD >= 2600)
				}
			}
			
			FileIndex++;
			Ccb->CurrentByteOffset = FileIndex;
			if (UsedLength && ReturnSingleEntry)
			{
				Status = STATUS_SUCCESS;
				__leave;
			}
		}
		if (!UsedLength)
		{
			if (FirstQuery)
			{
				Status = STATUS_NO_SUCH_FILE;
			}
			else
			{
				Status = STATUS_NO_MORE_FILES;
			}
		}
		else
		{
			Status = STATUS_SUCCESS;
		}
	}
	__finally
	{
		if (FcbResourceAcquired)
		{
			ExReleaseResourceForThreadLite(
			&Fcb->MainResource,
			ExGetCurrentResourceThread()
			);
		}

		KeLeaveCriticalRegion();

		if (UpcaseFileName.Buffer != NULL)
		{
			FsdFreePool(UpcaseFileName.Buffer);
		}

		if (Inode != NULL)
		{
			FsdFreePool(Inode);
		}

		if (InodeFileName.Buffer != NULL)
		{
			FsdFreePool(InodeFileName.Buffer);
		}

		if (NextEntryOffset != NULL)
		{
			DbgPrint(DRIVER_NAME ": Last Entry Closin' Up\n");
			*NextEntryOffset = 0;
		}

		if (!AbnormalTermination())
		{
			if (Status == STATUS_PENDING)
			{
				Status = FsdLockUserBuffer(
				IrpContext->Irp,
				Length,
				IoWriteAccess
				);

				if (NT_SUCCESS(Status))
				{
					Status = FsdQueueRequest(IrpContext);
				}
				else
				{
					IrpContext->Irp->IoStatus.Status = Status;
					FsdCompleteRequest(IrpContext->Irp, IO_NO_INCREMENT);
					FsdFreeIrpContext(IrpContext);
				}
			}
			else
			{
				IrpContext->Irp->IoStatus.Information = UsedLength;
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

NTSTATUS
FsdNotifyChangeDirectory (
IN PFSD_IRP_CONTEXT IrpContext
)
{
	PDEVICE_OBJECT      DeviceObject;
	BOOLEAN             CompleteRequest;
	NTSTATUS            Status = STATUS_UNSUCCESSFUL;
	PFSD_VCB            Vcb;
	PFILE_OBJECT        FileObject;
	PFSD_FCB            Fcb;
	PIRP                Irp;
	PIO_STACK_LOCATION  IrpSp;
	ULONG               CompletionFilter;
	BOOLEAN             WatchTree;

	PAGED_CODE();

	__try
	{
		ASSERT(IrpContext);

		ASSERT((IrpContext->Identifier.Type == ICX) &&
		(IrpContext->Identifier.Size == sizeof(FSD_IRP_CONTEXT)));

		DeviceObject = IrpContext->DeviceObject;

		if (DeviceObject == FsdGlobalData.DeviceObject)
		{
			CompleteRequest = TRUE;
			Status = STATUS_INVALID_DEVICE_REQUEST;
			__leave;
		}

		Vcb = (PFSD_VCB) DeviceObject->DeviceExtension;

		ASSERT(Vcb != NULL);

		ASSERT((Vcb->Identifier.Type == VCB) &&
		(Vcb->Identifier.Size == sizeof(FSD_VCB)));

		FileObject = IrpContext->FileObject;

		Fcb = (PFSD_FCB) FileObject->FsContext;

		ASSERT(Fcb);

		if (Fcb->Identifier.Type == VCB)
		{
			CompleteRequest = TRUE;
			DbgPrint((DRIVER_NAME ": FsdQueryDirectory file is volume\n"));
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		ASSERT((Fcb->Identifier.Type == FCB) &&
		(Fcb->Identifier.Size == sizeof(FSD_FCB)));

		if (!FlagOn(Fcb->FileAttributes, FILE_ATTRIBUTE_DIRECTORY))
		{
			DbgPrint((DRIVER_NAME ": FsdQueryDirectory filse not directory\n"));
			CompleteRequest = TRUE;
			Status = STATUS_INVALID_PARAMETER;
			__leave;
		}

		Irp = IrpContext->Irp;

		IrpSp = IoGetCurrentIrpStackLocation(Irp);

#ifndef _GNU_NTIFS_

		CompletionFilter =
		IrpSp->Parameters.NotifyDirectory.CompletionFilter;

#else

		CompletionFilter = ((PEXTENDED_IO_STACK_LOCATION)
		IrpSp)->Parameters.NotifyDirectory.CompletionFilter;

#endif

		WatchTree = FlagOn(IrpSp->Flags, SL_WATCH_TREE);

		CompleteRequest = FALSE;

		Status = STATUS_PENDING;

		FsRtlNotifyChangeDirectory(
		Vcb->NotifySync,
		FileObject->FsContext2,
		(PSTRING)&Fcb->FileName,
		&Vcb->NotifyList,
		WatchTree,
		CompletionFilter,
		Irp
		);
	}
	__finally
	{
		if (!AbnormalTermination())
		{
			if (CompleteRequest)
			{
				IrpContext->Irp->IoStatus.Status = Status;

				FsdCompleteRequest(
				IrpContext->Irp,
				(CCHAR)
				(NT_SUCCESS(Status) ? IO_DISK_INCREMENT : IO_NO_INCREMENT)
				);
			}

			FsdFreeIrpContext(IrpContext);
		}
	}

	return Status;
}

#pragma code_seg() // end FSD_PAGED_CODE
