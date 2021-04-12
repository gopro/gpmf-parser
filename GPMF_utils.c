/*! @file GPMF_utils.c
 *
 *  @brief Utilities GPMF and MP4 handling
 *
 *  @version 1.2.0
 *
 *  (C) Copyright 2020 GoPro Inc (http://gopro.com/).
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
#include "GPMF_utils.h"


double GetGPMFSampleRate(mp4callbacks cb, uint32_t fourcc, uint32_t timeBaseFourCC, uint32_t flags, double *firstsampletime, double *lastsampletime)
{
	if (cb.mp4handle == 0) 
		return 0.0;

	uint32_t indexcount = cb.cbGetNumberPayloads(cb.mp4handle);

	GPMF_stream metadata_stream, *ms = &metadata_stream;
	uint32_t teststart = 0;
	uint32_t testend = indexcount;
	double rate = 0.0;

	uint32_t *payload;
	uint32_t payloadsize;
	size_t payloadres = 0;

	GPMF_ERR ret;

	if (indexcount < 1)
		return 0.0;

	payloadsize = cb.cbGetPayloadSize(cb.mp4handle, teststart);
	payloadres = cb.cbGetPayloadResource(cb.mp4handle, 0, payloadsize);
	payload = cb.cbGetPayload(cb.mp4handle, payloadres, teststart);

	ret = GPMF_Init(ms, payload, payloadsize);

	if (ret != GPMF_OK)
		goto cleanup;

	{
		uint64_t basetimestamp = 0;
		uint64_t starttimestamp = 0;
		uint64_t endtimestamp = 0;
		uint32_t startsamples = 0;
		uint32_t endsamples = 0;
		double intercept = 0.0;



		while (teststart < indexcount && ret == GPMF_OK && GPMF_OK != GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
		{
			teststart++;
			payloadsize = cb.cbGetPayloadSize(cb.mp4handle, teststart);
			payload = cb.cbGetPayload(cb.mp4handle, payloadres, teststart); // second last payload
			ret = GPMF_Init(ms, payload, payloadsize);
		}

		if (ret == GPMF_OK && payload)
		{
			double startin, startout, endin, endout;
			int usedTimeStamps = 0;

			uint32_t samples = GPMF_PayloadSampleCount(ms);
			GPMF_stream find_stream;
			GPMF_CopyState(ms, &find_stream);  //ms is at the searched fourcc
			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
				startsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream)) - samples;

			GPMF_CopyState(ms, &find_stream);
			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TIME_STAMP, GPMF_CURRENT_LEVEL))
				starttimestamp = BYTESWAP64(*(uint64_t *)GPMF_RawData(&find_stream));

			if (starttimestamp) // how does this compare to other streams in this early payload?
			{
				GPMF_stream any_stream;
				if (GPMF_OK == GPMF_Init(&any_stream, payload, payloadsize))
				{
					basetimestamp = starttimestamp;  

					if (timeBaseFourCC)
					{
						if (GPMF_OK == GPMF_FindNext(&any_stream, timeBaseFourCC, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
						{
							if (GPMF_OK == GPMF_FindPrev(&any_stream, GPMF_KEY_TIME_STAMP, GPMF_CURRENT_LEVEL))
							{
								basetimestamp = BYTESWAP64(*(uint64_t*)GPMF_RawData(&any_stream));
							}
						}
					}
					else
					{
						while (GPMF_OK == GPMF_FindNext(&any_stream, GPMF_KEY_TIME_STAMP, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
						{
							uint64_t timestamp = BYTESWAP64(*(uint64_t*)GPMF_RawData(&any_stream));
							if (timestamp < basetimestamp)
								basetimestamp = timestamp;
						}
					}
				}
			}
			//Note: basetimestamp is used the remove offset from the timestamp, 
			// however 0.0 may not be the same zero for your video or audio presentation time (although it should be close.)
			// On GoPro camera, metadata streams like SHUT and ISOE are metadata fields associated with video, and these can be used
			// to accurately sync meta with video.

			testend = indexcount;
			do
			{
				testend--;// last payload with the fourcc needed
				payloadsize = cb.cbGetPayloadSize(cb.mp4handle, testend);
				payload = cb.cbGetPayload(cb.mp4handle, payloadres, testend); // second last payload
				ret = GPMF_Init(ms, payload, payloadsize);
			} while (testend > 0 && ret == GPMF_OK &&  GPMF_OK != GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS | GPMF_TOLERANT));

			cb.cbGetPayloadTime(cb.mp4handle, teststart, &startin, &startout);
			cb.cbGetPayloadTime(cb.mp4handle, testend, &endin, &endout);

			GPMF_CopyState(ms, &find_stream);
			if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TOTAL_SAMPLES, GPMF_CURRENT_LEVEL))
				endsamples = BYTESWAP32(*(uint32_t *)GPMF_RawData(&find_stream));
			else // If there is no TSMP we have to count the samples.
			{
				uint32_t i;
				for (i = teststart; i <= testend; i++)
				{
					payloadsize = cb.cbGetPayloadSize(cb.mp4handle, i);
					payload = cb.cbGetPayload(cb.mp4handle, payloadres, i); 
					if (GPMF_OK == GPMF_Init(ms, payload, payloadsize))
						if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
							endsamples += GPMF_PayloadSampleCount(ms);
				}
			}

			if (starttimestamp != 0)
			{
				uint32_t last_samples = GPMF_PayloadSampleCount(ms);
				uint32_t totaltimestamped_samples = endsamples - last_samples - startsamples;
				double time_stamp_scale = 1000000000.0; // scan for nanoseconds, microseconds to seconds, all base 10.

				GPMF_CopyState(ms, &find_stream);
				if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TIME_STAMP, GPMF_CURRENT_LEVEL))
					endtimestamp = BYTESWAP64(*(uint64_t *)GPMF_RawData(&find_stream));

				if (endtimestamp)
				{
					double approxrate = 0.0;
					if (endsamples > startsamples)
						approxrate = (double)(endsamples - startsamples) / (endout - startin);

					if (approxrate == 0.0)
						approxrate = (double)(samples) / (endout - startin);


					while (time_stamp_scale >= 1)
					{
						rate = (double)(totaltimestamped_samples) / ((double)(endtimestamp - starttimestamp) / time_stamp_scale);
						if (rate*0.9 < approxrate && approxrate < rate*1.1)
							break;

						time_stamp_scale *= 0.1;
					}
					if (time_stamp_scale < 1.0) rate = 0.0;
					intercept = (((double)basetimestamp - (double)starttimestamp) / time_stamp_scale) * rate;
					usedTimeStamps = 1;
				}
			}

			if (rate == 0.0) //Timestamps didn't help, or weren't available
			{
				if (!(flags & GPMF_SAMPLE_RATE_PRECISE))
				{
					if (endsamples > startsamples)
						rate = (double)(endsamples - startsamples) / (endout - startin);

					if (rate == 0.0)
						rate = (double)(samples) / (endout - startin);

					intercept = (double)-startin * rate;
				}
				else // for increased precision, for older GPMF streams sometimes missing the total sample count 
				{
					uint32_t payloadpos = 0, payloadcount = 0;
					double slope, top = 0.0, bot = 0.0, meanX = 0, meanY = 0;
					uint32_t *repeatarray = (uint32_t *)malloc(indexcount * 4 + 4);
					memset(repeatarray, 0, indexcount * 4 + 4);

					samples = 0;

					for (payloadpos = teststart; payloadpos <= testend; payloadpos++)
					{

						payloadsize = cb.cbGetPayloadSize(cb.mp4handle, payloadpos);
						payload = cb.cbGetPayload(cb.mp4handle, payloadres, payloadpos); // second last payload
						ret = GPMF_Init(ms, payload, payloadsize);

						if (ret != GPMF_OK)
							goto cleanup;

						if (GPMF_OK == GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS | GPMF_TOLERANT))
						{
							GPMF_stream find_stream2;
							GPMF_CopyState(ms, &find_stream2);

							payloadcount++;

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

									if (GPMF_OK == cb.cbGetPayloadTime(cb.mp4handle, payloadpos, &in, &out))
										meanX += out;
								}
							}
							else
							{
								uint32_t repeat = GPMF_PayloadSampleCount(ms);
								samples += repeat;

								if (repeatarray)
								{
									double in, out;

									repeatarray[payloadpos] = samples;
									meanY += (double)samples;

									if (GPMF_OK == cb.cbGetPayloadTime(cb.mp4handle, payloadpos, &in, &out))
										meanX += out;
								}
							}
						}
						else
						{
							repeatarray[payloadpos] = 0;
						}
					}

					// Compute the line of best fit for a jitter removed sample rate.  
					// This does assume an unchanging clock, even though the IMU data can thermally impacted causing small clock changes.  
					// TODO: Next enhancement would be a low order polynominal fit the compensate for any thermal clock drift.
					if (repeatarray)
					{
						meanY /= (double)payloadcount;
						meanX /= (double)payloadcount;

						for (payloadpos = teststart; payloadpos <= testend; payloadpos++)
						{
							double in, out;
							if (repeatarray[payloadpos] && GPMF_OK == cb.cbGetPayloadTime(cb.mp4handle, payloadpos, &in, &out))
							{
								top += ((double)out - meanX)*((double)repeatarray[payloadpos] - meanY);
								bot += ((double)out - meanX)*((double)out - meanX);
							}
						}

						slope = top / bot;
						rate = slope;

						// This sample code might be useful for compare data latency between channels.
						intercept = meanY - slope * meanX;
#if 0
						printf("%c%c%c%c start offset = %f (%.3fms) rate = %f\n", PRINTF_4CC(fourcc), intercept, 1000.0 * intercept / slope, rate);
						printf("%c%c%c%c first sample at time %.3fms\n", PRINTF_4CC(fourcc), -1000.0 * intercept / slope);
#endif
					}
					else
					{
						rate = (double)(samples) / (endout - startin);
					}

					free(repeatarray);
				}
			}

			if (firstsampletime && lastsampletime)
			{
				uint32_t endpayload = indexcount;
				do
				{
					endpayload--;// last payload with the fourcc needed
					payloadsize = cb.cbGetPayloadSize(cb.mp4handle, endpayload);
					payload = cb.cbGetPayload(cb.mp4handle, payloadres, endpayload);
					ret = GPMF_Init(ms, payload, payloadsize);
				} while (endpayload > 0 && ret == GPMF_OK && GPMF_OK != GPMF_FindNext(ms, fourcc, GPMF_RECURSE_LEVELS | GPMF_TOLERANT));

				if (endpayload > 0 && ret == GPMF_OK)
				{
					uint32_t totalsamples = endsamples - startsamples;
					float timo = 0.0;

					GPMF_CopyState(ms, &find_stream);
					if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_TIME_OFFSET, GPMF_CURRENT_LEVEL))
						GPMF_FormattedData(&find_stream, &timo, 4, 0, 1);

					double first, last;
					first = -intercept / rate - timo;
					last = first + (double)totalsamples / rate;

					//Apply any Edit List corrections.
					if (usedTimeStamps)  // clips with STMP have the Edit List already applied via GetPayloadTime()
					{
						if (cb.cbGetEditListOffsetRationalTime)
						{
							int32_t num = 0;
							uint32_t dem = 1;
							cb.cbGetEditListOffsetRationalTime(cb.mp4handle, &num, &dem);
							first += (double)num / (double)dem;
							last += (double)num / (double)dem;
						}
					}

					//printf("%c%c%c%c first sample at time %.3fms, last at %.3fms\n", PRINTF_4CC(fourcc), 1000.0*first, 1000.0*last);

					if (firstsampletime) *firstsampletime = first;

					if (lastsampletime) *lastsampletime = last;
				}
			}
		}
	}

cleanup:
	if (payloadres) cb.cbFreePayloadResource(cb.mp4handle, payloadres);
	return rate;
}

