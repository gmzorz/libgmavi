# libgmavi
A compact library made for simple video writing using uncompressed buffers. libgmavi is partially a study, and an addition to the [CODMVM](https://codmvm.com/) toolset, which handles multi-pass videogame recording whilst being hooked to the supported game. libgmavi covers the OpenDML AVI 2.0 Extensions as well, but does not feature compression methods.

# Use case and example
_(not much knowledge required)_
This library is made for the sole purpose of writing RAW (uncompressed) AVI 2.0 video files. Specifically those of a higher file-size. The format can be particularly useful for raw image data interpretation for post-processing, video editing and compositing. The provided functions are minimal, and do not require much knowledge in video writing in order to function nicely. For example, one could easily make a simple still video using the following layout:
```c++
int   main(void)
{
      // Instance and buffer set up
      void*             gmav  =     gmav_open("testing.avi", 100, 100, 30);
      unsigned char*    buffer =    malloc(100 * 100 * 3);

      // Fill the buffer
      for (int i = 0; i < 100 * 100 * 3; i++) {
            *(unsigned char *)(buffer + i)      = 0xFF;     // Blue channel
            *(unsigned char *)(buffer + i + 1)  = 0xB8;     // Green channel
            *(unsigned char *)(buffer + i + 2)  = 0x2A;     // Red channel
      }

      // Add a couple of frames
      for (int i = 0; i < 10; i++) {
            gmav_add(gmav_buffer);
      }

      gmav_finish(gmav);
}
```

# Theory
_(In case you've heard of file headers, padding, the BMP format, and hopefully had some run-ins with fseek/fwrite!)_
Nowadays the focus is on video encoding for web and live or realtime broadcasts. Packing and compressing videos is one step further into my research, so i figured starting from the roots would be the best way to approach it.

Because the focus is more on encoding, it was a bit hard to actually acquire the knowledge i had hoped to find on the internet, which was a bit of a shame. I will try and run through the process as clearly and simple as possible in the hopes that it might help those who are willing to learn more about the format

#### **Understanding the format**
A good place to start is getting to know the format itself. Wikipedia is always good, sure! But learning requires doing, and in my case i would fall asleep looking at the white front page of wikipedia. [VirtualDub](https://www.virtualdub.org/) uses the AVI 2.0 format and can produce videos from sequences in a matter of seconds. How? we don't really care too much at this point, but the output is more important to us.

Because we are dealing with uncompressed footage, we can easily view the structure of the AVI files using software like [HxD](https://mh-nexus.de/en/hxd/). Which is intimidating, but most definitely required when debugging file memory addresses. I later found out about useful software such as [RIFFPad](https://www.menasoft.com/blog/?p=34), which allows you to see the structure of the RIFF AVI file, and [010 Editor](https://www.sweetscape.com/010editor/), which does the same using it's plug-ins but has a memory viewer/editor and is capable of manipulation.

Lastly, and most importantly, the [Microsoft reference](https://docs.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference) and the [OpenDML reference](http://www.jmcgowan.com/odmlff2.pdf) are essential to this topic, the second article talks about eliminating the 2GB size limitation, which is simply caused by a 32bit variable in the header defining the size. Back in the day, nobody ever thought we needed AVI files of over 2GB.

#### **A structure of structures**

Basically what we are looking at. The format is written as a tree, which starts at the root: the RIFF AVI Chunk. This chunk contains all the data we need, up until the size limitation! Let's tackle this before we get to writing bigger files.

To make things easier to understand, this graph displays the way structures are aligned within the format:
![avifmt](https://gmzorz.com/avifmt.png)

The Microsoft RIFF reference shows the following tree: 
```c++
RIFF ('AVI '
      LIST ('hdrl'
            'avih'(<Main AVI Header>)
            LIST ('strl'
                  'strh'(<Stream header>)
                  'strf'(<Stream format>)
                  [ 'strd'(<Additional header data>) ]
                  [ 'strn'(<Stream name>) ]
                  ...
                 )
             ...
           )
      LIST ('movi'
            {SubChunk | LIST ('rec '
                              SubChunk1
                              SubChunk2
                              ...
                             )
               ...
            }
            ...
           )
      ['idx1' (<AVI Index>) ]
     )
```
Which is great! but does not provide much detailed information.
Let's start from the top.
> _(Structures can be obtained from the `aviriff.h` header file, and are also included in this repo)_

#### **Main RIFF AVI chunk (lists)**
```c++
typedef struct	_riffList
{
	FOURCC		fcc;            //  FCC identifier
	uint32_t	cb;             //  Size of chunk*
	FOURCC		fccListType;    //  List type
}	RIFFLIST;
```
Chunks or Lists are frequently used in the file structure, primarely to define the main RIFF Chunks which hold up to 2GB of data. We start off with a list with FCC type `'RIFF'` (four chars, in binary form), the size of the chunk, which is unknown at this point, and the list type, which we define as the FCC (**four** character) type `'AVI '`.

The size of this RIFF Chunk will be determined later, so we need to save this memory address: `0x4`. It is also important to know the size does **not** include the first 8 bytes, the FCC Type and Size variables.
> The `aviriff.h` header file contains a compiler #define (`FCC()`) which can translate four character identifiers into a 32-bit unsigned integer

Following this list, we need to define the AVI header. This is done by writing another `RIFFLIST` directly after the RIFF Chunk, with the type set to `'LIST'` and the list type to `'hdrl'`. The MS Reference states that these two lists are **mandatory** and should always be written.

#### **AVI main header**
```c++
typedef struct 	_aviMainHeader
{
	FOURCC		fcc;
	uint32_t	cb;
	uint32_t	microSecPerFrame;
	uint32_t	maxBytesPerSec;
	uint32_t	paddingGranularity;
	uint32_t	flags;
	uint32_t	totalFrames;
	uint32_t	initialFames;
	uint32_t	streams;
	uint32_t	suggestedBufferSize;
	uint32_t	width;
	uint32_t	height;
	uint32_t	reserved[4];
}	AVIMAINHEADER;
```
Followed by the header list, we can start filling in the main header. 
* **`fcc`** - The identifier (`'strh'` for streamheader)
* **`cb`**  - Size of everything below (again, not including fcc and cb)
* **`microSecPerFrame`** - 1000000 / Frames per sec
* **`maxBytesPerSec`** - How many bytes to buffer per second, this has [limitations](https://github.com/gmzorz/libgmavi/blob/32ab42bc94f3b971caca8d4ab038da1d2694d9d2/src/libgmavi.c#L98-L102)
* **`paddingGranularity`** - We don't care about padding: `0`
* **`flags`** - Must be `AVIF_HASINDEX`
* **`totalFrames`** - Undetermined, we save this address for now
* **`initialFames`** - Not applicable for noninterleaved videos: `0`
* **`streams`** -
* **`suggestedBufferSize`** - Probably Width * Height * bytes/pixel, VDub ignores this.
* **`width`** - Width in pixels
* **`height`** - Height in pixels
* **`reserved`** - Unused, zeroed

All done! most of these can be filled in based on just the Width, Height and frames per second. Even better: the `cb` variable can already be filled in [`STATIC_AVI_HEADER_SIZE`], because all the information that follows within this sub-chunk will be the same for all future video files. What we do need to save is the amount of **total frames**. Keep in mind, the offsets to these variables will get gradually harder to keep track of. Don't be shy to play around in your favorite Hex editor!
> Read more about the [AVIMAINHEADER](https://docs.microsoft.com/en-us/previous-versions/ms779632(v=vs.85)) structure

>Macros mentioned in this page can always be found in the [avistruct.h](https://github.com/gmzorz/libgmavi/blob/32ab42bc94f3b971caca8d4ab038da1d2694d9d2/src/aviStruct.h) file in this repo

Before we move on to the next struct we need to define another `RIFFLIST` for what is about to follow. This list contains the `'LIST'` FCC code, the size (already known): `STATIC_STREAM_LIST_SIZE`, and the type indication of the stream header: `'strl'`

#### **stream header**
```c++
typedef struct	_aviStreamHeader
{
	FOURCC			fcc;
	uint32_t		cb;
	FOURCC			fccType;
	FOURCC			fccHandler;
	uint32_t		flags;
	uint16_t		priority;
	uint16_t		language;
	uint32_t		initialFrames;
	uint32_t		scale;
	uint32_t		rate;
	uint32_t		start;
	uint32_t		length;
	uint32_t		suggestedBufferSize;
	uint32_t		quality;
	uint32_t		sampleSize;
	RECT			frame;
}	AVISTREAMHEADER;
```
Okay, no more bullshit. I am going to spoil all the secrets of this header. I am going to be skipping the variables we do not care so much about. You can always check these in detail on the MS RIFF Reference
* **`fcc`** - `FCC('strh')`
* **`cb`** - `STATIC_STREAM_HEADER_SIZE`
* **`fccType`** - `FCC('vids')` or for audio: `'auds'`
* **`fccHandler`** - `FCC('DIB ')` (device independent bitmap)
* **`scale`** - should be `1`, this is the scale of 'time' itself.
* **`rate`** - Frames Per Second
* **`start`** - `0`, we start at the beginning
* **`length`** - Undetermined
* **`suggestedBufferSize`** - `0xFFFFFFFF`
* **`quality`** - `0` for uncompressed
* **`frame`** - Size of the display frame `(RECT){0, 0, width, height}`

Yet again we have another variable to keep track of, which is the **length** of the video segment in the initial RIFF Chunk. This does not mean ALL frames, but only the ones that fit within the size limit.

The suggested buffer size is meant for the entire chunk, so in this case we want to set it to the UINT_MAX limit. The `RECT` structure consists of four shorts (2 bytes): top, bottom, left, and right.
> [AVISTREAMHEADER reference](https://docs.microsoft.com/en-us/previous-versions/ms779638(v=vs.85))

Following the declaration of the device independent bitmap (DIB) within the stream header, the actual writing of this structure comes next. Before we do this, we must first place a `RIFFCHUNK` structure that declares the stream header format, which is essentially a smaller `RIFFLIST` but without it's type.
```c++
typedef struct	_riffChunk
{
	FOURCC		fcc;
	uint32_t	cb;
}	RIFFCHUNK;
```
The fourcc code becomes `'strf'`, and the size will be a static value: `STATIC_STREAM_FORMAT_SIZE`
#### **Bitmap header**
```c++
typedef struct	_bitMapInfoHeader
{
	uint32_t		size;
	uint32_t		width;
	uint32_t		height;
	uint16_t		planes;
	uint16_t		bitCount;
	uint32_t		compression;
	uint32_t		sizeImage;
	uint32_t		xPelsPerMeter;
	uint32_t		yPelsPerMeter;
	uint32_t		clrUsed;
	uint32_t		clrImportant;
}	BITMAPINFOHEADER;
```
This header is not available within the aviriff.h header, but it is widely available in other resources. cb becomes `STATIC_BITMAP_HEADER_SIZE`, it will have `1` plane, and `24` as bit count. Everything else but the width/height is not required to complete this part.
> [BITMAPINFOHEADER reference](https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)

#### **Reserving space and inserting JUNK**

We are close to writing actual image data, but we need to keep something in mind. In order to utilize the 2GB workaround, OpenDML offers a Super Index in which we can store references to indices (or indexes) that are not within the main RIFF chunk. We don't actually have access to this data yet until we have written enough data.

A super index will contain a set of entries, and we specifically need to know the size of this entire struct so we can reserve some space using the `'JUNK'` FCC code, whilst making sure we can index enough frames. The index stores an array with each element being 32 bytes long, and you might think we need one for every frame. This is false, we are dealing with an *index of indexes*, meaning every entry is an index itself.

Every index of frames can hold up to 2GB of references, using a base offset in the index itself, which is 8 bytes and can be used across a file of more than 2^64-1 bytes (**18446744000** Gigabytes), and simple 4 byte offsets within each frame reference entry that communicate with the base offset.
Defining the size is left up to you, i went with 256 entries which will cover 512 Gigabytes of video data.
```c++
contents.superIndex = (AVISUPERINDEX)
{
      FCC('JUNK'),
      STATIC_SUPER_INDEX_SIZE,
};
```
> `STATIC_SUPER_INDEX_SIZE` is defined as 4120, which includes the super index size: `24` (minus 8 for type and cb), and the total entry size: `4096` (16 * 256).
>
> [Jump to AVISUPERINDEX](?)

One could define this as a `RIFFCHUNK`, but it is easier to keep the original structure to keep track of the variables we wish to seek in the future.
```c++
fileAddr.superIndex = (uint64_t)&contents.superIndex - fileAddr._fileBaseStart;
fileAddr.entriesInUse = (uint64_t)&contents.superIndex.entriesInUse - fileAddr._fileBaseStart;
fileAddr.superIndexEntries = (uint64_t)&contents.superIndex.index[0] - fileAddr._fileBaseStart;
```
In these examples, i use a struct holding all my addresses to keep track of, as well as `contents` which contains all of the previously explained structures. As it might be noticable, it becomes easier to compute the file offsets by simply subtracting them from the base address. Do keep in mind that most modern compilers optimize collections of variable types in a way that it messes with the byte count we need, preprocessor statements can fix this issue.
> [Structure **padding**](https://docs.microsoft.com/en-us/cpp/preprocessor/pack?view=msvc-170) is almost always applied during compile-time. The use of `#pragma pack(push,1)` is essential to our use case!

OpenDML also features an *extended header*, with the (complete) frame count specified. It is very minimal and can be expressed like so:
```c++
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
```
As shown above, we have another `RIFFLIST` with type `'odml'` and it's default sizes. The total frame count (`grandFrames`) will be filled in after the final frame has been written, so we save this address.

#### **JUNK & Alignment**

As mentioned above, JUNK can be placed anywhere in the file alongside a size indicator. JUNK is used in data alignment and to add comments to the file. I am a bit unsure as to whether this is necessary, but for safety i just did it the way VirtualDub does it.
```c++
contents.junk = (RIFFCHUNK){
      FCC('JUNK'),
      STATIC_JUNKFILL_SIZE
};
```
the `STATIC_JUNKFILL_SIZE` macro aligns the junk up until `0x2000`

#### **Video frames & Bitmap data**

We've made it past the header! finally, video frames are added, and at this point we can almost write the data we have collected so far.

```c++
contents.movi = (RIFFLIST){
      FCC('LIST'),
      TO_BE_DETERMINED,
      FCC('movi')
};

fileAddr.cbMovi = (uint64_t)&contents.movi.cb - fileAddr._fileBaseStart;
fileAddr.moviStart = fileAddr.cbMovi + 8;
```
Our final addition to our structure collection will be the list that holds all the frames. We do not only keep track of the size of this structure, but also the start of the array. This will become clear when we get to the indexing part.

Right now, the size of our structure collection will be that of the file size itself as well. If we subtract 8 from this size, we get the current value of the main RIFF `cb` variable. Instead of writing now, it is better to save the size in memory and only write the variable and other key elements when everything has been written.
```c++
out->fileHandler = fopen(filePath, "wb+");
fwrite(&out->contents, sizeof(gmavi_static_t), 1, out->fileHandler);
```

This concludes `gmav_open()`, we have successfully created a file that holds the AVI header. in `gmav_add()` we can start writing video frames.

The process is simple, the list is already defined so all that is left is two small additions to the image buffer itself. One being the fcc chunk ID and the other being the size. Chunk ID's can be taken from the aviriff.h file, which in this case will be `'00db'` (meaning: uncompressed frame). The size is simply the width multiplied by the height, times the color channels: `1920*1080*3`. We write the expressed `RIFFCHUNK`, followed by the image buffer.
> It is useful to store the size of the image buffer, we need to write this value a couple more times!

#### **Finishing up**

Assuming *n* amount of image buffers have been added, and the size does not exceed 2GB, finalizing the AVI file comes with a bit of ease. We still have write the final index (not super index, since we determined it to be JUNK) and update our saved addresses.
```c++
typedef struct	_aviOldIndex_entry
{
	uint32_t			chunkId;
	uint32_t			flags;
	uint32_t			offset;
	uint32_t			size;
}	AVIOLDINDEX_ENTRY;

typedef struct	_aviOldIndex
{
	FOURCC				fcc;
	uint32_t			cb;
}	AVIOLDINDEX;
```
The index is called the **old** index for a reason, it is part of the AVI 1.0 standard. VirtualDub also includes this list when using OpenDML for higher file-sizes, and it is not clear whether the DML index fully overrides the old index in any way, it is therefore always a good option to write it regardless.
In the old index, the fcc identifier for the old index becomes `'idx1'`, whereas the size becomes the size of an entry multiplied with the framecount.

Each entry has the corresponding chunkId `'00db'`, the `AVIF_HASINDEX` flag, the offset to the bitmap, and the size of the bitmap.
The offset starts from the first `'movi'` appearance, hence, it needs to be `4` for the first entry. The following entries have an offset to the next frames. One can simply add the size and the additional 8 bytes of the `RIFFCHUNK` (00db, size).

Filling up the array, Where `streamTickSize` is the size of the bitmap + 8:
```c++
for (uint32_t i = 0; i < avi->frameCount; i++) {
      avi->mainIndexEntries[i] = (AVIOLDINDEX_ENTRY){
            FCC('00db'),
            AVIF_HASINDEX,
            4 + avi->streamTickSize * i,
            avi->bitmapSize
      };
}
```

Finally, all bytes have been written, and the remaining task is to seek to the bytes we need to update:
* `fileAddr.cbMain`           ->    size of everything minus 8
* `fileAddr.firstFrames`      ->    frame count
* `fileAddr.totalFrames`      ->    frame count
* `fileAddr.grandFrames`      ->    frame count
* `fileAddr.cbMovi`           ->    size of the `movi` list

So many frame counts! well, they are currently all the same, the only exception would be `grandFrames` when we actually use the DML standard.

#### **AVI 2.0**
_(If you've come this far, why not continue?)_

Nothing to celebrate about... yet! We are almost there, the next steps are simple and the DML extensions are actually not that hard to implement.

When we reach a frame count that surpasses 2GB we have to still finish writing our base. The above applies, and our empty JUNK section will have to be filled in based on what we do next. Remember AVI 2.0 is an *extension* that, if you were to erase it, still allows the original AVI 1.0 file to be played.

First up is indexing the initial frames, up until the limit, we allocate and save a `AVISTDINDEX` struct with the fcc type `'ix00'` and create the indices for it:
```c++
avi->ix00[avi->riffChunks].avixIndex = (AVISTDINDEX){
      FCC('ix00'),                  /* identifier           */
      24 + size * 8,                /* size                 */ 
      2,                            /* longs per entry (2)  */
      0,                            /* no subtype           */
      AVI_INDEX_OF_CHUNKS,          /* index type           */
      size,                         /* amount of frames     */
      FCC('00db'),                  /* uncompressed         */
      avi->fileAddr.moviStart,      /* start of bitmap array*/
      0                             /* reserved             */
};

avi->ix00[avi->riffChunks].avixIndexEntries = (AVISTDINDEX_ENTRY *)calloc(1, sizeof(AVISTDINDEX_ENTRY) * size);
for (uint32_t i = 0; i < size; i++) {
      /* The offset is based on the start of the bitmap array (base offset) */
      avi->ix00[avi->riffChunks].avixIndexEntries[i].dwOffset = avi->streamTickSize * i;
      avi->ix00[avi->riffChunks].avixIndexEntries[i].dwSize = avi->bitmapSize;
}
```
Subsequently, the newer RIFF DML list needs to be initialized and written:
```c++
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
```
> keep track of size!

Any frame that exceeds the limit should be written under this new AVIX chunk, which is essentially the DML 2.0 identifier, and happens exactly the way we wrote the older frames. Every 2GB, a new AVIX chunk should always be created, and a corresponding index must be made.

#### **The super index**

The final index must be made based on the remaining frames, and finally we can actually start to write those to the end of the file.

Let's refer back to the super index, currently residing near the start of the file identified as JUNK: this will be the index that holds all the references (or pointers) to the `AVISTDINDEX` structs we made for each AVI(X) chunk. Time to fill it in:
```c++
AVISUPERINDEX	superIndex = {
      FCC('indx'),                  /* super index id       */
      STATIC_SUPER_INDEX_SIZE,      /* we already know this */
      4,                            /* longs per entry (4)  */
      0,                            /* no subtype           */
      0,                            /* no type              */
      avi->riffChunks + 1,          /* amount of chunks     */
      FCC('00db'),                  /* uncompressed         */
      {0, 0, 0},                    /* reserved             */
      TO_BE_DETERMINED              /* entries              */
};
_fseeki64(avi->fileHandler, avi->fileAddr.superIndex, SEEK_SET);
fwrite(&superIndex, sizeof(AVISUPERINDEX), 1, avi->fileHandler);
```
> In libgmavi, the first RIFF chunk is not counted, hence the extra +1

It is no longer classified as JUNK! it should be fairly straightforward to fill in the remaining entries that hold the 64bit `offset` to the start of each bitmap array, the `size` of the entire chunk, and the `duration` (frame count).

## **That was it?**

Pretty much, if you wish to ask questions, feel free to leave an issue or contact me directly.

