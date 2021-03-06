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

# include "hfs.h"
# include "data.h"
# include "copyin.h"
# include "charset.h"
# include "binhex.h"
# include "crc.h"

const char *cpi_error = "no error";

//extern int errno;

# define __ERROR(code, str)	(cpi_error = (str), errno = (code))

# define MACB_BLOCKSZ	128

# define TEXT_TYPE	"TEXT"
# define TEXT_CREA	"UNIX"

# define RAW_TYPE	"????"
# define RAW_CREA	"UNIX"

/* Copy routines =========================================================== */

/*
 * NAME:	fork->macb()
 * DESCRIPTION:	copy a single fork for MacBinary II
 */
static
int fork_macb(int ifile, hfsfile *ofile, unsigned long size)
{
	char buf[HFS_BLOCKSZ * 4];
	unsigned long chunk, bytes;

	while (size)
		{
			chunk = (size < sizeof(buf)) ?
	(size + (MACB_BLOCKSZ - 1)) & ~(MACB_BLOCKSZ - 1) : sizeof(buf);

			bytes = read(ifile, buf, chunk);
			if (bytes == (unsigned long) -1)
	{
		__ERROR(errno, "error reading data");
		return -1;
	}
			else if (bytes != chunk)
	{
		__ERROR(EIO, "read incomplete chunk");
		return -1;
	}

			chunk = (size > bytes) ? bytes : size;

			bytes = hfs_write(ofile, buf, chunk);
			if (bytes == (unsigned long) -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}
			else if (bytes != chunk)
	{
		__ERROR(EIO, "wrote incomplete chunk");
		return -1;
	}

			size -= chunk;
		}

	return 0;
}

/*
 * NAME:	do_macb()
 * DESCRIPTION:	perform copy using MacBinary II translation
 */
static
int do_macb(int ifile, hfsfile *ofile,
			unsigned long dsize, unsigned long rsize)
{
	if (hfs_setfork(ofile, 0) == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}

	if (fork_macb(ifile, ofile, dsize) == -1)
		return -1;

	if (hfs_setfork(ofile, 1) == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}

	if (fork_macb(ifile, ofile, rsize) == -1)
		return -1;

	return 0;
}

/*
 * NAME:	fork->binh()
 * DESCRIPTION:	copy a single fork for BinHex
 */
static
int fork_binh(hfsfile *ofile, unsigned long size)
{
	char buf[HFS_BLOCKSZ * 4];
	long chunk, bytes;

	while (size)
		{
			chunk = (size > sizeof(buf)) ? sizeof(buf) : size;

			bytes = bh_read(buf, chunk);
			if (bytes == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}
			else if (bytes != chunk)
	{
		__ERROR(EIO, "read incomplete chunk");
		return -1;
	}

			bytes = hfs_write(ofile, buf, chunk);
			if (bytes == -1)
	{
		__ERROR(errno, hfs_error);
		return -1;
	}
			else if (bytes != chunk)
	{
		__ERROR(EIO, "wrote incomplete chunk");
		return -1;
	}

			size -= chunk;
		}

	if (bh_readcrc() == -1)
		{
			__ERROR(errno, bh_error);
			return -1;
		}

	return 0;
}

/*
 * NAME:	do_binh()
 * DESCRIPTION:	perform copy using BinHex translation
 */
static
int do_binh(hfsfile *ofile, unsigned long dsize, unsigned long rsize)
{
	if (hfs_setfork(ofile, 0) == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}

	if (fork_binh(ofile, dsize) == -1)
		return -1;

	if (hfs_setfork(ofile, 1) == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}

	if (fork_binh(ofile, rsize) == -1)
		return -1;

	return 0;
}

/*
 * NAME:	do_text()
 * DESCRIPTION:	perform copy using text translation
 */
static
int do_text(int ifile, hfsfile *ofile)
{
	char buf[HFS_BLOCKSZ * 4], *ptr;
	long chunk_size, bytes;
	int len;

	while (1)
	{
		chunk_size = read(ifile, buf, sizeof(buf));
		if (chunk_size == -1)
		{
			__ERROR(errno, "error reading source file");
			return -1;
		}
		else if (chunk_size == 0)
			break;

		// LF to CR
		for (ptr = buf; ptr < buf + chunk_size; ++ptr)
		{
			if (*ptr == '\n')
				*ptr = '\r';
		}

		// NEW: UTF-8 to MacRoman
		ptr = utf8ToMacRomanChunk(buf, chunk_size, &len);
		if (ptr == 0)
		{
			__ERROR(ENOMEM, 0);
			return -1;
		}
		bytes = hfs_write(ofile, ptr, len);
		free(ptr);

		if (bytes == -1)
		{
			__ERROR(errno, hfs_error);
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
int do_raw(int ifile, hfsfile *ofile)
{
	char buf[HFS_BLOCKSZ * 4];
	long chunk, bytes;

	while (1)
	{
		chunk = read(ifile, buf, sizeof(buf));

		if (chunk == -1)
		{
			__ERROR(errno, "error reading source file");
			return -1;
		}
		else if (chunk == 0)
			break;

		bytes = hfs_write(ofile, buf, chunk);

		if (bytes == -1)
		{
			__ERROR(errno, hfs_error);
			return -1;
		}
		else if (bytes != chunk)
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
int opensrc(const wchar_t *srcname, const wchar_t **dsthint, const char *ext, BOOL binary)
{
	int fd, len;
	static char name[HFS_MAX_FLEN + 1];
	const char *cptr;
	char *ptr;
	char * srcname_mac;

	if (wcscmp(srcname, L"-") == 0)
	{
		fd = dup(STDIN_FILENO);
		srcname = L"";
	}
	else
		fd = _wopen(srcname, binary ? O_RDONLY | O_BINARY : O_RDONLY);

	if (fd == -1)
	{
		__ERROR(errno, "error opening source file");
		return -1;
	}

	srcname_mac = utf16ToMacRoman(srcname);
	if (srcname_mac == 0)
	{
		close(fd);
		__ERROR(ENOMEM, 0);
		return -1;
	}
	cptr = strrchr(srcname_mac, L'\\'); // WIN
	if (cptr == 0)
		cptr = srcname_mac;
	else
		++cptr;

	if (ext == 0)
		len = strlen(cptr);
	else
	{
		//Returns a pointer to the first occurrence of str2 in str1, or a null pointer if str2 is not part of str1.
		ext = strstr(cptr, ext);
		if (ext == 0)
			len = strlen(cptr);
		else
			len = ext - cptr;
	}

	if (len > HFS_MAX_FLEN)
		len = HFS_MAX_FLEN;

	memcpy(name, cptr, len);
	name[len] = 0;

	//for (ptr = name; *ptr; ++ptr)
	//{
	//	switch (*ptr)
	//	{
	//		case ':':
	//			*ptr = '-';
	//			break;

	//		case '_':
	//			*ptr = ' ';
	//			break;
	//	}
	//}

	free(srcname_mac);

	*dsthint = macRomanToUtf16(name);

	return fd;
}

/*
 * NAME:	opendst()
 * DESCRIPTION:	open the destination file
 */
static
hfsfile *opendst(hfsvol *vol, const char *dstname, const char *hint,
		 const char *type, const char *creator)
{
	hfsdirent ent;
	hfsfile *file;
	unsigned long cwd;

	if (hfs_stat(vol, dstname, &ent) != -1 && (ent.flags & HFS_ISDIR))
	{
		cwd = hfs_getcwd(vol);

		if (hfs_setcwd(vol, ent.cnid) == -1)
		{
			__ERROR(errno, hfs_error);
			return 0;
		}

		dstname = hint;
	}

	hfs_delete(vol, dstname);

	file = hfs_create(vol, dstname, type, creator);
	if (file == 0)
	{
		__ERROR(errno, hfs_error);

		if (dstname == hint)
			hfs_setcwd(vol, cwd);

		return 0;
	}

	if (dstname == hint)
	{
		if (hfs_setcwd(vol, cwd) == -1)
		{
			__ERROR(errno, hfs_error);

			hfs_close(file);
			return 0;
		}
	}

	return file;
}

/*
 * NAME:	closefiles()
 * DESCRIPTION:	close source and destination files
 */
static
void closefiles(int ifile, hfsfile *ofile, int *result)
{
	if (ofile && hfs_close(ofile) == -1 && *result == 0)
	{
		__ERROR(errno, hfs_error);
		*result = -1;
	}

	if (close(ifile) == -1 && *result == 0)
	{
		__ERROR(errno, "error closing source file");
		*result = -1;
	}
}

/* Interface Routines ====================================================== */

/*
 * NAME:	cpi->macb()
 * DESCRIPTION:	copy a UNIX file to an HFS file using MacBinary II translation
 */
int cpi_macb(const wchar_t *srcname, hfsvol *vol, const char *dstname)
{
	int ifile, result = 0;
	hfsfile *ofile;
	hfsdirent ent;
	const wchar_t *dsthint;
	const char *dsthint_macroman;
	char type[5], creator[5];
	unsigned char buf[MACB_BLOCKSZ];
	unsigned short crc;
	unsigned long dsize, rsize;

	ifile = opensrc(srcname, &dsthint, ".bin", 1);
	if (ifile == -1)
		return -1;

	if (read(ifile, buf, MACB_BLOCKSZ) < MACB_BLOCKSZ)
	{
		__ERROR(errno, "error reading MacBinary file header");

		close(ifile);
		return -1;
	}

	if (buf[0] != 0 || buf[74] != 0)
	{
		__ERROR(EINVAL, "invalid MacBinary file header");

		close(ifile);
		return -1;
	}

	crc = d_getuw(&buf[124]);

	if (crc_macb(buf, 124, 0x0000) != crc)
	{
		/* (buf[82] == 0) => MacBinary I? */

		__ERROR(EINVAL, "unknown, unsupported, or corrupt MacBinary file");

		close(ifile);
		return -1;
	}

	if (buf[123] > 129)
	{
		__ERROR(EINVAL, "unsupported MacBinary file version");

		close(ifile);
		return -1;
	}

	if (buf[1] < 1 || buf[1] > 63 ||
			buf[2 + buf[1]] != 0)
	{
		__ERROR(EINVAL, "invalid MacBinary file header (bad file name)");

		close(ifile);
		return -1;
	}

	dsize = d_getul(&buf[83]);
	rsize = d_getul(&buf[87]);

	if (dsize > 0x7fffffff || rsize > 0x7fffffff)
	{
		__ERROR(EINVAL, "invalid MacBinary file header (bad file length)");

		close(ifile);
		return -1;
	}

	dsthint_macroman = (char *) &buf[2];

	memcpy(type, &buf[65], 4);
	memcpy(creator, &buf[69], 4);
	type[4] = creator[4] = 0;

	ofile = opendst(vol, dstname, dsthint_macroman, type, creator);
	if (ofile == 0)
	{
		close(ifile);
		return -1;
	}

	result = do_macb(ifile, ofile, dsize, rsize);

	if (result == 0 && hfs_fstat(ofile, &ent) == -1)
	{
		__ERROR(errno, hfs_error);
		result = -1;
	}

	ent.fdflags = (buf[73] << 8 | buf[101]) &
		~(HFS_FNDR_ISONDESK | HFS_FNDR_HASBEENINITED | HFS_FNDR_RESERVED);

	ent.crdate = d_ltime(d_getul(&buf[91]));
	ent.mddate = d_ltime(d_getul(&buf[95]));

	if (result == 0 && hfs_fsetattr(ofile, &ent) == -1)
	{
		__ERROR(errno, hfs_error);
		result = -1;
	}

	closefiles(ifile, ofile, &result);

	return result;
}

/*
 * NAME:	binhx()
 * DESCRIPTION:	auxiliary BinHex routine
 */
static
int binhx(char *fname, char *type, char *creator, short *fdflags,
		unsigned long *dsize, unsigned long *rsize)
{
	int len;
	unsigned char byte, word[2], lword[4];

	if (bh_read(&byte, 1) < 1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	len = (unsigned char) byte;

	if (len < 1 || len > HFS_MAX_FLEN)
	{
		__ERROR(EINVAL, "invalid BinHex file header (bad file name)");
		return -1;
	}

	if (bh_read(fname, len + 1) < len + 1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	if (fname[len] != 0)
	{
		__ERROR(EINVAL, "invalid BinHex file header (bad file name)");
		return -1;
	}

	if (bh_read(type, 4) < 4 ||
			bh_read(creator, 4) < 4 ||
			bh_read(word, 2) < 2)
	{
		__ERROR(errno, bh_error);
		return -1;
	}
	*fdflags = d_getsw(word);

	if (bh_read(lword, 4) < 4)
	{
		__ERROR(errno, bh_error);
		return -1;
	}
	*dsize = d_getul(lword);

	if (bh_read(lword, 4) < 4)
	{
		__ERROR(errno, bh_error);
		return -1;
	}
	*rsize = d_getul(lword);

	if (*dsize > 0x7fffffff || *rsize > 0x7fffffff)
	{
		__ERROR(EINVAL, "invalid BinHex file header (bad file length)");
		return -1;
	}

	if (bh_readcrc() == -1)
	{
		__ERROR(errno, bh_error);
		return -1;
	}

	return 0;
}

/*
 * NAME:	cpi->binh()
 * DESCRIPTION:	copy a UNIX file to an HFS file using BinHex translation
 */
int cpi_binh(const wchar_t *srcname, hfsvol *vol, const char *dstname)
{
	int ifile, result;
	hfsfile *ofile;
	hfsdirent ent;
	const wchar_t *dsthint;
	char fname[HFS_MAX_FLEN + 1], type[5], creator[5];
	short fdflags;
	unsigned long dsize, rsize;

	ifile = opensrc(srcname, &dsthint, ".hqx", 0);
	if (ifile == -1)
		return -1;

	if (bh_open(ifile) == -1)
	{
		__ERROR(errno, bh_error);

		close(ifile);
		return -1;
	}

	if (binhx(fname, type, creator, &fdflags, &dsize, &rsize) == -1)
	{
		bh_close();
		close(ifile);
		return -1;
	}

	ofile = opendst(vol, dstname, fname, type, creator);
	if (ofile == 0)
	{
		bh_close();
		close(ifile);
		return -1;
	}

	result = do_binh(ofile, dsize, rsize);

	if (bh_close() == -1 && result == 0)
	{
		__ERROR(errno, bh_error);
		result = -1;
	}

	if (result == 0 && hfs_fstat(ofile, &ent) == -1)
	{
		__ERROR(errno, hfs_error);
		result = -1;
	}

	ent.fdflags = fdflags &
		~(HFS_FNDR_ISONDESK | HFS_FNDR_HASBEENINITED | HFS_FNDR_ISINVISIBLE);

	if (result == 0 && hfs_fsetattr(ofile, &ent) == -1)
	{
		__ERROR(errno, hfs_error);
		result = -1;
	}

	closefiles(ifile, ofile, &result);

	return result;
}

/*
 * NAME:	cpi->text()
 * DESCRIPTION:	copy a UNIX file to an HFS file using text translation
 */
int cpi_text(const wchar_t *srcname, hfsvol *vol, const char *dstname)
{
	int ifile, result = 0;
	hfsfile *ofile;
	const wchar_t *dsthint;
	char *dsthint_macroman;

	ifile = opensrc(srcname, &dsthint, ".txt", 0);
	if (ifile == -1)
		return -1;

	dsthint_macroman = utf16ToMacRoman(dsthint);
	if (dsthint_macroman == 0)
		return -1;
	ofile = opendst(vol, dstname, dsthint_macroman, TEXT_TYPE, TEXT_CREA);
	free(dsthint_macroman);
	if (ofile == 0)
	{
		close(ifile);
		return -1;
	}

	result = do_text(ifile, ofile);

	closefiles(ifile, ofile, &result);

	return result;
}

/*
 * NAME:	cpi->raw()
 * DESCRIPTION:	copy a UNIX file to the data fork of an HFS file
 */
int cpi_raw(const wchar_t *srcname, hfsvol *vol, const char *dstname)
{
	int ifile, result = 0;
	hfsfile *ofile;
	const wchar_t *dsthint;
	char *dsthint_macroman;

	ifile = opensrc(srcname, &dsthint, 0, 1);
	if (ifile == -1)
		return -1;

	dsthint_macroman = utf16ToMacRoman(dsthint);
	if (dsthint_macroman == 0)
		return -1;

	ofile = opendst(vol, dstname, dsthint_macroman, RAW_TYPE, RAW_CREA);
	free(dsthint_macroman);
	if (ofile == 0)
	{
		close(ifile);
		return -1;
	}

	result = do_raw(ifile, ofile);

	closefiles(ifile, ofile, &result);

	return result;
}
