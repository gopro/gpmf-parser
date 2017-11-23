/*! @file GPMF_mp4reader.c
 *
 *  @brief Way Too Crude MP4 reader
 *
 *  @version 1.1.1
 *
 *  (C) Copyright 2017 GoPro Inc (http://gopro.com/).
 *	
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0  
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
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

#include "GPMF_mp4reader.h"
#include "../GPMF_parser.h"

#define PRINT_MP4_STRUCTURE		0

#ifdef WIN32
#define LONGSEEK  _fseeki64
#else
#define LONGSEEK  fseeko
#endif


uint32_t *metasizes = NULL; 
uint32_t metasize_count = 0;
uint64_t *metaoffsets = NULL;
uint32_t indexcount = 0;
double videolength = 0.0;
double metadatalength = 0.0;
uint32_t clockdemon, clockcount;
uint32_t trak_clockdemon, trak_clockcount;
uint32_t meta_clockdemon, meta_clockcount;
uint32_t basemetadataduration = 0;
uint32_t basemetadataoffset = 0;
FILE *fp = NULL;
SampleToChunk *metastsc = NULL;
uint32_t metastsc_count = 0;


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
	else if(lastpayload)
	{
		free(lastpayload);
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

#define NESTSIZE(x) { int i = nest; while (i > 0 && nestsize[i] > 0) { nestsize[i] -= x; if(nestsize[i]>=0 && nestsize[i] <= 8) { nestsize[i]=0; nest--; } i--; } }



double OpenGPMFSourceUDTA(char *filename)
{
#ifdef _WINDOWS
	fopen_s(&fp, filename, "rb");
#else
	fp = fopen(filename, "rb");
#endif

	metasizes = NULL;
	metaoffsets = NULL;
	indexcount = 0;
	videolength = 0.0;
	metadatalength = 0.0;
	basemetadataduration = 0;
	basemetadataoffset = 0;

	if (fp)
	{
		uint32_t qttag, qtsize32, len;
		int32_t nest = 0;
		uint64_t nestsize[64] = { 0 };
		uint64_t lastsize = 0, qtsize;

		do
		{
			len = fread(&qtsize32, 1, 4, fp);
			len += fread(&qttag, 1, 4, fp);
			if (len == 8)
			{
				if (!GPMF_VALID_FOURCC(qttag))
				{
					LONGSEEK(fp, lastsize - 8 - 8, SEEK_CUR);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					fread(&qtsize, 1, 8, fp);
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p'))
				{
					LONGSEEK(fp, qtsize - 8, SEEK_CUR);
					NESTSIZE(qtsize);
					continue;
				}

				if (qttag == MAKEID('G', 'P', 'M', 'F'))
				{
					videolength += 1.0;
					metadatalength += 1.0;

					indexcount = (int)metadatalength;

					metasizes = (uint32_t *)malloc(indexcount * 4 + 4);  memset(metasizes, 0, indexcount * 4 + 4);
					metaoffsets = (uint64_t *)malloc(indexcount * 8 + 8);  memset(metaoffsets, 0, indexcount * 8 + 8);

					metasizes[0] = (int)qtsize-8;
					metaoffsets[0] = ftell(fp);

					return metadatalength;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
				}
				if (qttag != MAKEID('m', 'o', 'o', 'v') && //skip over all but these atoms
					qttag != MAKEID('u', 'd', 't', 'a'))
				{
					LONGSEEK(fp, qtsize - 8, SEEK_CUR);
					NESTSIZE(qtsize);
					continue;
				}
				else
				{
					NESTSIZE(8);
				}
			}
		} while (len > 0);
	}
	return metadatalength;
}


double OpenGPMFSource(char *filename)  //RAW or within MP4
{
#ifdef _WINDOWS
	fopen_s(&fp, filename, "rb");
#else
	fp = fopen(filename, "rb");
#endif

	metasizes = NULL;
	metaoffsets = NULL;
	indexcount = 0;
	videolength = 0.0;
	metadatalength = 0.0;
	basemetadataduration = 0;
	basemetadataoffset = 0;
	
	if (fp)
	{
		uint32_t tag, qttag, qtsize32, skip, type = 0, subtype = 0, num;
		size_t len;
		int32_t nest = 0;
		uint64_t nestsize[64] = { 0 };
		uint64_t lastsize = 0, qtsize;

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

			metasizes = (uint32_t *)malloc(indexcount * 4 + 4);  
			if (metasizes == NULL)
				return 0;

			metaoffsets = (uint64_t *)malloc(indexcount * 8 + 8);  
			if (metaoffsets == NULL)
			{
				free(metasizes);
				metasizes = 0;
				return 0;
			}

			memset(metasizes, 0, indexcount * 4 + 4); 
			memset(metaoffsets, 0, indexcount * 8 + 8);

			metasizes[0] = (filesize)&~3;
			metaoffsets[0] = 0;

			return metadatalength;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
		}
		LONGSEEK(fp, 0, SEEK_SET); // back to start

		do
		{
			len = fread(&qtsize32, 1, 4, fp);
			len += fread(&qttag, 1, 4, fp);
			if (len == 8)
			{
				if (!GPMF_VALID_FOURCC(qttag))
				{
					LONGSEEK(fp, lastsize - 8 - 8, SEEK_CUR);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					fread(&qtsize, 1, 8, fp);
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

#if PRINT_MP4_STRUCTURE	

				for (int i = 1; i < nest; i++) printf("    ");
				printf("%c%c%c%c (%lld)\n", (qttag & 0xff), ((qttag >> 8) & 0xff), ((qttag >> 16) & 0xff), ((qttag >> 24) & 0xff), qtsize);

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p') ||
					qttag == MAKEID('u', 'd', 't', 'a'))
				{
					LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);

					continue;
				}
#else
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
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('a', 'l', 'i', 's') &&
					qttag != MAKEID('s', 't', 'b', 'l') &&
					qttag != MAKEID('s', 't', 't', 's') &&
					qttag != MAKEID('s', 't', 's', 'c') &&
					qttag != MAKEID('s', 't', 's', 'z') &&
					qttag != MAKEID('s', 't', 'c', 'o') &&
					qttag != MAKEID('c', 'o', '6', '4') &&
					qttag != MAKEID('h', 'd', 'l', 'r'))
				{
					LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else 
#endif
				if (qttag == MAKEID('m', 'v', 'h', 'd')) //mvhd  movie header
				{
					len = fread(&skip, 1, 4, fp);
					len += fread(&skip, 1, 4, fp);
					len += fread(&skip, 1, 4, fp);
					len += fread(&clockdemon, 1, 4, fp); clockdemon = BYTESWAP32(clockdemon);
					len += fread(&clockcount, 1, 4, fp); clockcount = BYTESWAP32(clockcount);
					LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd

					NESTSIZE(qtsize);
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
							videolength = (double)trak_clockcount / (double)trak_clockdemon;
						}
					}
					LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd

					NESTSIZE(qtsize);
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

					NESTSIZE(qtsize);

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

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 's', 'c')) // metadata stsc - offset chunks
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						metastsc_count = num = BYTESWAP32(num);
						if (metastsc) free(metastsc);
						metastsc = (SampleToChunk *)malloc(num * 12);
						if (metastsc)
						{
							len += fread(metastsc, 1, num * sizeof(SampleToChunk), fp);

							do
							{
								num--;
								metastsc[num].chunk_num = BYTESWAP32(metastsc[num].chunk_num);
								metastsc[num].samples = BYTESWAP32(metastsc[num].samples);
								metastsc[num].id = BYTESWAP32(metastsc[num].id);
							} while (num > 0);
						}

						if (metastsc_count == 1 && metastsc[0].samples == 1) // Simplify if the stsc is not reporting any grouped chunks.
						{
							if (metastsc) free(metastsc);
							metastsc = NULL;
							metastsc_count = 0;
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stsz
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 's', 'z')) // metadata stsz - sizes
				{
					if (type == TRAK_TYPE) // meta
					{
						uint32_t equalsamplesize;
						
						len = fread(&skip, 1, 4, fp);
						len += fread(&equalsamplesize, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						metasize_count = num = BYTESWAP32(num);
						if (metasizes) free(metasizes);
						metasizes = (uint32_t *)malloc(num * 4);
						if (metasizes)
						{
							if (equalsamplesize == 0)
							{
								len += fread(metasizes, 1, num * 4, fp);
								do
								{
									num--;
									metasizes[num] = BYTESWAP32(metasizes[num]);
								} while (num > 0);
							}
							else
							{
								equalsamplesize = BYTESWAP32(equalsamplesize);
								do
								{
									num--;
									metasizes[num] = equalsamplesize;
								} while (num > 0);
							}
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stsz
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else if (qttag == MAKEID('s', 't', 'c', 'o')) // metadata stco - offsets
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						num = BYTESWAP32(num);
						if (metastsc_count > 0 && num != metasize_count)
						{
							indexcount = metasize_count;
							if (metaoffsets) free(metaoffsets);
							metaoffsets = (uint64_t *)malloc(metasize_count * 8);
							if (metaoffsets)
							{
								uint32_t *metaoffsets32 = NULL;
								metaoffsets32 = (uint32_t *)malloc(num * 4);
								if (metaoffsets32)
								{
									uint64_t fileoffset = 0;
									int stsc_pos = 0;
									int stco_pos = 0;
									len += fread(metaoffsets32, 1, num * 4, fp);
									do
									{
										num--;
										metaoffsets32[num] = BYTESWAP32(metaoffsets32[num]);
									} while (num > 0);

									fileoffset = metaoffsets32[0];
									metaoffsets[0] = fileoffset;

									num = 1;
									while (num < metasize_count)
									{
										if (num != metastsc[stsc_pos].chunk_num - 1 && 0 == (num - (metastsc[stsc_pos].chunk_num - 1)) % metastsc[stsc_pos].samples)
										{
											stco_pos++;
											fileoffset = (uint64_t)metaoffsets32[stco_pos];
										}
										else
										{
											fileoffset += (uint64_t)metasizes[num - 1];
										}

										metaoffsets[num] = fileoffset;
										num++;
									}

									if (metastsc) free(metastsc);
									metastsc_count = 0;

									free(metaoffsets32);
								}
							}
						}
						else
						{
							indexcount = num;
							if (metaoffsets) free(metaoffsets);
							metaoffsets = (uint64_t *)malloc(num * 8);
							if (metaoffsets)
							{
								uint32_t *metaoffsets32 = NULL;
								metaoffsets32 = (uint32_t *)malloc(num * 4);
								if (metaoffsets32)
								{
									len += fread(metaoffsets32, 1, num * 4, fp);
									do
									{
										num--;
										metaoffsets[num] = BYTESWAP32(metaoffsets32[num]);
									} while (num > 0);

									free(metaoffsets32);
								}
							}
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}

				else if (qttag == MAKEID('c', 'o', '6', '4')) // metadata stco - offsets
				{
					if (type == TRAK_TYPE) // meta
					{
						len = fread(&skip, 1, 4, fp);
						len += fread(&num, 1, 4, fp);
						num = BYTESWAP32(num);
						if (metastsc_count > 0 && num != metasize_count)
						{
							indexcount = metasize_count;
							if (metaoffsets) free(metaoffsets);
							metaoffsets = (uint64_t *)malloc(metasize_count * 8);
							if (metaoffsets)
							{
								uint64_t *metaoffsets64 = NULL;
								metaoffsets64 = (uint64_t *)malloc(num * 8);
								if (metaoffsets64)
								{
									uint64_t fileoffset = 0;
									int stsc_pos = 0;
									int stco_pos = 0;
									len += fread(metaoffsets64, 1, num * 8, fp);
									do
									{
										num--;
										metaoffsets64[num] = BYTESWAP64(metaoffsets64[num]);
									} while (num > 0);

									fileoffset = metaoffsets64[0];
									metaoffsets[0] = fileoffset;
									//printf("%3d:%08x, delta = %08x\n", 0, (int)fileoffset, 0);

									num = 1;
									while (num < metasize_count)
									{
										if (num != metastsc[stsc_pos].chunk_num - 1 && 0 == (num - (metastsc[stsc_pos].chunk_num - 1)) % metastsc[stsc_pos].samples)
										{
											stco_pos++;
											fileoffset = (uint64_t)metaoffsets64[stco_pos];
										}
										else
										{
											fileoffset += (uint64_t)metasizes[num - 1];
										}

										metaoffsets[num] = fileoffset;
										//int delta = metaoffsets[num] - metaoffsets[num - 1];
										//printf("%3d:%08x, delta = %08x\n", num, (int)fileoffset, delta);

										num++;
									}

									if (metastsc) free(metastsc);
									metastsc_count = 0;

									free(metaoffsets64);
								}
							}
						}
						else
						{
							indexcount = num;
							if (metaoffsets) free(metaoffsets);
							metaoffsets = (uint64_t *)malloc(num * 8);
							if (metaoffsets)
							{
								len += fread(metaoffsets, 1, num * 8, fp);
								do
								{
									num--;
									metaoffsets[num] = BYTESWAP64(metaoffsets[num]);
								} while (num > 0);
							}
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
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
							metadatalength += ((double)samplecount * (double)duration / (double)meta_clockdemon);
						}
						LONGSEEK(fp, qtsize - 8 - len, SEEK_CUR); // skip over stco
					}
					else
						LONGSEEK(fp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else
				{
					NESTSIZE(8);
				}
			}
			else
			{
				break;
			}
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


uint32_t GetGPMFPayloadTime(uint32_t index, double *in, double *out)
{
	if (metaoffsets == 0 || basemetadataduration == 0 || meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	*in = ((double)index * (double)basemetadataduration / (double)meta_clockdemon);
	*out = ((double)(index+1) * (double)basemetadataduration / (double)meta_clockdemon);
	return 0;
}



double GetGPMFSampleRate(uint32_t fourcc, uint32_t flags)
{
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	uint32_t teststart = 0;
	uint32_t testend = indexcount;
	double rate = 0.0;

	if (indexcount < 1)
		return 0.0;

	if (indexcount > 3) // samples after first and before last are statistically the best, avoiding camera start up or shutdown anomollies. 
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
		uint32_t missing_samples = 0;

		while (ret == GPMF_OK && GPMF_OK != GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
		{
			missing_samples = 1;
			teststart++;
			payload = GetGPMFPayload(payload, teststart); // second last payload
			payloadsize = GetGPMFPayloadSize(teststart);
			ret = GPMF_Init(ms, payload, payloadsize);
		}

		if (missing_samples)
		{
			teststart++;   //samples after sensor start are statistically the best
			payload = GetGPMFPayload(payload, teststart); 
			payloadsize = GetGPMFPayloadSize(teststart);
			ret = GPMF_Init(ms, payload, payloadsize);
		}

		if (ret == GPMF_OK)
		{
			uint32_t samples = GPMF_Repeat(ms);
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (!(flags & GPMF_SAMPLE_RATE_PRECISE) && GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
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
						rate = (double)(endsamples - startsamples) / (metadatalength * ((double)(testend - teststart + 1)) / (double)indexcount);
						goto cleanup;
					}
				}

				rate = (double)(samples) / (metadatalength * ((double)(testend - teststart + 1)) / (double)indexcount);
			}
			else // for increased precision, for older GPMF streams sometimes missing the total sample count 
			{
				uint32_t payloadpos = 0, payloadcount = 0;
				double slope, top = 0.0, bot = 0.0, meanX = 0, meanY = 0;
				uint32_t *repeatarray = malloc(indexcount * 4 + 4);
				memset(repeatarray, 0, indexcount * 4 + 4);

				samples = 0;

				for (payloadpos = teststart; payloadpos < testend; payloadcount++, payloadpos++)
				{
					payload = GetGPMFPayload(payload, payloadpos); // second last payload
					payloadsize = GetGPMFPayloadSize(payloadpos);
					ret = GPMF_Init(ms, payload, payloadsize);

					if (ret != GPMF_OK)
						goto cleanup;

					if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
					{
						GPMF_stream find_stream2;
						GPMF_CopyState(ms, &find_stream2);

						if (GPMF_OK == GPMF_FindNext(&find_stream2, fourcc, GPMF_CURRENT_LEVEL)) // Count the instances, not the repeats
						{
							if (repeatarray)
							{
								double in, out;

								do
								{
									samples++;
								} while (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_CURRENT_LEVEL));

								repeatarray[payloadpos] = samples;
								meanY += (double)samples;

								GetGPMFPayloadTime(payloadpos, &in, &out);
								meanX += out;
							} 
						}
						else
						{
							uint32_t repeat = GPMF_Repeat(ms);
							samples += repeat;

							if (repeatarray)
							{
								double in, out;

								repeatarray[payloadpos] = samples;
								meanY += (double)samples;

								GetGPMFPayloadTime(payloadpos, &in, &out);
								meanX += out;
							}						
						} 
					}
				}

				// Compute the line of best fit for a jitter removed sample rate.  
				// This does assume an unchanging clock, even though the IMU data can thermally impacted causing small clock changes.  
				// TODO: Next enhancement would be a low order polynominal fit the compensate for any thermal clock drift.
 				if (repeatarray)
				{
					meanY /= (double)payloadcount;
					meanX /= (double)payloadcount;

					for (payloadpos = teststart; payloadpos < testend; payloadpos++)
					{
						double in, out;
						GetGPMFPayloadTime(payloadpos, &in, &out);

						top += ((double)out - meanX)*((double)repeatarray[payloadpos] - meanY);
						bot += ((double)out - meanX)*((double)out - meanX);
					}

					slope = top / bot;

#if 0
					// This sample code might be useful for compare data latency between channels.
					{
						double intercept;
						intercept = meanY - slope*meanX;
						printf("%c%c%c%c start offset = %f (%.3fms)\n", PRINTF_4CC(fourcc), intercept, 1000.0 * intercept / slope );
					}
#endif
					rate = slope;
				}
				else
				{
					rate = (double)(samples) / (metadatalength * ((double)(testend - teststart + 1)) / (double)indexcount);
				}
				
				free(repeatarray);

				goto cleanup;
			}
		}
	}

cleanup:
	if(payload) if (payload) FreeGPMFPayload(payload); payload = NULL;

	return rate;
}


double GetGPMFSampleRateAndTimes(GPMF_stream *gs, double rate, uint32_t index, double *in, double *out)
{
	uint32_t key, insamples;
	uint32_t repeat, outsamples;
	GPMF_stream find_stream;

	if (gs == NULL || metaoffsets == 0 || indexcount == 0 || basemetadataduration == 0 || meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	key = GPMF_Key(gs);
	repeat = GPMF_Repeat(gs);
	if (rate == 0.0)
		rate = GetGPMFSampleRate(key, GPMF_SAMPLE_RATE_FAST);

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

		*in = ((double)insamples / (double)rate);
		*out = ((double)outsamples / (double)rate);
	}
	else
	{
		// might too costly in some applications read all the samples to determine the clock jitter, here I return the estimate from the MP4 track.
		*in = ((double)index * (double)basemetadataduration / (double)meta_clockdemon);
		*out = ((double)(index + 1) * (double)basemetadataduration / (double)meta_clockdemon);
	}
	return rate;
}
