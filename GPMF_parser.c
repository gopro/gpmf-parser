/*! @file GPMF_parser.c
 * 
 *  @brief GPMF Parser library
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "GPMF_parser.h"


#ifdef DBG
#if _WINDOWS
#define DBG_MSG printf
#else
#define DBG_MSG(...)
#endif
#else
#define DBG_MSG(...)
#endif

GPMF_ERR GPMF_Validate(GPMF_stream *ms, GPMF_LEVELS recurse)
{
	if (ms)
	{
		uint32_t currpos = ms->pos;
		int32_t nestsize = (int32_t)ms->nest_size[ms->nest_level];
		if (nestsize == 0 && ms->nest_level == 0)
			nestsize = ms->buffer_size_longs;
		
		while (nestsize > 0)
		{
			uint32_t key = ms->buffer[ms->pos];

			if (ms->nest_level == 0 && key != GPMF_KEY_DEVICE && ms->device_count == 0)
			{
				return GPMF_ERROR_BAD_STRUCTURE;
			}

			if (GPMF_VALID_FOURCC(key))
			{
				uint32_t type_size_repeat = ms->buffer[ms->pos + 1];
				int32_t size = GPMF_DATA_SIZE(type_size_repeat) >> 2;
				uint8_t type = GPMF_SAMPLE_TYPE(type_size_repeat);
				if (size + 2 > nestsize)
				{
					DBG_MSG("ERROR: nest size too small within %c%c%c%c-- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
					return GPMF_ERROR_BAD_STRUCTURE;
				}

				if (!GPMF_VALID_FOURCC(key))
				{
					DBG_MSG("ERROR: invalid 4CC -- GPMF_ERROR_BAD_STRUCTURE\n");
					return GPMF_ERROR_BAD_STRUCTURE;
				}

				if (type == GPMF_TYPE_NEST)
				{
					uint32_t validnest;
					ms->pos += 2;
					ms->nest_level++;
					if (ms->nest_level > GPMF_NEST_LIMIT)
					{
						DBG_MSG("ERROR: nest level within %c%c%c%c too deep -- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
						return GPMF_ERROR_BAD_STRUCTURE;
					}
					ms->nest_size[ms->nest_level] = size;
					validnest = GPMF_Validate(ms, recurse);
					ms->nest_level--;
					if (GPMF_OK != validnest)
					{
						DBG_MSG("ERROR: invalid nest within %c%c%c%c -- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
						return GPMF_ERROR_BAD_STRUCTURE;
					}
					else
					{
						if (ms->nest_level == 0)
							ms->device_count++;
					}

					ms->pos += size;
					nestsize -= 2 + size;
				}
				else
				{
					ms->pos += 2 + size;
					nestsize -= 2 + size;
				}
			}
			else
			{
				if (ms->nest_level == 0 && ms->device_count > 0)
					return GPMF_OK;
				else
				{
					DBG_MSG("ERROR: bad struct within %c%c%c%c -- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
					return GPMF_ERROR_BAD_STRUCTURE;
				}
			}
		}

		ms->pos = currpos;

		return GPMF_OK;
	}
	else
	{
		DBG_MSG("ERROR: Invalid handle -- GPMF_ERROR_MEMORY\n");
		return GPMF_ERROR_MEMORY;
	}
}


GPMF_ERR GPMF_ResetState(GPMF_stream *ms)
{
	if (ms)
	{
		ms->pos = 0;
		ms->nest_level = 0;
		ms->device_count = 0;
		ms->nest_size[ms->nest_level] = 0;
		ms->last_level_pos[ms->nest_level] = 0;
		ms->last_seek = 0;
		ms->device_id = 0;
		ms->device_name[0] = 0;

		return GPMF_OK;
	}
	
	return GPMF_ERROR_MEMORY;
}


GPMF_ERR GPMF_Init(GPMF_stream *ms, uint32_t *buffer, int datasize)
{
	if(ms)
	{
		ms->buffer = buffer;
		ms->buffer_size_longs = datasize >>2;

		GPMF_ResetState(ms);

		return GPMF_OK;
	}
	
	return GPMF_ERROR_MEMORY;
}


GPMF_ERR GPMF_CopyState(GPMF_stream *msrc, GPMF_stream *mdst)
{
	if (msrc && mdst)
	{
		memcpy(mdst, msrc, sizeof(GPMF_stream));
		return GPMF_OK;
	}
	return GPMF_ERROR_MEMORY;
}


GPMF_ERR GPMF_Next(GPMF_stream *ms, GPMF_LEVELS recurse)
{
	if (ms)
	{
		if (ms->pos < ms->buffer_size_longs)
		{

			uint32_t key, type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
			uint32_t size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);

			if (GPMF_TYPE_NEST == type && GPMF_KEY_DEVICE == ms->buffer[ms->pos] && ms->nest_level == 0)
			{
				ms->last_level_pos[ms->nest_level] = ms->pos;
				ms->nest_size[ms->nest_level] = size;
				ms->pos += 2;
			}
			else
			{
				if (size + 2 > ms->nest_size[ms->nest_level])
					return GPMF_ERROR_BAD_STRUCTURE;

				if (recurse && type == GPMF_TYPE_NEST)
				{
					ms->last_level_pos[ms->nest_level] = ms->pos;
					ms->pos += 2;
					ms->nest_size[ms->nest_level] -= size + 2;

					ms->nest_level++;
					if (ms->nest_level > GPMF_NEST_LIMIT)
						return GPMF_ERROR_BAD_STRUCTURE;

					ms->nest_size[ms->nest_level] = size;
				}
				else
				{
					if (recurse)
					{
						ms->pos += size + 2;
						ms->nest_size[ms->nest_level] -= size + 2;
					}
					else
					{
						if (ms->nest_size[ms->nest_level] - (size + 2) > 0)
						{
							ms->pos += size + 2;
							ms->nest_size[ms->nest_level] -= size + 2;
						}
						else
						{
							return GPMF_ERROR_LAST;   
						}
					}
				}
			}

			while (ms->nest_size[ms->nest_level] == 0 && ms->nest_level > 0)
			{
				ms->nest_level--;
				if (ms->nest_level == 0)
				{
					//ms->device_count++;
				}
			}

			if (ms->pos < ms->buffer_size_longs)
			{
				key = ms->buffer[ms->pos];
				if (!GPMF_VALID_FOURCC(key))
					return GPMF_ERROR_BAD_STRUCTURE;

				if (key == GPMF_KEY_DEVICE_ID)
					ms->device_id = BYTESWAP32(ms->buffer[ms->pos + 2]);
				if (key == GPMF_KEY_DEVICE_NAME)
				{
					size = GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]); // in bytes
					if (size > sizeof(ms->device_name) - 1)
						size = sizeof(ms->device_name) - 1;
					memcpy(ms->device_name, &ms->buffer[ms->pos + 2], size);
					ms->device_name[size] = 0;
				}
			}
			else
			{
				// end of buffer
				return GPMF_ERROR_BUFFER_END;
			}

			return GPMF_OK;
		}
		else
		{
			// end of buffer
			return GPMF_ERROR_BUFFER_END;
		}
	}
	return GPMF_ERROR_MEMORY;
}



GPMF_ERR GPMF_FindNext(GPMF_stream *ms, uint32_t fourcc, GPMF_LEVELS recurse)
{
	GPMF_stream prevstate;

	if (ms)
	{
		memcpy(&prevstate, ms, sizeof(GPMF_stream));

		if (ms->pos < ms->buffer_size_longs)
		{
			while (0 == GPMF_Next(ms, recurse))
			{
				if (ms->buffer[ms->pos] == fourcc)
				{
					return GPMF_OK; //found match
				}
			}

			// restore read position
			memcpy(ms, &prevstate, sizeof(GPMF_stream));
			return GPMF_ERROR_FIND;
		}
	}
	return GPMF_ERROR_FIND;
}

GPMF_ERR GPMF_SeekToSamples(GPMF_stream *ms)
{
	GPMF_stream prevstate;

	if (ms)
	{

		if (ms->pos < ms->buffer_size_longs)
		{
			uint32_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

			memcpy(&prevstate, ms, sizeof(GPMF_stream));

			if (type == GPMF_TYPE_NEST)
				GPMF_Next(ms, GPMF_RECURVSE_LEVELS); // open STRM and recurse in

			while (0 == GPMF_Next(ms, GPMF_CURRENT_LEVEL))
			{
				uint32_t size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);
				type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);


				if (type == GPMF_TYPE_NEST)  // Nest with-in nest
				{
					return GPMF_OK; //found match
				}

				if (size + 2 == ms->nest_size[ms->nest_level])
				{
					return GPMF_OK; //found match
				}

				if (ms->buffer[ms->pos] == ms->buffer[ms->pos + size + 2]) // Matching tags
				{
					return GPMF_OK; //found match
				}
			}

			// restore read position
			memcpy(ms, &prevstate, sizeof(GPMF_stream));
			return GPMF_ERROR_FIND;
		}
	}
	return GPMF_ERROR_FIND;
}


GPMF_ERR GPMF_FindPrev(GPMF_stream *ms, uint32_t fourcc)
{
	GPMF_stream prevstate;

	if (ms)
	{
		memcpy(&prevstate, ms, sizeof(GPMF_stream));

		if (ms->pos < ms->buffer_size_longs && ms->nest_level > 0)
		{
			ms->last_seek = ms->pos;
			ms->pos = ms->last_level_pos[ms->nest_level-1] + 2;
			ms->nest_size[ms->nest_level] += ms->last_seek - ms->pos;
			do
			{
				if (ms->last_seek > ms->pos && ms->buffer[ms->pos] == fourcc)
				{

					return GPMF_OK; //found match
				}
			} while (ms->last_seek > ms->pos && 0 == GPMF_Next(ms, GPMF_CURRENT_LEVEL));

			// restore read position
			memcpy(ms, &prevstate, sizeof(GPMF_stream));

			return GPMF_ERROR_FIND;
		}
	}

	return GPMF_ERROR_FIND;
}





uint32_t GPMF_Key(GPMF_stream *ms)
{
	if (ms)
	{
		uint32_t key = ms->buffer[ms->pos];
		return key;
	}
	return 0;
}


uint32_t GPMF_Type(GPMF_stream *ms)
{
	if (ms && ms->pos < ms->buffer_size_longs)
	{
		uint32_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos+1]);
		return type;
	}
	return 0;
}


uint32_t GPMF_StructSize(GPMF_stream *ms)
{
	if (ms && ms->pos < ms->buffer_size_longs)
	{
		uint32_t ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		return ssize;
	}
	return 0;
}


uint32_t GPMF_ElementsInStruct(GPMF_stream *ms)
{
	if (ms && ms->pos < ms->buffer_size_longs)
	{
		uint32_t ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		GPMF_SampleType type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

		if (type != GPMF_TYPE_NEST && type != GPMF_TYPE_COMPLEX)
		{
			int32_t tsize = GPMF_SizeofType(type);
			if (tsize > 0)
				return ssize / tsize;
			else
				return 0;
		}

		if (type == GPMF_TYPE_COMPLEX)
		{
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE))
			{
				char tmp[64] = "";
				char *data = (char *)GPMF_RawData(&find_stream);
				int size = GPMF_RawDataSize(&find_stream);
				
				GPMF_ExpandComplexTYPE(data, size, tmp, sizeof(tmp));
				return strlen(tmp);
			}
		}
	}
	return 0;
}


uint32_t GPMF_Repeat(GPMF_stream *ms)
{
	if (ms && ms->pos < ms->buffer_size_longs)
	{
		uint32_t repeat = GPMF_SAMPLES(ms->buffer[ms->pos + 1]);
		return repeat;
	}
	return 0;
}

uint32_t GPMF_RawDataSize(GPMF_stream *ms)
{
	if (ms && ms->pos < ms->buffer_size_longs)
	{
		uint32_t size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1])*GPMF_SAMPLES(ms->buffer[ms->pos + 1]);
		return size;
	}
	return 0;
}


uint32_t GPMF_NestLevel(GPMF_stream *ms)
{
	if (ms)
	{
		return ms->nest_level;
	}
	return 0;
}

uint32_t GPMF_DeviceID(GPMF_stream *ms)
{
	if (ms)
	{
		return ms->device_id;
	}
	return 0;
}

GPMF_ERR GPMF_DeviceName(GPMF_stream *ms, char *devicenamebuf, uint32_t devicename_buf_size)
{
	if (ms && devicenamebuf)
	{
		if (strlen(ms->device_name) < devicename_buf_size)
			return GPMF_ERROR_MEMORY;

		memcpy(devicenamebuf, ms->device_name, strlen(ms->device_name));
		return GPMF_OK;
	}
	return GPMF_ERROR_MEMORY;
}


void *GPMF_RawData(GPMF_stream *ms)
{
	if (ms)
	{
		return (void *)&ms->buffer[ms->pos + 2];
	}
	return NULL;
}




uint32_t GPMF_SizeofType(GPMF_SampleType type)
{
	uint32_t ssize = 0;

	switch ((int)type)
	{
	case GPMF_TYPE_STRING_ASCII:		ssize = 1; break;
	case GPMF_TYPE_SIGNED_BYTE:			ssize = 1; break;
	case GPMF_TYPE_UNSIGNED_BYTE:		ssize = 1; break;

	// These datatypes are always be stored in Big-Endian
	case GPMF_TYPE_SIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_UNSIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_FLOAT:				ssize = 4; break;
	case GPMF_TYPE_FOURCC:				ssize = 4; break;
	case GPMF_TYPE_SIGNED_LONG:			ssize = 4; break;
	case GPMF_TYPE_UNSIGNED_LONG:		ssize = 4; break;
	case GPMF_TYPE_Q15_16_FIXED_POINT:  ssize = 4; break;
	case GPMF_TYPE_Q31_32_FIXED_POINT:  ssize = 8; break;
	case GPMF_TYPE_DOUBLE:				ssize = 8; break;
	case GPMF_TYPE_SIGNED_64BIT_INT:	ssize = 8; break;
	case GPMF_TYPE_UNSIGNED_64BIT_INT:  ssize = 8; break;

	//All unknown or larger than 8-bytes stored as is:
	case GPMF_TYPE_GUID:				ssize = 16; break;
	case GPMF_TYPE_UTC_DATE_TIME:		ssize = 16; break;
	}

	return ssize;
}

uint32_t GPMF_ExpandComplexTYPE(char *src, uint32_t srcsize, char *dst, uint32_t dstsize)
{
	uint32_t i = 0, k = 0, count = 0;

	while (i<srcsize && k<dstsize)
	{
		if (src[i] == '[' && i>0)
		{
			int j = 1;
			count = 0;
			while (src[i + j] >= '0' && src[i + j] <= '9')
			{
				count *= 10;
				count += src[i + j] - '0';
				j++;
			}

			if (count > 1)
			{
				uint32_t l;
				for (l = 1; l<count; l++)
				{
					dst[k] = src[i - 1];
					k++;
				}
			}
			i += j;
			if (src[i] == ']') i++;
		}
		else
		{
			dst[k] = src[i];
			if (dst[k] == 0) break;
			i++, k++;
		}
	}

	if (k >= dstsize)
		return GPMF_ERROR_MEMORY; // bad structure formed

	dst[k] = 0;

	return GPMF_OK;
}



uint32_t GPMF_SizeOfComplexTYPE(char *typearray)
{
	uint32_t size = 0;
	int i,len = strlen(typearray);
	for (i = 0; i < len; i++)
	{
		uint32_t typesize = GPMF_SizeofType((GPMF_SampleType)typearray[i]);

		if (typesize < 1) return 0;
		size += typesize;
	}

	return size;
}


GPMF_ERR GPMF_FormattedData(GPMF_stream *ms, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples)
{
	if (ms && buffer)
	{
		uint8_t *data = (uint8_t *)&ms->buffer[ms->pos + 2];
		uint8_t *output = (uint8_t *)buffer;
		uint32_t sample_size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		uint32_t remaining_sample_size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 1]);
		uint8_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		uint32_t typesize = 1;
		uint32_t elements = 0;
		char complextype[64] = "L";

		if (type == GPMF_TYPE_NEST)
			return GPMF_ERROR_MEMORY;

		if (sample_size * read_samples > buffersize)
			return GPMF_ERROR_MEMORY;

		remaining_sample_size -= sample_offset * sample_size; // skip samples
		data += sample_offset * sample_size;

		if (remaining_sample_size < sample_size * read_samples)
			return GPMF_ERROR_MEMORY;

		if (type == GPMF_TYPE_COMPLEX)
		{
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE))
			{
				char *data1 = (char *)GPMF_RawData(&find_stream);
				int size = GPMF_RawDataSize(&find_stream);

				if (size < sizeof(complextype))
				{
					memcpy(complextype, data1, size);
					complextype[size] = 0;
				}
			}

			if (GPMF_SizeOfComplexTYPE(complextype) != sample_size)
				return GPMF_ERROR_MEMORY;


			elements = strlen(complextype);
		}
		else
		{
			typesize = GPMF_SizeofType((GPMF_SampleType)type);

			if (type == GPMF_TYPE_FOURCC)
				typesize = 1; // Do not ByteSWAP

			if (typesize == 0)
				return GPMF_ERROR_MEMORY;

			elements = sample_size / typesize;
		}

		while (read_samples--)
		{
			uint32_t i,j;

			for (i = 0; i < elements; i++)
			{
				if (type == GPMF_TYPE_COMPLEX)
				{
					if (complextype[i] == GPMF_TYPE_FOURCC)
					{
						*output++ = *data++;
						*output++ = *data++;
						*output++ = *data++;
						*output++ = *data++;
						typesize = 0;
					}
					else
						typesize = GPMF_SizeofType(complextype[i]);
				}

				switch (typesize)
				{
				case 2:
				{
					uint16_t *data16 = (uint16_t *)data;
					uint16_t *output16 = (uint16_t *)output;
					*output16 = BYTESWAP16(*data16);
					output16++;
					data16++;

					data = (uint8_t *)data16;
					output = (uint8_t *)output16;
				}
				break;
				case 4:
				{
					uint32_t *data32 = (uint32_t *)data;
					uint32_t *output32 = (uint32_t *)output;
					*output32 = BYTESWAP32(*data32);
					output32++;
					data32++;

					data = (uint8_t *)data32;
					output = (uint8_t *)output32;
				}
				break;
				case 8:
				{
					uint64_t *data64 = (uint64_t *)data;
					uint64_t *output64 = (uint64_t *)output;
					*output64 = BYTESWAP64(*data64);
					output64++;
					data64++;

					data = (uint8_t *)data64;
					output = (uint8_t *)output64;
				}
				break;
				default: //1, 16 or more not byteswapped
					for (j = 0; j < typesize; j++)
						*output++ = *data++;
					break;
				}
			}
		}

		return GPMF_OK;
	}

	return GPMF_ERROR_MEMORY;
}


#define MACRO_CAST_SCALE_UNSIGNED_TYPE(casttype)		\
{																												\
	casttype *tmp = (casttype *)output;																			\
	switch (scaletype)																							\
	{																											\
	case GPMF_TYPE_SIGNED_BYTE:		*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((int8_t *)scaledata8);	break;	\
	case GPMF_TYPE_UNSIGNED_BYTE:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((uint8_t *)scaledata8);	break;	\
	case GPMF_TYPE_SIGNED_SHORT:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((int16_t *)scaledata8);	break;	\
	case GPMF_TYPE_UNSIGNED_SHORT:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((uint16_t *)scaledata8);	break;	\
	case GPMF_TYPE_SIGNED_LONG:		*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((int32_t *)scaledata8);	break;	\
	case GPMF_TYPE_UNSIGNED_LONG:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((uint32_t *)scaledata8);	break;  \
	case GPMF_TYPE_FLOAT:			*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((float *)scaledata8);	break;	\
	default: break;																								\
	}																											\
	output = (uint8_t *)tmp;																					\
}

#define MACRO_CAST_SCALE_SIGNED_TYPE(casttype)		\
{																											\
	casttype *tmp = (casttype *)output;																		\
	switch (scaletype)																						\
	{																										\
	case GPMF_TYPE_SIGNED_BYTE:		*tmp++ = (casttype)*val / (casttype)*((int8_t *)scaledata8);	break;	\
	case GPMF_TYPE_UNSIGNED_BYTE:	*tmp++ = (casttype)*val / (casttype)*((uint8_t *)scaledata8);	break;	\
	case GPMF_TYPE_SIGNED_SHORT:	*tmp++ = (casttype)*val / (casttype)*((int16_t *)scaledata8);	break;	\
	case GPMF_TYPE_UNSIGNED_SHORT:	*tmp++ = (casttype)*val / (casttype)*((uint16_t *)scaledata8);	break;	\
	case GPMF_TYPE_SIGNED_LONG:		*tmp++ = (casttype)*val / (casttype)*((int32_t *)scaledata8);	break;	\
	case GPMF_TYPE_UNSIGNED_LONG:	*tmp++ = (casttype)*val / (casttype)*((uint32_t *)scaledata8);	break;  \
	case GPMF_TYPE_FLOAT:			*tmp++ = (casttype)*val / (casttype)*((float *)scaledata8);		break;	\
	default: break;																							\
	}																										\
	output = (uint8_t *)tmp;																				\
}
																					
#define MACRO_CAST_SCALE																	\
		switch (outputType)	{																\
		case GPMF_TYPE_SIGNED_BYTE: 	MACRO_CAST_SCALE_SIGNED_TYPE(int8_t)	break;		\
		case GPMF_TYPE_UNSIGNED_BYTE:	MACRO_CAST_SCALE_UNSIGNED_TYPE(uint8_t)	break;		\
		case GPMF_TYPE_SIGNED_SHORT: 	MACRO_CAST_SCALE_SIGNED_TYPE(int16_t)	break;		\
		case GPMF_TYPE_UNSIGNED_SHORT:	MACRO_CAST_SCALE_UNSIGNED_TYPE(uint16_t)	break;	\
		case GPMF_TYPE_FLOAT:			MACRO_CAST_SCALE_SIGNED_TYPE(float)	break;			\
		case GPMF_TYPE_SIGNED_LONG:		MACRO_CAST_SCALE_SIGNED_TYPE(int32_t)	break;		\
		case GPMF_TYPE_UNSIGNED_LONG:	MACRO_CAST_SCALE_UNSIGNED_TYPE(uint32_t)	break;	\
		case GPMF_TYPE_DOUBLE:			MACRO_CAST_SCALE_SIGNED_TYPE(double)	break;		\
		default: break;																		\
		}																									
		
#define MACRO_BSWAP_CAST_SCALE(swap, inputcast, tempcast)	\
{														\
	inputcast *val;										\
	tempcast temp,  *datatemp = (tempcast *)data;		\
	temp = swap(*datatemp);								\
	val = (inputcast *)&temp;							\
	MACRO_CAST_SCALE									\
	datatemp++;											\
	data = (uint8_t *)datatemp;							\
}

GPMF_ERR GPMF_ScaledData(GPMF_stream *ms, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples, GPMF_SampleType outputType)
{
	if (ms && buffer)
	{
		uint8_t *data = (uint8_t *)&ms->buffer[ms->pos + 2];
		uint8_t *output = (uint8_t *)buffer;
		uint32_t sample_size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		uint32_t output_sample_size = GPMF_SizeofType(outputType);
		uint32_t remaining_sample_size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 1]);
		uint8_t inputtype = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		uint32_t inputtypesize = GPMF_SizeofType(inputtype);
		uint8_t scaletype = 0;
		uint8_t scalecount = 0;
		uint32_t scaletypesize = 0;
		uint32_t *scaledata = NULL;
		uint32_t tmpbuffer[64];
		uint32_t tmpbuffersize = sizeof(tmpbuffer);
		uint32_t elements;
			
		if (inputtype == GPMF_TYPE_NEST)
			return GPMF_ERROR_MEMORY;

		remaining_sample_size -= sample_offset * sample_size; // skip samples
		data += sample_offset * sample_size;

		if (remaining_sample_size < sample_size * read_samples)
			return GPMF_ERROR_MEMORY;

		if (inputtype == GPMF_TYPE_COMPLEX)
			return GPMF_ERROR_TYPE_NOT_SUPPORTED;

		elements = sample_size / inputtypesize;

		if (output_sample_size * elements * read_samples > buffersize)
			return GPMF_ERROR_MEMORY;

		switch (outputType)	{
		case GPMF_TYPE_SIGNED_BYTE:
		case GPMF_TYPE_UNSIGNED_BYTE:
		case GPMF_TYPE_SIGNED_SHORT:
		case GPMF_TYPE_UNSIGNED_SHORT:
		case GPMF_TYPE_FLOAT:
		case GPMF_TYPE_SIGNED_LONG:
		case GPMF_TYPE_UNSIGNED_LONG:
		case GPMF_TYPE_DOUBLE:
			// All supported formats.
			{
				GPMF_stream fs;
				GPMF_CopyState(ms, &fs);

				if (GPMF_OK == GPMF_FindPrev(&fs, GPMF_KEY_SCALE))
				{
					scaledata = (uint32_t *)GPMF_RawData(&fs);
					scaletype = GPMF_SAMPLE_TYPE(fs.buffer[fs.pos + 1]);

					switch (scaletype)
					{
					case GPMF_TYPE_SIGNED_BYTE:
					case GPMF_TYPE_UNSIGNED_BYTE:
					case GPMF_TYPE_SIGNED_SHORT:
					case GPMF_TYPE_UNSIGNED_SHORT:
					case GPMF_TYPE_SIGNED_LONG:
					case GPMF_TYPE_UNSIGNED_LONG:
					case GPMF_TYPE_FLOAT:
						scalecount = GPMF_SAMPLES(fs.buffer[fs.pos + 1]);
						scaletypesize = GPMF_SizeofType(scaletype);

						if (scalecount > 1)
							if (scalecount != elements)
								return GPMF_ERROR_SCALE_COUNT;

						GPMF_FormattedData(&fs, tmpbuffer, tmpbuffersize, 0, scalecount);

						scaledata = (uint32_t *)tmpbuffer;
						break;
					default:
						return GPMF_ERROR_TYPE_NOT_SUPPORTED;
						break;
					}
				}
				else
				{
					scaletype = 'L';
					scalecount = 1;
					tmpbuffer[0] = 1; // set the scale to 1 is no scale was provided
					scaledata = (uint32_t *)tmpbuffer;
				}
			}

			while (read_samples--)
			{
				uint32_t i;
				uint8_t *scaledata8 = (uint8_t *)scaledata;

				for (i = 0; i < elements; i++)
				{
					if (scalecount > 1)
						scaledata8 += scaletypesize;

					switch (inputtype)
					{
					case GPMF_TYPE_FLOAT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP16, float, uint32_t) break;
					case GPMF_TYPE_SIGNED_SHORT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP16, int16_t, uint16_t) break;
					case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP16, uint16_t, uint16_t) break;
					case GPMF_TYPE_SIGNED_LONG:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, int32_t, uint32_t) break;
					case GPMF_TYPE_UNSIGNED_LONG:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, uint32_t, uint32_t) break;
					default:
						return GPMF_ERROR_TYPE_NOT_SUPPORTED;
						break;
					}
				}
			}
			break;

		default:
			return GPMF_ERROR_TYPE_NOT_SUPPORTED;
			break;
		}
		
		return GPMF_OK;
	}

	return GPMF_ERROR_MEMORY;
}
