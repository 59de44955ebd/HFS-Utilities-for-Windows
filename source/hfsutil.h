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

//extern int errno;

# define __ERROR(code, str)	(hfs_error = (str), errno = (code))

//extern const wchar_t *argv0, *bargv0;
extern const wchar_t *bargv0;

void hfsutil_perror(const char *);
void hfsutil_perror_w(const wchar_t *msg);

void hfsutil_perrorp(const char *);
void hfsutil_perrorp_w(const wchar_t *);

hfsvol *hfsutil_remount(mountent *, int);
void hfsutil_unmount(hfsvol *, int *);

void hfsutil_pinfo(hfsvolent *);

//char **hfsutil_glob(hfsvol *, int, char *[], int *, int *);
char **hfsutil_glob(hfsvol *, int, wchar_t *[], int *, int *);

char *hfsutil_getcwd(hfsvol *);

int hfsutil_samepath(const char *, const char *);
int hfsutil_samepath_w(const wchar_t *, const wchar_t *);

//char *hfsutil_abspath(const char *);
