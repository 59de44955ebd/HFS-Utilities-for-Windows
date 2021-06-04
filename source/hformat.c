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
# include <stdlib.h>
# include <string.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "suid.h"
# include "hformat.h"
# include "charset.h"
# include "getopt.h"

# define O_FORCE	0x01

//extern char *optarg;
extern int optind;

/*
 * NAME:	usage()
 * DESCRIPTION:	display usage message
 */
static
void usage(void)
{
	fwprintf(stderr, L"Usage: format [-f] [-l label] path [partition-no]\n");
}

/*
 * NAME:	do_format()
 * DESCRIPTION:	call hfs_format() with necessary privileges
 */
static
hfsvol *do_format(const wchar_t *path, int partno, int mode, const char *vname)
{
	hfsvol *vol = 0;

	suid_enable();

	if (hfs_format(path, partno, mode, vname, 0, 0) != -1)
		vol = hfs_mount(path, partno, HFS_MODE_ANY);

	suid_disable();

	return vol;
}

/*
 * NAME:	hformat->main()
 * DESCRIPTION:	implement hformat command
 */
int hformat_main(int argc, wchar_t *argv[])
{
	const char *vname;
	char *vname_macroman = 0;
	wchar_t *path = 0;
	wchar_t *name;
	hfsvol *vol;
	hfsvolent ent;
	int nparts, partno, options = 0, result = 0;

	vname = "Untitled";

	optind = 2;

	while (1)
	{
		int opt;

		opt = getopt(argc, argv, L"fl:?");
		if (opt == EOF)
			break;

		switch (opt)
		{
		case '?':
			usage();
			goto fail;

		case 'f':
			options |= O_FORCE;
			break;

		case 'l':
			vname_macroman = utf16ToMacRoman(optarg);
			if (vname_macroman == 0)
			{
				fwprintf(stderr, L"format: not enough memory\n");
				goto fail;
			}
			vname = vname_macroman;
			break;
		}
	}

	if (argc - optind < 1 || argc - optind > 2)
	{
		usage();
		goto fail;
	}

	//path = hfsutil_abspath(argv[optind]);
	path = _wfullpath(NULL, argv[optind], _MAX_PATH);

	if (path == 0)
	{
		fwprintf(stderr, L"format: not enough memory\n");
		goto fail;
	}

	suid_enable();
	nparts = hfs_nparts(path);
	suid_disable();

	//hfs format [-l <label>] <destination-path> [<partition-no>]
	if (argc - optind == 2)
	{
		partno = _wtoi(argv[optind + 1]);

		if (nparts != -1 && partno == 0)
		{
			if (options & O_FORCE)
			{
				fwprintf(stderr, L"format: warning: erasing partition information\n");
			}
			else
			{
				fwprintf(stderr, L"format: medium is partitioned; "
					L"select partition > 0 or use -f\n");
				goto fail;
			}
		}
	}
	else
	{
		if (nparts > 1)
		{
			fwprintf(stderr, L"format: must specify partition number (%d available)\n", nparts);
			goto fail;
		}
		else if (nparts == -1)
			partno = 0;
		else
			partno = 1;
	}

	vol = do_format(path, partno, 0, vname);
	if (vol == 0)
	{
		hfsutil_perror_w(path);
		goto fail;
	}

	hfs_vstat(vol, &ent);
	hfsutil_pinfo(&ent);

	name = macRomanToUtf16(ent.name);
	if (name == 0)
	{
		fwprintf(stderr, L"format: not enough memory\n");
		result = 1;
	}
	if (hcwd_mounted(name, ent.crdate, path, partno) == -1)
	{
		_wperror(L"Failed to record mount");
		result = 1;
	}
	free(name);

	hfsutil_unmount(vol, &result);

	if (vname_macroman)
		free(vname_macroman);
	if (path)
		free(path);

	return result;

fail:
	if (vname_macroman)
		free(vname_macroman);
	if (path)
		free(path);

	return 1;
}
