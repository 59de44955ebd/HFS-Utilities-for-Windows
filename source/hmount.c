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
# include <stdlib.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "suid.h"
# include "hmount.h"
# include "charset.h"

/*
 * NAME:	hmount->main()
 * DESCRIPTION:	implement hmount command
 */
int hmount_main(int argc, wchar_t *argv[])
{
	wchar_t *path = 0;
	hfsvol *vol;
	hfsvolent ent;
	int nparts, partno, result = 0;
	wchar_t *wname;

	//if (argc < 2 || argc > 3)
	if (argc < 3 || argc > 4)
	{
		fwprintf(stderr, L"Usage: mount source-path [partition-no]\n");
		goto fail;
	}

	//printf("%s\n", argv[1]);
	//path = hfsutil_abspath(argv[1]);
	//printf("ABS: %s\n", path);

	path = _wfullpath(NULL, argv[2], _MAX_PATH);

	if (path == 0)
	{
		fwprintf(stderr, L"mount: not enough memory\n");
		goto fail;
	}

	suid_enable();
	nparts = hfs_nparts(path);
	suid_disable();

	if (nparts >= 0)
		wprintf(L"%s: contains %d HFS partition%s\n", path, nparts, nparts == 1 ? L"" : L"s");

	if (argc == 3)
		partno = _wtoi(argv[2]);
	else
	{
		if (nparts > 1)
		{
			fwprintf(stderr, L"mount: must specify partition number\n");
			goto fail;
		}
		else if (nparts == -1)
			partno = 0;
		else
			partno = 1;

	}

	suid_enable();
	vol = hfs_mount(path, partno, HFS_MODE_ANY);
	suid_disable();

	if (vol == 0)
	{
		hfsutil_perror_w(path);
		goto fail;
	}

	hfs_vstat(vol, &ent);
	hfsutil_pinfo(&ent);

	wname = macRomanToUtf16(ent.name);
	if (wname == 0)
	{
		fwprintf(stderr, L"mount: not enough memory\n");
		goto fail;
	}
	if (hcwd_mounted(wname, ent.crdate, path, partno) == -1)
	{
		_wperror(L"Failed to record mount");
		result = 1;
	}

	hfsutil_unmount(vol, &result);

	free(path);

	return result;

fail:
	if (path)
		free(path);

	return 1;
}
