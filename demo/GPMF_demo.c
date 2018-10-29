/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 1.0.1
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
#include "GPMF_mp4reader.h"


extern void PrintGPMF(GPMF_stream *ms);

int main(int argc, char *argv[])
{
	int32_t ret = GPMF_OK;
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	double metadatalength;
	uint32_t *payload = NULL; //buffer to store GPMF samples from the MP4.


	// get file return data
	if (argc != 2)
	{
		printf("usage: %s <file_with_GPMF>\n", argv[0]);
		return -1;
	}

	size_t mp4 = OpenMP4Source(argv[1], MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE);
//	size_t mp4 = OpenMP4SourceUDTA(argv[1]);  //Search for GPMF payload with MP4's udta 

	metadatalength = GetDuration(mp4);

	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberPayloads(mp4);
//		printf("found %.2fs of metadata, from %d payloads, within %s\n", metadatalength, payloads, argv[1]);

#if 1
		if (payloads == 1) // Printf the contents of the single payload
		{
			uint32_t payloadsize = GetPayloadSize(mp4,0);
			payload = GetPayload(mp4, payload, 0);
			if(payload == NULL)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;

			// Output (printf) all the contained GPMF data within this payload
			ret = GPMF_Validate(ms, GPMF_RECURSE_LEVELS); // optional
			if (GPMF_OK != ret)
			{
				printf("Invalid Structure\n");
				goto cleanup;
			}

			GPMF_ResetState(ms);
			do
			{
				PrintGPMF(ms);  // printf current GPMF KLV
			} while (GPMF_OK == GPMF_Next(ms, GPMF_RECURSE_LEVELS));
			GPMF_ResetState(ms);
			printf("\n");

		}
#endif


		for (index = 0; index < payloads; index++)
		{
			uint32_t payloadsize = GetPayloadSize(mp4, index);
			float in = 0.0, out = 0.0; //times
			payload = GetPayload(mp4, payload, index);
			if (payload == NULL)
				goto cleanup;

			ret = GetPayloadTime(mp4, index, &in, &out);
			if (ret != GPMF_OK)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;

#if 1		// Find all the available Streams and the data carrying FourCC
			if (index == 0) // show first payload 
			{
				ret = GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS);
				while (GPMF_OK == ret)
				{
					ret = GPMF_SeekToSamples(ms);
					if (GPMF_OK == ret) //find the last FOURCC within the stream
					{
						uint32_t key = GPMF_Key(ms);
						GPMF_SampleType type = GPMF_Type(ms);
						uint32_t elements = GPMF_ElementsInStruct(ms);
						//uint32_t samples = GPMF_Repeat(ms);
						uint32_t samples = GPMF_PayloadSampleCount(ms);

						if (samples)
						{
							printf("  STRM of %c%c%c%c ", PRINTF_4CC(key));

							if (type == GPMF_TYPE_COMPLEX)
							{
								GPMF_stream find_stream;
								GPMF_CopyState(ms, &find_stream);

								if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TYPE, GPMF_CURRENT_LEVEL))
								{
									char tmp[64];
									char *data = (char *)GPMF_RawData(&find_stream);
									int size = GPMF_RawDataSize(&find_stream);

									if (size < sizeof(tmp))
									{
										memcpy(tmp, data, size);
										tmp[size] = 0;
										printf("of type %s ", tmp);
									}
								}

							}
							else
							{
								printf("of type %c ", type);
							}

							printf("with %d sample%s ", samples, samples > 1 ? "s" : "");

							if (elements > 1)
								printf("-- %d elements per sample", elements);

							printf("\n");
						}

						ret = GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS);
					}
					else
					{
						if (ret == GPMF_ERROR_BAD_STRUCTURE) // some payload element was corrupt, skip to the next valid GPMF KLV at the previous level.
						{
							ret = GPMF_Next(ms, GPMF_CURRENT_LEVEL); // this will be the next stream if any more are present.
						}
					}
				}
				GPMF_ResetState(ms);
				printf("\n");
			}
#endif 




#if 1		// Find GPS values and return scaled doubles. 
			if (index == 0) // show first payload 
			{
				if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPS5"), GPMF_RECURSE_LEVELS) || //GoPro Hero5/6/7 GPS
					GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GPRI"), GPMF_RECURSE_LEVELS))   //GoPro Karma GPS
				{
					uint32_t key = GPMF_Key(ms);
					uint32_t samples = GPMF_Repeat(ms);
					uint32_t elements = GPMF_ElementsInStruct(ms);
					uint32_t buffersize = samples * elements * sizeof(double);
					GPMF_stream find_stream;
					double *ptr, *tmpbuffer = malloc(buffersize);
					char units[10][6] = { "" };
					uint32_t unit_samples = 1;

					printf("MP4 Payload time %.3f to %.3f seconds\n", in, out);

					if (tmpbuffer && samples)
					{
						uint32_t i, j;

						//Search for any units to display
						GPMF_CopyState(ms, &find_stream);
						if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL) ||
							GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL))
						{
							char *data = (char *)GPMF_RawData(&find_stream);
							int ssize = GPMF_StructSize(&find_stream);
							unit_samples = GPMF_Repeat(&find_stream);

							for (i = 0; i < unit_samples; i++)
							{
								memcpy(units[i], data, ssize);
								units[i][ssize] = 0;
								data += ssize;
							}
						}

						//GPMF_FormattedData(ms, tmpbuffer, buffersize, 0, samples); // Output data in LittleEnd, but no scale
						GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE);  //Output scaled data as floats

						ptr = tmpbuffer;
						for (i = 0; i < samples; i++)
						{
							printf("%c%c%c%c ", PRINTF_4CC(key));
							for (j = 0; j < elements; j++)
								printf("%.3f%s, ", *ptr++, units[j%unit_samples]);

							printf("\n");
						}
						free(tmpbuffer);
					}
				}
				GPMF_ResetState(ms);
				printf("\n");
			}
#endif 
		}

#if 1
		// Find all the available Streams and compute they sample rates
		while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS))
		{
			if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
			{
				uint32_t fourcc = GPMF_Key(ms);
				double rate = GetGPMFSampleRate(mp4, fourcc, GPMF_SAMPLE_RATE_PRECISE);// GPMF_SAMPLE_RATE_FAST);
				printf("%c%c%c%c sampling rate = %f Hz\n", PRINTF_4CC(fourcc), rate);
			}
		}
#endif


	cleanup:
		if (payload) FreePayload(payload); payload = NULL;
		CloseSource(mp4);
	}

	return ret;
}
