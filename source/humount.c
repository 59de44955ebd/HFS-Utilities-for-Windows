/*
 * hfsutils - tools for reading and writing Macintosh HFS volumes
 * Copyright (C) 1996-1998 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

# include <unistd.h>
# include <stdio.h>
# include <string.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "humount.h"
# include "charset.h"

/*
 * NAME:	humount->main()
 * DESCRIPTION:	implement humount command
 */
int humount_main(int argc, wchar_t *argv[])
{
	int vnum;
	mountent *ent;

	//if (argc > 2)
	if (argc > 3)
	{
		fwprintf(stderr, L"Usage: umount [volume-name-or-path]\n");
		return 1;
	}

	//if (argc == 1)
	if (argc == 2)
	{
		if (hcwd_umounted(-1) == -1 && hcwd_getvol(-1) == 0)
		{
			fwprintf(stderr, L"umount: No volume is current\n");
			return 1;
		}

		return 0;
	}

	for (ent = hcwd_getvol(vnum = 0); ent; ent = hcwd_getvol(++vnum))
	{
		if (hfsutil_samepath_w(argv[1], ent->path) || _wcsicmp(argv[1], ent->vname) == 0)
		{
			hcwd_umounted(vnum);
			return 0;
		}
	}

	fwprintf(stderr, L"umount: Unknown volume \"%s\"\n", argv[2]);

	return 1;
}
