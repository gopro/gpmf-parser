/*! @file GPMF_parser.c
 * 
 *  @brief GPMF Parser library
 *
 *  @version 2.2.3
 * 
 *  (C) Copyright 2017-2023 GoPro Inc (http://gopro.com/).
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
#include "GPMF_bitstream.h"


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
		uint32_t nestsize = (uint32_t)ms->nest_size[ms->nest_level];
		if (nestsize == 0 && ms->nest_level == 0)
			nestsize = ms->buffer_size_longs;

		if (size + 2 <= nestsize) return GPMF_OK;
	}
	return GPMF_ERROR_BAD_STRUCTURE;
}

GPMF_ERR SkipLevel(GPMF_stream* ms)
{
	if (ms)
	{
		ms->pos += ms->nest_size[ms->nest_level];
		ms->nest_size[ms->nest_level] = 0;
		while (ms->nest_level > 0 && ms->nest_size[ms->nest_level] == 0)
			ms->nest_level--;

		uint32_t size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);
		if (GPMF_OK != IsValidSize(ms, size))
			return GPMF_ERROR_BAD_STRUCTURE;
		else
			return GPMF_OK;
	}
	return GPMF_ERROR_BAD_STRUCTURE;
}


GPMF_ERR GPMF_Validate(GPMF_stream *ms, GPMF_LEVELS recurse)
{
	if (ms)
	{
		GPMF_ERR ret = GPMF_OK;
		uint32_t currpos = ms->pos;
		uint32_t nestsize = ms->nest_size[ms->nest_level];
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
				uint32_t size = GPMF_DATA_SIZE(type_size_repeat) >> 2;
				uint8_t type = GPMF_SAMPLE_TYPE(type_size_repeat);
				if(type != GPMF_TYPE_NEST && type != GPMF_TYPE_COMPLEX && type != GPMF_TYPE_COMPRESSED && GPMF_SizeofType((GPMF_SampleType)type) == 0)
				{
					ret = GPMF_ERROR_UNKNOWN_TYPE;
					DBG_MSG("MINOR ERROR: unknown datatype-- GPMF_ERROR_UNKNOWN_TYPE\n");
					//continue looking for structural errors
				}

				if (GPMF_SAMPLE_SIZE(type_size_repeat) == 0)
				{
					DBG_MSG("ERROR: zero for datatype size-- GPMF_ERROR_BAD_STRUCTURE\n");
					return GPMF_ERROR_BAD_STRUCTURE;
				}

				if (size + 2 > nestsize)
				{
					DBG_MSG("ERROR: nest size too small within %c%c%c%c-- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
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
					if (GPMF_ERROR_BAD_STRUCTURE == validnest)
					{
						DBG_MSG("ERROR: invalid nest within %c%c%c%c -- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
						return GPMF_ERROR_BAD_STRUCTURE;
					}
					else
					{
						if (GPMF_ERROR_UNKNOWN_TYPE == validnest)
							ret = GPMF_ERROR_UNKNOWN_TYPE;

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
					return ret;
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
					return ret;
				}
				else
				{
					DBG_MSG("ERROR: bad struct within %c%c%c%c -- GPMF_ERROR_BAD_STRUCTURE\n", PRINTF_4CC(key));
					return GPMF_ERROR_BAD_STRUCTURE;
				}
			}
		}

		ms->pos = currpos;
		return ret;
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


GPMF_ERR GPMF_Init(GPMF_stream *ms, uint32_t *buffer, uint32_t datasize)
{
	if(ms && buffer && datasize > 0)
	{
		uint32_t pos = 0;
		
		memset(ms, 0, sizeof(GPMF_stream));

		//Validate DEVC GPMF
		while((pos+1) * 4 < datasize && buffer[pos] == GPMF_KEY_DEVICE)
		{
			uint32_t size = GPMF_DATA_SIZE(buffer[pos+1]);
			pos += 2 + (size >> 2);
		}

		if (pos > 0 && pos * 4 <= datasize)
		{
			ms->buffer = buffer;
			ms->buffer_size_longs = pos;
			ms->cbhandle = 0;

			GPMF_ResetState(ms);

			return GPMF_OK;
		}
		else
		{
			return GPMF_ERROR_BAD_STRUCTURE;
		}
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

			if (GPMF_OK != IsValidSize(ms, size))
			{
				if (recurse & GPMF_TOLERANT && recurse & GPMF_RECURSE_LEVELS) // Skip this nest level as the sizes within this level are corrupt.
					return SkipLevel(ms);
				else
					return GPMF_ERROR_BAD_STRUCTURE;
			}

			if (GPMF_TYPE_NEST == type && GPMF_KEY_DEVICE == ms->buffer[ms->pos] && ms->nest_level == 0)
			{
				ms->last_level_pos[ms->nest_level] = ms->pos;
				ms->nest_size[ms->nest_level] = size;
				if (recurse & GPMF_RECURSE_LEVELS)
					ms->pos += 2;
				else
					ms->pos += 2 + size;
			}
			else
			{
				if (size + 2 > ms->nest_size[ms->nest_level])
					return GPMF_ERROR_BAD_STRUCTURE;

				if (recurse & GPMF_RECURSE_LEVELS && type == GPMF_TYPE_NEST)
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
					if (recurse & GPMF_RECURSE_LEVELS)
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
				while (ms->pos+1 < ms->buffer_size_longs && ms->nest_size[ms->nest_level] > 0 && ms->buffer[ms->pos] == GPMF_KEY_END)
				{
					ms->pos++;
					ms->nest_size[ms->nest_level]--;
				}

				if (ms->pos + 1 < ms->buffer_size_longs)
				{
					key = ms->buffer[ms->pos];
					if (!GPMF_VALID_FOURCC(key))
					{
						if (recurse & GPMF_TOLERANT && recurse & GPMF_RECURSE_LEVELS) // Skip this nest level as the sizes within this level are corrupt.
							return SkipLevel(ms);
						else
							return GPMF_ERROR_BAD_STRUCTURE;
					}

					if (GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]) == 0)
					{
						if (recurse & GPMF_TOLERANT && recurse & GPMF_RECURSE_LEVELS) // Skip this nest level as the sizes within this level are corrupt.
							return SkipLevel(ms);
						else
							return GPMF_ERROR_BAD_STRUCTURE;
					}

					type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos+1]);
					if (type != GPMF_TYPE_NEST && type != GPMF_TYPE_COMPLEX && type != GPMF_TYPE_COMPRESSED && GPMF_SizeofType((GPMF_SampleType)type) == 0)
					{
						if (recurse & GPMF_TOLERANT)
							return GPMF_Next(ms, recurse);
						else
							return GPMF_ERROR_UNKNOWN_TYPE;
					}
					if (key == GPMF_KEY_DEVICE_ID && ms->pos + 2 < ms->buffer_size_longs)
						ms->device_id = BYTESWAP32(ms->buffer[ms->pos + 2]);
					if (key == GPMF_KEY_DEVICE_NAME)
					{
						if (ms->pos + 1 >= ms->buffer_size_longs)
							return GPMF_ERROR_BAD_STRUCTURE;

						size = GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]); // in bytes
						if (size > sizeof(ms->device_name) - 1)
							size = sizeof(ms->device_name) - 1;

						if ((ms->pos + 1 + ((size + 3) >> 2)) >= ms->buffer_size_longs)
							return GPMF_ERROR_BAD_STRUCTURE;

						memcpy(ms->device_name, &ms->buffer[ms->pos + 2], size);
						ms->device_name[size] = 0;
					}
				}
				else
				{
					// end of buffer
					return GPMF_ERROR_BUFFER_END;
				}
			}
			else
			{
				// end of buffer
				return GPMF_ERROR_BUFFER_END;
			}
			

			size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);
			if (GPMF_OK != IsValidSize(ms, size))
			{
				if (recurse & GPMF_TOLERANT && recurse & GPMF_RECURSE_LEVELS) // Skip this nest level as the sizes within this level are corrupt.
					return SkipLevel(ms);
				else
					return GPMF_ERROR_BAD_STRUCTURE;
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
			GPMF_ERR ret = GPMF_OK;
			do
			{
				ret = GPMF_Next(ms, recurse);
				if (GPMF_OK == ret)
				{
					if (ms->buffer[ms->pos] == fourcc)
					{
						return GPMF_OK; //found match
					}
				}
			} while (GPMF_OK == ret);

			memcpy(ms, &prevstate, sizeof(GPMF_stream)); // restore read position		
			return ret; // the error code returned from GPMF_Next()
		}
		else
			return GPMF_ERROR_BUFFER_END;
	}
	else
		return GPMF_ERROR_MEMORY;
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

	if (key == GPMF_KEY_MATRIX)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_ORIENTATION_IN)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_ORIENTATION_OUT)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_TIME_STAMP)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_TIME_STAMPS)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_PREFORMATTED)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_TEMPERATURE_C)
		return GPMF_ERROR_RESERVED;

	if (key == GPMF_KEY_FREESPACE)
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

		if (GPMF_OK == GPMF_FindNext(&find_stream, fourcc, GPMF_CURRENT_LEVEL|GPMF_TOLERANT)) // Count the instances, not the repeats
		{
			count=2;
			while (GPMF_OK == GPMF_FindNext(&find_stream, fourcc, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
			{
				count++;
			} 
		}
		else
		{
			count = GPMF_Repeat(ms);
			if (count == 0) // this can happen with an empty FACE, yet this is still a FACE fouce
				count = 1;
		}
	}
	return count;
}


GPMF_ERR GPMF_SeekToSamples(GPMF_stream *ms)
{
	GPMF_stream prevstate;

	if (ms)
	{

		if (ms->pos + 1 < ms->buffer_size_longs)
		{
			GPMF_ERR ret = GPMF_OK;
			uint32_t size, type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

			memcpy(&prevstate, ms, sizeof(GPMF_stream));

			if (type == GPMF_TYPE_NEST)
				ret = GPMF_Next(ms, GPMF_RECURSE_LEVELS | GPMF_TOLERANT); // open STRM and recurse in

			if (GPMF_OK != ret)
			{
				memcpy(ms, &prevstate, sizeof(GPMF_stream));
				return ret;
			}

			while (GPMF_OK == (ret = GPMF_Next(ms, GPMF_CURRENT_LEVEL | GPMF_TOLERANT)))
			{
				if (ms->pos + 1 >= ms->buffer_size_longs)
				{
					memcpy(ms, &prevstate, sizeof(GPMF_stream));
					return GPMF_ERROR_BAD_STRUCTURE;
				}

				size = (GPMF_DATA_SIZE(ms->buffer[ms->pos + 1]) >> 2);
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
					{
						memcpy(ms, &prevstate, sizeof(GPMF_stream));
						return GPMF_ERROR_FIND;
					}
					return GPMF_OK; //found match
				}

				if (ms->pos + size + 2 >= ms->buffer_size_longs)
				{
					memcpy(ms, &prevstate, sizeof(GPMF_stream));
					return GPMF_ERROR_BAD_STRUCTURE;
				}

				if (ms->buffer[ms->pos] == ms->buffer[ms->pos + size + 2]) // Matching tags
				{
					return GPMF_OK; //found match
				}
			}

			// restore read position
			memcpy(ms, &prevstate, sizeof(GPMF_stream));
			return ret;
		}
		else
			return GPMF_ERROR_BUFFER_END;
	}
	else 
		return GPMF_ERROR_MEMORY;
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
				} while (ms->last_seek[curr_level] > ms->pos && GPMF_OK == GPMF_Next(ms, GPMF_CURRENT_LEVEL|(recurse&GPMF_TOLERANT)));

				curr_level--;
			} while (recurse & GPMF_RECURSE_LEVELS && curr_level > 0);

			// restore read position
			memcpy(ms, &prevstate, sizeof(GPMF_stream));
			return GPMF_ERROR_FIND;
		}
		else
			return GPMF_ERROR_BUFFER_END;
	}
	else
		return GPMF_ERROR_MEMORY;
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


GPMF_SampleType GPMF_Type(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		GPMF_SampleType type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos+1]);
		if (type == GPMF_TYPE_COMPRESSED && ms->pos+2 < ms->buffer_size_longs)
		{
			type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 2]);
		}
		return type;
	}
	return GPMF_TYPE_ERROR;
}


uint32_t GPMF_StructSize(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		uint32_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		if (type == GPMF_TYPE_COMPRESSED && ms->pos+2 < ms->buffer_size_longs)
		{
			ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 2]);
		}
		return ssize;
	}
	return 0;
}


uint32_t GPMF_ElementsInStruct(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		uint32_t ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		uint32_t type = GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

		if (type != GPMF_TYPE_NEST && type != GPMF_TYPE_COMPLEX && type != GPMF_TYPE_COMPRESSED)
		{
			uint32_t tsize = GPMF_SizeofType((GPMF_SampleType)type);
			if (tsize > 0)
				return ssize / tsize;
			else
				return 0;
		}
		if (type == GPMF_TYPE_COMPLEX)
		{
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
			{
				char tmp[64] = "";
				uint32_t tmpsize = sizeof(tmp);
				char *data = (char *)GPMF_RawData(&find_stream);
				uint32_t size = GPMF_RawDataSize(&find_stream);
				
				if (GPMF_OK == GPMF_ExpandComplexTYPE(data, size, tmp, &tmpsize))
					return tmpsize;
			}
		}
		if (type == GPMF_TYPE_COMPRESSED && ms->pos+2 < ms->buffer_size_longs)
		{
			type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 2]);
			ssize = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 2]);
			uint32_t tsize = GPMF_SizeofType((GPMF_SampleType)type);
			if (tsize > 0)
				return ssize / tsize;
			else
				return 0;
		}
	}
	return 0;
}


uint32_t GPMF_Repeat(GPMF_stream *ms)
{
	if (ms && ms->pos+1 < ms->buffer_size_longs)
	{
		GPMF_SampleType type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		uint32_t repeat = GPMF_SAMPLES(ms->buffer[ms->pos + 1]);
		if(type == GPMF_TYPE_COMPRESSED && ms->pos+2 < ms->buffer_size_longs)
		{
			repeat = GPMF_SAMPLES(ms->buffer[ms->pos + 2]);
		}
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

uint32_t GPMF_FormattedDataSize(GPMF_stream *ms)
{
	if (ms && ms->pos + 1 < ms->buffer_size_longs)
	{
		GPMF_SampleType type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		uint32_t size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1])*GPMF_SAMPLES(ms->buffer[ms->pos + 1]);

		if (type == GPMF_TYPE_COMPRESSED && ms->pos+2 < ms->buffer_size_longs)
		{
			size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 2])*GPMF_SAMPLES(ms->buffer[ms->pos + 2]);
		}
		return size;
	}
	return 0;
}

uint32_t GPMF_ScaledDataSize(GPMF_stream *ms, GPMF_SampleType type)
{
	if (ms && ms->pos + 1 < ms->buffer_size_longs)
	{
		uint32_t elements = GPMF_ElementsInStruct(ms);
		uint32_t samples = GPMF_Repeat(ms);
		return GPMF_SizeofType((GPMF_SampleType)type) * elements * samples;
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




int32_t GPMFTypeEndianSize(int type)
{
	int32_t ssize = -1;

	switch ((int)type)
	{
	case GPMF_TYPE_STRING_ASCII:		ssize = 1; break;
	case GPMF_TYPE_SIGNED_BYTE:			ssize = 1; break;
	case GPMF_TYPE_UNSIGNED_BYTE:		ssize = 1; break;
	case GPMF_TYPE_STRING_UTF8:			ssize = 1; break;

		// These datatype can always be stored in Big-Endian
	case GPMF_TYPE_SIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_UNSIGNED_SHORT:		ssize = 2; break;
	case GPMF_TYPE_FLOAT:				ssize = 4; break;
	case GPMF_TYPE_FOURCC:				ssize = 1; break;
	case GPMF_TYPE_SIGNED_LONG:			ssize = 4; break;
	case GPMF_TYPE_UNSIGNED_LONG:		ssize = 4; break;
	case GPMF_TYPE_Q15_16_FIXED_POINT:  ssize = 4; break;
	case GPMF_TYPE_Q31_32_FIXED_POINT:  ssize = 8; break;
	case GPMF_TYPE_DOUBLE:				ssize = 8; break;
	case GPMF_TYPE_SIGNED_64BIT_INT:	ssize = 8; break;
	case GPMF_TYPE_UNSIGNED_64BIT_INT:  ssize = 8; break;

	case GPMF_TYPE_GUID:				ssize = 1; break; // Do not byte swap
	case GPMF_TYPE_UTC_DATE_TIME:		ssize = 1; break; // Do not byte swap

	//All unknown,complex or larger than 8-bytes store as is:
	default:							ssize = -1;  // unsupported for structsize type
	}

	return ssize;
}


void ByteSwap2Buffer(uint32_t* input, uint32_t* output, GPMF_SampleType data_type, uint32_t structSize, uint32_t repeat)
{
	int32_t i, len = 0, endianSize = GPMFTypeEndianSize(data_type);
	if (endianSize == 8) // 64-bit swap required
	{
		for (i = 0; i < (int32_t)((repeat * structSize + 3) / sizeof(int32_t)); i += 2)
		{
			output[len++] = BYTESWAP32(input[i + 1]); 
			output[len++] = BYTESWAP32(input[i]);
		}
	}
	else if (endianSize >= 1)
	{
		for (i = 0; i < (int32_t)((repeat * structSize + 3) / sizeof(int32_t)); i++)
		{
			switch (endianSize)
			{
			case 2:		output[len++] = BYTESWAP2x16(input[i]); break;
			case 4:		output[len++] = BYTESWAP32(input[i]); break;
			default:	output[len++] = input[i]; break;
			}
		}
	}
}



//find and inplace overwrite a GPMF KLV with new KLV, if the lengths match.
GPMF_ERR GPMF_Modify(GPMF_stream* ms, uint32_t origfourCC, uint32_t newfourCC, 
	GPMF_SampleType newType, uint32_t newStructSize, uint32_t newRepeat, void* newData)
{
	uint32_t dataSizeLongs = (newStructSize * newRepeat + 3) >> 2;

	if (ms && ms->pos + 1 + dataSizeLongs < ms->buffer_size_longs)
	{
		GPMF_stream fs;
		GPMF_CopyState(ms, &fs);

		uint32_t key = fs.buffer[fs.pos];
		uint32_t tsr = fs.buffer[fs.pos + 1];
		uint32_t ssize = GPMF_SAMPLE_SIZE(tsr);
		uint32_t repeat = GPMF_SAMPLES(tsr);

		if (key == origfourCC && (((ssize * repeat + 3) >> 2) == ((newStructSize * newRepeat + 3) >> 2)))  // no find required and data will fit
		{
			fs.buffer[fs.pos] = newfourCC;
			fs.buffer[fs.pos + 1] = GPMF_MAKE_TYPE_SIZE_COUNT(newType, newStructSize, newRepeat);

			ByteSwap2Buffer((uint32_t*)newData, (uint32_t*)&fs.buffer[fs.pos + 2], newType, newStructSize, newRepeat);
			return GPMF_OK;
		}
		else
		{
			// search forward from the current position at this level
			if (GPMF_OK == GPMF_FindNext(&fs, origfourCC, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
			{
				tsr = fs.buffer[fs.pos + 1];
				ssize = GPMF_SAMPLE_SIZE(tsr);
				repeat = GPMF_SAMPLES(tsr);

				if (((ssize * repeat + 3) >> 2) == ((newStructSize * newRepeat + 3) >> 2))  //will the new data fit
				{
					fs.buffer[fs.pos] = newfourCC;
					fs.buffer[fs.pos + 1] = GPMF_MAKE_TYPE_SIZE_COUNT(newType, newStructSize, newRepeat);

					ByteSwap2Buffer((uint32_t*)newData, (uint32_t*)&fs.buffer[fs.pos + 2], newType, newStructSize, newRepeat);
					return GPMF_OK;
				}
				return GPMF_ERROR_BAD_STRUCTURE;  // sizes don't match
			}
			// search backward from the current position at this level
			else if (GPMF_OK == GPMF_FindPrev(&fs, origfourCC, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
			{
				tsr = fs.buffer[fs.pos + 1];
				ssize = GPMF_SAMPLE_SIZE(tsr);
				repeat = GPMF_SAMPLES(tsr);

				if (((ssize * repeat + 3) >> 2) == ((newStructSize * newRepeat + 3) >> 2))  //will the new data fit
				{
					fs.buffer[fs.pos] = newfourCC;
					fs.buffer[fs.pos + 1] = GPMF_MAKE_TYPE_SIZE_COUNT(newType, newStructSize, newRepeat);

					ByteSwap2Buffer((uint32_t*)newData, (uint32_t*)&fs.buffer[fs.pos + 2], newType, newStructSize, newRepeat);
					return GPMF_OK;
				}
				return GPMF_ERROR_BAD_STRUCTURE;  // sizes don't match
			}
			else 
			{
				// search from the beginning through all levels
				GPMF_ResetState(&fs);
				if (GPMF_OK == GPMF_FindNext(&fs, origfourCC, GPMF_RECURSE_LEVELS|GPMF_TOLERANT))
				{ 
					tsr = fs.buffer[fs.pos + 1];
					ssize = GPMF_SAMPLE_SIZE(tsr);
					repeat = GPMF_SAMPLES(tsr);

					if (((ssize * repeat + 3) >> 2) == ((newStructSize * newRepeat + 3) >> 2))  //will the new data fit
					{
						fs.buffer[fs.pos] = newfourCC;
						fs.buffer[fs.pos + 1] = GPMF_MAKE_TYPE_SIZE_COUNT(newType, newStructSize, newRepeat);

						ByteSwap2Buffer((uint32_t*)newData, (uint32_t*)&fs.buffer[fs.pos + 2], newType, newStructSize, newRepeat);
						return GPMF_OK;
					}
					return GPMF_ERROR_BAD_STRUCTURE;  // sizes don't match
				}
				else
					return GPMF_ERROR_FIND; // if can't find the data to replace.
			}
		}
	}
	return GPMF_ERROR_BAD_STRUCTURE;  // sizes don't match
}



uint32_t GPMF_SizeofType(GPMF_SampleType type)
{
	uint32_t ssize = 0;

	switch (type)
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
	default: ssize = 0; break;
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
			uint32_t j = 1;
			count = 0;
			while (src[i + j] >= '0' && src[i + j] <= '9')
			{
				count *= 10;
				count += (uint32_t) (src[i + j] - '0');
				j++;
			}

			if (count > 1)
			{
				uint32_t l;
				for (l = 1; l<count && k < *dstsize; l++)
				{
					dst[k] = src[i - 1];
					k++;
				}
				if (k >= *dstsize)
					return GPMF_ERROR_MEMORY; // bad structure formed
			}
			i += j;
			if (src[i] == ']') i++;
		}
		else
		{
			dst[k] = src[i];
			if (dst[k] == 0) break;
            i++;
            k++;
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
	char exptypearray[64];

	for (i = 0; i < len; i++)
		if (typearray[i] == '[')
			expand = 1;
			
	if (expand)
	{
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
		if (typearray[i])
		{
			uint32_t typesize = GPMF_SizeofType((GPMF_SampleType)typearray[i]);

			if (typesize < 1) return 0;
			size += typesize;
		}
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

		if (type == GPMF_TYPE_COMPRESSED)
		{
			if (GPMF_OK == GPMF_Decompress(ms, (uint32_t *)output, buffersize))
			{
				uint32_t compressed_typesize = ms->buffer[ms->pos + 2];
				sample_size = GPMF_SAMPLE_SIZE(compressed_typesize);
				remaining_sample_size = GPMF_DATA_PACKEDSIZE(compressed_typesize);
				type = GPMF_SAMPLE_TYPE(compressed_typesize);
				data = output;
			}
			else
				return GPMF_ERROR_MEMORY;
		}

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

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_RECURSE_LEVELS|GPMF_TOLERANT))
			{
				char *data1 = (char *)GPMF_RawData(&find_stream);
				uint32_t size = GPMF_RawDataSize(&find_stream);

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
						typesize = GPMF_SizeofType((GPMF_SampleType) complextype[i]);
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
	switch (scal_type)																							\
	{																											\
	case GPMF_TYPE_SIGNED_BYTE:		*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((int8_t *)scal_data8);	break;	\
	case GPMF_TYPE_UNSIGNED_BYTE:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((uint8_t *)scal_data8);	break;	\
	case GPMF_TYPE_SIGNED_SHORT:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((int16_t *)scal_data8);	break;	\
	case GPMF_TYPE_UNSIGNED_SHORT:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((uint16_t *)scal_data8);	break;	\
	case GPMF_TYPE_SIGNED_LONG:		*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((int32_t *)scal_data8);	break;	\
	case GPMF_TYPE_UNSIGNED_LONG:	*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((uint32_t *)scal_data8);	break;  \
	case GPMF_TYPE_FLOAT:			*tmp++ = (casttype)(*val < 0 ? 0 : *val) / (casttype)*((float *)scal_data8);	break;	\
	default: break;																								\
	}																											\
	output = (uint8_t *)tmp;																					\
}

#define MACRO_CAST_SCALE_SIGNED_TYPE(casttype)		\
{																											\
	casttype *tmp = (casttype *)output;																		\
	switch (scal_type)																						\
	{																										\
	case GPMF_TYPE_SIGNED_BYTE:		*tmp++ = (casttype)*val / (casttype)*((int8_t *)scal_data8);	break;	\
	case GPMF_TYPE_UNSIGNED_BYTE:	*tmp++ = (casttype)*val / (casttype)*((uint8_t *)scal_data8);	break;	\
	case GPMF_TYPE_SIGNED_SHORT:	*tmp++ = (casttype)*val / (casttype)*((int16_t *)scal_data8);	break;	\
	case GPMF_TYPE_UNSIGNED_SHORT:	*tmp++ = (casttype)*val / (casttype)*((uint16_t *)scal_data8);	break;	\
	case GPMF_TYPE_SIGNED_LONG:		*tmp++ = (casttype)*val / (casttype)*((int32_t *)scal_data8);	break;	\
	case GPMF_TYPE_UNSIGNED_LONG:	*tmp++ = (casttype)*val / (casttype)*((uint32_t *)scal_data8);	break;  \
	case GPMF_TYPE_FLOAT:			*tmp++ = (casttype)*val / (casttype)*((float *)scal_data8);		break;	\
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

#define MACRO_CAST_UNSIGNED_SCALE															\
		switch (outputType)	{																\
		case GPMF_TYPE_SIGNED_BYTE: 	MACRO_CAST_SCALE_SIGNED_TYPE(int8_t)	break;		\
		case GPMF_TYPE_UNSIGNED_BYTE:	MACRO_CAST_SCALE_SIGNED_TYPE(uint8_t)	break;		\
		case GPMF_TYPE_SIGNED_SHORT: 	MACRO_CAST_SCALE_SIGNED_TYPE(int16_t)	break;		\
		case GPMF_TYPE_UNSIGNED_SHORT:	MACRO_CAST_SCALE_SIGNED_TYPE(uint16_t)	break;		\
		case GPMF_TYPE_FLOAT:			MACRO_CAST_SCALE_SIGNED_TYPE(float)		break;		\
		case GPMF_TYPE_SIGNED_LONG:		MACRO_CAST_SCALE_SIGNED_TYPE(int32_t)	break;		\
		case GPMF_TYPE_UNSIGNED_LONG:	MACRO_CAST_SCALE_SIGNED_TYPE(uint32_t)	break;		\
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


#define MACRO_BSWAP_CAST_UNSIGNED_SCALE(swap, inputcast, tempcast)	\
{														\
	inputcast *val;										\
	tempcast temp,  *datatemp = (tempcast *)data;		\
	temp = swap(*datatemp);								\
	val = (inputcast *)&temp;							\
	MACRO_CAST_UNSIGNED_SCALE							\
	datatemp++;											\
	data = (uint8_t *)datatemp;							\
}

#define MACRO_NOSWAP_CAST_SCALE(inputcast)	\
{														\
	inputcast *val;										\
	inputcast temp,  *datatemp = (inputcast *)data;		\
	temp = *(inputcast *)data;							\
	val = (inputcast *)&temp;							\
	MACRO_CAST_SCALE									\
	datatemp++;											\
	data = (uint8_t *)datatemp;							\
}


#define MACRO_NOSWAP_CAST_UNSIGNED_SCALE(inputcast)	\
{														\
	inputcast *val;										\
	inputcast temp,  *datatemp = (inputcast *)data;		\
	temp = *(inputcast *)data;							\
	val = (inputcast *)&temp;							\
	MACRO_CAST_UNSIGNED_SCALE							\
	datatemp++;											\
	data = (uint8_t *)datatemp;							\
}

// a sensor matrix with only [1,0,0, 0,-1,0, 0,0,1], is just a form of non-calibrated sensor orientation
#define MACRO_IS_MATRIX_CALIBRATION(inputcast)				\
{															\
	uint32_t m;												\
	inputcast *md = (inputcast *)mtrx_data;					\
	inputcast one = (inputcast)1;							\
	inputcast negone = (inputcast)-1;						\
	mtrx_calibration = 0;									\
	for (m = 0; m < elements*elements; m++, md++)	        \
	{														\
		if (*md != one && *md != negone && *md != 0)		\
			mtrx_calibration = 1;							\
	}														\
}


#define MACRO_APPLY_CALIBRATION(matrixcast, outputcast)																		\
{																															\
	uint32_t x,y;																											\
	outputcast tmpbuf[8];																									\
	outputcast *tmp = (outputcast *)output;																					\
	tmp -= elements;																										\
	matrixcast *mtrx = (matrixcast *)mtrx_data;																				\
	for (y = 0; y < elements; y++) tmpbuf[y] = 0;																			\
	for (y = 0; y < elements; y++) for (x = 0; x < elements; x++)  tmpbuf[y] += tmp[x] * (outputcast)mtrx[y*elements + x];	\
	for (y = 0; y < elements; y++) tmp[y] = tmpbuf[y];																		\
}


#define MACRO_APPLY_MATRIX_CALIBRATION(matrixcast)												\
{																								\
	switch (outputType)	{																		\
		case GPMF_TYPE_SIGNED_BYTE: 	MACRO_APPLY_CALIBRATION(matrixcast, int8_t)		break;	\
		case GPMF_TYPE_UNSIGNED_BYTE:	MACRO_APPLY_CALIBRATION(matrixcast, uint8_t)	break;	\
		case GPMF_TYPE_SIGNED_SHORT: 	MACRO_APPLY_CALIBRATION(matrixcast, int16_t)	break;	\
		case GPMF_TYPE_UNSIGNED_SHORT:	MACRO_APPLY_CALIBRATION(matrixcast, uint16_t)	break;	\
		case GPMF_TYPE_SIGNED_LONG:		MACRO_APPLY_CALIBRATION(matrixcast, int32_t)	break;	\
		case GPMF_TYPE_UNSIGNED_LONG:	MACRO_APPLY_CALIBRATION(matrixcast, uint32_t)	break;	\
		case GPMF_TYPE_FLOAT:			MACRO_APPLY_CALIBRATION(matrixcast, float)		break;	\
		case GPMF_TYPE_DOUBLE:			MACRO_APPLY_CALIBRATION(matrixcast, double)		break;	\
		default: break;																			\
	}																							\
}

#define MACRO_SET_MATRIX(matrixcast, orin, orio, pos)	\
{														\
	matrixcast *mtrx = (matrixcast *)mtrx_data;			\
	if (orin == orio)									\
		mtrx[pos] = (matrixcast)1;						\
	else if ((orin - 'a') == (orio - 'A'))				\
		mtrx[pos] = (matrixcast)-1;						\
	else if ((orin - 'A') == (orio - 'a'))				\
		mtrx[pos] = (matrixcast)-1;						\
	else												\
		mtrx[pos] = 0;									\
}



GPMF_ERR GPMF_ScaledData(GPMF_stream *ms, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples, GPMF_SampleType outputType)
{
	if (ms && buffer)
	{
		GPMF_ERR ret = GPMF_OK;
		uint8_t *data = (uint8_t *)&ms->buffer[ms->pos + 2];
		uint8_t *output = (uint8_t *)buffer;
		uint32_t sample_size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 1]);
		uint32_t output_sample_size = GPMF_SizeofType(outputType);
		uint32_t remaining_sample_size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 1]);
		uint32_t total_sample_data_bytes = remaining_sample_size;
		GPMF_SampleType type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);
		char complextype[64] = "L";
		uint32_t inputtypesize = 0;
		uint32_t inputtypeelements = 0;

		uint8_t scal_type = 0;
		uint8_t scal_count = 0;
		uint32_t scal_typesize = 0;
		uint32_t *scal_data = NULL;
		uint32_t scal_buffer[64];
		uint32_t scal_buffersize = sizeof(scal_buffer);

		uint8_t mtrx_type = 0;
		uint8_t mtrx_count = 0;
		uint32_t mtrx_typesize = 0;
		uint32_t mtrx_sample_size = 0;
		uint32_t *mtrx_data = NULL;
		uint32_t mtrx_buffer[64];
		uint32_t mtrx_buffersize = sizeof(mtrx_buffer);
		uint32_t mtrx_calibration = 0;

		char *orin_data = NULL;
		uint32_t orin_len = 0;
		char *orio_data = NULL;
		uint32_t orio_len = 0;

		uint32_t *uncompressedSamples = NULL;
		uint32_t elements = 1;
		uint32_t noswap = 0;

		type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 1]);

		if (type == GPMF_TYPE_NEST)
			return GPMF_ERROR_MEMORY;

		if (type == GPMF_TYPE_COMPRESSED)
		{
			uint32_t neededunc = GPMF_FormattedDataSize(ms);
			uint32_t samples = GPMF_Repeat(ms);

			remaining_sample_size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 2]);
			total_sample_data_bytes = remaining_sample_size;

			uncompressedSamples = (uint32_t *)malloc(neededunc + 12);
			if (uncompressedSamples)
			{
				if (GPMF_OK == GPMF_FormattedData(ms, uncompressedSamples, neededunc, 0, samples))
				{
					read_samples = samples;
					elements = GPMF_ElementsInStruct(ms);
					type = GPMF_Type(ms);
					complextype[0] = (char)type;
					inputtypesize = GPMF_SizeofType((GPMF_SampleType)type);
					if (inputtypesize == 0)
					{
						ret = GPMF_ERROR_MEMORY;
						goto cleanup;
					}
					inputtypeelements = 1;
					noswap = 1; // data is formatted to LittleEndian

					data = (uint8_t *)uncompressedSamples;

					remaining_sample_size -= sample_offset * sample_size; // skip samples
					data += sample_offset * sample_size;

					if (remaining_sample_size < sample_size * read_samples)
					{
						ret = GPMF_ERROR_MEMORY;
						goto cleanup;
					}
				}
			}
		}
		else if (type == GPMF_TYPE_COMPLEX)
		{

			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);

			remaining_sample_size -= sample_offset * sample_size; // skip samples
			data += sample_offset * sample_size;

			if (remaining_sample_size < sample_size * read_samples)
				return GPMF_ERROR_MEMORY;

			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_RECURSE_LEVELS|GPMF_TOLERANT))
			{
				char *data1 = (char *)GPMF_RawData(&find_stream);
				uint32_t size = GPMF_RawDataSize(&find_stream);
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

			remaining_sample_size -= sample_offset * sample_size; // skip samples
			data += sample_offset * sample_size;

			if (remaining_sample_size < sample_size * read_samples)
				return GPMF_ERROR_MEMORY;

			complextype[0] = type;
			inputtypesize = GPMF_SizeofType((GPMF_SampleType)type);
			if (inputtypesize == 0)
				return GPMF_ERROR_MEMORY;
			inputtypeelements = 1;
			elements = sample_size / inputtypesize;
		}

		if (output_sample_size * elements * read_samples > buffersize)
		{
			ret = GPMF_ERROR_MEMORY;
			goto cleanup;
		}

		if (read_samples * sample_size > total_sample_data_bytes)
		{
			ret = GPMF_ERROR_MEMORY;
			goto cleanup;
		}

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

			if (GPMF_OK == GPMF_FindPrev(&fs, GPMF_KEY_SCALE, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
			{
				scal_data = (uint32_t *)GPMF_RawData(&fs);
				scal_type = GPMF_SAMPLE_TYPE(fs.buffer[fs.pos + 1]);

				switch (scal_type)
				{
				case GPMF_TYPE_SIGNED_BYTE:
				case GPMF_TYPE_UNSIGNED_BYTE:
				case GPMF_TYPE_SIGNED_SHORT:
				case GPMF_TYPE_UNSIGNED_SHORT:
				case GPMF_TYPE_SIGNED_LONG:
				case GPMF_TYPE_UNSIGNED_LONG:
				case GPMF_TYPE_FLOAT:
					scal_count = GPMF_SAMPLES(fs.buffer[fs.pos + 1]);
					scal_typesize = GPMF_SizeofType((GPMF_SampleType)scal_type);

					if (scal_count > 1)
					{
						if (scal_count != elements)
						{
							ret = GPMF_ERROR_SCALE_COUNT;
							goto cleanup;
						}
					}

					GPMF_FormattedData(&fs, scal_buffer, scal_buffersize, 0, scal_count);

					scal_data = (uint32_t *)scal_buffer;
					break;
				default:
					return GPMF_ERROR_SCALE_NOT_SUPPORTED;
					break;
				}
			}
			else
			{
				scal_type = 'L';
				scal_count = 1;
				scal_buffer[0] = 1; // set the scale to 1 is no scale was provided
				scal_data = (uint32_t *)scal_buffer;
			}

			GPMF_CopyState(ms, &fs);
			if (GPMF_OK == GPMF_FindPrev(&fs, GPMF_KEY_MATRIX, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
			{
				uint32_t mtrx_found_size = 0;
				uint32_t matrix_size = elements * elements;
				mtrx_data = (uint32_t *)GPMF_RawData(&fs);
				mtrx_type = GPMF_SAMPLE_TYPE(fs.buffer[fs.pos + 1]);

				switch (mtrx_type)
				{
				case GPMF_TYPE_SIGNED_BYTE:
				case GPMF_TYPE_UNSIGNED_BYTE:
				case GPMF_TYPE_SIGNED_SHORT:
				case GPMF_TYPE_UNSIGNED_SHORT:
				case GPMF_TYPE_SIGNED_LONG:
				case GPMF_TYPE_UNSIGNED_LONG:
				case GPMF_TYPE_FLOAT:
				case GPMF_TYPE_DOUBLE:
					mtrx_count = GPMF_SAMPLES(fs.buffer[fs.pos + 1]);
					mtrx_sample_size = GPMF_SAMPLE_SIZE(fs.buffer[fs.pos + 1]);
					mtrx_typesize = GPMF_SizeofType((GPMF_SampleType)mtrx_type);
					mtrx_found_size = mtrx_count * mtrx_sample_size / mtrx_typesize;
					if (mtrx_found_size != matrix_size)  // e.g XYZ is a 3x3 matrix, RGBA is a 4x4 matrix
					{
						ret = GPMF_ERROR_SCALE_COUNT;
						goto cleanup;
					}
					
					GPMF_FormattedData(&fs, mtrx_buffer, mtrx_buffersize, 0, mtrx_count);
					mtrx_data = (uint32_t *)mtrx_buffer;
					break;
				default:
					return GPMF_ERROR_SCALE_NOT_SUPPORTED;
					break;
				}

				switch (mtrx_type)
				{
				case GPMF_TYPE_SIGNED_BYTE:  MACRO_IS_MATRIX_CALIBRATION(int8_t) break;
				case GPMF_TYPE_UNSIGNED_BYTE:  MACRO_IS_MATRIX_CALIBRATION(uint8_t) break;
				case GPMF_TYPE_SIGNED_SHORT:  MACRO_IS_MATRIX_CALIBRATION(int16_t) break;
				case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_IS_MATRIX_CALIBRATION(uint16_t) break;
				case GPMF_TYPE_SIGNED_LONG:  MACRO_IS_MATRIX_CALIBRATION(int32_t) break;
				case GPMF_TYPE_UNSIGNED_LONG:  MACRO_IS_MATRIX_CALIBRATION(uint32_t) break;
				case GPMF_TYPE_FLOAT: MACRO_IS_MATRIX_CALIBRATION(float); break;
				case GPMF_TYPE_DOUBLE: MACRO_IS_MATRIX_CALIBRATION(double); break;
				}
			}

			if (!mtrx_calibration)
			{
				GPMF_CopyState(ms, &fs);
				if (GPMF_OK == GPMF_FindPrev(&fs, GPMF_KEY_ORIENTATION_IN, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
				{
					orin_data = (char *)GPMF_RawData(&fs);
					orin_len = GPMF_DATA_PACKEDSIZE(fs.buffer[fs.pos + 1]);
				}
				GPMF_CopyState(ms, &fs);
				if (GPMF_OK == GPMF_FindPrev(&fs, GPMF_KEY_ORIENTATION_OUT, GPMF_CURRENT_LEVEL|GPMF_TOLERANT))
				{
					orio_data = (char *)GPMF_RawData(&fs);
					orio_len = GPMF_DATA_PACKEDSIZE(fs.buffer[fs.pos + 1]);
				}
				if (orio_len == orin_len && orin_len > 1 && orio_len == elements)
				{
					uint32_t x, y, pos = 0;

					mtrx_data = (uint32_t*)mtrx_buffer;
					mtrx_type = outputType;

					for (y = 0; y < elements; y++)
					{
						for (x = 0; x < elements; x++)
						{
							switch (mtrx_type)
							{
							case GPMF_TYPE_FLOAT:			MACRO_SET_MATRIX(float,       orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_DOUBLE:			MACRO_SET_MATRIX(double,      orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_SIGNED_BYTE:		MACRO_SET_MATRIX(int8_t,      orio_data[y], orin_data[x], pos);   break;
							case GPMF_TYPE_UNSIGNED_BYTE:	MACRO_SET_MATRIX(uint8_t,     orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_SIGNED_SHORT:	MACRO_SET_MATRIX(int16_t,     orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_SET_MATRIX(uint16_t,    orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_SIGNED_LONG:		MACRO_SET_MATRIX(int32_t,     orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_UNSIGNED_LONG:	MACRO_SET_MATRIX(uint32_t,    orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_SIGNED_64BIT_INT:  MACRO_SET_MATRIX(int64_t,   orio_data[y], orin_data[x], pos);  break;
							case GPMF_TYPE_UNSIGNED_64BIT_INT: MACRO_SET_MATRIX(uint64_t, orio_data[y], orin_data[x], pos);  break;
							default:
								ret = GPMF_ERROR_SCALE_NOT_SUPPORTED;
								goto cleanup;
								break;
							}

							pos++;
						}
					}

					mtrx_calibration = 1;
				}
			}
		}


		while (read_samples--)
		{
			uint32_t i;
			uint8_t *scal_data8 = (uint8_t *)scal_data;

			for (i = 0; i < elements; i++)
			{
				if (inputtypeelements == 0)
				{
					ret = GPMF_ERROR_MEMORY;
					goto cleanup;
				}

				if (noswap)
				{
					switch (complextype[i % inputtypeelements])
					{
					case GPMF_TYPE_FLOAT:  MACRO_NOSWAP_CAST_SCALE(float) break;
					case GPMF_TYPE_SIGNED_BYTE:  MACRO_NOSWAP_CAST_SCALE(int8_t) break;
					case GPMF_TYPE_UNSIGNED_BYTE:  MACRO_NOSWAP_CAST_UNSIGNED_SCALE(uint8_t) break;
					case GPMF_TYPE_SIGNED_SHORT:  MACRO_NOSWAP_CAST_SCALE(int16_t) break;
					case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_NOSWAP_CAST_UNSIGNED_SCALE(uint16_t) break;
					case GPMF_TYPE_SIGNED_LONG:  MACRO_NOSWAP_CAST_SCALE(int32_t) break;
					case GPMF_TYPE_UNSIGNED_LONG:  MACRO_NOSWAP_CAST_UNSIGNED_SCALE(uint32_t) break;
					case GPMF_TYPE_SIGNED_64BIT_INT:  MACRO_NOSWAP_CAST_SCALE(int64_t) break;
					case GPMF_TYPE_UNSIGNED_64BIT_INT:  MACRO_NOSWAP_CAST_UNSIGNED_SCALE(uint64_t) break;
					default:
						ret = GPMF_ERROR_SCALE_NOT_SUPPORTED;
						goto cleanup;
						break;
					}
				}
				else
				{
					switch (complextype[i % inputtypeelements])
					{
					case GPMF_TYPE_FLOAT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, float, uint32_t) break;
					case GPMF_TYPE_SIGNED_BYTE:  MACRO_BSWAP_CAST_SCALE(NOSWAP8, int8_t, uint8_t) break;
					case GPMF_TYPE_UNSIGNED_BYTE:  MACRO_BSWAP_CAST_UNSIGNED_SCALE(NOSWAP8, uint8_t, uint8_t) break;
					case GPMF_TYPE_SIGNED_SHORT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP16, int16_t, uint16_t) break;
					case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_BSWAP_CAST_UNSIGNED_SCALE(BYTESWAP16, uint16_t, uint16_t) break;
					case GPMF_TYPE_SIGNED_LONG:  MACRO_BSWAP_CAST_SCALE(BYTESWAP32, int32_t, uint32_t) break;
					case GPMF_TYPE_UNSIGNED_LONG:  MACRO_BSWAP_CAST_UNSIGNED_SCALE(BYTESWAP32, uint32_t, uint32_t) break;
					case GPMF_TYPE_SIGNED_64BIT_INT:  MACRO_BSWAP_CAST_SCALE(BYTESWAP64, int64_t, uint64_t) break;
					case GPMF_TYPE_UNSIGNED_64BIT_INT:  MACRO_BSWAP_CAST_UNSIGNED_SCALE(BYTESWAP64, uint64_t, uint64_t) break;
					case GPMF_TYPE_FOURCC: // Don't scale, just store, if it will fit.
						{					
						uint32_t* out = (uint32_t*)output;
						switch (outputType) {
							case GPMF_TYPE_FLOAT:	
							case GPMF_TYPE_SIGNED_LONG:	
							case GPMF_TYPE_UNSIGNED_LONG:	
								*out++ = *(uint32_t*)data;
								data += 4;
								output += 4;
								break;
							case GPMF_TYPE_DOUBLE:			
								*out++ = *(uint32_t*)data;
								*out++ = 0;
								data += 4;
								output += 8;
								break;
							default: //bytes and shorts
								ret = GPMF_ERROR_SCALE_NOT_SUPPORTED; // FourCC can't fit in these target buffers
								goto cleanup;
								break;
							}			
						} break;
					default:
						ret = GPMF_ERROR_SCALE_NOT_SUPPORTED;
						goto cleanup;
						break;
					}
				}
				if (scal_count > 1)
					scal_data8 += scal_typesize;
			}


			if (inputtypeelements == 1)
			{
				if (mtrx_calibration)
				{
					switch (mtrx_type)
					{
					case GPMF_TYPE_SIGNED_BYTE:  MACRO_APPLY_MATRIX_CALIBRATION(int8_t) break;
					case GPMF_TYPE_UNSIGNED_BYTE:  MACRO_APPLY_MATRIX_CALIBRATION(uint8_t) break;
					case GPMF_TYPE_SIGNED_SHORT:  MACRO_APPLY_MATRIX_CALIBRATION(int16_t) break;
					case GPMF_TYPE_UNSIGNED_SHORT:  MACRO_APPLY_MATRIX_CALIBRATION(uint16_t) break;
					case GPMF_TYPE_SIGNED_LONG:  MACRO_APPLY_MATRIX_CALIBRATION(int32_t) break;
					case GPMF_TYPE_UNSIGNED_LONG:  MACRO_APPLY_MATRIX_CALIBRATION(uint32_t) break;
					case GPMF_TYPE_FLOAT: MACRO_APPLY_MATRIX_CALIBRATION(float); break;
					case GPMF_TYPE_DOUBLE: MACRO_APPLY_MATRIX_CALIBRATION(double); break;
					default: break;
					}
				}
			}
		}
		break;

		default:
			ret = GPMF_ERROR_SCALE_NOT_SUPPORTED;
			goto cleanup;
			break;
		}

cleanup:
		if(uncompressedSamples)
			free(uncompressedSamples);

		return ret;
	}

	return GPMF_ERROR_MEMORY;
}



GPMF_ERR GPMF_DecompressedSize(GPMF_stream *ms, uint32_t *neededsize)
{
	if (ms && neededsize)
	{
		*neededsize = GPMF_DATA_SIZE(ms->buffer[ms->pos + 2]); // The first 32-bit of data, is the uncomresseded type-size-repeat
		return GPMF_OK;
	}
	
	return GPMF_ERROR_MEMORY;
}


GPMF_ERR GPMF_Decompress(GPMF_stream *ms, uint32_t *localbuf, uint32_t localbuf_size)
{
	if (ms && localbuf && localbuf_size)
	{
		if (ms->cbhandle == 0)
			if (GPMF_OK != GPMF_AllocCodebook(&ms->cbhandle))
				return GPMF_ERROR_MEMORY;

		memset(localbuf, 0, localbuf_size); 

		// unpack here
		GPMF_SampleType type = (GPMF_SampleType)GPMF_SAMPLE_TYPE(ms->buffer[ms->pos + 2]);// The first 32-bit of data, is the uncomresseded type-size-repeat
		uint8_t *start = (uint8_t *)&ms->buffer[ms->pos + 3];
		uint16_t quant;
		size_t sOffset = 0;
		uint16_t *compressed_data;
		uint32_t sample_size = GPMF_SAMPLE_SIZE(ms->buffer[ms->pos + 2]);
		uint32_t sizeoftype = GPMF_SizeofType(type);
		if(sizeoftype == 0) 
			return GPMF_ERROR_MEMORY;
		uint32_t chn = 0, channels = sample_size / sizeoftype;
		uint32_t compressed_size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 1]);
		uint32_t uncompressed_size = GPMF_DATA_PACKEDSIZE(ms->buffer[ms->pos + 2]);

		if (sizeoftype == 0 && localbuf_size < uncompressed_size)
			return GPMF_ERROR_MEMORY;

		int signed_type = 1;

		GPMF_codebook *cb = (GPMF_codebook *)ms->cbhandle;

		if (sizeoftype == 4) // LONGs are handled at two channels of SHORTs
		{
			sizeoftype = 2;
			channels *= 2;

			if (type == 'l')
				type = GPMF_TYPE_SIGNED_SHORT;
			else
				type = GPMF_TYPE_UNSIGNED_SHORT; 
		}


		if (type == GPMF_TYPE_SIGNED_SHORT || type == GPMF_TYPE_SIGNED_BYTE)
			signed_type = -1; //signed


		uint16_t *buf_u16 = (uint16_t *)localbuf;
		int16_t *buf_s16 = (int16_t *)localbuf;
		uint8_t *buf_u8 = (uint8_t *)localbuf;
		int8_t *buf_s8 = (int8_t *)localbuf;
		int32_t last;
		uint32_t pos, end = 0;

		if(sample_size > compressed_size)
			return GPMF_ERROR_MEMORY;
			
		memcpy(&buf_u8[0], start, sample_size);

		sOffset += sample_size;

		for (chn = 0; chn<channels; chn++)
		{
			pos = 1;

			if (sOffset >= localbuf_size)
				return GPMF_ERROR_MEMORY;

			switch ((int)sizeoftype * signed_type)
			{
			default:
			case -2: last = BYTESWAP16(buf_s16[chn]); quant = *((uint16_t *)&start[sOffset]); quant = BYTESWAP16(quant); sOffset += 2; break;
			case -1: last = (int8_t)buf_s8[chn]; quant = *((uint8_t *)&start[sOffset]); sOffset++; break;
			case 1: last = buf_u8[chn]; quant = *((uint8_t *)&start[sOffset]); sOffset++; break;
			case 2: last = BYTESWAP16(buf_u16[chn]); quant = *((uint16_t *)&start[sOffset]); quant = BYTESWAP16(quant); sOffset += 2;  break;
			}
						
			sOffset = ((sOffset + 1) & (uint32_t)~1); //16-bit aligned compressed data

			if (sOffset >= compressed_size)
				return GPMF_ERROR_MEMORY;

			compressed_data = (uint16_t *)&start[sOffset];

			uint16_t currWord = BYTESWAP16(*compressed_data); compressed_data++;
			uint16_t nextWord = BYTESWAP16(*compressed_data); compressed_data++;
			int currBits = 16;
			int nextBits = 16;

			do
			{
				switch (cb[currWord].command)
				{
				case 0:  // store zeros and/or a value
					{
						int usedbits = cb[currWord].bits_used;
						uint32_t zeros = cb[currWord].offset;
						int delta = (int)cb[currWord].value * quant;

						last += delta * cb[currWord].bytes_stored;

						if (channels * (pos + zeros) * sizeoftype > localbuf_size) 
							return GPMF_ERROR_MEMORY;

						switch ((int)sizeoftype * signed_type)
						{
						default:
						case -2:
							while (zeros) { buf_s16[channels*pos++ + chn] = (int16_t) BYTESWAP16(last); zeros--; }
							buf_s16[channels*pos + chn] = (int16_t) BYTESWAP16(last);
							break;
						case -1:
							while (zeros) { buf_s8[channels*pos++ + chn] = (int8_t)last; zeros--; }
							buf_s8[channels*pos + chn] = (int8_t)last;
							break;
						case 1:
							while (zeros) { buf_u8[channels*pos++ + chn] = (uint8_t)last; zeros--; }
							buf_u8[channels*pos + chn] = (uint8_t)last;
							break;
						case 2:
							while (zeros) { buf_u16[channels*pos++ + chn] = BYTESWAP16(last); zeros--; }
							buf_u16[channels*pos + chn] = BYTESWAP16(last);
							break;
						}
										
						pos += (uint32_t) cb[currWord].bytes_stored;
						currWord <<= usedbits;
						currBits -= usedbits;
					}
					break;

				case 1: //channel END code detected, store the remaining zero deltas
					{
						int zeros = (int)(uncompressed_size/(channels*sizeoftype) - pos);

						if (channels * (pos + zeros) * sizeoftype > localbuf_size)
							return GPMF_ERROR_MEMORY;

						switch ((int)sizeoftype*signed_type)
						{
						default:
						case -2:
							while (zeros) { buf_s16[channels*pos++ + chn] = (int16_t) BYTESWAP16(last); zeros--; }
							break;
						case -1:
							while (zeros) { buf_s8[channels*pos++ + chn] = (int8_t)last; zeros--; }
							break;
						case 1:
							while (zeros) { buf_u8[channels*pos++ + chn] = (uint8_t)last; zeros--; }
							break;
						case 2:
							while (zeros) { buf_u16[channels*pos++ + chn] = (uint16_t) BYTESWAP16(last); zeros--; }
							break;
						}
					}
					end = 1;
					break;

				case 2: //ESC code, next byte or short contains the delta.
					{
						int usedbits = cb[currWord].bits_used;
						int delta;
						currWord <<= usedbits;
						currBits -= usedbits;

						//Get more bits
						while (currBits < 16)
						{
							int needed = 16 - currBits;
							currWord |= nextWord >> currBits;
							if (nextBits >= needed) currBits = 16; else currBits += nextBits;	
							nextWord <<= needed;
							nextBits -= needed;
							if (nextBits <= 0)
							{
								if(((size_t)compressed_data - (size_t)start) >= compressed_size)
									return GPMF_ERROR_MEMORY;
								nextWord = BYTESWAP16(*compressed_data);
								compressed_data++;
								nextBits = 16;
							}
						}

						if((channels * pos * sizeoftype) >= localbuf_size)
							return GPMF_ERROR_MEMORY;
						
						switch ((int)sizeoftype*signed_type)
						{
						default:
						case -2:
							delta = (int16_t)(currWord);
							delta *= quant;
							last += delta;
							buf_s16[channels*pos++ + chn] = (int16_t) BYTESWAP16(last);
							break;
						case -1:
							delta = (int8_t)(currWord >> 8);
							delta *= quant;
							last += delta;
							buf_s8[channels*pos++ + chn] = (int8_t)last;
							break;
						case 1: 
							delta = (int8_t)(currWord >> 8);
							delta *= quant;
							last += delta;
							buf_u8[channels*pos++ + chn] = (uint8_t)last;
							break;
						case 2:
							delta = (int16_t)(currWord);
							delta *= quant;
							last += delta;
							buf_u16[channels*pos++ + chn] = BYTESWAP16(last);
							break;
						}
						currWord <<= 8 * sizeoftype;
						currBits -= 8 * sizeoftype;
					}
					break;

				default: //Invalid codeword read
					end = 1; 
					return GPMF_ERROR_MEMORY;
					break;
				}

				//Get more bits
				while (currBits < 16)
				{
					int needed = 16 - currBits;
					currWord |= nextWord >> currBits;
					if (nextBits >= needed) currBits = 16; else currBits += nextBits;
					nextWord <<= needed;
					nextBits -= needed;
					if (nextBits <= 0)
					{
						if (((size_t)compressed_data - (size_t)start) >= compressed_size)
							return GPMF_ERROR_MEMORY;
						nextWord = BYTESWAP16(*compressed_data);
						compressed_data++;
						nextBits = 16;
					}
				}
			} while (!end);
			
			if (nextBits == 16) compressed_data--;
			sOffset = (size_t)compressed_data - (size_t)start;
			end = 0;
		}

		return GPMF_OK;
	}

	return GPMF_ERROR_MEMORY;
}


GPMF_ERR GPMF_AllocCodebook(size_t *cbhandle)
{
	*cbhandle = (size_t)malloc(65536 * sizeof(GPMF_codebook));
	if (*cbhandle)
	{
		int i,v,z;
		GPMF_codebook *cb = (GPMF_codebook *)*cbhandle;

		for (i = 0; i <= 0xffff; i++)
		{
			uint16_t code = (uint16_t)i;
			uint16_t mask = 0x8000;
			int zeros = 0, used = 0;

			cb->command = 0;
			
			// all commands are 16-bits long
			if (code == enccontrolcodestable.entries[HUFF_ESC_CODE_ENTRY].bits)
			{
				cb->command = 2;
				cb->bytes_stored = 1;
				cb->bits_used = 16;
				cb->offset = 0;
				cb++;
				continue;
			}
			if (code == enccontrolcodestable.entries[HUFF_END_CODE_ENTRY].bits)
			{
				cb->command = 1;
				cb->bytes_stored = 0;
				cb->bits_used = 16;
				cb->offset = 0;
				cb++;
				continue;
			}
			
			for (z = enczerorunstable.length-1; z >= 0; z--)
			{
				if (16 - used >= enczerorunstable.entries[z].size)
				{
					if ((code >> (16 - enczerorunstable.entries[z].size)) == enczerorunstable.entries[z].bits)
					{
						zeros += enczerorunstable.entries[z].count;
						used  += enczerorunstable.entries[z].size;
						mask >>= enczerorunstable.entries[z].size;
						break;
					}
				}
				else break;
			}

			// count single zeros.
			while (!(code & mask) && mask)
			{
				zeros++;
				used++;
				mask >>= 1;
			}

			//move the code word up to see if is a complete code for a value following the zeros.  
			code <<= used;

			cb->bytes_stored = 0;
			for (v=enchuftable.length-1; v>0; v--)
			{
				if (16-used >= enchuftable.entries[v].size+1) // codeword + sign bit
				{
					if ((code >> (16 - enchuftable.entries[v].size)) == enchuftable.entries[v].bits)
					{
						int sign = 1-(((code >> (16 - (enchuftable.entries[v].size + 1))) & 1)<<1); // last bit is the sign.
						cb->value = enchuftable.entries[v].value * (int16_t)sign;
						used += enchuftable.entries[v].size+1;
						cb->bytes_stored = 1;
						break;
					}
				}
			}
			
			if (used == 0)
			{
				used = 16;
				cb->command = -1; // ERROR invalid code
			}
			cb->bits_used = (uint8_t)used;
			cb->offset = (uint8_t)zeros;
			cb++;
		}

		return GPMF_OK;
	}

	return GPMF_ERROR_MEMORY;
}

GPMF_ERR GPMF_FreeCodebook(size_t cbhandle)
{
	GPMF_codebook *cb = (GPMF_codebook *)cbhandle;

	if (cb)
	{
		free(cb);

		return GPMF_OK;
	}
	return GPMF_ERROR_MEMORY;
}

GPMF_ERR GPMF_Free(GPMF_stream* ms)
{
	if (ms)
	{
		if (ms->cbhandle != 0)
		{
			GPMF_FreeCodebook(ms->cbhandle);
			ms->cbhandle = 0;
		}
		return GPMF_OK;
	}
	return GPMF_ERROR_MEMORY;
}