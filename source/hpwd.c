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

# include <stdio.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "hpwd.h"
# include "charset.h"

/*
 * NAME:	hpwd->main()
 * DESCRIPTION:	implement hpwd command
 */
int hpwd_main(int argc, wchar_t *argv[])
{
	mountent *ent;

	if (argc != 2)
	{
		fwprintf(stderr, L"Usage: pwd\n");
		return 1;
	}

	ent = hcwd_getvol(-1);
	if (ent == 0)
	{
		fwprintf(stderr, L"pwd: No volume is current; use `hmount' or `hvol'\n");
		return 1;
	}

	if (wcscmp(ent->cwd, L":") == 0)
		wprintf(L"%s:\n", ent->vname);
	else
		wprintf(L"%s%s:\n", ent->vname, ent->cwd);

	return 0;
}
