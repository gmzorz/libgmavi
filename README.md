# libgmavi
A compact library made for simple video writing using uncompressed buffers. libgmavi is partially a study, and an addition to the [CODMVM](https://codmvm.com/) toolset, which handles multi-pass videogame recording whilst being hooked to the supported game. 

# Use case and example
_(not much knowledge required)_
This library is made for the sole purpose of writing RAW (uncompressed) AVI 2.0 video files. Specifically those of a higher file-size. The format can be particularly useful for raw image data interpretation for post-processing, video editing and compositing. The provided functions are minimal, and do not require much knowledge in video writing in order to function nicely. For example, one could easily make a simple still video using the following layout:
![alt text](https://github.com/gmzorz/libgmavi/blob/main/exmp.png)

# Theory
_(In case you've heard of file headers, padding, the BMP format, and hopefully had some run-ins with fseek/fwrite!)_
Nowadays the focus is on video encoding for web and live or realtime broadcasts. Packing and compressing videos is one step further into my research, so i figured starting from the roots would be the best way to approach it.

Because the focus is more on encoding, it was a bit hard to actually acquire the knowledge i had hoped to find on the internet, which was a bit of a shame. I will try and run through the process as clearly and simple as possible in the hopes that it might help those who are willing to learn more about the format

##### Understanding the format
A good place to start is getting to know the format itself. Wikipedia is always good, sure! But learning requires doing, and in my case i would fall asleep looking at the white front page of wikipedia. [VirtualDub](https://www.virtualdub.org/) uses the AVI 2.0 format and can produce videos from sequences in a matter of seconds. How? we don't really care too much at this point, but the output is more important to us.

Because we are dealing with uncompressed footage, we can easily view the structure of the AVI files using software like [HxD](https://mh-nexus.de/en/hxd/). Which is intimidating, but most definitely required when debugging file memory addresses. I later found out about useful software such as [RIFFPad](https://www.menasoft.com/blog/?p=34), which allows you to see the structure of the RIFF AVI file, and [010 Editor](https://www.sweetscape.com/010editor/), which does the same using it's plug-ins but has a memory viewer/editor and is capable of manipulation.

Lastly, and most importantly, the [Microsoft reference](https://docs.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference) and the [OpenDML reference](http://www.jmcgowan.com/odmlff2.pdf) are essential to this topic, the second article talks about eliminating the 2GB size limitation, which is simply caused by a 32bit variable in the header defining the size. Back in the day, nobody ever thought we needed AVI files of over 2GB.

##### A structure of structures
Basically what we are looking at. The format is written as a tree, which starts at the root: the RIFF AVI Chunk. This chunk contains all the data we need, up until the size limitation! Let's tackle this before we get to writing bigger files.

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
> _(Structures can be obtained from the `aviriff.h` header file, and are also included in this source)_

**Main RIFF AVI chunk (lists)**
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

**AVI main header**
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
Macros mentioned in this page can always be found in the [avistruct.h](https://github.com/gmzorz/libgmavi/blob/32ab42bc94f3b971caca8d4ab038da1d2694d9d2/src/aviStruct.h) file in this repo

Before we move on to the next struct we need to define another `RIFFLIST` for what is about to follow. This list contains the `'LIST'` FCC code, the size (already known): `STATIC_STREAM_LIST_SIZE`, and the type indication of the stream header: `'strl'`

**stream header**
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
**Bitmap header**
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

**Reserving space and inserting JUNK**

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
In these examples, i use a struct holding all my addresses to keep track of, as well as `contents` which contains all of the previously explained structures. As it might be noticable, it becomes easier to compute the file offsets.
> [Structure **padding**](https://docs.microsoft.com/en-us/cpp/preprocessor/pack?view=msvc-170) is almost always applied during compile-time. The use of `#pragma pack(push,1)` is essential to our use case!







# Links




# Dillinger
## _The Last Markdown Editor, Ever_

[![N|Solid](https://cldup.com/dTxpPi9lDf.thumb.png)](https://nodesource.com/products/nsolid)

[![Build Status](https://travis-ci.org/joemccann/dillinger.svg?branch=master)](https://travis-ci.org/joemccann/dillinger)

Dillinger is a cloud-enabled, mobile-ready, offline-storage compatible,
AngularJS-powered HTML5 Markdown editor.

- Type some Markdown on the left
- See HTML in the right
- ✨Magic ✨

## Features

- Import a HTML file and watch it magically convert to Markdown
- Drag and drop images (requires your Dropbox account be linked)
- Import and save files from GitHub, Dropbox, Google Drive and One Drive
- Drag and drop markdown and HTML files into Dillinger
- Export documents as Markdown, HTML and PDF

Markdown is a lightweight markup language based on the formatting conventions
that people naturally use in email.
As [John Gruber] writes on the [Markdown site][df1]

> The overriding design goal for Markdown's
> formatting syntax is to make it as readable
> as possible. The idea is that a
> Markdown-formatted document should be
> publishable as-is, as plain text, without
> looking like it's been marked up with tags
> or formatting instructions.

This text you see here is *actually- written in Markdown! To get a feel
for Markdown's syntax, type some text into the left window and
watch the results in the right.

## Tech

Dillinger uses a number of open source projects to work properly:

- [AngularJS] - HTML enhanced for web apps!
- [Ace Editor] - awesome web-based text editor
- [markdown-it] - Markdown parser done right. Fast and easy to extend.
- [Twitter Bootstrap] - great UI boilerplate for modern web apps
- [node.js] - evented I/O for the backend
- [Express] - fast node.js network app framework [@tjholowaychuk]
- [Gulp] - the streaming build system
- [Breakdance](https://breakdance.github.io/breakdance/) - HTML
to Markdown converter
- [jQuery] - duh

And of course Dillinger itself is open source with a [public repository][dill]
 on GitHub.

## Installation

Dillinger requires [Node.js](https://nodejs.org/) v10+ to run.

Install the dependencies and devDependencies and start the server.

```sh
cd dillinger
npm i
node app
```

For production environments...

```sh
npm install --production
NODE_ENV=production node app
```

## Plugins

Dillinger is currently extended with the following plugins.
Instructions on how to use them in your own application are linked below.

| Plugin | README |
| ------ | ------ |
| Dropbox | [plugins/dropbox/README.md][PlDb] |
| GitHub | [plugins/github/README.md][PlGh] |
| Google Drive | [plugins/googledrive/README.md][PlGd] |
| OneDrive | [plugins/onedrive/README.md][PlOd] |
| Medium | [plugins/medium/README.md][PlMe] |
| Google Analytics | [plugins/googleanalytics/README.md][PlGa] |

## Development

Want to contribute? Great!

Dillinger uses Gulp + Webpack for fast developing.
Make a change in your file and instantaneously see your updates!

Open your favorite Terminal and run these commands.

First Tab:

```sh
node app
```

Second Tab:

```sh
gulp watch
```

(optional) Third:

```sh
karma test
```

#### Building for source

For production release:

```sh
gulp build --prod
```

Generating pre-built zip archives for distribution:

```sh
gulp build dist --prod
```

## Docker

Dillinger is very easy to install and deploy in a Docker container.

By default, the Docker will expose port 8080, so change this within the
Dockerfile if necessary. When ready, simply use the Dockerfile to
build the image.

```sh
cd dillinger
docker build -t <youruser>/dillinger:${package.json.version} .
```

This will create the dillinger image and pull in the necessary dependencies.
Be sure to swap out `${package.json.version}` with the actual
version of Dillinger.

Once done, run the Docker image and map the port to whatever you wish on
your host. In this example, we simply map port 8000 of the host to
port 8080 of the Docker (or whatever port was exposed in the Dockerfile):

```sh
docker run -d -p 8000:8080 --restart=always --cap-add=SYS_ADMIN --name=dillinger <youruser>/dillinger:${package.json.version}
```

> Note: `--capt-add=SYS-ADMIN` is required for PDF rendering.

Verify the deployment by navigating to your server address in
your preferred browser.

```sh
127.0.0.1:8000
```

## License

MIT

**Free Software, Hell Yeah!**

[//]: # (These are reference links used in the body of this note and get stripped out when the markdown processor does its job. There is no need to format nicely because it shouldn't be seen. Thanks SO - http://stackoverflow.com/questions/4823468/store-comments-in-markdown-syntax)

   [dill]: <https://github.com/joemccann/dillinger>
   [git-repo-url]: <https://github.com/joemccann/dillinger.git>
   [john gruber]: <http://daringfireball.net>
   [df1]: <http://daringfireball.net/projects/markdown/>
   [markdown-it]: <https://github.com/markdown-it/markdown-it>
   [Ace Editor]: <http://ace.ajax.org>
   [node.js]: <http://nodejs.org>
   [Twitter Bootstrap]: <http://twitter.github.com/bootstrap/>
   [jQuery]: <http://jquery.com>
   [@tjholowaychuk]: <http://twitter.com/tjholowaychuk>
   [express]: <http://expressjs.com>
   [AngularJS]: <http://angularjs.org>
   [Gulp]: <http://gulpjs.com>

   [PlDb]: <https://github.com/joemccann/dillinger/tree/master/plugins/dropbox/README.md>
   [PlGh]: <https://github.com/joemccann/dillinger/tree/master/plugins/github/README.md>
   [PlGd]: <https://github.com/joemccann/dillinger/tree/master/plugins/googledrive/README.md>
   [PlOd]: <https://github.com/joemccann/dillinger/tree/master/plugins/onedrive/README.md>
   [PlMe]: <https://github.com/joemccann/dillinger/tree/master/plugins/medium/README.md>
   [PlGa]: <https://github.com/RahulHP/dillinger/blob/master/plugins/googleanalytics/README.md>
