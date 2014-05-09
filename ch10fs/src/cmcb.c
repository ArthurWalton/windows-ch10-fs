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

BOOLEAN
FsdAcquireForLazyWrite (
    IN PVOID    Context,
    IN BOOLEAN  Wait
    )
{
    //
    // On a readonly filesystem this function still has to exist but it
    // doesn't need to do anything.
    //

    PFSD_FCB Fcb;

    Fcb = (PFSD_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == FCB) &&
           (Fcb->Identifier.Size == sizeof(FSD_FCB)));

    KdPrint((
        DRIVER_NAME ": %-16.16s %-31s %s\n",
        FsdGetCurrentProcessName(),
        "ACQUIRE_FOR_LAZY_WRITE",
        Fcb->AnsiFileName.Buffer
        ));

#ifdef FSD_RO

    return TRUE;

#else

    return ExAcquireResourceExclusiveLite(
        &Fcb->MainResource,
        Wait
        );

#endif

}

VOID
FsdReleaseFromLazyWrite (
    IN PVOID Context
    )
{
    //
    // On a readonly filesystem this function still has to exist but it
    // doesn't need to do anything.
    //

    PFSD_FCB Fcb;

    Fcb = (PFSD_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == FCB) &&
           (Fcb->Identifier.Size == sizeof(FSD_FCB)));

    KdPrint((
        DRIVER_NAME ": %-16.16s %-31s %s\n",
        FsdGetCurrentProcessName(),
        "RELEASE_FROM_LAZY_WRITE",
        Fcb->AnsiFileName.Buffer
        ));

#ifndef FSD_RO

    ExReleaseResourceForThreadLite(
        &Fcb->MainResource,
        ExGetCurrentResourceThread()
        );

#endif

}

BOOLEAN
FsdAcquireForReadAhead (
    IN PVOID    Context,
    IN BOOLEAN  Wait
    )
{
    PFSD_FCB Fcb;

    Fcb = (PFSD_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == FCB) &&
           (Fcb->Identifier.Size == sizeof(FSD_FCB)));

    KdPrint((
        DRIVER_NAME ": %-16.16s %-31s %s\n",
        FsdGetCurrentProcessName(),
        "ACQUIRE_FOR_READ_AHEAD",
        Fcb->AnsiFileName.Buffer
        ));

	//KeEnterCriticalRegion();

    return ExAcquireResourceSharedLite(
        &Fcb->MainResource,
        Wait
        );
}

VOID
FsdReleaseFromReadAhead (
    IN PVOID Context
    )
{
    PFSD_FCB Fcb;

    Fcb = (PFSD_FCB) Context;

    ASSERT(Fcb != NULL);

    ASSERT((Fcb->Identifier.Type == FCB) &&
           (Fcb->Identifier.Size == sizeof(FSD_FCB)));

    KdPrint((
        DRIVER_NAME ": %-16.16s %-31s %s\n",
        FsdGetCurrentProcessName(),
        "RELEASE_FROM_READ_AHEAD",
        Fcb->AnsiFileName.Buffer
        ));

    ExReleaseResourceForThreadLite(
        &Fcb->MainResource,
        ExGetCurrentResourceThread()
        );

	//KeLeaveCriticalRegion();
}
