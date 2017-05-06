/*! @file GPMF_mp4reader.c
*
*  @brief Way Too Crude MP4 reader
*
*  @version 1.0.0
*
*  (C) Copyright 2017 GoPro Inc (http://gopro.com/).
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

/* This is not an elegant MP4 parser, only used to help demonstrate extraction of GPMF */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../GPMF_parser.h"

#ifdef WIN32
#define LONGSEEK  _fseeki64
#else
#define LONGSEEK  fseeko
#endif


uint32_t *metasizes = NULL;
uint32_t *metaoffsets = NULL;
uint32_t indexcount = 0;
float videolength = 0.0;
float metadatalength = 0.0;
uint32_t clockdemon, clockcount;
uint32_t trak_clockdemon, trak_clockcount;
uint32_t meta_clockdemon, meta_clockcount;
uint32_t basemetadataduration = 0;
uint32_t basemetadataoffset = 0;
FILE *fp = NULL;



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




#define MAKETAG(d,c,b,a)		(((d&0xff)<<24)|((c&0xff)<<16)|((b&0xff)<<8)|(a&0xff))



uint32_t GetNumberGPMFPayloads(void)
{
	return indexcount;
}


uint32_t *GetGPMFPayload(uint32_t *lastpayload, uint32_t index)
{
	uint32_t *GPMFbuffer = NULL;
	if (index < indexcount && fp)
	{
		GPMFbuffer = realloc(lastpayload, metasizes[index]);

		if (GPMFbuffer)
		{
			LONGSEEK(fp, metaoffsets[index], SEEK_SET);
			fread(GPMFbuffer, 1, metasizes[index], fp);
			return GPMFbuffer;
		}
	}
	return NULL;
}



void FreeGPMFPayload(uint32_t *lastpayload)
{
	free(lastpayload);
}


uint32_t GetGPMFPayloadSize(uint32_t index)
{
	return metasizes[index];
}


#define TRAK_TYPE		MAKEID('m', 'e', 't', 'a')		// track is the type for metadata
#define TRAK_SUBTYPE	MAKEID('g', 'p', 'm', 'd')		// subtype is GPMF

float OpenGPMFSource(char *filename)  //RAW or within MP4
{
	fp = fopen(filename, "rb");

	metasizes = NULL;
	metaoffsets = NULL;
	indexcount = 0;
	videolength = 0.0;
	metadatalength = 0.0;
	basemetadataduration = 0;
	basemetadataoffset = 0;
	
	if (fp)
	{
		uint32_t tag, qttag, qtsize, len, skip, type = 0, subtype = 0, num;
		int32_t lastsize = 0, nest = 0;

		len = fread(&tag, 1, 4, fp);
		if (tag == GPMF_KEY_DEVICE) // RAW GPMF data, not in an MP4
		{
			int filesize;

			videolength += 1.0;
			metadatalength += 1.0;

			fseek(fp, 0, SEEK_END);
			filesize = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			indexcount = (int)metadatalength;
			LONGSEEK(fp, 0, SEEK_SET); // back to start

			metasizes = (uint32_t *)malloc(indexcount * 4 + 4);  memset(metasizes, 0, indexcount * 4 + 4);
			metaoffsets = (uint32_t *)malloc(indexcount * 4 + 4);  memset(metaoffsets, 0, indexcount * 4 + 4);

			metasizes[0] = (filesize)&~3;
			metaoffsets[0] = 0;

			return metadatalength;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
		}
		LONGSEEK(fp, 0, SEEK_SET); // back to start

		do
		{
			len = fread(&qtsize, 1, 4, fp);
			len += fread(&qttag, 1, 4, fp);
			if (len == 8)
			{
				qtsize = BYTESWAP32(qtsize);

				if (!GPMF_VALID_FOURCC(qttag))
				{
					LONGSEEK(fp, lastsize - 8 - 8, SEEK_CUR);
					nest--;
					continue;
				}
				else
					nest++;

				lastsize = qtsize;

				if (qttag != MAKEID('m', 'o', 'o', 'v') && //skip over all but these atoms
					qttag != MAKEID('m', 'v', 'h', 'd') &&
					qttag != MAKEID('t', 'r', 'a', 'k') &&
					qttag != MAKEID('m', 'd', 'i', 'a') &&
					qttag != MAKEID('m', 'd', 'h', 'd') &&
					qttag != MAKEID('m', 'i', 'n', 'f') &&
					qttag != MAKEID('g', 'm', 'i', 'n') &&
					qttag != MAKEID('d', 'i', 'n', 'f') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('s', 't', 's', 'd') &&
					qttag != MAKEID('s', 't', 's', 's') &&
					qttag != MAKEID('s', 't', 's', 'c') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('s', 't', 'b', 'l') &&
					qttag != MAKEID('s', 't', 't', 's') &&
					qttag != MAKEID('s', 't', 's', 'z') &&
					qttag != MAKEID('s', 't', 'c', 'o') &&
					qttag != MAKEID('h', 'd', 'l', 'r'))
				{
					LONGSEEK(fp, qtsize - 8, SEEK_CUR);
					nest--;
				}
				else if (qttag == MAKEID('m', 'v', 'h', 'd')) //mvhd  movie header
				{
					len = fread(&skip, 1, 4, fp);
					len += fread(&skip, 1, 4, fp);
					len += fread(&skip, 1, 4, fp);
					len += fread(&clockdemon, 1, 4, fp); clockdemon = BYTESWAP32(clockdemon);
					len += fread(&clockcount, 1, 4, fp); clockcount = BYTESWAP32(clockcount);
					LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd
					nest--;
				}
				else if (qttag == MAKEID('m', 'd', 'h', 'd')) //mdhd  media header
				{
					media_header md;
					len = fread(&md, 1, sizeof(md), fp);
					if (len == sizeof(md))
					{
						md.creation_time = BYTESWAP32(md.creation_time);
						md.modification_time = BYTESWAP32(md.modification_time);
						md.time_scale = BYTESWAP32(md.time_scale);
						md.duration = BYTESWAP32(md.duration);

						trak_clockdemon = md.time_scale;
						trak_clockcount = md.duration;

						if (videolength == 0.0) // Get the video length from the first track
						{
							videolength = (float)((double)trak_clockcount / (double)trak_clockdemon);
						}
					}
					LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd
					nest--;
				}
				else if (qttag == MAKEID('h', 'd', 'l', 'r')) //hldr
				{
					uint32_t temp;
					len = fread(&skip, 1, 4, fp);
					len += fread(&skip, 1, 4, fp);
					len += fread(&temp, 1, 4, fp);  // type will be 'meta' for the correct trak.

					if (temp != MAKEID('a', 'l', 'i', 's'))
						type = temp;

					LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over hldr
					nest--;
				}
				else if (qttag == MAKEID('s', 't', 's', 'd')) //read the sample decription to determine the type of metadata
				{
					if (type == TRAK_TYPE) // meta 
					{
						len = fread(&skip, 1, 4, fp);
						len += fread(&skip, 1, 4, fp);
						len += fread(&skip, 1, 4, fp);
						len += fread(&subtype, 1, 4, fp);  // type will be 'meta' for the correct trak.
						if (len == 16)
						{
							if (subtype != TRAK_SUBTYPE) // GPMF metadata 
							{
								type = 0; // GPMF
							}
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stsd
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);
					
					nest--;
				}
				else if (qttag == MAKEID('s', 't', 's', 'z')) // metadata stsz - sizes
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, fp);
						len += fread(&skip, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						num = BYTESWAP32(num);
						if (metasizes) free(metasizes);
						metasizes = (uint32_t *)malloc(num * 4);
						if (metasizes)
						{
							len += fread(metasizes, 1, num * 4, fp);
							do
							{
								num--;
								metasizes[num] = BYTESWAP32(metasizes[num]);
							} while (num > 0);
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stsz
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					nest--;
				}
				else if (qttag == MAKEID('s', 't', 'c', 'o')) // metadata stco - offsets
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						num = BYTESWAP32(num);
						indexcount = num;
						if (metaoffsets) free(metaoffsets);
						metaoffsets = (uint32_t *)malloc(num * 4);
						if (metaoffsets)
						{
							len += fread(metaoffsets, 1, num * 4, fp);
							do
							{
								num--;
								metaoffsets[num] = BYTESWAP32(metaoffsets[num]);
							} while (num > 0);
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					nest--;
				}
				else if (qttag == MAKEID('s', 't', 't', 's')) // time to samples
				{
					if (type == TRAK_TYPE) // meta 
					{
						uint32_t totaldur = 0;
						int32_t entries = 0;
						len = fread(&skip, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						num = BYTESWAP32(num);
						entries = num;

						meta_clockdemon = trak_clockdemon;
						meta_clockcount = trak_clockcount;

						while (entries > 0)
						{
							int32_t samplecount;
							int32_t duration;
							len += fread(&samplecount, 1, 4, fp);
							samplecount = BYTESWAP32(samplecount);
							len += fread(&duration, 1, 4, fp);
							duration = BYTESWAP32(duration);

							if (samplecount > 1)
							{
								basemetadataoffset = totaldur;
								basemetadataduration = duration;
							}
							entries--;

							totaldur += duration;
							metadatalength += (float)((double)samplecount * (double)duration / (double)meta_clockdemon);
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					nest--;
				}
			}
			else
				break;

		} while (len > 0);
	}

	return metadatalength;
}


void CloseGPMFSource(void)
{
	if (fp) fclose(fp), fp = NULL;
	if (metasizes) free(metasizes), metasizes = 0;
	if (metaoffsets) free(metaoffsets), metaoffsets = 0;
}


uint32_t GetGPMFPayloadTime(uint32_t index, float *in, float *out)
{
	if (metaoffsets == 0 || basemetadataduration == 0 || meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	*in = (float)((double)index * (double)basemetadataduration / (double)meta_clockdemon);
	*out = (float)((double)(index+1) * (double)basemetadataduration / (double)meta_clockdemon);
	return 0;
}



float GetGPMFSampleRate(uint32_t fourcc)
{
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	uint32_t teststart = 0;
	uint32_t testend = indexcount-1;
	float rate = 0.0;

	if (indexcount < 1)
		return 0.0;

	if (indexcount > 3) // samples after first and before last are statisticly the best, avoiding camera start up or shutdown anomollies. 
	{
		teststart++;
		testend--;
	}

	uint32_t *payload = GetGPMFPayload(NULL, teststart); // second payload
	uint32_t payloadsize = GetGPMFPayloadSize(teststart);
	int32_t ret = GPMF_Init(ms, payload, payloadsize);

	if (ret != GPMF_OK)
		goto cleanup;

	{
		uint32_t startsamples = 0;
		uint32_t endsamples = 0;

		if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
		{
			uint32_t samples = GPMF_Repeat(ms);
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
			{
				startsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream)) - samples;

				payload = GetGPMFPayload(payload, testend); // second last payload
				payloadsize = GetGPMFPayloadSize(testend);
				ret = GPMF_Init(ms, payload, payloadsize);
				if (ret != GPMF_OK)
					goto cleanup;

				if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
				{
					GPMF_CopyState(ms, &find_stream);
					if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
					{
						endsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream));
						rate = (float)(endsamples - startsamples) / (metadatalength * ((float)(testend - teststart + 1)) / (float)indexcount);
						goto cleanup;
					}
				}
			}
			else // older GPMF sometimes missing the total sample count 
			{
				uint32_t payloadcount = teststart+1;
				while (payloadcount <= testend)
				{
					payload = GetGPMFPayload(payload, payloadcount); // second last payload
					payloadsize = GetGPMFPayloadSize(payloadcount);
					ret = GPMF_Init(ms, payload, payloadsize);

					if (ret != GPMF_OK)
						goto cleanup;

					if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
						samples += GPMF_Repeat(ms);

					payloadcount++;
				}

				rate = (float)(samples) / (metadatalength * ((float)(testend - teststart + 1)) / (float)indexcount);
				goto cleanup;
			}
		}
	}

cleanup:
	if(payload) if (payload) FreeGPMFPayload(payload); payload = NULL;

	return rate;
}


float GetGPMFSampleRateAndTimes(GPMF_stream *gs, float rate, uint32_t index, float *in, float *out)
{
	uint32_t key, insamples;
	uint32_t repeat, outsamples;
	GPMF_stream find_stream;

	if (gs == NULL && metaoffsets == 0 || indexcount == 0 || basemetadataduration == 0 || meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	key = GPMF_Key(gs);
	repeat = GPMF_Repeat(gs);
	if (rate == 0.0)
		rate = GetGPMFSampleRate(key);

	if (rate == 0.0)
	{
		*in = *out = 0.0;
		return 0.0;
	}

	GPMF_CopyState(gs, &find_stream);
	if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
	{
		outsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream));
		insamples = outsamples - repeat;

		*in = (float)((double)insamples / (double)rate);
		*out = (float)((double)outsamples / (double)rate);
	}
	else
	{
		// might too costly in some applications read all the samples to determine the clock jitter, here I return the estimate from the MP4 track.
		*in = (float)((double)index * (double)basemetadataduration / (double)meta_clockdemon);
		*out = (float)((double)(index + 1) * (double)basemetadataduration / (double)meta_clockdemon);
	}
	return rate;
}
