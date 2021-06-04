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
# include <string.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "hcd.h"
# include "charset.h"

/*
 * NAME:	hcd->main()
 * DESCRIPTION:	implement hcd command
 */
int hcd_main(int argc, wchar_t *argv[])
{
	mountent *ment;
	hfsvol *vol;
	hfsvolent vent;
	char *path = "", root[HFS_MAX_VLEN + 1 + 1];
	wchar_t *wpath;
	int fargc;
	char **fargv = 0;
	int result = 0;

	if (argc > 3)
	{
		fwprintf(stderr, L"Usage: cd [hfs-path]\n");
		return 1;
	}

	vol = hfsutil_remount(ment = hcwd_getvol(-1), HFS_MODE_RDONLY);
	if (vol == 0)
		return 1;

	if (argc == 3)
	{

		//fargv = hfsutil_glob(vol, argc - optind - 1, &argv[optind], &fargc, &result);
		//const char * hfs_path = MACROMAN(argv[1]);

		fargv = hfsutil_glob(vol, 1, &argv[2], &fargc, &result);

		if (result == 0)
		{
			if (fargc != 1)
			{
				fwprintf(stderr, L"cd: %s: ambiguous path\n", argv[2]);
				result = 1;
			}
			else
				path = fargv[0];
		}
	}
	else
	{
		hfs_vstat(vol, &vent);

		strcpy(root, vent.name);
		strcat(root, ":");
		path = root;
	}

	if (result == 0)
	{
		if (hfs_chdir(vol, path) == -1)
		{
			hfsutil_perrorp(path);
			result = 1;
		}
		else
		{
			path = hfsutil_getcwd(vol);
			if (path == 0)
			{
				hfsutil_perror("Can't get new HFS directory path");
				result = 1;
			}
			else
			{
				wpath = macRomanToUtf16(path);
				if (wpath == 0)
				{
					fwprintf(stderr, L"cd: not enough memory\n");
					result = 1;
				}
				else
				{
					if (hcwd_setcwd(ment, wpath) == -1) //???
					{
						_wperror(L"Can't set new HFS directory");
						result = 1;
					}
					free(wpath);
				}
				free(path);
			}
		}
	}

	hfsutil_unmount(vol, &result);

	if (fargv)
		free(fargv);

	return result;
}
