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

#include "ntifs.h"
#include "fsd.h"

VOID
FsdCopyWchar (
    IN OUT PWCHAR   Destination,
    IN PWCHAR        Source,
    IN ULONG        Length
    )
{
    ULONG Index;

    ASSERT(Destination != NULL);
    ASSERT(Source != NULL);

    for (Index = 0; Index < Length; Index++)
    {
        Destination[Index] = (WCHAR) Source[Index];
    }
}

VOID
FsdCharToWchar (
    IN OUT PWCHAR   Destination,
    IN PCHAR        Source,
    IN ULONG        Length
    )
{
    ULONG Index;

    ASSERT(Destination != NULL);
    ASSERT(Source != NULL);

    for (Index = 0; Index < Length; Index++)
    {
        Destination[Index] = (WCHAR) Source[Index];
    }
}

NTSTATUS
FsdWcharToChar (
    IN OUT PCHAR    Destination,
    IN PWCHAR       Source,
    IN ULONG        Length
    )
{
    ULONG       Index;
    NTSTATUS    Status;

    ASSERT(Destination != NULL);
    ASSERT(Source != NULL);

    for (Index = 0, Status = STATUS_SUCCESS; Index < Length; Index++)
    {
        Destination[Index] = (CHAR) Source[Index];

        //
        // Check that the wide character fits in a normal character
        // but continue with the conversion anyway
        //
        if ( ((WCHAR) Destination[Index]) != Source[Index] )
        {
            Status = STATUS_OBJECT_NAME_INVALID;
        }
    }

    return Status;
}
