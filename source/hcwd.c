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
# include <errno.h>

# include "hfs.h"
# include "hcwd.h"

# define STATEFNAME	L".hcwd"
# define TABCHUNKSZ	8

static FILE *statef = 0;
static mountent *mounts = 0;
static int mtabsz = 0, nmounts = 0;
static int curvol = -1, dirty = 0;

//extern int errno;

/*
 * NAME:	addent()
 * DESCRIPTION:	insert mount entry into table
 */
static
int addent(mountent *ent)
{
	if (nmounts >= mtabsz)
	{
		mountent *newmem;

		mtabsz += TABCHUNKSZ;
		newmem = mounts ? realloc(mounts, mtabsz * sizeof(mountent)) : malloc(mtabsz * sizeof(mountent));
		if (newmem == 0)
			return -1;
		mounts = newmem;
	}

	mounts[nmounts++] = *ent;

	dirty = 1;

	return 0;
}

/*
 * NAME:	hcwd->init()
 * DESCRIPTION:	read state file
 */
int hcwd_init(void)
{
	const wchar_t *home, *start;
	wchar_t buf[512], *path, *ptr;
	mountent entry;
	int newcur = -1;

	home = _wgetenv(L"USERPROFILE");
	if (home == 0)
		home = L"";

	path = malloc(wcslen(home)*sizeof(wchar_t) + sizeof(STATEFNAME) + 2);
	if (path == 0)
		return -1;

	wcscpy(path, home);
	wcscat(path, L"\\" STATEFNAME);

	statef = _wfopen(path, L"r+, ccs=UTF-8");
	if (statef == 0 && errno == ENOENT)
	{
		statef = _wfopen(path, L"w+, ccs=UTF-8");
	}

	free(path);

	if (statef == 0)
		return -1;

	if (fgetws(buf, 512, statef))
		newcur = _wtoi(buf);

	while (fgetws(buf, 512, statef))
	{
		start = ptr = buf;

		while (*ptr && *ptr != L'\n' && *ptr != L'\t')
			++ptr;

		entry.vname[0] = L'\0';
		if (*ptr == L'\t')
		{
			*ptr++ = 0;
			wcscpy(entry.vname, start);
			start = ptr;
		}

		while (*ptr && *ptr != L'\n' && *ptr != L'\t')
			++ptr;

		if (*ptr == L'\t')
		{
			*ptr++ = 0;
			entry.vcrdate = wcstol(start, 0, 0);
			start = ptr;
		}

		while (*ptr && *ptr != '\n' && *ptr != '\t')
			++ptr;

		if (*ptr == '\t')
		{
			*ptr++ = 0;
			entry.path = wcsdup(start);
			if (entry.path == 0)
				return -1;

			start = ptr;
		}

		while (*ptr && *ptr != '\n' && *ptr != '\t')
			++ptr;

		if (*ptr == '\t')
		{
			*ptr++ = 0;
			entry.partno = _wtoi(start);
			start = ptr;
		}

		while (*ptr && *ptr != L'\n' && *ptr != L'\t')
			++ptr;

		if (*ptr == L'\n' || *ptr == L'\t')
			*ptr = 0;

		if (*start)
		{
			entry.cwd = wcsdup(start);
			if (entry.cwd == 0)
				return -1;

			if (addent(&entry) == -1)
				return -1;
		}
	}

	curvol = (newcur >= 0 && newcur < nmounts) ? newcur : -1;

	return 0;
}

/*
 * NAME:	hcwd->finish()
 * DESCRIPTION:	flush changes to state file
 */
int hcwd_finish(void)
{
	if (statef && mounts && dirty)
	{
		int i;

		rewind(statef);
		if (ftruncate(fileno(statef), 0) == -1)
			return -1;

		fwprintf(statef, L"%d\n", curvol);

		for (i = 0; i < nmounts; ++i)
			fwprintf(statef, L"%s\t%ld\t%s\t%d\t%s\n",
				mounts[i].vname,
				mounts[i].vcrdate,
				mounts[i].path,
				mounts[i].partno,
				mounts[i].cwd);

		dirty = 0;
	}

	if (mounts)
	{
		mountent *ent;

		for (ent = mounts; ent < mounts + nmounts; ++ent)
		{
			if (ent->path)
				free(ent->path);
			if (ent->cwd)
				free(ent->cwd);
		}

		free(mounts);
	}

	if (statef && fclose(statef) == EOF)
		return -1;

	return 0;
}

/*
 * NAME:	hcwd->mounted()
 * DESCRIPTION:	register a mounted volume
 */
//int hcwd_mounted(const char *vname, long vcrdate, const char *path, int partno)
int hcwd_mounted(const wchar_t *vname, long vcrdate, const wchar_t *path, int partno)
{
	mountent *entry, new;

	for (entry = mounts; entry < mounts + nmounts; ++entry)
	{
		if (wcscmp(entry->path, path) == 0 && entry->partno == partno)
		{
			/* update entry */

			if (entry->cwd)
				free(entry->cwd);

			wcscpy(entry->vname, vname);
			entry->vcrdate = vcrdate;
			entry->cwd = wcsdup(L":");
			if (entry->cwd == 0)
				return -1;

			curvol = entry - mounts;
			dirty	= 1;

			return 0;
		}
	}

	wcscpy(new.vname, vname);
	new.vcrdate = vcrdate;
	new.path = wcsdup(path);
	new.partno = partno;
	new.cwd = wcsdup(L":");

	if (new.path == 0 || new.cwd	 == 0)
		return -1;

	if (addent(&new) == -1)
		return -1;

	curvol = nmounts - 1;

	return 0;
}

/*
 * NAME:	hcwd->umounted()
 * DESCRIPTION:	unregister a previously mounted volume
 */
int hcwd_umounted(int vol)
{
	mountent *entry;
	int i;

	if (vol < 0)
		vol = curvol;

	if (vol < 0 || vol >= nmounts)
		return -1;

	entry = &mounts[vol];

	if (entry->path)
		free(entry->path);
	if (entry->cwd)
		free(entry->cwd);

	for (i = vol + 1; i < nmounts; ++i)
		mounts[i - 1] = mounts[i];

	--nmounts;

	if (curvol > vol)
		--curvol;
	else if (curvol == vol)
		curvol = -1;

	return 0;
}

/*
 * NAME:	hcwd->getvol()
 * DESCRIPTION:	return a mount entity
 */
mountent *hcwd_getvol(int vol)
{
	if (vol < 0)
		vol = curvol;

	if (vol < 0 || vol >= nmounts)
		return 0;

	return &mounts[vol];
}

/*
 * NAME:	hcwd->setvol()
 * DESCRIPTION:	change the current volume
 */
int hcwd_setvol(int vol)
{
	if (vol < 0 || vol >= nmounts)
		return -1;

	if (curvol != vol)
	{
		curvol = vol;
		dirty	= 1;
	}

	return 0;
}

/*
 * NAME:	hcwd->setcwd()
 * DESCRIPTION:	change the current working directory for a mount entity
 */
int hcwd_setcwd(mountent *ent, const wchar_t *newcwd)
{
	const wchar_t *path;
	wchar_t *cwd;

	path = newcwd;
	while (*path && *path != L':')
		++path;

	memcpy(ent->vname, newcwd, path - newcwd);
	ent->vname[path - newcwd] = 0;

	cwd = (*path == 0) ? wcsdup(L":") : wcsdup(path);
	if (cwd == 0)
		return -1;

	if (ent->cwd)
		free(ent->cwd);

	ent->cwd = cwd;
	dirty		= 1;

	return 0;
}
