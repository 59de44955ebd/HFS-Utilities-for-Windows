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

# include <fcntl.h>
# include <unistd.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <sys/stat.h>

# include "hfs.h"
# include "data.h"
# include "copyout.h"
# include "charset.h"
# include "binhex.h"
# include "crc.h"

const char *cpo_error = "no error";

#define STDOUT_FILENO 1

# define __ERROR(code, str)	(cpo_error = (str), errno = (code))

# define MACB_BLOCKSZ	128

/* Copy Routines =========================================================== */

/*
 * NAME:	fork->macb()
 * DESCRIPTION:	copy a single fork for MacBinary II
 */
static
int fork_macb(hfsfile *ifile, int ofile, unsigned long size)
{
	char buf[HFS_BLOCKSZ * 4];
	long chunk, bytes;
	unsigned long total = 0;

	while (1)
	{
		chunk = hfs_read(ifile, buf, sizeof(buf));
		if (chunk == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}
		else if (chunk == 0)
			break;

		bytes = write(ofile, buf, chunk);
		if (bytes == -1)
		{
			__ERROR(errno, "error writing data");
			return -1;
		}
		else if (bytes != chunk)
		{
			__ERROR(EIO, "wrote incomplete chunk");
			return -1;
		}

		total += bytes;
	}

	if (total != size)
	{
		__ERROR(EIO, "inconsistent fork length");
		return -1;
	}

	chunk = total % MACB_BLOCKSZ;
	if (chunk)
	{
		memset(buf, 0, MACB_BLOCKSZ);
		bytes = write(ofile, buf, MACB_BLOCKSZ - chunk);
		if (bytes == -1)
		{
			__ERROR(errno, "error writing data");
			return -1;
		}
		else if (bytes != MACB_BLOCKSZ - chunk)
		{
			__ERROR(EIO, "wrong incomplete chunk");
			return -1;
		}
	}

	return 0;
}

/*
 * NAME:	do_macb()
 * DESCRIPTION:	perform copy using MacBinary II translation
 */
static
int do_macb(hfsfile *ifile, int ofile)
{
	hfsdirent ent;
	unsigned char buf[MACB_BLOCKSZ];
	long bytes;

	if (hfs_fstat(ifile, &ent) == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}

	memset(buf, 0, MACB_BLOCKSZ);

	buf[1] = strlen(ent.name);
	strcpy((char *) &buf[2], ent.name);

	memcpy(&buf[65], ent.u.file.type,		4);
	memcpy(&buf[69], ent.u.file.creator, 4);

	buf[73] = ent.fdflags >> 8;

	d_putul(&buf[83], ent.u.file.dsize);
	d_putul(&buf[87], ent.u.file.rsize);

	d_putul(&buf[91], d_mtime(ent.crdate));
	d_putul(&buf[95], d_mtime(ent.mddate));

	buf[101] = ent.fdflags & 0xff;
	buf[122] = buf[123] = 129;

	d_putuw(&buf[124], crc_macb(buf, 124, 0x0000));

	bytes = write(ofile, buf, MACB_BLOCKSZ);
	if (bytes == -1)
	{
		__ERROR(errno, "error writing data");
		return -1;
	}
	else if (bytes != MACB_BLOCKSZ)
	{
		__ERROR(EIO, "wrote incomplete chunk");
		return -1;
	}

	if (hfs_setfork(ifile, 0) == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}

	if (fork_macb(ifile, ofile, ent.u.file.dsize) == -1)
		return -1;

	if (hfs_setfork(ifile, 1) == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}

	if (fork_macb(ifile, ofile, ent.u.file.rsize) == -1)
		return -1;

	return 0;
}

/*
 * NAME:	fork->binh()
 * DESCRIPTION:	copy a single fork for BinHex
 */
static
int fork_binh(hfsfile *ifile, unsigned long size)
{
	char buf[HFS_BLOCKSZ * 4];
	long bytes;
	unsigned long total = 0;

	while (1)
	{
		bytes = hfs_read(ifile, buf, sizeof(buf));
		if (bytes == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}
		else if (bytes == 0)
			break;

		if (bh_insert(buf, bytes) == -1)
		{
			__ERROR(errno, bh_error);
			return -1;
		}

		total += bytes;
	}

	if (total != size)
	{
		__ERROR(EIO, "inconsistent fork length");
		return -1;
	}

	if (bh_insertcrc() == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	return 0;
}

/*
 * NAME:	binhx()
 * DESCRIPTION:	auxiliary BinHex routine
 */
static
int binhx(hfsfile *ifile)
{
	hfsdirent ent;
	unsigned char byte, word[2], lword[4];

	if (hfs_fstat(ifile, &ent) == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}

	byte = strlen(ent.name);
	if (bh_insert(&byte, 1) == -1 ||
			bh_insert(ent.name, byte + 1) == -1 ||
			bh_insert(ent.u.file.type, 4) == -1 ||
			bh_insert(ent.u.file.creator, 4) == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	d_putsw(word, ent.fdflags);
	if (bh_insert(word, 2) == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	d_putul(lword, ent.u.file.dsize);
	if (bh_insert(lword, 4) == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	d_putul(lword, ent.u.file.rsize);
	if (bh_insert(lword, 4) == -1 || bh_insertcrc() == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	if (hfs_setfork(ifile, 0) == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}

	if (fork_binh(ifile, ent.u.file.dsize) == -1)
		return -1;

	if (hfs_setfork(ifile, 1) == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}

	if (fork_binh(ifile, ent.u.file.rsize) == -1)
		return -1;

	return 0;
}

/*
 * NAME:	do_binh()
 * DESCRIPTION:	perform copy using BinHex translation
 */
static
int do_binh(hfsfile *ifile, int ofile)
{
	int result;

	if (bh_start(ofile) == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	result = binhx(ifile);

	if (bh_end() == -1 && result == 0)
	{
		__ERROR(errno, bh_error);
		result = -1;
	}

	return result;
}

/*
 * NAME:	do_text()
 * DESCRIPTION:	perform copy using text translation
 */
static
int do_text(hfsfile *ifile, int ofile)
{
	char buf[HFS_BLOCKSZ * 4], *ptr;
	long chunk_size, bytes;
	int len;

	while (1)
	{
		chunk_size = hfs_read(ifile, buf, sizeof(buf));
		if (chunk_size == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}
		else if (chunk_size == 0)
			break;

		// CR to LF
		for (ptr = buf; ptr < buf + chunk_size; ++ptr)
		{
			if (*ptr == '\r')
				*ptr = '\n';
		}

		// NEW: MacRoman to UTF-8
		ptr = macRomanToUtf8Chunk(buf, chunk_size, &len);
		if (ptr == 0)
		{
			__ERROR(ENOMEM, 0);
			return -1;
		}
		bytes = write(ofile, ptr, len);
		free(ptr);

		if (bytes == -1)
		{
			__ERROR(errno, "error writing data");
			return -1;
		}
		else if (bytes != len)
		{
			__ERROR(EIO, "wrote incomplete chunk");
			return -1;
		}
	}

	return 0;
}

/*
 * NAME:	do_raw()
 * DESCRIPTION:	perform copy using no translation
 */
static
int do_raw(hfsfile *ifile, int ofile)
{
	char buf[HFS_BLOCKSZ * 4];
	long chunk_size, bytes;

	while (1)
	{
		chunk_size = hfs_read(ifile, buf, sizeof(buf));
		if (chunk_size == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}
		else if (chunk_size == 0)
			break;

		bytes = write(ofile, buf, chunk_size);
		if (bytes == -1)
		{
			__ERROR(errno, "error writing data");
			return -1;
		}
		else if (bytes != chunk_size)
		{
			__ERROR(EIO, "wrote incomplete chunk");
			return -1;
		}
	}
	return 0;
}

/* Utility Routines ======================================================== */

/*
 * NAME:	opensrc()
 * DESCRIPTION:	open the source file; set hint for destination filename
 */
static
hfsfile *opensrc(hfsvol *vol, const char *srcname,
		 const char **dsthint, const char *ext)
{
	hfsfile *file;
	hfsdirent ent;
	static char name[HFS_MAX_FLEN + 4 + 1];
	char *ptr;

	file = hfs_open(vol, srcname);
	if (file == 0)
	{
		__ERROR(errno, hfs_error);
		return 0;
	}

	if (hfs_fstat(file, &ent) == -1)
	{
		__ERROR(errno, hfs_error);

		hfs_close(file);
		return 0;
	}

	strcpy(name, ent.name);

	for (ptr = name; *ptr; ++ptr)
	{
		switch (*ptr)
		{
		case '/':
			*ptr = '-';
			break;

		case ' ':
			*ptr = '_';
			break;
		}
	}

	if (ext)
		strcat(name, ext);

	*dsthint = name;

	return file;
}

/*
 * NAME:	opendst()
 * DESCRIPTION:	open the destination file
 */
static
int opendst(const wchar_t *dstname, const wchar_t *hint, BOOL binary)
{
	int fd;

	if (wcscmp(dstname, L"-") == 0)
		fd = dup(STDOUT_FILENO);
	else
	{
		struct _stat64i32 sbuf;
		wchar_t *path = 0;

		if (_wstat(dstname, &sbuf) != -1 && S_ISDIR(sbuf.st_mode))
		{
			path = malloc((wcslen(dstname) + 1 + wcslen(hint) + 1) * sizeof(wchar_t));
			if (path == 0)
			{
				__ERROR(ENOMEM, 0);
				return -1;
			}

			wcscpy(path, dstname);
			wcscat(path, L"\\");
			wcscat(path, hint);

			dstname = path;
		}

		fd = _wopen(dstname, binary ? O_WRONLY | O_CREAT | O_TRUNC | O_BINARY : O_WRONLY | O_CREAT | O_TRUNC, 0666);

		if (path)
			free(path);
	}

	if (fd == -1)
	{
		__ERROR(errno, "error opening destination file");
		return -1;
	}

	return fd;
}

/*
 * NAME:	openfiles()
 * DESCRIPTION:	open source and destination files
 */
static
int openfiles(hfsvol *vol, const char *srcname, const wchar_t *dstname, const char *ext, hfsfile **ifile, int *ofile, BOOL binary)
{
	wchar_t *dsthint = 0;
	const char *dsthint_macroman;

	*ifile = opensrc(vol, srcname, &dsthint_macroman, ext);
	if (*ifile == 0)
		return -1;

	dsthint = macRomanToUtf16(dsthint_macroman);
	if (dsthint == 0)
	{
		hfs_close(*ifile);
		return -1;
	}
	*ofile = opendst(dstname, dsthint, binary);
	free(dsthint);

	if (*ofile == -1)
	{
		hfs_close(*ifile);
		return -1;
	}

	return 0;
}

/*
 * NAME:	closefiles()
 * DESCRIPTION:	close source and destination files
 */
static
void closefiles(hfsfile *ifile, int ofile, int *result)
{
	if (close(ofile) == -1 && *result == 0)
	{
		__ERROR(errno, "error closing destination file");
		*result = -1;
	}

	if (hfs_close(ifile) == -1 && *result == 0)
	{
		__ERROR(errno, hfs_error);
		*result = -1;
	}
}

/* Interface Routines ====================================================== */

/*
 * NAME:	cpo->macb()
 * DESCRIPTION:	copy an HFS file to a UNIX file using MacBinary II translation
 */
int cpo_macb(hfsvol *vol, const char *srcname, const wchar_t *dstname)
{
	hfsfile *ifile;
	int ofile, result = 0;

	if (openfiles(vol, srcname, dstname, ".bin", &ifile, &ofile, 1) == -1)
		return -1;

	result = do_macb(ifile, ofile);

	closefiles(ifile, ofile, &result);

	return result;
}

/*
 * NAME:	cpo->binh()
 * DESCRIPTION:	copy an HFS file to a UNIX file using BinHex translation
 */
int cpo_binh(hfsvol *vol, const char *srcname, const wchar_t *dstname)
{
	hfsfile *ifile;
	int ofile, result;

	if (openfiles(vol, srcname, dstname, ".hqx", &ifile, &ofile, 0) == -1)
		return -1;

	result = do_binh(ifile, ofile);

	closefiles(ifile, ofile, &result);

	return result;
}

/*
 * NAME:	cpo->text()
 * DESCRIPTION:	copy an HFS file to a UNIX file using text translation
 */
int cpo_text(hfsvol *vol, const char *srcname, const wchar_t *dstname)
{
	const char *ext = 0;
	hfsfile *ifile;
	int ofile, result = 0;

	if (strchr(srcname, '.') == 0)
		ext = ".txt";

	if (openfiles(vol, srcname, dstname, ext, &ifile, &ofile, 0) == -1)
		return -1;

	result = do_text(ifile, ofile);

	closefiles(ifile, ofile, &result);

	return result;
}

/*
 * NAME:	cpo->raw()
 * DESCRIPTION:	copy the data fork of an HFS file to a UNIX file
 */
int cpo_raw(hfsvol *vol, const char *srcname, const wchar_t *dstname)
{
	hfsfile *ifile;
	int ofile, result = 0;

	if (openfiles(vol, srcname, dstname, 0, &ifile, &ofile, 1) == -1)
		return -1;

	result = do_raw(ifile, ofile);

	closefiles(ifile, ofile, &result);

	return result;
}
