/*! @file mp4reader.c
*
*  @brief Way Too Crude MP4|MOV reader
*
*  @version 1.2.1
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

/* This is not an elegant MP4 parser, only used to help demonstrate extraction of MP4 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "GPMF_mp4reader.h"

#define PRINT_MP4_STRUCTURE		0

#ifdef WIN32
#define LONGSEEK  _fseeki64
#else
#define LONGSEEK  fseeko
#endif


uint32_t GetNumberPayloads(size_t handle)
{
	mp4object *mp4 = (mp4object *)handle;

	if (mp4)
	{
		return mp4->indexcount;
	}

	return 0;
}


uint32_t *GetPayload(size_t handle, uint32_t *lastpayload, uint32_t index)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return NULL;

	uint32_t *MP4buffer = NULL;
	if (index < mp4->indexcount && mp4->mediafp)
	{
		MP4buffer = (uint32_t *)realloc((void *)lastpayload, mp4->metasizes[index]);

		if (MP4buffer)
		{
			LONGSEEK(mp4->mediafp, mp4->metaoffsets[index], SEEK_SET);
			fread(MP4buffer, 1, mp4->metasizes[index], mp4->mediafp);
			return MP4buffer;
		}
	}
	return NULL;
}


void SavePayload(size_t handle, uint32_t *payload, uint32_t index)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return;

	uint32_t *MP4buffer = NULL;
	if (index < mp4->indexcount && mp4->mediafp && payload)
	{
		LONGSEEK(mp4->mediafp, mp4->metaoffsets[index], SEEK_SET);
		fwrite(payload, 1, mp4->metasizes[index], mp4->mediafp);
	}
	return;
}



void FreePayload(uint32_t *lastpayload)
{
	if (lastpayload)
		free(lastpayload);
}


uint32_t GetPayloadSize(size_t handle, uint32_t index)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0;

	if (mp4->metasizes && mp4->metasize_count > index)
		return mp4->metasizes[index];

	return 0;
}

#define MAX_NEST_LEVEL	20

size_t OpenMP4Source(char *filename, uint32_t traktype, uint32_t traksubtype)  //RAW or within MP4
{
	mp4object *mp4 = (mp4object *)malloc(sizeof(mp4object));
	if (mp4 == NULL) return 0;

	memset(mp4, 0, sizeof(mp4object));

#ifdef _WINDOWS
	fopen_s(&mp4->mediafp, filename, "rb");
#else
	mp4->mediafp = fopen(filename, "rb");
#endif

	if (mp4->mediafp)
	{
		uint32_t qttag, qtsize32, skip, type = 0, subtype = 0, num;
		size_t len;
		int32_t nest = 0;
		uint64_t nestsize[MAX_NEST_LEVEL] = { 0 };
		uint64_t lastsize = 0, qtsize;

		do
		{
			len = fread(&qtsize32, 1, 4, mp4->mediafp);
			len += fread(&qttag, 1, 4, mp4->mediafp);
			if (len == 8)
			{
				if (!VALID_FOURCC(qttag))
				{
					LONGSEEK(mp4->mediafp, lastsize - 8 - 8, SEEK_CUR);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					fread(&qtsize, 1, 8, mp4->mediafp);
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				if (qtsize < 8) break;
				if (nest >= MAX_NEST_LEVEL) break;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

#if PRINT_MP4_STRUCTURE	

				for (int i = 1; i < nest; i++) printf("    ");
				printf("%c%c%c%c (%lld)\n", (qttag & 0xff), ((qttag >> 8) & 0xff), ((qttag >> 16) & 0xff), ((qttag >> 24) & 0xff), qtsize);

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p') ||
					qttag == MAKEID('u', 'd', 't', 'a'))
				{
					LONGSEEK(mediafp, qtsize - 8, SEEK_CUR);

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
					LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

					NESTSIZE(qtsize);
				}
				else
#endif
					if (qttag == MAKEID('m', 'v', 'h', 'd')) //mvhd  movie header
					{
						len = fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&mp4->clockdemon, 1, 4, mp4->mediafp); mp4->clockdemon = BYTESWAP32(mp4->clockdemon);
						len += fread(&mp4->clockcount, 1, 4, mp4->mediafp); mp4->clockcount = BYTESWAP32(mp4->clockcount);
						LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('m', 'd', 'h', 'd')) //mdhd  media header
					{
						media_header md;
						len = fread(&md, 1, sizeof(md), mp4->mediafp);
						if (len == sizeof(md))
						{
							md.creation_time = BYTESWAP32(md.creation_time);
							md.modification_time = BYTESWAP32(md.modification_time);
							md.time_scale = BYTESWAP32(md.time_scale);
							md.duration = BYTESWAP32(md.duration);

							mp4->trak_clockdemon = md.time_scale;
							mp4->trak_clockcount = md.duration;

							if (mp4->videolength == 0.0) // Get the video length from the first track
							{
								mp4->videolength = (float)((double)mp4->trak_clockcount / (double)mp4->trak_clockdemon);
							}
						}
						LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over mvhd

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('h', 'd', 'l', 'r')) //hldr
					{
						uint32_t temp;
						len = fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&temp, 1, 4, mp4->mediafp);  // type will be 'meta' for the correct trak.

						if (temp != MAKEID('a', 'l', 'i', 's'))
							type = temp;

						LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over hldr

						NESTSIZE(qtsize);

					}
					else if (qttag == MAKEID('s', 't', 's', 'd')) //read the sample decription to determine the type of metadata
					{
						if (type == traktype) //like meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&subtype, 1, 4, mp4->mediafp);  // type will be 'meta' for the correct trak.
							if (len == 16)
							{
								if (subtype != traksubtype) // MP4 metadata 
								{
									type = 0; // MP4
								}
							}
							LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over stsd
						}
						else
							LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 's', 'c')) // metadata stsc - offset chunks
					{
						if (type == traktype) // meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);

							num = BYTESWAP32(num);
							if (num * 12 <= qtsize - 8 - len)
							{
								mp4->metastsc_count = num;
								if (mp4->metastsc) free(mp4->metastsc);
								mp4->metastsc = (SampleToChunk *)malloc(num * 12);
								if (mp4->metastsc)
								{
									uint32_t total_stsc = num;
									len += fread(mp4->metastsc, 1, num * sizeof(SampleToChunk), mp4->mediafp);

									do
									{
										num--;
										mp4->metastsc[num].chunk_num = BYTESWAP32(mp4->metastsc[num].chunk_num);
										mp4->metastsc[num].samples = BYTESWAP32(mp4->metastsc[num].samples);
										mp4->metastsc[num].id = BYTESWAP32(mp4->metastsc[num].id);
									} while (num > 0);
								}

								if (mp4->metastsc_count == 1 && mp4->metastsc[0].samples == 1) // Simplify if the stsc is not reporting any grouped chunks.
								{
									if (mp4->metastsc) free(mp4->metastsc);
									mp4->metastsc = NULL;
									mp4->metastsc_count = 0;
								}
							}
							LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over stsx
						}
						else
							LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 's', 'z')) // metadata stsz - sizes
					{
						if (type == traktype) // meta
						{
							uint32_t equalsamplesize;

							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&equalsamplesize, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);

							num = BYTESWAP32(num);
							if (num * 4 <= qtsize - 8 - len)
							{
								mp4->metasize_count = num;
								if (mp4->metasizes) free(mp4->metasizes);
								mp4->metasizes = (uint32_t *)malloc(num * 4);
								if (mp4->metasizes)
								{
									if (equalsamplesize == 0)
									{
										len += fread(mp4->metasizes, 1, num * 4, mp4->mediafp);
										do
										{
											num--;
											mp4->metasizes[num] = BYTESWAP32(mp4->metasizes[num]);
										} while (num > 0);
									}
									else
									{
										equalsamplesize = BYTESWAP32(equalsamplesize);
										do
										{
											num--;
											mp4->metasizes[num] = equalsamplesize;
										} while (num > 0);
									}
								}
							}
							LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over stsz
						}
						else
							LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 'c', 'o')) // metadata stco - offsets
					{
						if (type == traktype) // meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);
							if (num * 4 <= qtsize - 8 - len)
							{
								if (mp4->metastsc_count > 0 && num != mp4->metasize_count)
								{
									mp4->indexcount = mp4->metasize_count;
									if (mp4->metaoffsets) free(mp4->metaoffsets);
									mp4->metaoffsets = (uint64_t *)malloc(mp4->metasize_count * 8);
									if (mp4->metaoffsets)
									{
										uint32_t *metaoffsets32 = NULL;
										metaoffsets32 = (uint32_t *)malloc(num * 4);
										if (metaoffsets32)
										{
											uint64_t fileoffset = 0;
											int stsc_pos = 0;
											int stco_pos = 0;
											int repeat = 1;
											len += fread(metaoffsets32, 1, num * 4, mp4->mediafp);
											do
											{
												num--;
												metaoffsets32[num] = BYTESWAP32(metaoffsets32[num]);
											} while (num > 0);

											mp4->metaoffsets[0] = fileoffset = metaoffsets32[stco_pos];
											num = 1;
											while (num < mp4->metasize_count)
											{
												if (stsc_pos + 1 < (int)mp4->metastsc_count && num == stsc_pos)
												{
													stco_pos++; stsc_pos++;
													fileoffset = (uint64_t)metaoffsets32[stco_pos];
													repeat = 1;
												}
												else if (repeat == mp4->metastsc[stsc_pos].samples)
												{
													stco_pos++;
													fileoffset = (uint64_t)metaoffsets32[stco_pos];
													repeat = 1;
												}
												else
												{
													fileoffset += (uint64_t)mp4->metasizes[num - 1];
													repeat++;
												}

												mp4->metaoffsets[num] = fileoffset;
												//int delta = metaoffsets[num] - metaoffsets[num - 1];
												//printf("%3d:%08x, delta = %08x\n", num, (int)fileoffset, delta);

												num++;
											}

											if (mp4->metastsc) free(mp4->metastsc);
											mp4->metastsc = NULL;
											mp4->metastsc_count = 0;

											free(metaoffsets32);
										}
									}
								}
								else
								{
									mp4->indexcount = num;
									if (mp4->metaoffsets) free(mp4->metaoffsets);
									mp4->metaoffsets = (uint64_t *)malloc(num * 8);
									if (mp4->metaoffsets)
									{
										uint32_t *metaoffsets32 = NULL;
										metaoffsets32 = (uint32_t *)malloc(num * 4);
										if (metaoffsets32)
										{
											size_t readlen = fread(metaoffsets32, 1, num * 4, mp4->mediafp);
											len += readlen;
											do
											{
												num--;
												mp4->metaoffsets[num] = BYTESWAP32(metaoffsets32[num]);
											} while (num > 0);

											free(metaoffsets32);
										}
									}
								}
							}
							LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over stco
						}
						else
							LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

						NESTSIZE(qtsize);
					}

					else if (qttag == MAKEID('c', 'o', '6', '4')) // metadata stco - offsets
					{
						if (type == traktype) // meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);
							if (num * 8 <= qtsize - 8 - len)
							{
								if (mp4->metastsc_count > 0 && num != mp4->metasize_count)
								{
									mp4->indexcount = mp4->metasize_count;
									if (mp4->metaoffsets) free(mp4->metaoffsets);
									mp4->metaoffsets = (uint64_t *)malloc(mp4->metasize_count * 8);
									if (mp4->metaoffsets)
									{
										uint64_t *metaoffsets64 = NULL;
										metaoffsets64 = (uint64_t *)malloc(num * 8);
										if (metaoffsets64)
										{
											uint64_t fileoffset = 0;
											int stsc_pos = 0;
											int stco_pos = 0;
											len += fread(metaoffsets64, 1, num * 8, mp4->mediafp);
											do
											{
												num--;
												metaoffsets64[num] = BYTESWAP64(metaoffsets64[num]);
											} while (num > 0);

											fileoffset = metaoffsets64[0];
											mp4->metaoffsets[0] = fileoffset;
											//printf("%3d:%08x, delta = %08x\n", 0, (int)fileoffset, 0);

											num = 1;
											while (num < mp4->metasize_count)
											{
												if (num != mp4->metastsc[stsc_pos].chunk_num - 1 && 0 == (num - (mp4->metastsc[stsc_pos].chunk_num - 1)) % mp4->metastsc[stsc_pos].samples)
												{
													stco_pos++;
													fileoffset = (uint64_t)metaoffsets64[stco_pos];
												}
												else
												{
													fileoffset += (uint64_t)mp4->metasizes[num - 1];
												}

												mp4->metaoffsets[num] = fileoffset;
												//int delta = metaoffsets[num] - metaoffsets[num - 1];
												//printf("%3d:%08x, delta = %08x\n", num, (int)fileoffset, delta);

												num++;
											}

											if (mp4->metastsc) free(mp4->metastsc);
											mp4->metastsc = NULL;
											mp4->metastsc_count = 0;

											free(metaoffsets64);
										}
									}
								}
								else
								{
									mp4->indexcount = num;
									if (mp4->metaoffsets) free(mp4->metaoffsets);
									mp4->metaoffsets = (uint64_t *)malloc(num * 8);
									if (mp4->metaoffsets)
									{
										len += fread(mp4->metaoffsets, 1, num * 8, mp4->mediafp);
										do
										{
											num--;
											mp4->metaoffsets[num] = BYTESWAP64(mp4->metaoffsets[num]);
										} while (num > 0);
									}
								}
							}
							LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over stco
						}
						else
							LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 't', 's')) // time to samples
					{
						if (type == traktype) // meta 
						{
							uint32_t totaldur = 0, samples = 0;
							int32_t entries = 0;
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);
							if (num * 8 <= qtsize - 8 - len)
							{
								entries = num;

								mp4->meta_clockdemon = mp4->trak_clockdemon;
								mp4->meta_clockcount = mp4->trak_clockcount;

								while (entries > 0)
								{
									int32_t samplecount;
									int32_t duration;
									len += fread(&samplecount, 1, 4, mp4->mediafp);
									samplecount = BYTESWAP32(samplecount);
									len += fread(&duration, 1, 4, mp4->mediafp);
									duration = BYTESWAP32(duration);

									samples += samplecount;
									entries--;

									totaldur += duration;
									mp4->metadatalength += (double)((double)samplecount * (double)duration / (double)mp4->meta_clockdemon);
								}
								mp4->basemetadataduration = mp4->metadatalength * (double)mp4->meta_clockdemon / (double)samples;
							}
							LONGSEEK(mp4->mediafp, qtsize - 8 - len, SEEK_CUR); // skip over stco
						}
						else
							LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);

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
	else
	{
		//	printf("Could not open %s for input\n", filename);
		//	exit(1);

		free(mp4);
		mp4 = NULL;
	}

	return (size_t)mp4;
}


float GetDuration(size_t handle)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0.0;

	return (float)mp4->metadatalength;
}


void CloseSource(size_t handle)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return;

	if (mp4->mediafp) fclose(mp4->mediafp), mp4->mediafp = NULL;
	if (mp4->metasizes) free(mp4->metasizes), mp4->metasizes = 0;
	if (mp4->metaoffsets) free(mp4->metaoffsets), mp4->metaoffsets = 0;

	free(mp4);
}


uint32_t GetPayloadTime(size_t handle, uint32_t index, float *in, float *out)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0;

	if (mp4->metaoffsets == 0 || mp4->basemetadataduration == 0 || mp4->meta_clockdemon == 0 || in == NULL || out == NULL) return 1;

	*in = (float)((double)index * (double)mp4->basemetadataduration / (double)mp4->meta_clockdemon);
	*out = (float)((double)(index + 1) * (double)mp4->basemetadataduration / (double)mp4->meta_clockdemon);
	return 0;
}




size_t OpenMP4SourceUDTA(char *filename)
{
	mp4object *mp4 = (mp4object *)malloc(sizeof(mp4object));
	if (mp4 == NULL) return 0;

	memset(mp4, 0, sizeof(mp4object));

#ifdef _WINDOWS
	fopen_s(&mp4->mediafp, filename, "rb");
#else
	mp4->mediafp = fopen(filename, "rb");
#endif

	if (mp4->mediafp)
	{
		uint32_t qttag, qtsize32, len;
		int32_t nest = 0;
		uint64_t nestsize[MAX_NEST_LEVEL] = { 0 };
		uint64_t lastsize = 0, qtsize;

		do
		{
			len = fread(&qtsize32, 1, 4, mp4->mediafp);
			len += fread(&qttag, 1, 4, mp4->mediafp);
			if (len == 8)
			{
				if (!GPMF_VALID_FOURCC(qttag))
				{
					LONGSEEK(mp4->mediafp, lastsize - 8 - 8, SEEK_CUR);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					fread(&qtsize, 1, 8, mp4->mediafp);
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				if (qtsize < 8) break;
				if (nest >= MAX_NEST_LEVEL) break;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p'))
				{
					LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);
					NESTSIZE(qtsize);
					continue;
				}

				if (qttag == MAKEID('G', 'P', 'M', 'F'))
				{
					mp4->videolength += 1.0;
					mp4->metadatalength += 1.0;

					mp4->indexcount = (int)mp4->metadatalength;

					mp4->metasizes = (uint32_t *)malloc(mp4->indexcount * 4 + 4);  memset(mp4->metasizes, 0, mp4->indexcount * 4 + 4);
					mp4->metaoffsets = (uint64_t *)malloc(mp4->indexcount * 8 + 8);  memset(mp4->metaoffsets, 0, mp4->indexcount * 8 + 8);

					mp4->metasizes[0] = (int)qtsize - 8;
					mp4->metaoffsets[0] = ftell(mp4->mediafp);
					mp4->metasize_count = 1;

					return (size_t)mp4;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
				}
				if (qttag != MAKEID('m', 'o', 'o', 'v') && //skip over all but these atoms
					qttag != MAKEID('u', 'd', 't', 'a'))
				{
					LONGSEEK(mp4->mediafp, qtsize - 8, SEEK_CUR);
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
	return (size_t)mp4;
}


double GetGPMFSampleRate(size_t handle, uint32_t fourcc, uint32_t flags)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0.0;

	GPMF_stream metadata_stream, *ms = &metadata_stream;
	uint32_t teststart = 0;
	uint32_t testend = mp4->indexcount;
	double rate = 0.0;

	if (mp4->indexcount < 1)
		return 0.0;

	if (mp4->indexcount > 3) // samples after first and before last are statistically the best, avoiding camera start up or shutdown anomollies. 
	{
		teststart++;
		testend--;
	}

	uint32_t *payload = GetPayload(handle, NULL, teststart); // second payload
	uint32_t payloadsize = GetPayloadSize(handle, teststart);
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
			payload = GetPayload(handle, payload, teststart); // second last payload
			payloadsize = GetPayloadSize(handle, teststart);
			ret = GPMF_Init(ms, payload, payloadsize);
		}

		if (missing_samples)
		{
			teststart++;   //samples after sensor start are statistically the best
			payload = GetPayload(handle, payload, teststart);
			payloadsize = GetPayloadSize(handle, teststart);
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

				payload = GetPayload(handle, payload, testend); // second last payload
				payloadsize = GetPayloadSize(handle, testend);
				ret = GPMF_Init(ms, payload, payloadsize);
				if (ret != GPMF_OK)
					goto cleanup;

				if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS))
				{
					GPMF_CopyState(ms, &find_stream);
					if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
					{
						endsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream));
						rate = (double)(endsamples - startsamples) / (mp4->metadatalength * ((double)(testend - teststart + 1)) / (double)mp4->indexcount);
						goto cleanup;
					}
				}

				rate = (double)(samples) / (mp4->metadatalength * ((double)(testend - teststart + 1)) / (double)mp4->indexcount);
			}
			else // for increased precision, for older GPMF streams sometimes missing the total sample count 
			{
				uint32_t payloadpos = 0, payloadcount = 0;
				double slope, top = 0.0, bot = 0.0, meanX = 0, meanY = 0;
				uint32_t *repeatarray = malloc(mp4->indexcount * 4 + 4);
				memset(repeatarray, 0, mp4->indexcount * 4 + 4);

				samples = 0;

				for (payloadpos = teststart; payloadpos < testend; payloadcount++, payloadpos++)
				{
					payload = GetPayload(handle, payload, payloadpos); // second last payload
					payloadsize = GetPayloadSize(handle, payloadpos);
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
								float in, out;

								do
								{
									samples++;
								} while (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_CURRENT_LEVEL));

								repeatarray[payloadpos] = samples;
								meanY += (double)samples;

								GetPayloadTime(handle, payloadpos, &in, &out);
								meanX += out;
							}
						}
						else
						{
							uint32_t repeat = GPMF_Repeat(ms);
							samples += repeat;

							if (repeatarray)
							{
								float in, out;

								repeatarray[payloadpos] = samples;
								meanY += (double)samples;

								GetPayloadTime(handle, payloadpos, &in, &out);
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
						float in, out;
						GetPayloadTime(handle, payloadpos, &in, &out);

						top += ((double)out - meanX)*((double)repeatarray[payloadpos] - meanY);
						bot += ((double)out - meanX)*((double)out - meanX);
					}

					slope = top / bot;

#if 0
					// This sample code might be useful for compare data latency between channels.
					{
						double intercept;
						intercept = meanY - slope*meanX;
						printf("%c%c%c%c start offset = %f (%.3fms)\n", PRINTF_4CC(fourcc), intercept, 1000.0 * intercept / slope);
					}
#endif
					rate = slope;
				}
				else
				{
					rate = (double)(samples) / (mp4->metadatalength * ((double)(testend - teststart + 1)) / (double)mp4->indexcount);
				}

				free(repeatarray);

				goto cleanup;
			}
		}
	}

cleanup:
	if (payload)
		FreePayload(payload);
	payload = NULL;

	return rate;
}


double GetGPMFSampleRateAndTimes(size_t handle, GPMF_stream *gs, double rate, uint32_t index, double *in, double *out)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0.0;

	uint32_t key, insamples;
	uint32_t repeat, outsamples;
	GPMF_stream find_stream;

	if (gs == NULL || mp4->metaoffsets == 0 || mp4->indexcount == 0 || mp4->basemetadataduration == 0 || mp4->meta_clockdemon == 0 || in == NULL || out == NULL) return 0.0;

	key = GPMF_Key(gs);
	repeat = GPMF_Repeat(gs);
	if (rate == 0.0)
		rate = GetGPMFSampleRate(handle, key, GPMF_SAMPLE_RATE_FAST);

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
		*in = ((double)index * (double)mp4->basemetadataduration / (double)mp4->meta_clockdemon);
		*out = ((double)(index + 1) * (double)mp4->basemetadataduration / (double)mp4->meta_clockdemon);
	}
	return rate;
}
