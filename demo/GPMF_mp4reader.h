/*! @file GPMF_mp4reader.h
 *
 *  @brief Way Too Crude MP4 reader 
 *
 *  @version 1.0.0
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

#ifndef _GPMF_MP4READER_H
#define _GPMF_MP4READER_H

#include "../GPMF_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SampleToChunk
{
	uint32_t chunk_num;
	uint32_t samples;
	uint32_t id;
} SampleToChunk;

double OpenGPMFSource(char *filename);
double OpenGPMFSourceUDTA(char *filename);
void CloseGPMFSource(void);
uint32_t GetNumberGPMFPayloads(void);
uint32_t *GetGPMFPayload(uint32_t *lastpayload, uint32_t index);
void FreeGPMFPayload(uint32_t *lastpayload);
uint32_t GetGPMFPayloadSize(uint32_t index);
uint32_t GetGPMFPayloadTime(uint32_t index, double *in, double *out); //MP4 timestamps for the payload

#define GPMF_SAMPLE_RATE_FAST		0
#define GPMF_SAMPLE_RATE_PRECISE	1

double GetGPMFSampleRate(uint32_t fourcc, uint32_t flags);
double GetGPMFSampleRateAndTimes(GPMF_stream *gs, double lastrate, uint32_t index, double *in, double *out); //Jitter corrected sample(s) time, if lastrate is unknown, send 0.0 and it will be computed

#ifdef __cplusplus
}
#endif

#endif
