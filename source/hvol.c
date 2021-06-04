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
# include "hvol.h"
# include "charset.h"

/*
 * NAME:	showvol()
 * DESCRIPTION:	output information about a mounted volume
 */
static
int showvol(mountent *ment)
{
	hfsvol *vol;
	hfsvolent vent;
	int result = 0;

	wprintf(L"Current volume is mounted from");
	if (ment->partno > 0)
		wprintf(L" partition %d of", ment->partno);
	wprintf(L":\n	%s\n", ment->path);

	vol = hfsutil_remount(ment, HFS_MODE_ANY);
	if (vol == 0)
		return 1;

	wprintf(L"\n");

	hfs_vstat(vol, &vent);
	hfsutil_pinfo(&vent);
	hfsutil_unmount(vol, &result);

	return result;
}

/*
 * NAME:	hvol->main()
 * DESCRIPTION:	implement hvol command
 */
int hvol_main(int argc, wchar_t *argv[])
{
	int vnum;
	mountent *ment;

	if (argc > 3)
	{
		fwprintf(stderr, L"Usage: vol [volume-name-or-path]\n");
		return 1;
	}

	if (argc == 2)
	{
		int output = 0, header = 0;

		ment = hcwd_getvol(-1);
		if (ment)
		{
			showvol(ment);
			output = 1;
		}

		for (vnum = 0; ; ++vnum)
		{
			mountent *ent;

			ent = hcwd_getvol(vnum);
			if (ent == 0)
				break;

			if (ent == ment)
				continue;

			if (header == 0)
			{
				wprintf(L"%s volumes:\n", ment ? L"\nOther known" : L"Known");
				header = 1;
			}

			if (ent->partno <= 0)
				wprintf(L"	%-35s		 \"%s\"\n", ent->path, ent->vname);
			else
				wprintf(L"	%-35s %2d	\"%s\"\n", ent->path, ent->partno, ent->vname);

			output = 1;
		}

		if (output == 0)
			wprintf(L"No known volumes; use `hmount' to introduce new volumes\n");
		return 0;
	}

	for (ment = hcwd_getvol(vnum = 0); ment; ment = hcwd_getvol(++vnum))
	{
		if (hfsutil_samepath_w(argv[2], ment->path) || _wcsicmp(argv[2], ment->vname) == 0)
		{
			hcwd_setvol(vnum);
			return showvol(ment);
		}
	}

	fwprintf(stderr, L"vol: Unknown volume \"%s\"\n", argv[2]);

	return 1;
}
