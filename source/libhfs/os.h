/*
 * libhfs - library for reading and writing Macintosh HFS volumes
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

struct flock
{
	short	l_type;		/* f_rdlck, f_wrlck, or f_unlck */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to eof */
	short	l_pid;		/* returned with f_getlk */
	short	l_xxx;		/* reserved for future use */
};

#define	F_RDLCK		1	/* read lock */
#define	F_WRLCK		2	/* write lock */

int os_open(void **, const wchar_t *, int);

int os_close(void **);

int os_same(void **, const wchar_t *);

unsigned long os_seek(void **, unsigned long);
unsigned long os_read(void **, void *, unsigned long);
unsigned long os_write(void **, const void *, unsigned long);
