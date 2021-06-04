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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <ctype.h>
# include <errno.h>
# include <sys/stat.h>
# include <unistd.h>
# include <locale.h>

# include "hfs.h"
# include "hcwd.h"
# include "hfsutil.h"
# include "suid.h"
# include "glob.h"
# include "version.h"

# include "hattrib.h"
# include "hcd.h"
# include "hcopy.h"
# include "hdel.h"
# include "hformat.h"
# include "hls.h"
# include "hmkdir.h"
# include "hmount.h"
# include "hpwd.h"
# include "hrename.h"
# include "hrmdir.h"
# include "humount.h"
# include "hvol.h"
# include "charset.h"

const wchar_t *argv0, *bargv0;

/*
 * NAME:	main()
 * DESCRIPTION:	program entry dispatch
 */
int wmain(int argc, wchar_t *argv[])
{
	int i, len;
	const wchar_t *dot;

	struct
	{
		const wchar_t *name;
		int(*func)(int, wchar_t *[]);
	} list[] = {
		{ L"attrib", hattrib_main },
		{ L"cd",     hcd_main     },
		{ L"copy",   hcopy_main   },
		{ L"del",    hdel_main    },
		{ L"dir",    hls_main     },
		{ L"format", hformat_main },
		{ L"ls",     hls_main     },
		{ L"mkdir",  hmkdir_main  },
		{ L"mount",  hmount_main  },
		{ L"pwd",    hpwd_main    },
		{ L"rename", hrename_main },
		{ L"rmdir",  hrmdir_main  },
		{ L"umount", humount_main },
		{ L"vol",    hvol_main    },
		{ 0,         0            }
	};

	suid_init();

	// force UTF-8
	setlocale(LC_CTYPE, "UTF-8");
	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);

	if (argc < 2)
	{
		wprintf(L"ERROR: no operation\n");
		return 0;
	}

	if (argc == 2)
	{
		if (wcscmp(argv[1], L"--version") == 0)
		{
			//wprintf(L"%hs - %hs\n", hfsutils_version, hfsutils_copyright);
			wprintf(L"%hs\n", hfsutils_version);
			wprintf(L"\"%s --license\" for licensing information.\n", argv[0]);
			return 0;
		}
		else if (wcscmp(argv[1], L"--license") == 0)
		{
			wprintf(L"\n%hs", hfsutils_license);
			return 0;
		}
	}

	argv0 = argv[0];
	bargv0 = argv[1];

	for (i = 0; list[i].name; ++i)
	{
		//if (wcsncmp(bargv0, list[i].name, len) == 0)
		if (wcscmp(bargv0, list[i].name) == 0)
		{
			int result;

			bargv0 = list[i].name;

			if (hcwd_init() == -1)
			{
			  _wperror(L"Failed to initialize HFS working directories");
			  return 1;
			}

			result = list[i].func(argc, argv);

			if (hcwd_finish() == -1)
			{
			  _wperror(L"Failed to save working directory state");
			  return 1;
			}

			return result;
		}
	}

	fwprintf(stderr, L"Unknown operation: %s\n", bargv0);
	return 1;
}

/*
 * NAME:	hfsutil->perror()
 * DESCRIPTION:	output an HFS error
 */
void hfsutil_perror(const char *msg)
{
	const char *str = strerror(errno);
	if (hfs_error == 0)
		fwprintf(stderr, L"%s: %hs: %c%hs\n", argv0, msg, tolower(*str), str + 1);
	else
		fwprintf(stderr, L"%s: %hs: %hs (%hs)\n", argv0, msg, hfs_error, str);
}

/*
 * NAME:	hfsutil->perror()
 * DESCRIPTION:	output an HFS error
 */
void hfsutil_perror_w(const wchar_t *msg)
{
	const char *str = strerror(errno);
	if (hfs_error == 0)
		fwprintf(stderr, L"%s: %s: %c%hs\n", argv0, msg, tolower(*str), str + 1);
	else
		fwprintf(stderr, L"%s: %s: %hs (%hs)\n", argv0, msg, hfs_error, str);
}

/*
 * NAME:	hfsutil->perrorp()
 * DESCRIPTION:	output an HFS error for a pathname
 */
void hfsutil_perrorp(const char *path)
{
	const char *str = strerror(errno);
	if (hfs_error == 0)
		fwprintf(stderr, L"%s: \"%hs\": %c%hs\n", argv0, path, tolower(*str), str + 1);
	else
		fwprintf(stderr, L"%s: \"%hs\": %hs (%hs)\n", argv0, path, hfs_error, str);
}

/*
 * NAME:	hfsutil->perrorp()
 * DESCRIPTION:	output an HFS error for a pathname
 */
void hfsutil_perrorp_w(const wchar_t *path)
{
	const char *str = strerror(errno);
	if (hfs_error == 0)
		fwprintf(stderr, L"%s: \"%s\": %c%hs\n", bargv0, path, tolower(*str), str + 1);
	else
		fwprintf(stderr, L"%s: \"%s\": %hs (%hs)\n", bargv0, path, hfs_error, str);
}

/*
 * NAME:	hfsutil->remount()
 * DESCRIPTION:	mount a volume as though it were still mounted
 */
hfsvol *hfsutil_remount(mountent *ment, int flags)
{
	hfsvol *vol;
	hfsvolent vent;
	char *macroman;

	if (ment == 0)
	{
		fwprintf(stderr, L"%s: No volume is current; use `hmount' or `hvol'\n", bargv0);
		return 0;
	}

	suid_enable();
	vol = hfs_mount(ment->path, ment->partno, flags);
	suid_disable();

	if (vol == 0)
	{
		hfsutil_perror_w(ment->path);
		return 0;
	}

	hfs_vstat(vol, &vent);

	macroman = utf16ToMacRoman(ment->vname);
	if (macroman == 0)
	{
		fwprintf(stderr, L"%s: not enough memory\n", bargv0);
		return 0;
	}
	if (strcmp(vent.name, macroman) != 0)
	{
		fwprintf(stderr, L"%s: Expected volume \"%s\" not found\n", bargv0, ment->vname);
		fwprintf(stderr, L"%s: Replace media on %s or use `hmount'\n", bargv0, ment->path);

		hfs_umount(vol);
		free(macroman);
		return 0;
	}

	free(macroman);
	macroman = utf16ToMacRoman(ment->cwd);
	if (macroman == 0)
	{
		fwprintf(stderr, L"%s: not enough memory\n", bargv0);
		return 0;
	}
	if (hfs_chdir(vol, macroman) == -1)
	{
			fwprintf(stderr, L"%s: Current HFS directory \"%s%s:\" no longer exists\n",
				bargv0, ment->vname, ment->cwd);
	}
	free(macroman);
	return vol;
}

/*
 * NAME:	hfsutil->unmount()
 * DESCRIPTION:	unmount a volume
 */
void hfsutil_unmount(hfsvol *vol, int *result)
{
	if (hfs_umount(vol) == -1 && *result == 0)
	{
		hfsutil_perror("Error closing HFS volume");
		*result = 1;
	}
}

/*
 * NAME:	hfsutil->pinfo()
 * DESCRIPTION:	print information about a volume
 */
void hfsutil_pinfo(hfsvolent *ent)
{
	wchar_t * name = macRomanToUtf16(ent->name);
	if (name != 0)
	{
		wprintf(L"Volume name is \"%s\"%s\n", name, (ent->flags & HFS_ISLOCKED) ? L" (locked)" : L"");
		wprintf(L"Volume was created on %hs", ctime(&ent->crdate));
		wprintf(L"Volume was last modified on %hs", ctime(&ent->mddate));
		wprintf(L"Volume has %lu bytes free\n", ent->freebytes);
		free(name);
	}
}

/*
 * NAME:	hfsutil->glob()
 * DESCRIPTION:	perform filename globbing
 */
char **hfsutil_glob(hfsvol *vol, int argc, wchar_t *argv[], int *nelts, int *result)
{
	char **fargv;

	fargv = hfs_glob_w(vol, argc, argv, nelts);
	if (fargv == 0 && *result == 0)
	{
		fwprintf(stderr, L"%s: globbing error\n", bargv0);
		*result = 1;
	}

	return fargv;
}

/*
 * NAME:	hfsutil->getcwd()
 * DESCRIPTION:	return full path to current directory (must be free()'d)
 */
char *hfsutil_getcwd(hfsvol *vol)
{
	char *path, name[HFS_MAX_FLEN + 1 + 1];
	long cwd;
	int pathlen;

	path    = malloc(1);
	path[0] = 0;
	pathlen = 0;
	cwd     = hfs_getcwd(vol);

	while (cwd != HFS_CNID_ROOTPAR)
	{
		char *new;
			int namelen, i;

		if (hfs_dirinfo(vol, &cwd, name) == -1)
			return 0;

		if (pathlen)
			strcat(name, ":");

		namelen = strlen(name);

		new = realloc(path, namelen + pathlen + 1);
		if (new == 0)
		{
			free(path);
			__ERROR(ENOMEM, 0);
			return 0;
		}

		if (pathlen == 0)
			new[0] = 0;

		path = new;

		/* push string down to make room for path prefix (memmove()-ish) */

		i = pathlen + 1;
		for (new = path + namelen + pathlen; i--; new--)
			*new = *(new - namelen);

		memcpy(path, name, namelen);

		pathlen += namelen;
	}

	return path;
}

/*
 * NAME:	hfsutil->samepath()
 * DESCRIPTION:	return 1 iff paths refer to same object
 */
int hfsutil_samepath(const char *path1, const char *path2)
{
	struct stat sbuf1, sbuf2;

	return
	stat(path1, &sbuf1) == 0 &&
	stat(path2, &sbuf2) == 0 &&
	sbuf1.st_dev == sbuf2.st_dev &&
	sbuf1.st_ino == sbuf2.st_ino;
}

int hfsutil_samepath_w(const wchar_t *path1, const wchar_t *path2)
{
	struct _stat64i32 sbuf1, sbuf2;

	return
		_wstat(path1, &sbuf1) == 0 &&
		_wstat(path2, &sbuf2) == 0 &&
		sbuf1.st_dev == sbuf2.st_dev &&
		sbuf1.st_ino == sbuf2.st_ino;
}
