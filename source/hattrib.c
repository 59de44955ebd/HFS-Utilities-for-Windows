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
# include "hattrib.h"
# include "charset.h"

/*
 * NAME:	usage()
 * DESCRIPTION:	display usage message
 */
static
int usage(void)
{
	fwprintf(stderr,
		L"Usage: attrib [-t TYPE] [-c CREA] [-|+i] [-|+l] hfs-path [...]\n"
		L"			 attrib -b hfs-path\n");

	return 1;
}

/*
 * NAME:	hattrib->main()
 * DESCRIPTION:	implement hattrib command
 */
int hattrib_main(int argc, wchar_t *argv[])
{
	const wchar_t *type = 0, *crea = 0;
	int invis = 0, lock = 0, bless = 0;
	hfsvol *vol;
	int fargc;
	char **fargv;
	int i, result = 0;

	for (i = 2; i < argc; ++i)
	{
		switch (argv[i][0])
		{
		case L'-':
			switch (argv[i][1])
			{
			case L't':
				type = argv[++i];

				if (type == 0)
					return usage();

				if (wcslen(type) != 4)
				{
					fwprintf(stderr, L"attrib: file type must be 4 characters\n");
					return 1;
				}
				continue;

			case L'c':
				crea = argv[++i];

				if (crea == 0)
					return usage();

				if (wcslen(crea) != 4)
				{
					fwprintf(stderr, L"attrib: file creator must be 4 characters\n");
					return 1;
				}
				continue;

			case L'i':
				invis = -1;
				continue;

			case L'l':
				lock = -1;
				continue;

			case L'b':
				bless = 1;
				continue;

			default:
				return usage();
			}
			break;

		case L'+':
			switch (argv[i][1])
			{
			case L'i':
				invis = 1;
				continue;

			case L'l':
				lock = 1;
				continue;

			default:
				return usage();
			}
			break;
		}
		break;
	}

	if (argc - i == 0)
		return usage();

	if (i == 2)
	{
		fwprintf(stderr, L"attrib: no attributes specified\n");
		return 1;
	}

	if (bless && (lock || invis || type || crea || argc - i > 1))
		return usage();

	vol = hfsutil_remount(hcwd_getvol(-1), HFS_MODE_ANY);
	if (vol == 0)
		return 1;

	fargv = hfsutil_glob(vol, argc - i, &argv[i], &fargc, &result);

	//const char * hfs_path = MACROMAN(argv[1]);
	//fargv = hfsutil_glob(vol, argc - i, &hfs_path, &fargc, &result);

	if (result == 0)
	{
		hfsdirent ent;

		if (bless)
		{
			if (fargc != 1)
			{
				fwprintf(stderr, L"attrib: %s: ambiguous path\n", argv[i]);
				result = 1;
			}
			else
			{
				hfsvolent volent;

				if (hfs_stat(vol, fargv[0], &ent) == -1 ||
						hfs_vstat(vol, &volent) == -1)
				{
					hfsutil_perrorp(fargv[0]);
					result = 1;
				}
				else
				{
					volent.blessed = ent.cnid;

					if (hfs_vsetattr(vol, &volent) == -1)
					{
						hfsutil_perrorp(fargv[0]);
						result = 1;
					}
				}
			}
		}
		else
		{
			for (i = 0; i < fargc; ++i)
			{
				if (hfs_stat(vol, fargv[i], &ent) == -1)
				{
					hfsutil_perrorp(fargv[i]);
					result = 1;
				}
				else
				{
					if (! (ent.flags & HFS_ISDIR))
					{
						if (type)
							memcpy(ent.u.file.type, type, 4);
						if (crea)
							memcpy(ent.u.file.creator, crea, 4);
					}

					if (invis < 0)
						ent.fdflags &= ~HFS_FNDR_ISINVISIBLE;
					else if (invis > 0)
						ent.fdflags |= HFS_FNDR_ISINVISIBLE;

					if (lock < 0)
						ent.flags &= ~HFS_ISLOCKED;
					else if (lock > 0)
						ent.flags |= HFS_ISLOCKED;

					if (hfs_setattr(vol, fargv[i], &ent) == -1)
					{
						hfsutil_perrorp(fargv[i]);
						result = 1;
					}
				}
			}
		}
	}

	hfsutil_unmount(vol, &result);

	if (fargv)
		free(fargv);

	return result;
}
