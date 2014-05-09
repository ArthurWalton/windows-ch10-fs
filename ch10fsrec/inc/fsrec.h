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

#ifndef _FSREC_
#define _FSREC_

//
// Name for the driver and it's device
//
#define DRIVER_NAME "Ch10FsRec"
#define DEVICE_NAME L"\\FileSystem\\Ch10FsRecognizer"

//
// Registry path for the FSD and this driver
//

#define FSD_REGISTRY_PATH \
    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Ch10Fs"

#define FSR_REGISTRY_PATH \
    L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\Ch10FsRec"

//
// Comment out these to make all driver code nonpaged
//
#define FSR_INIT_CODE   "init"
#define FSR_PAGED_CODE  "page"

//
// Function prototypes from blockdev.c
//

NTSTATUS
FsrReadBlockDevice (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PLARGE_INTEGER   Offset,
    IN ULONG            Length,
    IN OUT PVOID        Buffer
    );

//
// Function prototypes from fsrec.c
//

NTSTATUS
DriverEntry (
    IN PDRIVER_OBJECT   DriverObject,
    IN PUNICODE_STRING  RegistryPath
    );

VOID
DriverUnload (
    IN PDRIVER_OBJECT   DriverObject
    );

NTSTATUS
FsrFileSystemControl (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

//
// Function prototypes from ch10fsrec.c
//

NTSTATUS
FsrIsDeviceCh10fs (
    IN PDEVICE_OBJECT   DeviceObject
    );

#endif
