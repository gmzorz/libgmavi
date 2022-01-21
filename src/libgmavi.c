/*
*	Copyright (c) 2022 Gijs Oosterling
*	All rights reserved.
*	
*		Permission is hereby granted, free of charge, to any person obtaining a copy
*		of this software and associated documentation files (the "Software"), to deal
*		in the Software without restriction, including without limitation the rights
*		to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*		copies of the Software, and to permit persons to whom the Software is
*		furnished to do so, subject to the following conditions:
*	
*		The above copyright notice and this permission notice shall be included in all
*		copies or substantial portions of the Software.
*	
*		THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*		IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*		FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*		AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*		LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*		OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*		SOFTWARE.
*	
*	Redistributions in binary form must reproduce the above copyright notice.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../include/libgmavi.h"
#include "aviStruct.h"
#include <errno.h>
#include <sys/stat.h>

static bool	gmav_error(gmavi_t *avi, uint32_t errorCode, const char *additionalString)
{
	if (avi)
	{
		for (uint32_t i = 0; i < avi->riffChunks; i++) {
			if (avi->ix00[i].avixIndexEntries != NULL)
				free(avi->ix00[i].avixIndexEntries);
		}
		free(avi->filePath);
		free(avi);
	}
	if (additionalString)
		perror(additionalString);
	else if (errorCode == EBADF)
		perror("Invalid file descriptor");
	else if (errorCode == EACCES)
		perror("File is used by another process");
	return (false);
}

static bool	fileExists(const char *filePath)
{
	struct stat	buffer;
	return (stat(filePath, &buffer) == 0);
}

void		*gmav_open(
	const char 	*filePath,
	uint32_t	width,
	uint32_t	height,
	uint32_t	framesPerSec)
{
	gmavi_fileAddr_t	fileAddr;
	gmavi_static_t		contents;
	gmavi_t				*out;

	memset(&fileAddr, 0, sizeof(gmavi_fileAddr_t));
	memset(&contents, 0, sizeof(gmavi_static_t));
	out = (gmavi_t *)calloc(1, sizeof(gmavi_t));
	if (out == NULL)
	{
		gmav_error(NULL, errno, NULL);
		return (NULL);
	}

	out->filePath = strdup(filePath);
	out->bitmapSize = width * height * 3;
	out->streamTickSize = out->bitmapSize + 8;
	out->maxFrames = (uint32_t)RIFF_MAX_SIZE / out->streamTickSize;

	contents.main = (RIFFLIST){
		FCC('RIFF'),
		TO_BE_DETERMINED,
		FCC('AVI ')
	};

	fileAddr._fileBaseStart = (uint64_t)&contents.main;
	fileAddr.cbMain = (uint64_t)&contents.main.cb - fileAddr._fileBaseStart;

	contents.hdrl = (RIFFLIST){
		FCC('LIST'),
		STATIC_HEADER_LIST_SIZE,
		FCC('hdrl')
	};

	contents.aviHeader = (AVIMAINHEADER){
		FCC('avih'),
		STATIC_AVI_HEADER_SIZE,
		1000000 / framesPerSec,
		(out->bitmapSize + 30) * framesPerSec,
		0,
		AVIF_HASINDEX,
		TO_BE_DETERMINED,
		0,
		1,
		0,
		width,
		height
	};

	fileAddr.firstFrames = (uint64_t)&contents.aviHeader.totalFrames - fileAddr._fileBaseStart;

	contents.strl = (RIFFLIST){
		FCC('LIST'),
		STATIC_STREAM_LIST_SIZE,
		FCC('strl')
	};

	contents.streamHeader = (AVISTREAMHEADER){
		FCC('strh'),
		STATIC_STREAM_HEADER_SIZE,
		FCC('vids'),
		FCC('DIB '),
		0,
		0,
		0,
		0,
		1,
		framesPerSec,
		0,
		TO_BE_DETERMINED,
		out->bitmapSize,
		0xFFFFFFFF,
		0,
		(RECT){0, 0, width, height}
	};
	fileAddr.totalFrames = (uint64_t)&contents.streamHeader.length - fileAddr._fileBaseStart;

	contents.strf = (RIFFCHUNK){
		FCC('strf'),
		STATIC_STREAM_FORMAT_SIZE
	};

	contents.bitmapHeader = (BITMAPINFOHEADER){
		STATIC_BITMAP_HEADER_SIZE,
		width,
		height,
		1,
		24,
		0,
		out->bitmapSize,
		0,
		0,
		0,
		0
	};

	contents.superIndex = (AVISUPERINDEX){
		FCC('JUNK'),
		STATIC_SUPER_INDEX_SIZE,
	};
	fileAddr.superIndex = (uint64_t)&contents.superIndex - fileAddr._fileBaseStart;
	fileAddr.entriesInUse = (uint64_t)&contents.superIndex.entriesInUse - fileAddr._fileBaseStart;
	fileAddr.superIndexEntries = (uint64_t)&contents.superIndex.index[0] - fileAddr._fileBaseStart;

	contents.odml = (RIFFLIST){
		FCC('LIST'),
		STATIC_EXTENDED_LIST_SIZE,
		FCC('odml')
	};

	contents.extendedHeader = (AVIEXTHEADER){
		FCC('dmlh'),
		STATIC_EXTENDED_HEADER_SIZE,
		TO_BE_DETERMINED,
		0x0
	};
	fileAddr.grandFrames = (uint64_t)&contents.extendedHeader.grandFrames - fileAddr._fileBaseStart;

	contents.junk = (RIFFCHUNK){
		FCC('JUNK'),
		STATIC_JUNKFILL_SIZE
	};

	contents.movi = (RIFFLIST){
		FCC('LIST'),
		TO_BE_DETERMINED,
		FCC('movi')
	};

	fileAddr.cbMovi = (uint64_t)&contents.movi.cb - fileAddr._fileBaseStart;
	fileAddr.moviStart = fileAddr.cbMovi + 8;

	out->riffSize = sizeof(gmavi_static_t) - 8;
	out->contents = contents;
	out->fileAddr = fileAddr;
	out->fileHandler = fopen(filePath, "wb+");
	if (out->fileHandler == NULL)
	{
		gmav_error(out, errno, NULL);
		return (NULL);
	}

	fwrite(&out->contents, sizeof(gmavi_static_t), 1, out->fileHandler);
	if (fclose(out->fileHandler))
	{
		gmav_error(out, errno, NULL);
		return (NULL);
	}

	out->mainIndex.fcc = FCC('idx1');
	out->mainIndex.cb = 0;

	return (out);
}

static bool		gmav_finish_main(
	gmavi_t	*avi)
{
	avi->moviSize = (avi->frameCount * avi->streamTickSize) + 4;
	
	avi->mainIndex.cb = STATIC_OLD_INDEX_OFFSET * avi->frameCount;
	avi->riffSize += avi->moviSize + avi->mainIndex.cb + 4;
	avi->mainIndexEntries = (AVIOLDINDEX_ENTRY *)calloc(1, avi->mainIndex.cb);
	if (avi->mainIndexEntries == NULL)
		return (gmav_error(avi, errno, NULL));
	for (uint32_t i = 0; i < avi->frameCount; i++) {
		avi->mainIndexEntries[i] = (AVIOLDINDEX_ENTRY){
			FCC('00db'),
			AVIF_HASINDEX,
			4 + avi->streamTickSize * i,
			avi->bitmapSize
		};
	}

	avi->fileHandler = fopen(avi->filePath, "rb+");
	if (avi->fileHandler == NULL)
		return (gmav_error(avi, errno, NULL));
	_fseeki64(avi->fileHandler, 0, SEEK_END);
	fwrite(&avi->mainIndex, sizeof(AVIOLDINDEX), 1, avi->fileHandler);
	fwrite(avi->mainIndexEntries, sizeof(AVIOLDINDEX_ENTRY), avi->frameCount, avi->fileHandler);
	free(avi->mainIndexEntries);
	
	_fseeki64(avi->fileHandler, avi->fileAddr.cbMain, SEEK_SET);
	fwrite(&avi->riffSize, sizeof(uint32_t), 1, avi->fileHandler);
	avi->fileSize = avi->riffSize;
	
	_fseeki64(avi->fileHandler, avi->fileAddr.firstFrames + 1, SEEK_SET);
	fwrite(&avi->frameCount, sizeof(uint32_t), 1, avi->fileHandler);
	
	_fseeki64(avi->fileHandler, avi->fileAddr.totalFrames, SEEK_SET);
	fwrite(&avi->frameCount, sizeof(uint32_t), 1, avi->fileHandler);
	
	_fseeki64(avi->fileHandler, avi->fileAddr.grandFrames, SEEK_SET);
	fwrite(&avi->frameCount, sizeof(uint32_t), 1, avi->fileHandler);
	
	_fseeki64(avi->fileHandler, avi->fileAddr.cbMovi, SEEK_SET);
	fwrite(&avi->moviSize, sizeof(uint32_t), 1, avi->fileHandler);
	avi->fileAddr.moviStart = 0x2014;

	if (fclose(avi->fileHandler))
		return (gmav_error(avi, errno, NULL));
	return (true);
}

static bool gmav_create_index(gmavi_t *avi, size_t size)
{
	avi->ix00[avi->riffChunks].avixIndex = (AVISTDINDEX){
		FCC('ix00'),
		24 + size * 8,
		2,
		0,
		AVI_INDEX_OF_CHUNKS,
		size,
		FCC('00db'),
		avi->fileAddr.moviStart,
		0
	};

	avi->ix00[avi->riffChunks].avixIndexEntries = (AVISTDINDEX_ENTRY *)calloc(1, sizeof(AVISTDINDEX_ENTRY) * size);
	if (avi->ix00[avi->riffChunks].avixIndexEntries == NULL)
		return (gmav_error(avi, errno, NULL));
	
	for (uint32_t i = 0; i < size; i++) {
		avi->ix00[avi->riffChunks].avixIndexEntries[i].dwOffset = avi->streamTickSize * i;
		avi->ix00[avi->riffChunks].avixIndexEntries[i].dwSize = avi->bitmapSize;
	}
	return (true);
}

static bool	gmav_add_avix_chunk(gmavi_t *avi)
{
	if (avi->riffChunks == 0)
	{
		gmav_finish_main(avi);
		avi->fileSize += 8;
	}
	else
	{
		uint32_t	moviSize = 4 + (avi->streamTickSize * avi->maxFrames);
		uint32_t	riffSize = moviSize + 12;

		avi->fileSize += (avi->streamTickSize * avi->maxFrames);
		avi->fileHandler = fopen(avi->filePath, "rb+");
		if (avi->fileHandler == NULL)
			return (gmav_error(avi, errno, NULL));
		_fseeki64(avi->fileHandler, avi->fileAddr.cbMain, SEEK_SET);
		fwrite(&riffSize, sizeof(uint32_t), 1, avi->fileHandler);
		_fseeki64(avi->fileHandler, 8, SEEK_CUR);
		fwrite(&moviSize, sizeof(uint32_t), 1, avi->fileHandler);
		if (fclose(avi->fileHandler))
			return (gmav_error(avi, errno, NULL));
	}

	RIFFLIST	avix = {
		FCC('RIFF'),
		TO_BE_DETERMINED,
		FCC('AVIX')
	};
	RIFFLIST	movi = {
		FCC('LIST'),
		TO_BE_DETERMINED,
		FCC('movi')
	};

	avi->fileAddr.cbMain = avi->fileSize + 4;

	avi->fileHandler = fopen(avi->filePath, "rb+");
	if (avi->fileHandler == NULL)
			return (gmav_error(avi, errno, NULL));
	_fseeki64(avi->fileHandler, 0, SEEK_END);
	fwrite(&avix, sizeof(RIFFLIST), 1, avi->fileHandler);
	fwrite(&movi, sizeof(RIFFLIST), 1, avi->fileHandler);
	if (fclose(avi->fileHandler))
		return (gmav_error(avi, errno, NULL));
	avi->moviSize = 0;
	avi->fileSize += 24;

	gmav_create_index(avi, avi->maxFrames);
	avi->fileAddr.moviStart = avi->fileSize + 8;
	avi->riffChunks += 1;
	return (true);
}

static bool	gmav_avix_add(
	gmavi_t *avi,
	uint8_t *buffer)
{
	const uint32_t	fourcc_uncompressed = FCC('00db');

	avi->fileHandler = fopen(avi->filePath, "rb+");
	if (avi->fileHandler == NULL)
		return (gmav_error(avi, errno, NULL));

	_fseeki64(avi->fileHandler, 0, SEEK_END);
	fwrite(&fourcc_uncompressed, sizeof(uint32_t), 1, avi->fileHandler);
	fwrite(&avi->bitmapSize, sizeof(uint32_t), 1, avi->fileHandler);
	fwrite(buffer, avi->bitmapSize, 1, avi->fileHandler);
	if (fclose(avi->fileHandler))
		return (gmav_error(avi, errno, NULL));
	return (true);
}

bool	gmav_add(
	void *gmavi,
	uint8_t *buffer)
{
	gmavi_t	*avi = (gmavi_t *)gmavi;
	const uint32_t	fourcc_uncompressed = FCC('00db');

	if (avi == NULL)
		return (gmav_error(avi, 0, "No gmavi_t struct specified (null)"));
	if (buffer == NULL)
		return (gmav_error(avi, 0, "No buffer specified (null)"));
	if (!fileExists(avi->filePath))
		return (gmav_error(avi, 0, "No open file detected, did you remove it?"));

	if (avi->frameCount && avi->frameCount % avi->maxFrames == 0)
		gmav_add_avix_chunk(avi);
	
	avi->frameCount += 1;
	if (avi->riffChunks != 0)
		return (gmav_avix_add(avi, buffer));

	avi->fileHandler = fopen(avi->filePath, "rb+");
	if (avi->fileHandler == NULL)
		return (gmav_error(avi, errno, NULL));
	
	_fseeki64(avi->fileHandler, 0, SEEK_END);
	fwrite(&fourcc_uncompressed, sizeof(uint32_t), 1, avi->fileHandler);
	fwrite(&avi->bitmapSize, sizeof(uint32_t), 1, avi->fileHandler);
	fwrite(buffer, avi->bitmapSize, 1, avi->fileHandler);
	if (fclose(avi->fileHandler))
		return (gmav_error(avi, errno, NULL));
	return (true);
}

bool		gmav_finish(
	void *gmavi)
{
	if (gmavi == NULL)
		return (gmav_error(NULL, 0, "No gmavi_t struct specified (null)"));

	gmavi_t	*avi = (gmavi_t *)gmavi;

	if (!fileExists(avi->filePath))
		return (gmav_error(avi, 0, "No open file detected, did you remove it?"));

	if (avi->riffChunks == 0)
		return (gmav_finish_main(avi));

	uint32_t	framesLeft = avi->frameCount % avi->maxFrames;
	if (framesLeft == 0)
		framesLeft = avi->maxFrames;
	gmav_create_index(avi, framesLeft);

	avi->fileHandler = fopen(avi->filePath, "rb+");
	if (avi->fileHandler == NULL)
		return (gmav_error(avi, errno, NULL));

	AVISUPERINDEX	superIndex = {
		FCC('indx'),
		STATIC_SUPER_INDEX_SIZE,
		4,
		0,
		0,
		avi->riffChunks + 1,
		FCC('00db'),
		{0, 0, 0},
		TO_BE_DETERMINED
	};

	_fseeki64(avi->fileHandler, avi->fileAddr.superIndex, SEEK_SET);
	fwrite(&superIndex, sizeof(AVISUPERINDEX), 1, avi->fileHandler);

	avi->fileSize += avi->streamTickSize * framesLeft;
	for (uint32_t i = 0; i < avi->riffChunks; i++) {

		_fseeki64(avi->fileHandler, 0, SEEK_END);
		fwrite(&avi->ix00[i].avixIndex, sizeof(AVISTDINDEX), 1, avi->fileHandler);
		fwrite(avi->ix00[i].avixIndexEntries, sizeof(AVISTDINDEX_ENTRY), avi->maxFrames, avi->fileHandler);
		
		uint32_t	stdIndexSize = 32 + avi->maxFrames * 8;
		_fseeki64(avi->fileHandler, avi->fileAddr.superIndexEntries, SEEK_SET);
		fwrite(&avi->fileSize, sizeof(uint64_t), 1, avi->fileHandler);
		fwrite(&stdIndexSize, sizeof(uint32_t), 1, avi->fileHandler);
		fwrite(&avi->maxFrames, sizeof(uint32_t), 1, avi->fileHandler);

		free(avi->ix00[i].avixIndexEntries);
		avi->fileSize += sizeof(AVISTDINDEX_ENTRY) * avi->maxFrames;
		avi->fileSize += sizeof(AVISTDINDEX);
		avi->fileAddr.superIndexEntries += STATIC_SUPER_INDEX_OFFSET;
	}

	uint32_t	lastIndexSize = 32 * (avi->riffChunks + 1) + framesLeft * 8;
	fwrite(&avi->fileSize, sizeof(uint64_t), 1, avi->fileHandler);
	fwrite(&lastIndexSize, sizeof(uint32_t), 1, avi->fileHandler);
	fwrite(&framesLeft, sizeof(uint32_t), 1, avi->fileHandler);

	_fseeki64(avi->fileHandler, 0, SEEK_END);
	fwrite(&avi->ix00[avi->riffChunks].avixIndex, sizeof(AVISTDINDEX), 1, avi->fileHandler);
	fwrite(avi->ix00[avi->riffChunks].avixIndexEntries, sizeof(AVISTDINDEX_ENTRY), framesLeft, avi->fileHandler);

	uint32_t	moviSize = 4 + (avi->streamTickSize * framesLeft);
	uint32_t	riffSize = moviSize + 12 + sizeof(AVISTDINDEX) * (avi->riffChunks + 1);
	riffSize += sizeof(AVISTDINDEX_ENTRY) * avi->maxFrames * avi->riffChunks;
	riffSize += sizeof(AVISTDINDEX_ENTRY) * framesLeft;

	_fseeki64(avi->fileHandler, avi->fileAddr.cbMain, SEEK_SET);
	fwrite(&riffSize, sizeof(uint32_t), 1, avi->fileHandler);
	_fseeki64(avi->fileHandler, 8, SEEK_CUR);
	fwrite(&moviSize, sizeof(uint32_t), 1, avi->fileHandler);
	if (fclose(avi->fileHandler))
		return (gmav_error(avi, errno, NULL));
	return (true);
}
