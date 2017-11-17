/*! @file GPMF_print.c
 *
 *  @brief Demo to output GPMF for debugging
 *
 *  @version 1.0.2
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

#include "../GPMF_parser.h"


#define DBG_MSG printf


#define VERBOSE_OUTPUT		0

#if VERBOSE_OUTPUT
#define LIMITOUTPUT		arraysize = structsize;
#else
#define LIMITOUTPUT		if (arraysize > 1 && repeat > 3) repeat = 3, dots = 1; else if (repeat > 6) repeat = 6, dots = 1;
#endif

void printfData(uint32_t type, uint32_t structsize, uint32_t repeat, void *data)
{
	int dots = 0;

	switch (type)
	{
	case GPMF_TYPE_STRING_ASCII:
		{
			char t[256];
			int size = structsize*repeat;
			uint32_t arraysize = structsize;
			LIMITOUTPUT;

			if (size > 255)
			{
				size = 255;
			}
			memcpy(t, (char *)data, size);
			t[size] = 0;

			if (arraysize == 1 || repeat == 1)
			{
				DBG_MSG("\"%s\"", t);
				dots = 0;
			}
			else
			{
				uint32_t i,j,pos=0;
				for (i = 0; i < repeat; i++)
				{
					DBG_MSG("\"");
					for (j = 0; j < arraysize; j++)
					{
						if (t[pos] != '\0' && t[pos] != ' ')
							DBG_MSG("%c", t[pos]);
						pos++;
					}
					DBG_MSG("\", ");
				}
			}
		}
		break;
	case GPMF_TYPE_SIGNED_BYTE:
		{
			int8_t *b = (int8_t *)data;
			uint32_t arraysize = structsize;
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize;

				while (arraysize--)
				{
					DBG_MSG("%d,", (int8_t)*b);
					b++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_UNSIGNED_BYTE:
		{
			uint8_t *b = (uint8_t *)data;
			uint32_t arraysize = structsize;
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize;

				while (arraysize--)
				{
					DBG_MSG("%d,", *b);
					b++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_DOUBLE:	
		{
			uint64_t Swap, *L = (uint64_t *)data;
			double *d;
			uint32_t arraysize = structsize / sizeof(uint64_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(uint64_t);

				while (arraysize--)
				{
					Swap = BYTESWAP64(*L);
					d = (double *)&Swap;
					DBG_MSG("%.3f,", *d);
					L++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_FLOAT:
		{
			uint32_t Swap, *L = (uint32_t *)data;
			float *f;
			uint32_t arraysize = structsize / sizeof(uint32_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(uint32_t);

				while (arraysize--)
				{
					Swap = BYTESWAP32(*L);
					f = (float *)&Swap;
					DBG_MSG("%.3f,", *f);
					L++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
	break;
	case GPMF_TYPE_FOURCC:
		{
			uint32_t *L = (uint32_t *)data;
			uint32_t arraysize = structsize / sizeof(uint32_t);
			LIMITOUTPUT;
			while (repeat--)
			{
				arraysize = structsize / sizeof(uint32_t);

				while (arraysize--)
				{
					DBG_MSG("%c%c%c%c,", PRINTF_4CC(*L));
					L++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_GUID: // display GUID in this formatting ABCDEF01-02030405-06070809-10111213
		{
			uint8_t *B = (uint8_t *)data;
			uint32_t arraysize = structsize;
			LIMITOUTPUT;
			while (repeat--)
			{
				arraysize = structsize;

				while (arraysize--)
				{
					DBG_MSG("%02X", *B);
					B++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_SIGNED_SHORT:
		{
			int16_t *s = (int16_t *)data;
			uint32_t arraysize = structsize / sizeof(int16_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(int16_t);

				while (arraysize--)
				{
					DBG_MSG("%d,", (int16_t)BYTESWAP16(*s));
					s++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_UNSIGNED_SHORT:
		{
			uint16_t *S = (uint16_t *)data;
			uint32_t arraysize = structsize / sizeof(uint16_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(uint16_t);

				while (arraysize--)
				{
					DBG_MSG("%d,", BYTESWAP16(*S));
					S++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_SIGNED_LONG:
		{
			int32_t *l = (int32_t *)data;
			uint32_t arraysize = structsize / sizeof(uint32_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(uint32_t);

				while (arraysize--)
				{
					DBG_MSG("%d,", (int32_t)BYTESWAP32(*l));
					l++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_UNSIGNED_LONG:
		{
			uint32_t *L = (uint32_t *)data;
			uint32_t arraysize = structsize / sizeof(uint32_t);
			LIMITOUTPUT;
			while (repeat--)
			{
				arraysize = structsize / sizeof(uint32_t);

				while (arraysize--)
				{
					DBG_MSG("%d,", BYTESWAP32(*L));
					L++;
				}
				if(repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_Q15_16_FIXED_POINT:		
		{
			int32_t *q = (int32_t *)data;
			uint32_t arraysize = structsize / sizeof(int32_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(int32_t);

				while (arraysize--)
				{
					double dq = BYTESWAP32(*q);
					dq /= (double)65536.0;
					DBG_MSG("%.3f,", dq);
					q++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_Q31_32_FIXED_POINT:
		{
			int64_t *Q = (int64_t *)data;
			uint32_t arraysize = structsize / sizeof(int64_t);
			LIMITOUTPUT;

			while (repeat--)
			{
				arraysize = structsize / sizeof(int64_t);

				while (arraysize--)
				{
					uint64_t Q64 = (uint64_t)BYTESWAP64(*Q);
					double dq = (double)(Q64 >> (uint64_t)32);
					dq += (double)(Q64 & (uint64_t)0xffffffff) / (double)0x100000000;
					DBG_MSG("%.3f,", dq);
					Q++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_UTC_DATE_TIME:
		{
			char *U = (char *)data;
			uint32_t arraysize = structsize / 16;
			LIMITOUTPUT;
			while (repeat--)
			{
				arraysize = structsize / 16;
				char t[17];
				t[16] = 0;

				while (arraysize--)
				{
#ifdef _WINDOWS
					strncpy_s(t, 17, U, 16);
#else
					strncpy(t, U, 16);
#endif
					DBG_MSG("\"%s\",", t);
					U += 16;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_SIGNED_64BIT_INT:
		{
			int64_t *J = (int64_t *)data;
			uint32_t arraysize = structsize / sizeof(int64_t);
			LIMITOUTPUT;
			while (repeat--)
			{
				arraysize = structsize / sizeof(int64_t);

				while (arraysize--)
				{
					DBG_MSG("%lld,", BYTESWAP64(*J));
					J++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	case GPMF_TYPE_UNSIGNED_64BIT_INT:
		{
			uint64_t *J = (uint64_t *)data;
			uint32_t arraysize = structsize / sizeof(uint64_t);
			LIMITOUTPUT;
			while (repeat--)
			{
				arraysize = structsize / sizeof(uint64_t);

				while (arraysize--)
				{
					DBG_MSG("%llu,", BYTESWAP64(*J));
					J++;
				}
				if (repeat) DBG_MSG(" ");
			}
		}
		break;
	default:
		break;
	}

	if (dots) // more data was not output
		DBG_MSG("...");
}


void PrintGPMF(GPMF_stream *ms)
{
	if (ms)
	{
		uint32_t key = GPMF_Key(ms);
		uint32_t type = GPMF_Type(ms);
		uint32_t structsize = GPMF_StructSize(ms);
		uint32_t repeat = GPMF_Repeat(ms);
		uint32_t size = GPMF_RawDataSize(ms);
		uint32_t indent, level = GPMF_NestLevel(ms);
		void *data = GPMF_RawData(ms);

		if (key != GPMF_KEY_DEVICE) level++;

		indent = level;
		while (indent > 0 && indent < 10)
		{
			DBG_MSG("  ");
			indent--;
		}
		if (type == 0)
			DBG_MSG("%c%c%c%c nest size %d ", (key >> 0) & 0xff, (key >> 8) & 0xff, (key >> 16) & 0xff, (key >> 24) & 0xff, size);
		else if (structsize == 1 || (repeat == 1 && type != '?'))
			DBG_MSG("%c%c%c%c type '%c' size %d ", (key >> 0) & 0xff, (key >> 8) & 0xff, (key >> 16) & 0xff, (key >> 24) & 0xff, type == 0 ? '0' : type, size);
		else
			DBG_MSG("%c%c%c%c type '%c' samplesize %d repeat %d ", (key >> 0) & 0xff, (key >> 8) & 0xff, (key >> 16) & 0xff, (key >> 24) & 0xff, type == 0 ? '0' : type, structsize, repeat);

		if (type && repeat > 0)
		{
			DBG_MSG("data: ");

			if (type == GPMF_TYPE_COMPLEX)
			{
				GPMF_stream find_stream;
				GPMF_CopyState(ms, &find_stream);
				if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL))
				{
					char *srctype = GPMF_RawData(&find_stream);
					uint32_t typelen = GPMF_RawDataSize(&find_stream);
					int struct_size_of_type;

					struct_size_of_type = GPMF_SizeOfComplexTYPE(srctype, typelen);
					if (struct_size_of_type != (int32_t)structsize)
					{
						DBG_MSG("error: found structure of %d bytes reported as %d bytes", struct_size_of_type, structsize);
					}
					else
					{
						char typearray[64];
						uint32_t elements = sizeof(typearray);
						uint8_t *bdata = (uint8_t *)data;
						uint32_t i;

						if (GPMF_OK == GPMF_ExpandComplexTYPE(srctype, typelen, typearray, &elements))
						{
							uint32_t j;
#if !VERBOSE_OUTPUT
							if (repeat > 3) repeat = 3;
#endif
							for (j = 0; j < repeat; j++)
							{
								if (repeat > 1) {
									DBG_MSG("\n  "); 

									indent = level;
									while (indent > 0 && indent < 10)
									{
										DBG_MSG("  ");
										indent--;
									}
								}
								for (i = 0; i < elements; i++)
								{
									int elementsize = GPMF_SizeofType(typearray[i]);
									printfData(typearray[i], elementsize, 1, bdata);
									bdata += elementsize;
								}

							}
							if (repeat > 1)
								DBG_MSG("...");
						}
					}
				}
				else
				{
					DBG_MSG("unknown formatting");
				}
			}
			else
			{
				printfData(type, structsize, repeat, data);
			}
		}

		DBG_MSG("\n");
	}
}
