/*! @file mp4reader.c
*
*  @brief Way Too Crude MP4|MOV reader
*
*  @version 2.0.1
*
*  (C) Copyright 2017-2020 GoPro Inc (http://gopro.com/).
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
#include <sys/types.h>
#include <sys/stat.h>

#include "GPMF_mp4reader.h"

#define PRINT_MP4_STRUCTURE		0

#ifdef _WINDOWS
#define LONGTELL	_ftelli64
#else
#define LONGTELL	ftell
#endif


uint32_t GetNumberPayloads(size_t mp4handle)
{
	mp4object *mp4 = (mp4object *)mp4handle;

	if (mp4)
	{
		return mp4->indexcount;
	}

	return 0;
}


size_t GetPayloadResource(size_t mp4handle, size_t resHandle, uint32_t payloadsize)
{
	resObject* res = (resObject*)resHandle;

	if (res == NULL)
	{
		res = (resObject*)malloc(sizeof(resObject));
		if (res)
		{
			memset(res, 0, sizeof(resObject));
			resHandle = (size_t)res;
		}
	}

	if(res)
	{
		uint32_t myBufferSize = payloadsize + 256;

		if (res->buffer == NULL)
		{
			res->buffer = malloc(myBufferSize);
			if (res->buffer)
			{
				res->bufferSize = myBufferSize;
			}
			else
			{
				free(res);
				resHandle = 0;
			}
		}
		else if (payloadsize > res->bufferSize)
		{
			res->buffer = realloc(res->buffer, myBufferSize);
			res->bufferSize = myBufferSize;
			if (res->buffer == NULL)
			{
				free(res);
				resHandle = 0;
			}
		}
	}

	return resHandle;
}



void FreePayloadResource(size_t mp4handle, size_t resHandle)
{
	resObject* res = (resObject*)resHandle;

	if (res)
	{
		if (res->buffer) free(res->buffer);
		free(res);
	}
}


uint32_t *GetPayload(size_t mp4handle, size_t resHandle, uint32_t index)
{
	mp4object *mp4 = (mp4object *)mp4handle;
	resObject *res = (resObject *)resHandle;

	if (mp4 == NULL) return NULL;
	if (res == NULL) return NULL;

	if (index < mp4->indexcount && mp4->mediafp)
	{
		if ((mp4->filesize >= mp4->metaoffsets[index]+mp4->metasizes[index]) && (mp4->metasizes[index] > 0))
		{
			uint32_t buffsizeneeded = mp4->metasizes[index];  // Add a little more to limit reallocations

			resHandle = GetPayloadResource(mp4handle, resHandle, buffsizeneeded);
			if(resHandle)
			{
#ifdef _WINDOWS
				_fseeki64(mp4->mediafp, (__int64) mp4->metaoffsets[index], SEEK_SET);
#else
				fseeko(mp4->mediafp, (off_t) mp4->metaoffsets[index], SEEK_SET);
#endif
				fread(res->buffer, 1, mp4->metasizes[index], mp4->mediafp);
				mp4->filepos = mp4->metaoffsets[index] + mp4->metasizes[index];
				return res->buffer;
			}
		}
	}

	return NULL;
}


uint32_t WritePayload(size_t handle, uint32_t *payload, uint32_t payloadsize, uint32_t index)
{
	mp4object* mp4 = (mp4object*)handle;
	if (mp4 == NULL) return 0;

	if (index < mp4->indexcount && mp4->mediafp)
	{
		if ((mp4->filesize >= mp4->metaoffsets[index] + mp4->metasizes[index]) && mp4->metasizes[index] == payloadsize)
		{
#ifdef _WINDOWS
			_fseeki64(mp4->mediafp, (__int64)mp4->metaoffsets[index], SEEK_SET);
#else
			fseeko(mp4->mediafp, (off_t)mp4->metaoffsets[index], SEEK_SET);
#endif
			fwrite(payload, 1, payloadsize, mp4->mediafp);
			mp4->filepos = mp4->metaoffsets[index] + payloadsize;
			return payloadsize;
		}
	}

	return 0;
}



void LongSeek(mp4object *mp4, int64_t offset)
{
	if (mp4 && offset)
	{
		if (mp4->filepos + offset < mp4->filesize)
		{
#ifdef _WINDOWS
			_fseeki64(mp4->mediafp, (__int64)offset, SEEK_CUR);
#else
			fseeko(mp4->mediafp, (off_t)offset, SEEK_CUR);
#endif
			mp4->filepos += offset;
		}
		else
		{
			mp4->filepos = mp4->filesize;
		}
	}
}


uint32_t GetPayloadSize(size_t handle, uint32_t index)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0;

	if (mp4->metasizes && mp4->metasize_count > index)
		return mp4->metasizes[index] & (uint32_t)~0x3;  //All GPMF payloads are 32-bit aligned and sized

	return 0;
}


#define MAX_NEST_LEVEL	20

size_t OpenMP4Source(char *filename, uint32_t traktype, uint32_t traksubtype, int32_t flags)  //RAW or within MP4
{
	mp4object *mp4 = (mp4object *)malloc(sizeof(mp4object));
	if (mp4 == NULL) return 0;

	memset(mp4, 0, sizeof(mp4object));

#ifdef _WINDOWS
	struct _stat64 mp4stat;
	_stat64(filename, &mp4stat);
#else
	struct stat mp4stat;
	stat(filename, &mp4stat);
#endif
	mp4->filesize = (uint64_t) mp4stat.st_size;
//	printf("filesize = %ld\n", mp4->filesize);
	if (mp4->filesize < 64) 
	{
		free(mp4);
		return 0;
	}

	const char *mode = (flags & MP4_FLAG_READ_WRITE_MODE) ? "rb+" : "rb";
#ifdef _WINDOWS
	fopen_s(&mp4->mediafp, filename, mode);
#else
	mp4->mediafp = fopen(filename, mode);
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
			mp4->filepos += len;
			if (len == 8 && mp4->filepos < mp4->filesize)
			{
				if (mp4->filepos == 8 && qttag != MAKEID('f', 't', 'y', 'p'))
				{
					CloseSource((size_t)mp4);
					mp4 = NULL;
					break;
				}

				if (!VALID_FOURCC(qttag) && (qttag & 0xff) != 0xa9) // �xyz and �swr are allowed
				{
					CloseSource((size_t)mp4);
					mp4 = NULL;
					break;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					len = fread(&qtsize, 1, 8, mp4->mediafp);
					mp4->filepos += len;
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				if(qtsize-len > (mp4->filesize - mp4->filepos))  // not parser truncated files.
				{
					CloseSource((size_t)mp4);
					mp4 = NULL;
					break;
				}

				nest++;

				if (qtsize < 8) break;
				if (nest >= MAX_NEST_LEVEL) break;
				if (nest > 1 && qtsize > nestsize[nest - 1]) break;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

#if PRINT_MP4_STRUCTURE	

				for (int i = 1; i < nest; i++) printf("%5d ", nestsize[i]); //printf("    ");
				printf(" %c%c%c%c (%lld)\n", (qttag & 0xff), ((qttag >> 8) & 0xff), ((qttag >> 16) & 0xff), ((qttag >> 24) & 0xff), qtsize);

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p') ||
					qttag == MAKEID('u', 'd', 't', 'a') ||
					qttag == MAKEID('f', 'r', 'e', 'e'))
				{
					LongSeek(mp4, qtsize - 8);

					NESTSIZE(qtsize);

					continue;
				}
#endif
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
					qttag != MAKEID('s', 't', 'b', 'l') &&
					qttag != MAKEID('s', 't', 't', 's') &&
					qttag != MAKEID('s', 't', 's', 'c') &&
					qttag != MAKEID('s', 't', 's', 'z') &&
					qttag != MAKEID('s', 't', 'c', 'o') &&
					qttag != MAKEID('c', 'o', '6', '4') &&
					qttag != MAKEID('h', 'd', 'l', 'r') &&
					qttag != MAKEID('e', 'd', 't', 's'))
				{
					LongSeek(mp4, qtsize - 8);

					NESTSIZE(qtsize);
				}
				else
				{
					
					if (qttag == MAKEID('m', 'v', 'h', 'd')) //mvhd  movie header
					{
						len = fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&mp4->clockdemon, 1, 4, mp4->mediafp); mp4->clockdemon = BYTESWAP32(mp4->clockdemon);
						len += fread(&mp4->clockcount, 1, 4, mp4->mediafp); mp4->clockcount = BYTESWAP32(mp4->clockcount);

						mp4->filepos += len;
						LongSeek(mp4, qtsize - 8 - len); // skip over mvhd

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('t', 'r', 'a', 'k')) //trak header
					{

						if (mp4->trak_num+1 < MAX_TRACKS)
							mp4->trak_num++;

						NESTSIZE(8);
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

							if (mp4->trak_clockdemon == 0 || mp4->trak_clockcount == 0)
							{
								CloseSource((size_t)mp4);
								mp4 = NULL;
								break;
							}

							if (mp4->videolength == 0.0) // Get the video length from the first track
							{
								mp4->videolength = (float)((double)mp4->trak_clockcount / (double)mp4->trak_clockdemon);
							}
						}

						mp4->filepos += len;
						LongSeek(mp4, qtsize - 8 - len); // skip over mvhd

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('h', 'd', 'l', 'r')) //hldr
					{
						uint32_t temp;
						len = fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&temp, 1, 4, mp4->mediafp);  // type will be 'meta' for the correct trak.

						if (temp != MAKEID('a', 'l', 'i', 's') && temp != MAKEID('u', 'r', 'l', ' '))
							type = temp;

						mp4->filepos += len;
						LongSeek(mp4, qtsize - 8 - len); // skip over hldr

						NESTSIZE(qtsize);

					}
					else if (qttag == MAKEID('e', 'd', 't', 's')) //edit list
					{
						uint32_t elst,temp,readnum,i;
						len = fread(&skip, 1, 4, mp4->mediafp);
						len += fread(&elst, 1, 4, mp4->mediafp);
						if (elst == MAKEID('e', 'l', 's', 't'))
						{
							len += fread(&temp, 1, 4, mp4->mediafp);
							if (temp == 0)
							{
								len += fread(&readnum, 1, 4, mp4->mediafp);
								readnum = BYTESWAP32(readnum);
								if (readnum <= (qtsize / 12) && mp4->trak_clockdemon)
								{
									uint32_t segment_duration; //integer that specifies the duration of this edit segment in units of the movies time scale.
									uint32_t segment_mediaTime; //integer containing the starting time within the media of this edit segment(in media timescale units).If this field is set to 1, it is an empty edit.The last edit in a track should never be an empty edit.Any difference between the movies duration and the tracks duration is expressed as an implicit empty edit.
									uint32_t segment_mediaRate; //point number that specifies the relative rate at which to play the media corresponding to this edit segment.This rate value cannot be 0 or negative.
									for (i = 0; i < readnum; i++)
									{
										len += fread(&segment_duration, 1, 4, mp4->mediafp);
										len += fread(&segment_mediaTime, 1, 4, mp4->mediafp);
										len += fread(&segment_mediaRate, 1, 4, mp4->mediafp);

										segment_duration = BYTESWAP32(segment_duration);  // in MP4 clock base
										segment_mediaTime = BYTESWAP32(segment_mediaTime); // in trak clock base
										segment_mediaRate = BYTESWAP32(segment_mediaRate); // Fixed-point 65536 = 1.0X

										if (segment_mediaTime == 0xffffffff) // the segment_duration for blanked time
											mp4->trak_edit_list_offsets[mp4->trak_num] += (int32_t)segment_duration;  //samples are delay, data starts after presentation time zero.
										else if (i == 0) // If the first editlst starts after zero, the track is offset by this time (time before presentation time zero.)
											mp4->trak_edit_list_offsets[mp4->trak_num] -= (int32_t)((double)segment_mediaTime/(double)mp4->trak_clockdemon*(double)mp4->clockdemon); //convert to MP4 clock base.
									}
									if (type == traktype) // GPMF metadata 
									{
										mp4->metadataoffset_clockcount = mp4->trak_edit_list_offsets[mp4->trak_num]; //leave in MP4 clock base
									}
								}
							}
						}
						mp4->filepos += len;
						LongSeek(mp4, qtsize - 8 - len); // skip over edts

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
								if (subtype != traksubtype) // not MP4 metadata 
								{
									type = 0; // MP4
								}
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stsd
						}
						else
							LongSeek(mp4, qtsize - 8);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 's', 'c')) // metadata stsc - offset chunks
					{
						if (type == traktype) // meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);

							num = BYTESWAP32(num);
							if (num <= (qtsize/sizeof(SampleToChunk)))
							{
								mp4->metastsc_count = num;
								if (mp4->metastsc)
								{
									free(mp4->metastsc);
									mp4->metastsc = 0;
								}
								if (num > 0 && qtsize > (num * sizeof(SampleToChunk)))
								{
									mp4->metastsc = (SampleToChunk *)malloc(num * sizeof(SampleToChunk));
									if (mp4->metastsc)
									{
										len += fread(mp4->metastsc, 1, num * sizeof(SampleToChunk), mp4->mediafp);

										do
										{
											num--;
											mp4->metastsc[num].chunk_num = BYTESWAP32(mp4->metastsc[num].chunk_num);
											mp4->metastsc[num].samples = BYTESWAP32(mp4->metastsc[num].samples);
											mp4->metastsc[num].id = BYTESWAP32(mp4->metastsc[num].id);
										} while (num > 0);
									}
								}
								else
								{
									//size of null
									CloseSource((size_t)mp4);
									mp4 = NULL;
									break;
								}
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stsx
						}
						else
							LongSeek(mp4, qtsize - 8);

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
							// if equalsamplesize != 0, it is the size of all the samples and the length should be 20 (size,fourcc,flags,samplesize,samplecount)
                            if (qtsize >= (20 + (num * sizeof(uint32_t))) || (equalsamplesize != 0 && qtsize == 20))
							{
								if (mp4->metasizes)
								{
									free(mp4->metasizes);
									mp4->metasizes = 0;
								}
								//either the samples are different sizes or they are all the same size
								if(num > 0)
								{
									mp4->metasize_count = num;
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
								else
								{
									//size of null
									CloseSource((size_t)mp4);
									mp4 = NULL;
									break;
								}
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stsz
						}
						else
							LongSeek(mp4, qtsize - 8);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 'c', 'o')) // metadata stco - offsets
					{
						if (type == traktype) // meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);
							if (num <= ((qtsize - 8 - len) / sizeof(uint32_t)))
							{
								mp4->metastco_count = num;

								if (mp4->metastsc_count > 0 && num != mp4->metasize_count)
								{
									if (mp4->metaoffsets)
									{
										free(mp4->metaoffsets);
										mp4->metaoffsets = 0;
									}
									if (mp4->metastco_count && qtsize > (num * 4))
									{
										mp4->metaoffsets = (uint64_t*)malloc(mp4->metasize_count * 8);
										if (mp4->metaoffsets)
										{
											uint32_t* metaoffsets32 = NULL;
											metaoffsets32 = (uint32_t*)malloc(num * 4);
											if (metaoffsets32)
											{
												uint64_t fileoffset = 0;
												int stsc_pos = 0;
												int stco_pos = 0;
												len += fread(metaoffsets32, 1, num * 4, mp4->mediafp);
												do
												{
													num--;
													metaoffsets32[num] = BYTESWAP32(metaoffsets32[num]);
												} while (num > 0);

												fileoffset = metaoffsets32[stco_pos];
												mp4->metaoffsets[0] = fileoffset;

												num = 1;
												while (num < mp4->metasize_count)
												{
													if (num != mp4->metastsc[stsc_pos].chunk_num - 1 && 0 == (num - (mp4->metastsc[stsc_pos].chunk_num - 1)) % mp4->metastsc[stsc_pos].samples)
													{
														stco_pos++;
														fileoffset = (uint64_t)metaoffsets32[stco_pos];
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

												free(metaoffsets32);
											}
										}
									}
									else
									{
										//size of null
										CloseSource((size_t)mp4);
										mp4 = NULL;
										break;
									}
								}
								else
								{
									if (mp4->metaoffsets)
									{
										free(mp4->metaoffsets);
										mp4->metaoffsets = 0;
									}
									if (num > 0 && mp4->metasize_count && mp4->metasizes && qtsize > (num*4))
									{
										mp4->metaoffsets = (uint64_t*)malloc(num * 8);
										if (mp4->metaoffsets)
										{
											uint32_t* metaoffsets32 = NULL;
											metaoffsets32 = (uint32_t*)malloc(num * 4);
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
									else
									{
										//size of null
										CloseSource((size_t)mp4);
										mp4 = NULL;
										break;
									}
								}
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stco
						}
						else
							LongSeek(mp4, qtsize - 8);

						NESTSIZE(qtsize);
					}

					else if (qttag == MAKEID('c', 'o', '6', '4')) // metadata stco - offsets
					{
						if (type == traktype) // meta
						{
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);

							if(num == 0)
							{
								//size of null
								CloseSource((size_t)mp4);
								mp4 = NULL;
								break;
							}

							if (num <= ((qtsize - 8 - len) / sizeof(uint64_t)))
							{
								mp4->metastco_count = num;

								if (mp4->metastsc_count > 0 && num != mp4->metasize_count)
								{
									if (mp4->metaoffsets)
									{
										free(mp4->metaoffsets);
										mp4->metaoffsets = 0;
									}
									if (mp4->metasize_count && mp4->metasizes && qtsize > (num*8))
									{
										mp4->metaoffsets = (uint64_t*)malloc(mp4->metasize_count * 8);
										if (mp4->metaoffsets)
										{
											uint64_t* metaoffsets64 = NULL;
											metaoffsets64 = (uint64_t*)malloc(num * 8);
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
										//size of null
										CloseSource((size_t)mp4);
										mp4 = NULL;
										break;
									}
								}
								else
								{
									if (mp4->metaoffsets)
									{
										free(mp4->metaoffsets);
										mp4->metaoffsets = 0;
									}
									if (num > 0 && mp4->metasize_count && mp4->metasizes && qtsize > (num*8))
									{
										mp4->metaoffsets = (uint64_t*)malloc(num * 8);
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
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stco
						}
						else
							LongSeek(mp4, qtsize - 8);

						NESTSIZE(qtsize);
					}
					else if (qttag == MAKEID('s', 't', 't', 's')) // time to samples
					{
						if (type == MAKEID('v', 'i', 'd', 'e')) // video trak to get frame rate
						{
							uint32_t samples = 0;
							uint32_t entries = 0;
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);

							if (num <= (qtsize / 8))
							{
								entries = num;

								while (entries > 0)
								{
									uint32_t samplecount;
									uint32_t duration;
									len += fread(&samplecount, 1, 4, mp4->mediafp);
									samplecount = BYTESWAP32(samplecount);
									len += fread(&duration, 1, 4, mp4->mediafp);
									duration = BYTESWAP32(duration);

									samples += samplecount;
									entries--;

									if (mp4->video_framerate_numerator == 0)
									{
										mp4->video_framerate_numerator = mp4->trak_clockdemon;
										mp4->video_framerate_denominator = duration;
									}
								}
								mp4->video_frames = samples;
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stco
						}
						else
						if (type == traktype) // meta 
						{
							uint32_t totaldur = 0, samples = 0;
							uint32_t entries = 0;
							len = fread(&skip, 1, 4, mp4->mediafp);
							len += fread(&num, 1, 4, mp4->mediafp);
							num = BYTESWAP32(num);
							if (num <= (qtsize / 8))
							{
								entries = num;

								mp4->meta_clockdemon = mp4->trak_clockdemon;
								mp4->meta_clockcount = mp4->trak_clockcount;


								if(mp4->meta_clockdemon == 0) 
								{
									//prevent divide by zero
									CloseSource((size_t)mp4);
									mp4 = NULL;
									break;
								}

								while (entries > 0)
								{
									uint32_t samplecount;
									uint32_t duration;
									len += fread(&samplecount, 1, 4, mp4->mediafp);
									samplecount = BYTESWAP32(samplecount);
									len += fread(&duration, 1, 4, mp4->mediafp);
									duration = BYTESWAP32(duration);

									samples += samplecount;
									entries--;

									totaldur += duration;
									mp4->metadatalength += (double)((double)samplecount * (double)duration / (double)mp4->meta_clockdemon);
									if (samplecount > 1 || num == 1)
										mp4->basemetadataduration = mp4->metadatalength * (double)mp4->meta_clockdemon / (double)samples;
								}
							}
							mp4->filepos += len;
							LongSeek(mp4, qtsize - 8 - len); // skip over stco
						}
						else
							LongSeek(mp4, qtsize - 8);

						NESTSIZE(qtsize);
					}
					else
					{
						NESTSIZE(8);
					}
				}
			}
			else
			{
				break;
			}
		} while (len > 0);

		if (mp4)
		{
			if (mp4->metasizes == NULL || mp4->metaoffsets == NULL)
			{
				CloseSource((size_t)mp4);
				mp4 = NULL;
			}
			
			// set the numbers of payload with both size and offset
			if (mp4 != NULL)
			{
				mp4->indexcount = mp4->metasize_count;
			}
		}
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

uint32_t GetVideoFrameRateAndCount(size_t handle, uint32_t *numer, uint32_t *demon)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return 0;

	if (numer != NULL && demon != NULL && mp4->video_frames > 0)
	{
		*numer = mp4->video_framerate_numerator;
		*demon = mp4->video_framerate_denominator;
		return mp4->video_frames;
	}
	return 0;
}

void CloseSource(size_t handle)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) 
	{
		return;
	}

	if (mp4->mediafp)
	{
		fclose(mp4->mediafp);
		mp4->mediafp = NULL;
	}
	if (mp4->metasizes)
	{
		free(mp4->metasizes);
		mp4->metasizes = 0;
	}
	if (mp4->metaoffsets)
	{
		free(mp4->metaoffsets);
		mp4->metaoffsets = 0;
	}
	if (mp4->metastsc)
	{
		free(mp4->metastsc);
		mp4->metastsc = 0;
	}
 
 	free(mp4);
}


uint32_t GetPayloadTime(size_t handle, uint32_t index, double *in, double *out)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return MP4_ERROR_MEMORY;

	if (mp4->metaoffsets == 0 || mp4->basemetadataduration == 0 || mp4->meta_clockdemon == 0 || in == NULL || out == NULL) return MP4_ERROR_MEMORY;

	*in = ((double)index * (double)mp4->basemetadataduration / (double)mp4->meta_clockdemon);
	*out = ((double)(index + 1) * (double)mp4->basemetadataduration / (double)mp4->meta_clockdemon);

	if (*out > (double)mp4->metadatalength)
		*out = (double)mp4->metadatalength;

	// Add any Edit List offset
	*in += (double)mp4->metadataoffset_clockcount / (double)mp4->clockdemon;
	*out += (double)mp4->metadataoffset_clockcount / (double)mp4->clockdemon;
	return MP4_ERROR_OK;
}


uint32_t GetPayloadRationalTime(size_t handle, uint32_t index, int32_t *in_numerator, int32_t *out_numerator, uint32_t *denominator)
{
    mp4object *mp4 = (mp4object *)handle;
    if (mp4 == NULL) return MP4_ERROR_MEMORY;
    
    if (mp4->metaoffsets == 0 || mp4->basemetadataduration == 0 || mp4->meta_clockdemon == 0 || in_numerator == NULL || out_numerator == NULL) return MP4_ERROR_MEMORY;

	*in_numerator = (int32_t)(index * mp4->basemetadataduration);
	*out_numerator = (int32_t)((index + 1) * mp4->basemetadataduration);

	if (*out_numerator > (int32_t)((double)mp4->metadatalength*(double)mp4->meta_clockdemon))
		*out_numerator = (int32_t)((double)mp4->metadatalength*(double)mp4->meta_clockdemon);

	// Add any Edit List offset
	*in_numerator += (int32_t)(((double)mp4->metadataoffset_clockcount / (double)mp4->clockdemon) * mp4->meta_clockdemon);
	*out_numerator += (int32_t)(((double)mp4->metadataoffset_clockcount / (double)mp4->clockdemon) * mp4->meta_clockdemon);

	*denominator = mp4->meta_clockdemon;
    
    return MP4_ERROR_OK;
}


uint32_t GetEditListOffset(size_t handle, double *offset)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return MP4_ERROR_MEMORY;

	if (mp4->clockdemon == 0) return MP4_ERROR_MEMORY;

	*offset = (double)mp4->metadataoffset_clockcount / (double)mp4->clockdemon;

	return MP4_ERROR_OK;
}

uint32_t GetEditListOffsetRationalTime(size_t handle, int32_t *offset_numerator, uint32_t *denominator)
{
	mp4object *mp4 = (mp4object *)handle;
	if (mp4 == NULL) return MP4_ERROR_MEMORY;

	if (mp4->clockdemon == 0) return MP4_ERROR_MEMORY;

	*offset_numerator = mp4->metadataoffset_clockcount;
	*denominator = mp4->clockdemon;

	return MP4_ERROR_OK;
}



size_t OpenMP4SourceUDTA(char *filename, int32_t flags)
{
	mp4object *mp4 = (mp4object *)malloc(sizeof(mp4object));
	if (mp4 == NULL) return 0;

	memset(mp4, 0, sizeof(mp4object));

#ifdef _WINDOWS
	struct _stat64 mp4stat;
	_stat64(filename, &mp4stat);
#else
	struct stat mp4stat;
	stat(filename, &mp4stat);
#endif
	mp4->filesize = (uint64_t)mp4stat.st_size;
	if (mp4->filesize < 64) 
	{
		free(mp4);
		return 0;
	}

	const char *mode = (flags & MP4_FLAG_READ_WRITE_MODE) ? "rb+" : "rb";
#ifdef _WINDOWS
	fopen_s(&mp4->mediafp, filename, mode);
#else
	mp4->mediafp = fopen(filename, mode);
#endif

	if (mp4->mediafp)
	{
		uint32_t qttag, qtsize32;
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
				if (!VALID_FOURCC(qttag) && qttag != 0x7a7978a9)
				{
					mp4->filepos += len;
					LongSeek(mp4, lastsize - 8 - len);

					NESTSIZE(lastsize - 8);
					continue;
				}

				qtsize32 = BYTESWAP32(qtsize32);

				if (qtsize32 == 1) // 64-bit Atom
				{
					len += fread(&qtsize, 1, 8, mp4->mediafp);
					mp4->filepos += len;
					qtsize = BYTESWAP64(qtsize) - 8;
				}
				else
					qtsize = qtsize32;

				nest++;

				if (qtsize < 8) break;
				if (nest >= MAX_NEST_LEVEL) break;
				if (nest > 1 && qtsize > nestsize[nest - 1]) break;

				nestsize[nest] = qtsize;
				lastsize = qtsize;

				if (qttag == MAKEID('m', 'd', 'a', 't') ||
					qttag == MAKEID('f', 't', 'y', 'p'))
				{
					LongSeek(mp4, qtsize - 8);
					NESTSIZE(qtsize);
					continue;
				}

				if (qttag == MAKEID('G', 'P', 'M', 'F'))
				{
					mp4->videolength += 1.0;
					mp4->metadatalength += 1.0;

					mp4->indexcount = (uint32_t)mp4->metadatalength;

					mp4->metasizes = (uint32_t *)malloc(mp4->indexcount * 4 + 4);  memset(mp4->metasizes, 0, mp4->indexcount * 4 + 4);
					mp4->metaoffsets = (uint64_t *)malloc(mp4->indexcount * 8 + 8);  memset(mp4->metaoffsets, 0, mp4->indexcount * 8 + 8);

					mp4->basemetadataduration = 1.0;
					mp4->meta_clockdemon = 1;

					mp4->metasizes[0] = (uint32_t)qtsize - 8;
					mp4->metaoffsets[0] = (uint64_t) LONGTELL(mp4->mediafp);
					mp4->metasize_count = 1;

					return (size_t)mp4;  // not an MP4, RAW GPMF which has not inherent timing, assigning a during of 1second.
				}
				if (qttag != MAKEID('m', 'o', 'o', 'v') && //skip over all but these atoms
					qttag != MAKEID('u', 'd', 't', 'a'))
				{
					LongSeek(mp4, qtsize - 8);
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

