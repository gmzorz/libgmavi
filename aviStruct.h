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

#ifndef AVISTRUCT_H
# define AVISTRUCT_H
# include "Types.h"
//	using FCC()
# include "msaviriff.h"



/*	Size limitation for RIFFLISTs */
# define	AVI_MAX_RIFF_SIZE		0x40000000LL
# define	AVI_MASTER_INDEX_SIZE   256
# define	AVIIF_INDEX				0x10

/*
**	Reduce byte padding to one for precise data alignment
*/
# pragma pack(1)

/*
*	RIFFCHUNK
*
*	@param		fcc 					-	Describes the fcc ID of a chunk
*	@param		cb						-	Size of chunk (not including this struct)
*/
typedef struct	_riffChunk
{
	FOURCC		fcc;
	DWORD		cb;
}	RIFFCHUNK;

# define	FOURCC_RIFF				FCC('RIFF')
# define	FOURCC_LIST				FCC('LIST')
# define	FOURCC_TYPE_VIDEO		FCC('AVI ')
# define	FOURCC_TYPE_VIDEO_EXT	FCC('AVIX')
/*
*	RIFFLIST
*
*	@param		fcc 					-	Describes the fcc ID of a list
*	@param		cb						-	Size of list data (not including @fcc and @cb of this struct)
*	@param		fccListType				-	List Type
*
*	REMARKS
*
*	Common fcc ID are:
*	'RIFF' - Used for file header
*	'LIST' - RIFF List component
*
*	Common fcc List Types are:
*	'AVI ' - To declare the file format when 'RIFF' is used (video)
*	'AVIX' - To indicate a segmented file when 'RIFF' is used (For size limitations)
*/
typedef struct	_riffList
{
	FOURCC		fcc;
	DWORD		cb;
	FOURCC		fccListType;
}	RIFFLIST;

# define	FOURCC_HEADER_LIST		FCC('hdrl')
# define	FOURCC_AVI_HEADER		FCC('avih')
# define	AVIF_HASINDEX			0x00000010
# define	AVIF_MUSTUSEINDEX   	0x00000020
# define	AVIF_ISINTERLEAVED		0x00000100
# define	AVIF_TRUSTCKTYPE		0x00000800
# define	AVIF_WASCAPTUREFILE		0x00010000
# define	AVIF_COPYRIGHTED		0x00020000

/*
*	AVIMAINHEADER
*
*	@param		fcc						-	FOURCC code 'avih'	@FOURCC_AVI_HEADER
*	@param		cb						-	Size of this structure (minus @fcc and @cb)
*	@param		microSecPerFrame		-	Microseconds per frame (1000000 / frameRateSec)
*	@param		maxBytesPerSec			-	Approximate maximum data rate (W*H*FMT*FRAMES?)
*	@param		paddingGranularity		-	Data alignment. Multiples of this value
*	@param		flags					-	Metadata flags
*	@param		totalFrames				-	Amount of frames in the file		(*)
*	@param		initialFrames			-	Initial frame for interleaved files, otherwise zero
*	@param		streams					-	Amount of streams in this file (one for video and one for audio)
*	@param		suggestedBufferSize		-	Buffer size for reading frames (W*H*FMT*FRAMES?)
*	@param		width					-	Width of the fideo buffer
*	@param		height					-	Height of the video buffer
*	@param		reserved				-	Unused, should be NULL (zeroed)
*/
typedef struct	_aviMainHeader
{
	FOURCC		fcc;
	DWORD		cb;
	DWORD		microSecPerFrame;
	DWORD		maxBytesPerSec;
	DWORD		paddingGranularity;
	DWORD		flags;
	DWORD		totalFrames;
	DWORD		initialFames;
	DWORD		streams;
	DWORD		suggestedBufferSize;
	DWORD		width;
	DWORD		height;
	DWORD		reserved[4];
}	AVIMAINHEADER;

/*
*	RECT
*
*	rectangle definition, dimensions to screen.
*/
typedef struct	_rect
{
	WORD		left;
	WORD		top;
	WORD		right;
	WORD		bottom;
}	RECT;

# define	FOURCC_STREAM_LIST			FCC('strl')
# define	FOURCC_STREAM_HEADER		FCC('strh')
# define	FCCSTREAMTYPE_AUDS			FCC('auds')
# define	FCCSTREAMTYPE_MIDS			FCC('mids')
# define	FCCSTREAMTYPE_TXTS			FCC('txts')
# define	FCCSTREAMTYPE_VIDS			FCC('vids')		//	Standard for video
# define	FOURCC_HANDLER_DIB			FCC('DIB ')		//	Device independent bitmap

# define	AVISF_DISABLED				0x00000001
# define 	AVISF_VIDEO_PALCHANGES   	0x00010000

//riffpad <---
/*
*	AVISTREAMHEADER
*
*	@param		fcc						-	FOURCC code 'strh'	@FOURCC_STREAM_HEADER
*	@param		cb						-	Size of this structure (minus @fcc and @cb)
*	@param		fccType					-	FOURCC code specifying the type of data contained in the stream
*	@param		fccHandler				-	FOURCC code describing the codec used ('DIB ' for Bitstream)	@FOURCC_HANDLER_DIB
*	@param		flags					-	Flags for data stream
*	@param		priority				-	Priority for stream type
*	@param		language				-	Language tag
*	@param		initialFrames			-	Initial frame for interleaved files, otherwise zero
*	@param		scale					-	Frame scale (1)
*	@param		rate					-	Frame rate	(Frames per second)
*	@param		start					-	zero
*	@param		length					-	Frame count
*	@param		suggestedBufferSize		-	(W*H*FMT + 8)
*	@param		quality					-	0xFFFFFFFF (-1 when uncompressed)
*	@param		frame					-	Destination rectangle for cropping
*/
typedef struct	_aviStreamHeader
{
	FOURCC		fcc;
	DWORD		cb;
	FOURCC		fccType;
	FOURCC		fccHandler;
	DWORD		flags;
	WORD		priority;
	WORD		language;
	DWORD		initialFrames;
	DWORD		scale;
	DWORD		rate;
	DWORD		start;
	DWORD		length;
	DWORD		suggestedBufferSize;
	DWORD		quality;
	RECT		frame;
}	AVISTREAMHEADER;


# define	FOURCC_STREAM_FORMAT		FCC('strf')		//	stream format
/*
*	BITMAPINFOHEADER
*
*	Standard bitmap header (DIB)
*/
typedef struct	_bitMapInfoHeader
{
	DWORD		size;
	LONG		width;
	LONG		height;
	WORD		planes;
	WORD		bitCount;
	DWORD		compression;
	DWORD		sizeImage;
	LONG		xPelsPerMeter;
	LONG		yPelsPerMeter;
	DWORD		clrUsed;
	DWORD		clrImportant;
}	BITMAPINFOHEADER;

/*
*	AVISUPERINDEX_ENTRY
*
*	@param		offset			-	64 bit offset to field index chunk (@AVIFIELDINDEX)
*	@param		size			-	size of chunk
*	@param		duration		-	duration of chunk (in stream ticks/frames)
*/
typedef struct	_aviSuperIndex_entry
{
	DWORDLONG	offset;
	DWORD		size;
	DWORD		duration;
}	AVISUPERINDEX_ENTRY;

# define	FOURCC_INDEX_DATA		FCC('indx')

# define	AVI_INDEX_OF_INDEXES	0x00
# define	AVI_INDEX_OF_CHUNKS		0x01
/*
*	AVISUPERINDEX
*
*	@param				fcc				-	FOURCC identifier 'indx'	@FOURCC_INDEX_DATA
*	@param				cb				-	Size of this struct minus @fcc and @cb
*	@param				longsPerEntry	-	Must be 4
*	@param				indexSubType	-	0
*	@param				indexType		-	@AVI_INDEX_OF_INDEXES
*	@param				entriesInUse	-	?
*	@param				chunkId			-	Chunk ID of chunks being indexed ('DIB '?)
*	@param				index			-	Index entries pointing to field index chunk (@AVIFIELDINDEX)
*/
typedef struct	_aviSuperIndex
{
	FOURCC				fcc;
	DWORD				cb;
	WORD				longsPerEntry;
	BYTE				indexSubType;
	BYTE				indexType;
	DWORD				entriesInUse;
	DWORD				chunkId;
	DWORD				reserved[3];
	AVISUPERINDEX_ENTRY	index[256];
	//AVISUPERINDEX_ENTRY	index[1022];
}	AVISUPERINDEX;

# define	FOURCC_EXTENDED_LIST		FCC('odml')
# define	FOURCC_EXTENDED_HEADER		FCC('dmlh')

/*
*	AVIEXTHEADER
*
*	@param				fcc				-	FOURCC identifier 'dmlh'	@FOURCC_EXTENDED_HEADER
*	@param				cb				-	Size of this struct minus @fcc and @cb
*	@param				grandFrames		-	Total number of frames in the file
*	@param				future			-	undefined
*/
typedef struct	_aviExtHeader
{
	FOURCC				fcc;
	DWORD				cb;
	DWORD				grandFrames;
	DWORD				future[61];
}	AVIEXTHEADER;

# define	FOURCC_STREAM_LIST			FCC('movi')
# define	FOURCC_STREAM_RAW			FCC('00db')		//	uncompressed

/*
*	RGB24BITMAP
*
*	@param	fcc				-	FOURCC format identifier '00db'	@FOURCC_STREAM_RAW
*	@param	cb				-	size minus @fcc and @cb
*	@param	bitmap			-	Bitmap with size of (W * H * FMT eg. 1280*720*3)
* */
typedef struct	_rgb24Bitmap
{
	DWORD	fcc;
	DWORD	cb;
	BYTE	bitmap[];
}	RGB24BITMAP;

/*
*	OLD INDEX
*/
typedef struct	_aviOldIndex_entry
{
	DWORD	chunkId;
	DWORD	flags;
	DWORD	offset;
	DWORD	size;
}	AVIOLDINDEX_ENTRY;

# define	FOURCC_TYPE_INDEX_OLD		FCC('idx1')

typedef struct	_aviOldIndex
{
	FOURCC				fcc;
	DWORD				cb;
	AVIOLDINDEX_ENTRY	entries[];
}	AVIOLDINDEX;

/*
*	AVIFIELDINDEX_ENTRY
*
*	A newer indexing struct
*	@param		offset				-	offset to bitmap (from base offset)
*	@param		size				-	size of the bitmap information
*	@param		offsetField2		-	??
*/
typedef struct	_aviFieldIndex_entry
{
	DWORD		offset;
	DWORD		size;
	DWORD		offsetField2;
}	AVIFIELDINDEX_ENTRY;

# define	FOURCC_TYPE_FIELD_INDEX		FCC('ix00')
# define 	AVI_INDEX_2FIELD 			0x01
# define 	AVI_INDEX_OF_CHUNKS 		0x01

/*
*	AVIFIELDINDEX_CHUNK
*
*	@param				fcc				-	FOURCC identifier 'ix00'	@FOURCC_TYPE_FIELD_INDEX
*	@param				cb				-	Size of this struct minus @fcc and @cb
*	@param				longsPerEntry	-	3
*	@param				indexSubType	-	@AVI_INDEX_2FIELD
*	@param				indexType		-	@AVI_INDEX_OF_CHUNKS
*	@param				entriesInUse	-	?
*	@param				chunkId			-	'00db'
*	@param				baseOffset		-	offsets in field index entries are relative to this
*	@param				reserved3		-	0
*	AVIFIELDINDEX_ENTRY	index[ ];		-	field index entries		@AVIFIELDINDEX_ENTRY
*/
typedef struct	_aviFieldindex_chunk
{
	FOURCC				fcc;
	DWORD				cb;
	WORD				longsPerEntry;
	BYTE				indexSubType;
	BYTE				indexType;
	DWORD				entriesInUse;
	DWORD				chunkId;
	DWORDLONG			baseOffset;
	DWORD				reserved3;
	AVIFIELDINDEX_ENTRY	index[ ];
}	AVIFIELDINDEX_CHUNK;

/*	Only pack these structs		*/
# pragma reset

#endif
