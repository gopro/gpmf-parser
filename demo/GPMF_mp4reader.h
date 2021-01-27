/*! @file mp4reader.h
*
*  @brief Way Too Crude MP4|MOV reader
*
*  @version 2.0.0
*
*  (C) Copyright 2017-2020 GoPro Inc (http://gopro.com/).
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
*
*/

#ifndef _GPMF_MP4READER_H
#define _GPMF_MP4READER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct media_header
{
	uint8_t version_flags[4];
	uint32_t creation_time;
	uint32_t modification_time;
	uint32_t time_scale;
	uint32_t duration;
	uint16_t language;
	uint16_t quality;
} media_header;


typedef struct SampleToChunk
{
	uint32_t chunk_num;
	uint32_t samples;
	uint32_t id;
} SampleToChunk;

#define MAX_TRACKS	16
typedef struct mp4object
{
	uint32_t *metasizes;
	uint32_t metasize_count;
	uint64_t *metaoffsets;
	uint32_t metastco_count;
	SampleToChunk *metastsc;
	uint32_t metastsc_count;
	uint32_t indexcount;
	double videolength;
	double metadatalength;
	int32_t metadataoffset_clockcount;
	uint32_t clockdemon, clockcount;
	uint32_t trak_clockdemon, trak_clockcount;
	uint32_t meta_clockdemon, meta_clockcount;
	uint32_t video_framerate_numerator;
	uint32_t video_framerate_denominator;
	uint32_t video_frames;
	double basemetadataduration;
	int32_t trak_edit_list_offsets[MAX_TRACKS];
	uint32_t trak_num;
	FILE *mediafp;
	uint64_t filesize;
	uint64_t filepos;
} mp4object;

enum mp4flag
{
	MP4_FLAG_READ_WRITE_MODE = 1 << 0,
};

typedef struct resObject
{
	uint32_t* buffer;
	uint32_t bufferSize;
} resObject;


#define MAKEID(a,b,c,d)			(((d&0xff)<<24)|((c&0xff)<<16)|((b&0xff)<<8)|(a&0xff))
#define STR2FOURCC(s)			((s[0]<<0)|(s[1]<<8)|(s[2]<<16)|(s[3]<<24))

#define BYTESWAP64(a)			(((a&0xff)<<56)|((a&0xff00)<<40)|((a&0xff0000)<<24)|((a&0xff000000)<<8) | ((a>>56)&0xff)|((a>>40)&0xff00)|((a>>24)&0xff0000)|((a>>8)&0xff000000) )
#define BYTESWAP32(a)			(((a&0xff)<<24)|((a&0xff00)<<8)|((a>>8)&0xff00)|((a>>24)&0xff))
#define BYTESWAP16(a)			((((a)>>8)&0xff)|(((a)<<8)&0xff00))
#define NOSWAP8(a)				(a)


typedef enum MP4READER_ERROR
{
	MP4_ERROR_OK = 0,
	MP4_ERROR_MEMORY
} MP4READER_ERROR;


#define MOV_GPMF_TRAK_TYPE		MAKEID('m', 'e', 't', 'a')		// track is the type for metadata
#define MOV_GPMF_TRAK_SUBTYPE	MAKEID('g', 'p', 'm', 'd')		// subtype is GPMF
#define MOV_VIDE_TRAK_TYPE		MAKEID('v', 'i', 'd', 'e')		// MP4 track for video
#define MOV_SOUN_TRAK_TYPE		MAKEID('s', 'o', 'u', 'n')		// MP4 track for audio
#define MOV_AVC1_SUBTYPE		MAKEID('a', 'v', 'c', '1')		// subtype H264
#define MOV_HVC1_SUBTYPE		MAKEID('h', 'v', 'c', '1')		// subtype H265
#define MOV_MP4A_SUBTYPE		MAKEID('m', 'p', '4', 'a')		// subtype for audio
#define MOV_CFHD_SUBTYPE		MAKEID('C', 'F', 'H', 'D')		// subtype is CineForm HD
#define AVI_VIDS_TRAK_TYPE		MAKEID('v', 'i', 'd', 's')		// track is the type for video
#define AVI_CFHD_SUBTYPE		MAKEID('c', 'f', 'h', 'd')		// subtype is CineForm HD

#define NESTSIZE(x) { int _i = nest; while (_i > 0 && nestsize[_i] > 0) { (nestsize[_i] >= x) ? (nestsize[_i]-=x) : (nestsize[_i]=0); if(nestsize[_i] <= 8) { nestsize[_i]=0; nest--; } _i--; } }

#define VALID_FOURCC(a)	(((((a>>24)&0xff)>='a'&&((a>>24)&0xff)<='z') || (((a>>24)&0xff)>='A'&&((a>>24)&0xff)<='Z') || (((a>>24)&0xff)>='0'&&((a>>24)&0xff)<='9') || (((a>>24)&0xff)==' ') ) && \
						( (((a>>16)&0xff)>='a'&&((a>>16)&0xff)<='z') || (((a>>16)&0xff)>='A'&&((a>>16)&0xff)<='Z') || (((a>>16)&0xff)>='0'&&((a>>16)&0xff)<='9') || (((a>>16)&0xff)==' ') ) && \
						( (((a>>8)&0xff)>='a'&&((a>>8)&0xff)<='z') || (((a>>8)&0xff)>='A'&&((a>>8)&0xff)<='Z') || (((a>>8)&0xff)>='0'&&((a>>8)&0xff)<='9') || (((a>>8)&0xff)==' ') ) && \
						( (((a>>0)&0xff)>='a'&&((a>>0)&0xff)<='z') || (((a>>0)&0xff)>='A'&&((a>>0)&0xff)<='Z') || (((a>>0)&0xff)>='0'&&((a>>0)&0xff)<='9') || (((a>>0)&0xff)==' ') )) 


size_t OpenMP4Source(char *filename, uint32_t traktype, uint32_t subtype, int32_t flags);
size_t OpenMP4SourceUDTA(char *filename, int32_t flags);
void CloseSource(size_t mp4Handle);
float GetDuration(size_t mp4Handle);
uint32_t GetVideoFrameRateAndCount(size_t mp4Handle, uint32_t *numer, uint32_t *demon);
uint32_t GetNumberPayloads(size_t mp4Handle);
uint32_t WritePayload(size_t mp4Handle, uint32_t* payload, uint32_t payloadsize, uint32_t index);
size_t GetPayloadResource(size_t mp4Handle, size_t resHandle, uint32_t payloadBufferSize);
void FreePayloadResource(size_t mp4Handle, size_t resHandle);
uint32_t* GetPayload(size_t mp4Handle, size_t resHandle, uint32_t index);
uint32_t GetPayloadSize(size_t mp4Handle, uint32_t index);
uint32_t GetPayloadTime(size_t mp4Handle, uint32_t index, double *in, double *out); //MP4 timestamps for the payload
uint32_t GetPayloadRationalTime(size_t mp4Handle, uint32_t index, int32_t *in_numerator, int32_t *out_numerator, uint32_t *denominator);
uint32_t GetEditListOffset(size_t mp4Handle, double *offset);
uint32_t GetEditListOffsetRationalTime(size_t mp4Handle, int32_t *offset_numerator, uint32_t *denominator);

#ifdef __cplusplus
}
#endif

#endif
