/*
    This is a IRIG 106 Chapter 10 file system driver for Windows XP/7.
	Copyright (C) 2014 Arthur Walton.

	Heavily derived from the RomFS filesystem driver 
    Copyright (C) 1999, 2000, 2001, 2002 Bo Brant�n.
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

#ifndef _FSD_
#define _FSD_

#include <ntdddisk.h>
#include <ntverp.h>

#include "ch10_fs.h"

//
// Name for the driver and it's main device
//
#define DRIVER_NAME     "Ch10fs"
#define DEVICE_NAME     L"\\Ch10fs"
#if DBG
#define DOS_DEVICE_NAME L"\\DosDevices\\Ch10fs"
#endif

#define FSD_RO

//
// Comment out these to make all driver code nonpaged
//
#define FSD_INIT_CODE   "INIT"
#define FSD_PAGED_CODE  "PAGE"

//
// Private IOCTL to make the driver ready to unload
//
#define IOCTL_PREPARE_TO_UNLOAD \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 2048, METHOD_NEITHER, FILE_WRITE_ACCESS)

#undef FlagOn

#undef SetFlag

#undef ClearFlag

#define FlagOn(x,f)     ((BOOLEAN)((((x) & (f)) != 0)))

#define SetFlag(x,f)    ((x) |= (f))

#define ClearFlag(x,f)  ((x) &= ~(f))

VOID FsdCopyWchar (
    IN OUT PWCHAR   Destination,
    IN PWCHAR        Source,
    IN ULONG        Length
    );

//
// FSD_IDENTIFIER_TYPE
//
// Identifiers used to mark the structures
//
typedef enum _FSD_IDENTIFIER_TYPE {
    FGD = ':DGF',
    VCB = ':BCV',
    FCB = ':BCF',
    CCB = ':BCC',
    ICX = ':XCI',
    FSD = ':DSF'
} FSD_IDENTIFIER_TYPE;

//
// FSD_IDENTIFIER
//
// Header used to mark the structures
//
typedef struct _FSD_IDENTIFIER {
    FSD_IDENTIFIER_TYPE     Type;
    ULONG                   Size;
} FSD_IDENTIFIER, *PFSD_IDENTIFIER;

//
// FSD_GLOBAL_DATA
//
// Data that is not specific to a mounted volume
//
typedef struct _FSD_GLOBAL_DATA {

    // Identifier for this structure
    FSD_IDENTIFIER              Identifier;

    // Syncronization primitive for this structure
    ERESOURCE                   Resource;

    // Table of pointers to the fast I/O entry points
    FAST_IO_DISPATCH            FastIoDispatch;

    // Table of pointers to the Cache Manager callbacks
    CACHE_MANAGER_CALLBACKS     CacheManagerCallbacks;

    // Pointer to the driver object
    PDRIVER_OBJECT              DriverObject;

    // Pointer to the main device object
    PDEVICE_OBJECT              DeviceObject;

    // List of mounted volumes
    LIST_ENTRY                  VcbList;

    // Global flags for the driver
    ULONG                       Flags;

} FSD_GLOBAL_DATA, *PFSD_GLOBAL_DATA;

//
// Flags for FSD_GLOBAL_DATA
//
#define FSD_UNLOAD_PENDING      0x00000001

//
// The global data is declared in init.c
//
extern FSD_GLOBAL_DATA FsdGlobalData;

//
// FSD_VCB Volume Control Block
//
// Data that represents a mounted logical volume
// It is allocated as the device extension of the volume device object
//
typedef struct _FSD_VCB {

    // FCB header required by NT
    // The VCB is also used as an FCB for file objects
    // that represents the volume itself
    FSRTL_COMMON_FCB_HEADER     CommonFCBHeader;
    SECTION_OBJECT_POINTERS     SectionObject;
    ERESOURCE                   MainResource;
    ERESOURCE                   PagingIoResource;
    // end FCB header required by NT

    // Identifier for this structure
    FSD_IDENTIFIER              Identifier;

    // List of VCBs
    LIST_ENTRY                  Next;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
    // for files on this volume.
    ULONG                       OpenFileHandleCount;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
    // for both files on this volume and open instances of the
    // volume itself.
    ULONG                       ReferenceCount;

    // Pointer to the VPB in the target device object
    PVPB                        Vpb;

    // List of FCBs for open files on this volume
    LIST_ENTRY                  FcbList;

    // List of IRPs pending on directory change notify requests
    LIST_ENTRY                  NotifyList;

    // Pointer to syncronization primitive for this list
    PNOTIFY_SYNC                NotifySync;

    // This volumes device object
    PDEVICE_OBJECT              DeviceObject;

    // The physical device object (the disk)
    PDEVICE_OBJECT              TargetDeviceObject;

    // Information about the physical device object
    DISK_GEOMETRY               DiskGeometry;
    PARTITION_INFORMATION       PartitionInformation;

    // Pointer to the root directory block
    struct ch10_dir_block*      root_dirblock;
	
	struct ch10_dir_block      	dirblocks[CH10_MAX_DIR_BLOCKS];

    // Flags for the volume
    ULONG                       Flags;

} FSD_VCB, *PFSD_VCB;

//
// Flags for FSD_VCB
//
#define VCB_VOLUME_LOCKED       0x00000001
#define VCB_DISMOUNT_PENDING    0x00000002
#define VCB_READ_ONLY           0x00000004

//
// FSD_FCB File Control Block
//
// Data that represents an open file
// There is a single instance of the FCB for every open file
//
typedef struct _FSD_FCB {

    // FCB header required by NT
    FSRTL_COMMON_FCB_HEADER         CommonFCBHeader;
    SECTION_OBJECT_POINTERS         SectionObject;
    ERESOURCE                       MainResource;
    ERESOURCE                       PagingIoResource;
    // end FCB header required by NT

    // Identifier for this structure
    FSD_IDENTIFIER                  Identifier;

    // List of FCBs for this volume
    LIST_ENTRY                      Next;

    // Share Access for the file object
    SHARE_ACCESS                    ShareAccess;

    // List of byte-range locks for this file
    FILE_LOCK                       FileLock;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLEANUP
    ULONG                           OpenHandleCount;

    // Incremented on IRP_MJ_CREATE, decremented on IRP_MJ_CLOSE
    ULONG                           ReferenceCount;

    // The filename
    UNICODE_STRING                  FileName;
#if DBG
    ANSI_STRING                     AnsiFileName;
#endif

    // The file attributes
    ULONG                           FileAttributes;

    // The inode number
    LARGE_INTEGER                   IndexNumber;

    // Flags for the FCB
    ULONG                           Flags;

    // Pointer to the inode
    struct ch10_dir_entry*          ch10_direntry;

} FSD_FCB, *PFSD_FCB;

//
// Flags for FSD_FCB
//
#define FCB_PAGE_FILE               0x00000001
#define FCB_DELETE_PENDING          0x00000002

//
// FSD_CCB Context Control Block
//
// Data that represents one instance of an open file
// There is one instance of the CCB for every instance of an open file
//
typedef struct _FSD_CCB {

    // Identifier for this structure
    FSD_IDENTIFIER  Identifier;

    // State that may need to be maintained
    ULONG           CurrentByteOffset;
    UNICODE_STRING  DirectorySearchPattern;

} FSD_CCB, *PFSD_CCB;

//
// FSD_IRP_CONTEXT
//
// Used to pass information about a request between the drivers functions
//
typedef struct _FSD_IRP_CONTEXT {

    // Identifier for this structure
    FSD_IDENTIFIER      Identifier;

    // Pointer to the IRP this request describes
    PIRP                Irp;

    // The major and minor function code for the request
    UCHAR               MajorFunction;
    UCHAR               MinorFunction;

    // The device object
    PDEVICE_OBJECT      DeviceObject;

    // The file object
    PFILE_OBJECT        FileObject;

    // If the request is synchronous (we are allowed to block)
    BOOLEAN             IsSynchronous;

    // If the request is top level
    BOOLEAN             IsTopLevel;

    // Used if the request needs to be queued for later processing
#if (VER_PRODUCTBUILD >= 2195)
    PIO_WORKITEM        WorkQueueItem;
#else
    WORK_QUEUE_ITEM     WorkQueueItem;
#endif

    // The exception code when an exception is in progress
    NTSTATUS            ExceptionCode;

} FSD_IRP_CONTEXT, *PFSD_IRP_CONTEXT;

//
// FSD_ALLOC_HEADER
//
// In the checked version of the driver this header is put in the beginning of
// every memory allocation
//
typedef struct _FSD_ALLOC_HEADER {
    FSD_IDENTIFIER Identifier;
} FSD_ALLOC_HEADER, *PFSD_ALLOC_HEADER;

//
// Function prototypes from alloc.c
//

#if DBG

PVOID
FsdAllocatePool (
    IN POOL_TYPE    PoolType,
    IN ULONG        NumberOfBytes,
    IN ULONG        Tag
    );

VOID
FsdFreePool (
    IN PVOID p
    );

#else // !DBG

#define FsdAllocatePool(PoolType, NumberOfBytes, Tag) \
        ExAllocatePool(PoolType, NumberOfBytes)

#define FsdFreePool(p) \
        ExFreePool(p)

#endif // !DBG

PFSD_IRP_CONTEXT
FsdAllocateIrpContext (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

VOID
FsdFreeIrpContext (
    IN PFSD_IRP_CONTEXT IrpContext
    );

PFSD_FCB
FsdAllocateFcb (
    IN PFSD_VCB             	Vcb,
    IN PUNICODE_STRING      	FileName,
    IN ULONG                	IndexNumber,
    IN struct ch10_dir_entry*  	ch10_inode
    );

VOID
FsdFreeFcb (
    IN PFSD_FCB Fcb
    );

PFSD_CCB
FsdAllocateCcb (
    VOID
    );

VOID
FsdFreeCcb (
    IN PFSD_CCB Ccb
    );

VOID
FsdFreeVcb (
    IN PFSD_VCB Vcb
    );

//
// Function prototypes from blockdev.c
//

NTSTATUS 
FsdBlockDeviceIoControl (
    IN PDEVICE_OBJECT   DeviceObject,
    IN ULONG            IoctlCode,
    IN PVOID            InputBuffer,
    IN ULONG            InputBufferSize,
    IN OUT PVOID        OutputBuffer,
    IN OUT PULONG       OutputBufferSize
    );

NTSTATUS
FsdReadBlockDevice (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN OUT PVOID        Buffer
    );

#ifndef FSD_RO

NTSTATUS
FsdWriteBlockDevice (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN PVOID            Buffer
    );

#endif // !FSD_RO

NTSTATUS
FsdReadBlockDeviceOverrideVerify (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN OUT PVOID        Buffer
    );

NTSTATUS
FsdReadBlockDeviceAtApcLevel (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN OUT PVOID        Buffer
    );

#ifndef FSD_RO

NTSTATUS
FsdWriteBlockDeviceAtApcLevel (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN OUT PVOID        Buffer
    );

#endif // !FSD_RO

//
// Function prototypes from char.c
//

VOID
FsdCharToWchar (
    IN OUT PWCHAR   Destination,
    IN PCHAR        Source,
    IN ULONG        Length
    );

NTSTATUS
FsdWcharToChar (
    IN OUT PCHAR    Destination,
    IN PWCHAR       Source,
    IN ULONG        Length
    );

//
// Function prototypes from cleanup.c
//

NTSTATUS
FsdCleanup (
    IN PFSD_IRP_CONTEXT IrpContext
    );

//
// Function prototypes from close.c
//

NTSTATUS
FsdClose (
    IN PFSD_IRP_CONTEXT IrpContext
    );

VOID
FsdQueueCloseRequest (
    IN PFSD_IRP_CONTEXT IrpContext
    );

VOID
FsdDeQueueCloseRequest (
#if (VER_PRODUCTBUILD >= 2195)
    IN PDEVICE_OBJECT   DeviceObject,
#endif
    IN PVOID            Context
    );

//
// Function prototypes from cmcb.c
//

BOOLEAN
FsdAcquireForLazyWrite (
    IN PVOID    Context,
    IN BOOLEAN  Wait
    );

VOID
FsdReleaseFromLazyWrite (
    IN PVOID Context
    );

BOOLEAN
FsdAcquireForReadAhead (
    IN PVOID    Context,
    IN BOOLEAN  Wait
    );

VOID
FsdReleaseFromReadAhead (
    IN PVOID Context
    );

//
// Function prototypes from create.c
//

NTSTATUS
FsdCreate (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdCreateFs (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdCreateVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdCreateFile (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdLookupFileName (
    IN PFSD_VCB                 	Vcb,
    IN PUNICODE_STRING          	FullFileName,
    IN OUT PULONG               	Index,
    IN OUT struct ch10_dir_entry*  	Inode
    );

PFSD_FCB
FsdLookupFcbByFileName (
    IN PFSD_VCB         Vcb,
    IN PUNICODE_STRING  FullFileName
    );

//
// Function prototypes from debug.c
//

#if DBG

extern ULONG ProcessNameOffset;

#define FsdGetCurrentProcessName() ( \
    (PUCHAR) PsGetCurrentProcess() + ProcessNameOffset \
)

ULONG 
FsdGetProcessNameOffset (
    VOID
    );

VOID
FsdDbgPrintCall (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

VOID
FsdDbgPrintComplete (
    IN PIRP Irp
    );

PUCHAR
FsdNtStatusToString (
    IN NTSTATUS Status
    );

#define FsdCompleteRequest(Irp, PriorityBoost) \
        FsdDbgPrintComplete(Irp); \
        IoCompleteRequest(Irp, PriorityBoost)

#else // !DBG

#define FsdDbgPrintCall(DeviceObject, Irp)

#define FsdCompleteRequest(Irp, PriorityBoost) \
        IoCompleteRequest(Irp, PriorityBoost)

#endif // !DBG

//
// Function prototypes from devctl.c
//

NTSTATUS
FsdDeviceControl (
    IN PFSD_IRP_CONTEXT IrpContext
    );

//
// Function prototypes from dirctl.c
//

NTSTATUS
FsdDirectoryControl (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdQueryDirectory (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdNotifyChangeDirectory (
    IN PFSD_IRP_CONTEXT IrpContext
    );

//
// Function prototypes from fastio.c
//

BOOLEAN
FsdFastIoCheckIfPossible (
    IN PFILE_OBJECT         FileObject,
    IN PLARGE_INTEGER       FileOffset,
    IN ULONG                Length,
    IN BOOLEAN              Wait,
    IN ULONG                LockKey,
    IN BOOLEAN              CheckForReadOperation,
    OUT PIO_STATUS_BLOCK    IoStatus,
    IN PDEVICE_OBJECT       DeviceObject
    );

#if DBG

BOOLEAN
FsdFastIoRead (
    IN PFILE_OBJECT         FileObject,
    IN PLARGE_INTEGER       FileOffset,
    IN ULONG                Length,
    IN BOOLEAN              Wait,
    IN ULONG                LockKey,
    OUT PVOID               Buffer,
    OUT PIO_STATUS_BLOCK    IoStatus,
    IN PDEVICE_OBJECT       DeviceObject
    );

#endif // DBG

BOOLEAN
FsdFastIoQueryBasicInfo (
    IN PFILE_OBJECT             FileObject,
    IN BOOLEAN                  Wait,
    OUT PFILE_BASIC_INFORMATION Buffer,
    OUT PIO_STATUS_BLOCK        IoStatus,
    IN PDEVICE_OBJECT           DeviceObject
    );

BOOLEAN
FsdFastIoQueryStandardInfo (
    IN PFILE_OBJECT                 FileObject,
    IN BOOLEAN                      Wait,
    OUT PFILE_STANDARD_INFORMATION  Buffer,
    OUT PIO_STATUS_BLOCK            IoStatus,
    IN PDEVICE_OBJECT               DeviceObject
    );

BOOLEAN
FsdFastIoLock (
    IN PFILE_OBJECT         FileObject,
    IN PLARGE_INTEGER       FileOffset,
    IN PLARGE_INTEGER       Length,
    IN PEPROCESS            Process,
    IN ULONG                Key,
    IN BOOLEAN              FailImmediately,
    IN BOOLEAN              ExclusiveLock,
    OUT PIO_STATUS_BLOCK    IoStatus,
    IN PDEVICE_OBJECT       DeviceObject
    );

BOOLEAN
FsdFastIoUnlockSingle (
    IN PFILE_OBJECT         FileObject,
    IN PLARGE_INTEGER       FileOffset,
    IN PLARGE_INTEGER       Length,
    IN PEPROCESS            Process,
    IN ULONG                Key,
    OUT PIO_STATUS_BLOCK    IoStatus,
    IN PDEVICE_OBJECT       DeviceObject
    );

BOOLEAN
FsdFastIoUnlockAll (
    IN PFILE_OBJECT         FileObject,
    IN PEPROCESS            Process,
    OUT PIO_STATUS_BLOCK    IoStatus,
    IN PDEVICE_OBJECT       DeviceObject
    );

BOOLEAN
FsdFastIoUnlockAllByKey (
    IN PFILE_OBJECT         FileObject,
    IN PEPROCESS            Process,
    IN ULONG                Key,
    OUT PIO_STATUS_BLOCK    IoStatus,
    IN PDEVICE_OBJECT       DeviceObject
    );

BOOLEAN
FsdFastIoQueryNetworkOpenInfo (
    IN PFILE_OBJECT                     FileObject,
    IN BOOLEAN                          Wait,
    OUT PFILE_NETWORK_OPEN_INFORMATION  Buffer,
    OUT PIO_STATUS_BLOCK                IoStatus,
    IN PDEVICE_OBJECT                   DeviceObject
    );

//
// Function prototypes from fileinfo.c
//

NTSTATUS
FsdQueryInformation (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdSetInformation (
    IN PFSD_IRP_CONTEXT IrpContext
    );

//
// Function prototypes from fsctl.c
//

NTSTATUS
FsdFileSystemControl (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdUserFsRequest (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdLockVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdUnlockVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdDismountVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdIsVolumeMounted (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdMountVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdVerifyVolume (
    IN PFSD_IRP_CONTEXT IrpContext
    );

VOID
FsdPurgeVolume (
    IN PFSD_VCB Vcb,
    IN BOOLEAN  FlushBeforePurge
    );

VOID
FsdPurgeFile (
    IN PFSD_FCB Fcb,
    IN BOOLEAN  FlushBeforePurge
    );

VOID
FsdSetVpbFlag (
    IN PVPB     Vpb,
    IN USHORT   Flag
    );

VOID
FsdClearVpbFlag (
    IN PVPB     Vpb,
    IN USHORT   Flag
    );

//
// Function prototypes from fsd.c
//

NTSTATUS
FsdBuildRequest (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

NTSTATUS
FsdQueueRequest (
    IN PFSD_IRP_CONTEXT IrpContext
    );

VOID
FsdDeQueueRequest (
#if (VER_PRODUCTBUILD >= 2195)
    IN PDEVICE_OBJECT   DeviceObject,
#endif
    IN PVOID            Context
    );

NTSTATUS
FsdDispatchRequest (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdExceptionFilter (
    IN PFSD_IRP_CONTEXT     IrpContext,
    IN NTSTATUS             ExceptionCode
    );

NTSTATUS
FsdExceptionHandler (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdLockUserBuffer (
    IN PIRP             Irp,
    IN ULONG            Length,
    IN LOCK_OPERATION   Operation
    );

PVOID
FsdGetUserBuffer (
    IN PIRP Irp
    );

NTSTATUS
FsdReadDirBlockByOffset (
    IN PDEVICE_OBJECT           DeviceObject,
    IN ULONG                    BlockOffset,
    IN OUT struct ch10_dir_block*  Inode
    );
//
// Function prototypes from init.c
//

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    );

VOID
DriverUnload (
    IN PDRIVER_OBJECT DriverObject
    );

//
// Function prototypes from lockctl.c
//

NTSTATUS
FsdLockControl (
    IN PFSD_IRP_CONTEXT IrpContext
    );

//
// Function prototypes from read.c
//

NTSTATUS
FsdRead (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdReadNormal (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdReadComplete (
    IN PFSD_IRP_CONTEXT IrpContext
    );

NTSTATUS
FsdReadFileData (
    IN PDEVICE_OBJECT       DeviceObject,
    IN ULONG                Index,
    IN PLARGE_INTEGER       Offset,
    IN ULONG                Length,
    IN OUT PVOID            Buffer
    );

//
// Function prototypes from ch10fsrec.c
//

NTSTATUS
FsdIsDeviceCh10fs (
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
FsdIsDeviceSameCh10fs (
    IN PDEVICE_OBJECT   DeviceObject,
    IN ULONG            CheckSum
    );

//
// Function prototypes from string.c
//
DWORD32 RtlIndexOfCharInUnicodeString(
  PCUNICODE_STRING String,
  WCHAR w
);

size_t ch10fs_strnlen(const char * s, size_t count);
#define strnlen(s,n) ch10fs_strnlen(s,n)

//
// Function prototypes from volinfo.c
//

NTSTATUS
FsdQueryVolumeInformation (
    IN PFSD_IRP_CONTEXT IrpContext
    );

//
// These declarations is missing in some versions of the DDK and ntifs.h
//

#ifndef IoCopyCurrentIrpStackLocationToNext
#define IoCopyCurrentIrpStackLocationToNext( Irp ) { \
    PIO_STACK_LOCATION irpSp; \
    PIO_STACK_LOCATION nextIrpSp; \
    irpSp = IoGetCurrentIrpStackLocation( (Irp) ); \
    nextIrpSp = IoGetNextIrpStackLocation( (Irp) ); \
    RtlCopyMemory( \
        nextIrpSp, \
        irpSp, \
        FIELD_OFFSET(IO_STACK_LOCATION, CompletionRoutine) \
        ); \
    nextIrpSp->Control = 0; }
#endif

#ifndef IoSkipCurrentIrpStackLocation
#define IoSkipCurrentIrpStackLocation( Irp ) \
    (Irp)->CurrentLocation++; \
    (Irp)->Tail.Overlay.CurrentStackLocation++;
#endif

NTKERNELAPI
VOID
FsRtlNotifyChangeDirectory (
    IN PNOTIFY_SYNC NotifySync,
    IN PVOID        FsContext,
    IN PSTRING      FullDirectoryName,
    IN PLIST_ENTRY  NotifyList,
    IN BOOLEAN      WatchTree,
    IN ULONG        CompletionFilter,
    IN PIRP         NotifyIrp
);

#ifndef RtlUshortByteSwap
USHORT
FASTCALL
RtlUshortByteSwap(
    IN USHORT Source
    );
#endif

#ifndef RtlUlongByteSwap
ULONG
FASTCALL
RtlUlongByteSwap(
    IN ULONG Source
    );
#endif

#ifndef RtlUlonglongByteSwap
ULONGLONG
FASTCALL
RtlUlonglongByteSwap(
    IN ULONGLONG Source
    );
#endif

#endif
