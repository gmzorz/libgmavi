#include "aviStruct.h"
#include <stdio.h>

int		main(void)
{
	RIFFLIST	start = (RIFFLIST){
		FOURCC_RIFF,
		0,
		FOURCC_TYPE_VIDEO
	};
	RIFFLIST	hdrl = (RIFFLIST){
		FOURCC_LIST,
		0,
		FOURCC_HEADER_LIST
	};
	AVIMAINHEADER	avih = (AVIMAINHEADER){
		FOURCC_AVI_HEADER,
		0,
		4000,
		1280 * 720 * 3,
		0,
		AVIF_HASINDEX,
		0,
		1,
		1280 * 720 * 3,
		120,
		720,
		{0, 0, 0, 0}
	};
	RIFFLIST	strl = (RIFFLIST){
		FOURCC_LIST,
		0,
		FOURCC_STREAM_LIST
	};
	
}