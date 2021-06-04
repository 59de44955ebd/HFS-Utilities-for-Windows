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

# include "charset.h"
# include <windows.h>

char *utf16ToMacRoman(const wchar_t *input)
{
	// Get size of destination buffer
	int output_len = WideCharToMultiByte(CP_MACCP, 0, input, -1, NULL, 0, NULL, NULL);
	if (!output_len) return NULL;

	// Allocate destination buffer
	char * output = malloc(output_len + 1);
	if (!output) return NULL;

	int result = WideCharToMultiByte(
		CP_MACCP,    // convert to MacRoman
		0,

		input,       // source UTF-16 string
		-1,

		output,      // destination buffer
		output_len,  // size of destination buffer

		NULL,
		NULL
	);
	if (!result)
	{
		free(output);
		return NULL;
	}
	return output;
}

wchar_t *macRomanToUtf16(const char *input)
{
	// Get size of destination buffer (in wchar_t’s)
	int output_len = MultiByteToWideChar(CP_MACCP, 0, input, -1, NULL, 0);
	if (!output_len) return NULL;

	// Allocate destination buffer
	wchar_t * output = malloc((output_len + 1) * sizeof(wchar_t));
	if (!output) return NULL;

	int result = MultiByteToWideChar(
		CP_MACCP,   // convert from MacRoman
		0,

		input,      // source string
		-1,

		output,     // destination buffer
		output_len  // size of destination buffer (in wchar_t’s)
	);
	if (!result)
	{
		free(output);
		return NULL;
	}
	return output;
}

// input *not* null-terminated
char *macRomanToUtf8Chunk(const char *input, int chunk_size, int * result_size)
{
	// Get size of destination buffer (in wchar_t’s)
	int utf16_len = MultiByteToWideChar(CP_MACCP, 0, input, chunk_size, NULL, 0);
	if (!utf16_len) return NULL;

	// Allocate destination buffer
	wchar_t * utf16 = malloc(utf16_len * sizeof(wchar_t));
	if (!utf16) return NULL;

	// MacRoman to UTF-16
	int result = MultiByteToWideChar(
		CP_MACCP,	// convert from MacRoman
		0,

		input,		// source string
		chunk_size,	// total length of source string, in CHAR’s (= bytes), including end-of-string

		utf16,		// destination buffer
		utf16_len	// size of destination buffer, in WCHAR’s
	);

	if (!result)
	{
		free(utf16);
		return NULL;
	}

	// Get size of destination buffer
	int utf8_len = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
	if (!utf8_len)
	{
		free(utf16);
		return NULL;
	}

	// Allocate destination buffer
	char * utf8 = malloc(utf8_len + 1);
	if (!utf8)
	{
		free(utf16);
		return NULL;
	}

	// UTF-16 to UTF-8
	*result_size = WideCharToMultiByte(
		CP_UTF8,    // convert to UTF-8
		0,

		utf16,      // source UTF-16 string
		utf16_len,  // total length of source UTF-8 string, in WCHAR’s (= bytes), including end-of-string

		utf8,       // destination buffer
		utf8_len,   // size of destination buffer, in WCHAR’s

		NULL,
		NULL
	);

	free(utf16);
	if (!*result_size)
	{
		free(utf8);
		return NULL;
	}
	return utf8;
}

// input *not* null-terminated
char *utf8ToMacRomanChunk(const char *input, int chunk_size, int * result_size)
{
	// Get size of destination buffer (in wchar_t’s)
	int utf16_len = MultiByteToWideChar(CP_UTF8, 0, input, chunk_size, NULL, 0);
	if (!utf16_len) return NULL;

	// Allocate destination buffer
	wchar_t * utf16 = malloc(utf16_len * sizeof(wchar_t));
	if (!utf16) return NULL;

	// UTF-8 to UTF-16
	int result = MultiByteToWideChar(
		CP_UTF8,     // convert from UTF-8
		0,

		input,       // source UTF-8 string
		chunk_size,  // total length of source UTF-8 string

		utf16,       // destination buffer
		utf16_len    // size of destination buffer (in wchar_t’s)
	);

	if (!result)
	{
		free(utf16);
		return NULL;
	}

	// Get size of destination buffer
	int macroman_len = WideCharToMultiByte(CP_MACCP, 0, utf16, -1, NULL, 0, NULL, NULL);
	if (!macroman_len)
	{
		free(utf16);
		return NULL;
	}

	// Allocate destination buffer
	char * macroman = malloc(macroman_len);
	if (!macroman)
	{
		free(utf16);
		return NULL;
	}

	// UTF-16 to MacRoman
	*result_size = WideCharToMultiByte(
		CP_MACCP,    // convert to MacRoman
		0,

		utf16,       // source UTF-16 string
		utf16_len,	 // total length of source string (in wchar_t’s)

		macroman,    // destination buffer
		macroman_len,  // size of destination buffer

		NULL,
		NULL
	);

	free(utf16);
	if (!*result_size)
	{
		free(macroman);
		return NULL;
	}
	return macroman;
}
