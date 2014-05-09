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


DWORD32 RtlIndexOfCharInUnicodeString(
  PCUNICODE_STRING String,
  WCHAR w
) {
	DWORD32 i;
	for(i = 0; i < String->Length / 2; i++) {
		if(String->Buffer[i] == w) return i;
	}

	return -1;
}	

//
// The following is a subset of linux/lib/string.c from version 2.2.14
//

/*
 *  linux/lib/string.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

size_t ch10fs_strnlen(const char * s, size_t count)
{
    const char *sc;

    for (sc = s; count-- && *sc != '\0'; ++sc)
        /* nothing */;
    return sc - s;
}

//
// End of subset of linux/lib/string.c
//
