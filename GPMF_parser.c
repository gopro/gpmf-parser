/*! @file GPMF_parser.c
 * 
 *  @brief GPMF Parser library
 *
 *  @version 1.2.1
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


GPMF_ERR IsValidSize(GPMF_stream *ms, uint32_t size) // size is in longs not bytes.
{
	if (ms)
	{
		int32_t nestsize = (int32_t)ms->nest_size[ms->nest_level];
		if (nestsize == 0 && ms->nest_level == 0)
			nestsize = ms->buffer_size_longs;

		if (size + 2 <= nestsize) return GPMF_OK;
	}
	return GPMF_ERROR_BAD_STRUCTURE;
}


GPMF_ERR GPMF_Validate(GPMF_stream *ms, GPMF_LEVELS recurse)
{
	if (ms)
	{
		uint32_t currpos = ms->pos;
		int32_t nestsize = (int32_t)ms->nest_size[ms->nest_level];
		if (nestsize == 0 && ms->nest_level == 0)
			nestsize = ms->buffer_size_longs;
		
		while (ms->pos+1 < ms->buffer_size_longs && nestsize > 0)
		{
			uint32_t key = ms->buffer[ms->pos];

			if (ms->nest_level == 0 && key != GPMF_KEY_DEVICE && ms->device_count == 0 && ms->pos == 0)
			{
				DBG_MSG("ERROR: uninitized -- GPMF_ERROR_BAD_STRUCTURE\n");
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

				if (type == GPMF_TYPE_NEST && recurse == GPMF_RECURSE_LEVELS)
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

					while (ms->pos < ms->buffer_size_longs && nestsize > 0 && ms->buffer[ms->pos] == GPMF_KEY_END)
					{
						ms->pos++;
						nestsize--;
					}
				}
				else
				{
					ms->pos += 2 + size;
					nestsize -= 2 + size;
				}

				if (ms->pos == ms->buffer_size_longs)
				{
					ms->pos = currpos;
					return GPMF_OK;
				}
			}
			else
			{
				if (key == GPMF_KEY_END)
				{
					do
					{
						ms->pos++;
						nestsize--;
					} while (ms->pos < ms->buffer_size_longs && nestsize > 0 && ms->buffer[ms->pos] == 0);
				}
				else if (ms->nest_level == 0 && ms->device_count > 0)
				{
					ms->pos = currpos;
					return GPMF_OK;
				}
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
		ms->last_seek[ms->nest_level] = 0;
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
		if (ms->pos+1 < ms->buffer_size_longs)
		{

			uint32_t key, type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
			uint32_t size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);

			if (GPMF_OK != IsValidSize(ms, size)) return GPMF_ERROR_BAD_STRUCTURE;

			if (GPMF_TYPE_NEST == type && GPMF_KEY_DEVICE == ms->buffer[ms->pos] && ms->nest_level == 0)
			{
				ms->last_level_pos[ms->nest_level] = ms->pos;
				ms->nest_size[ms->nest_level] = size;
				if (recurse)
					ms->pos += 2;
				else
					ms->pos += 2 + size;
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

			while (ms->pos < ms->buffer_size_longs && ms->nest_size[ms->nest_level] > 0 && ms->buffer[ms->pos] == GPMF_KEY_END)
			{
				ms->pos++;
				ms->nest_size[ms->nest_level]--;
			}

			while (ms->nest_level > 0 && ms->nest_size[ms->nest_level] == 0)
			{
				ms->nest_level--;
				//if (ms->nest_level == 0)
				//{
				//	ms->device_count++;
				//}
			}

			if (ms->pos < ms->buffer_size_longs)
			{
				while (ms->pos < ms->buffer_size_longs && ms->nest_size[ms->nest_level] > 0 && ms->buffer[ms->pos] == GPMF_KEY_END)
				{
					ms->pos++;
					ms->nest_size[ms->nest_level]--;
				}

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

GPMF_ERR GPMF_Reserved(uint32_t key)
{
	if(key == GPMF_KEY_DEVICE)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_DEVICE_ID)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_DEVICE_NAME)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_STREAM)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_STREAM_NAME)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_SI_UNITS)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_UNITS)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_SCALE)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_TYPE)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_TOTAL_SAMPLES)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_TICK)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_TOCK)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_EMPTY_PAYLOADS)
		return GPMF_ERROR_RESERVED;

	if(key == GPMF_KEY_REMARK)
		return GPMF_ERROR_RESERVED;

	return GPMF_OK;
}

uint32_t GPMF_PayloadSampleCount(GPMF_stream *ms)
{
	uint32_t count = 0;
	if (ms)
	{
		uint32_t fourcc = GPMF_Key(ms);

		GPMF_stream find_stream;
		GPMF_CopyState(ms, &find_stream);

		if (GPMF_OK == GPMF_FindNext(&find_stream, fourcc, GPMF_CURRENT_LEVEL)) // Count the instances, not the repeats
		{
			count=2;
			while (GPMF_OK == GPMF_FindNext(&find_stream, fourcc, GPMF_CURRENT_LEVEL))
			{
				count++;
			} 
		}
		else
		{
			count = GPMF_Repeat(ms);
		}
	}
	return count;
}


GPMF_ERR GPMF_SeekToSamples(GPMF_stream *ms)
{
	GPMF_stream prevstate;

	if (ms)
	{

		if (ms->pos+1 < ms->buffer_size_longs)
		{
			uint32_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

			memcpy(&prevstate, ms, sizeof(GPMF_stream));

			if (type == GPMF_TYPE_NEST)
				GPMF_Next(ms, GPMF_RECURSE_LEVELS); // open STRM and recurse in

			while (0 == GPMF_Next(ms, GPMF_CURRENT_LEVEL))
			{
				uint32_t size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);
				if (GPMF_OK != IsValidSize(ms, size))
				{
					memcpy(ms, &prevstate, sizeof(GPMF_stream));
					return GPMF_ERROR_BAD_STRUCTURE;
				}

				type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);


				if (type == GPMF_TYPE_NEST)  // Nest with-in nest
				{
					return GPMF_OK; //found match
				}

				if (size + 2 == ms->nest_size[ms->nest_level])
				{
					uint32_t key = GPMF_Key(ms);

					if (GPMF_ERROR_RESERVED == GPMF_Reserved(key))
						return GPMF_ERROR_FIND;
					
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


GPMF_ERR GPMF_FindPrev(GPMF_stream *ms, uint32_t fourcc, GPMF_LEVELS recurse)
{
	GPMF_stream prevstate;

	if (ms)
	{
		uint32_t curr_level = ms->nest_level;

		memcpy(&prevstate, ms, sizeof(GPMF_stream));

		if (ms->pos < ms->buffer_size_longs && curr_level > 0)
		{

			do
			{
				ms->last_seek[curr_level] = ms->pos;
				ms->pos = ms->last_level_pos[curr_level - 1] + 2;
				ms->nest_size[curr_level] += ms->last_seek[curr_level] - ms->pos;
				do
				{
					if (ms->last_seek[curr_level] > ms->pos && ms->buffer[ms->pos] == fourcc)
					{

						return GPMF_OK; //found match
					}
				} while (ms->last_seek[curr_level] > ms->pos && 0 == GPMF_Next(ms, GPMF_CURRENT_LEVEL));

				curr_level--;
			} while (recurse == GPMF_RECURSE_LEVELS && curr_level > 0);

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
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos+1]);
		return type;
	}
	return 0;
}


uint32_t GPMF_StructSize(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		uint32_t size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);

		if (GPMF_OK != IsValidSize(ms, size)) return 0; // as the structure is corrupted. i.e. GPMF_ERROR_BAD_STRUCTURE;

		return ssize;
	}
	return 0;
}


uint32_t GPMF_ElementsInStruct(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t ssize = GPMF_StructSize(ms);
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

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL))
			{
				char tmp[64] = "";
				uint32_t tmpsize = sizeof(tmp);
				char *data = (char *)GPMF_RawData(&find_stream);
				int size = GPMF_RawDataSize(&find_stream);
				
				if (GPMF_OK == GPMF_ExpandComplexTYPE(data, size, tmp, &tmpsize))
					return tmpsize;
			}
		}
	}
	return 0;
}


uint32_t GPMF_Repeat(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t repeat = GPMF_SAMPLES(ms->buffer[ms->pos + 1]);
		return repeat;
	}
	return 0;
}

uint32_t GPMF_RawDataSize(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 1]);
		if (GPMF_OK != IsValidSize(ms, size >> 2)) return 0;

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
		uint32_t len = (uint32_t)strlen(ms->device_name);
		if (len >= devicename_buf_size)
			return GPMF_ERROR_MEMORY;

		memcpy(devicenamebuf, ms->device_name, len);
		devicenamebuf[len] = 0;
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

uint32_t GPMF_ExpandComplexTYPE(char *src, uint32_t srcsize, char *dst, uint32_t *dstsize)
{
	uint32_t i = 0, k = 0, count = 0;

	while (i<srcsize && k<*dstsize)
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

	if (k >= *dstsize)
		return GPMF_ERROR_MEMORY; // bad structure formed

	dst[k] = 0;
	*dstsize = k;

	return GPMF_OK;
}



uint32_t GPMF_SizeOfComplexTYPE(char *type, uint32_t typestringlength)
{
	char *typearray = type;
	uint32_t size = 0, expand = 0;
	uint32_t i, len = typestringlength;


	for (i = 0; i < len; i++)
		if (typearray[i] == '[')
			expand = 1;
			
	if (expand)
	{
		char exptypearray[64];
		uint32_t dstsize = sizeof(exptypearray);

		if (GPMF_OK == GPMF_ExpandComplexTYPE(typearray, len, exptypearray, &dstsize))
		{
			typearray = exptypearray;
			len = dstsize;
		}
		else
			return 0;
	}


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
		uint32_t typestringlength = 1;
		char complextype[64] = "L";

		if (type == GPMF_TYPE_NEST)
			return GPMF_ERROR_BAD_STRUCTURE;
		
		if (GPMF_OK != IsValidSize(ms, remaining_sample_size>>2))
			return GPMF_ERROR_BAD_STRUCTURE;

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

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_RECURSE_LEVELS))
			{
				char *data1 = (char *)GPMF_RawData(&find_stream);
				int size = GPMF_RawDataSize(&find_stream);

				typestringlength = sizeof(complextype);
				if (GPMF_OK == GPMF_ExpandComplexTYPE(data1, size, complextype, &typestringlength))
				{
					elements = (uint32_t)strlen(complextype);

					if (sample_size != GPMF_SizeOfComplexTYPE(complextype, typestringlength))
						return GPMF_ERROR_TYPE_NOT_SUPPORTED;
				}
				else
					return GPMF_ERROR_TYPE_NOT_SUPPORTED;
			}
			else
				return GPMF_ERROR_TYPE_NOT_SUPPORTED;
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
					uint32_t *data32 = (uint32_t *)data;
					uint32_t *output32 = (uint32_t *)output;
					*(output32+1) = BYTESWAP32(*data32);
					*(output32) = BYTESWAP32(*(data32+1));
					data32 += 2;
					output32 += 2;

					data = (uint8_t *)data32;
					output = (uint8_t *)output32;
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
		uint8_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		char complextype[64] = "L";
		uint32_t inputtypesize = 0;
		uint32_t inputtypeelements = 0;
		uint8_t scaletype = 0;
		uint8_t scalecount = 0;
		uint32_t scaletypesize = 0;
		uint32_t *scaledata = NULL;
		uint32_t tmpbuffer[64];
		uint32_t tmpbuffersize = sizeof(tmpbuffer);
		uint32_t elements = 1;

		type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

		if (type == GPMF_TYPE_NEST)
			return GPMF_ERROR_MEMORY;

		if (GPMF_OK != IsValidSize(ms, remaining_sample_size >> 2))
			return GPMF_ERROR_BAD_STRUCTURE;

		remaining_sample_size -= sample_offset * sample_size; // skip samples
		data += sample_offset * sample_size;

		if (remaining_sample_size < sample_size * read_samples)
			return GPMF_ERROR_MEMORY;

		if (type == GPMF_TYPE_COMPLEX)
		{

			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_RECURSE_LEVELS))
			{
				char *data1 = (char *)GPMF_RawData(&find_stream);
				int size = GPMF_RawDataSize(&find_stream);
				uint32_t typestringlength = sizeof(complextype);
				if (GPMF_OK == GPMF_ExpandComplexTYPE(data1, size, complextype, &typestringlength))
				{
					inputtypeelements = elements = typestringlength;

					if (sample_size != GPMF_SizeOfComplexTYPE(complextype, typestringlength))
						return GPMF_ERROR_TYPE_NOT_SUPPORTED;
				}
				else
					return GPMF_ERROR_TYPE_NOT_SUPPORTED;
			}
			else
				return GPMF_ERROR_TYPE_NOT_SUPPORTED;
		}
		else
		{
			complextype[0] = type;
			inputtypesize = GPMF_SizeofType(type);
			if (inputtypesize == 0)
				return GPMF_ERROR_MEMORY;
			inputtypeelements = 1;
			elements = sample_size / inputtypesize;
		}

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

			if (GPMF_OK == GPMF_FindPrev(&fs, GPMF_KEY_SCALE, GPMF_CURRENT_LEVEL))
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
				switch (complextype[i % inputtypeelements])
				{
				case GPMF_TYPE_FLOAT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, float, uint32_t) break;
				case GPMF_TYPE_SIGNED_BYTE:  MACRO_BSWAP_CAST_SCALE(NOSWAP8, int8_t, uint8_t) break;
				case GPMF_TYPE_UNSIGNED_BYTE:  MACRO_BSWAP_CAST_SCALE(NOSWAP8, uint8_t, uint8_t) break;
				case GPMF_TYPE_SIGNED_SHORT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP16, int16_t, uint16_t) break;
				case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP16, uint16_t, uint16_t) break;
				case GPMF_TYPE_SIGNED_LONG:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, int32_t, uint32_t) break;
				case GPMF_TYPE_UNSIGNED_LONG:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, uint32_t, uint32_t) break;
				case GPMF_TYPE_SIGNED_64BIT_INT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP64, uint64_t, uint64_t) break;
				case GPMF_TYPE_UNSIGNED_64BIT_INT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP64, uint64_t, uint64_t) break;
				default:
					return GPMF_ERROR_TYPE_NOT_SUPPORTED;
					break;
				}
				if (scalecount > 1)
					scaledata8 += scaletypesize;
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
