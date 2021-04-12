/*! @file GPMF_utils.h
*
*  @brief Utilities GPMF and MP4 handling
*
*  @version 1.2.0
*
*  (C) Copyright 2020 GoPro Inc (http://gopro.com/).
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

#ifndef _GPMF_UTILS_H
#define _GPMF_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#define GPMF_SAMPLE_RATE_FAST		0
#define GPMF_SAMPLE_RATE_PRECISE	1

typedef struct mp4callbacks
{
	size_t mp4handle;
	uint32_t (*cbGetNumberPayloads)(size_t mp4handle);									// number of indexed GPMF payloads
	uint32_t (*cbGetPayloadSize)(size_t mp4handle, uint32_t index);					// get payload size for a particular index
	uint32_t *(*cbGetPayload)(size_t mp4handle, size_t res, uint32_t index);			// get payload data for a particular index
	size_t	 (*cbGetPayloadResource)(size_t mp4handle, size_t reshandle, uint32_t initialMemorySize);	// get payload memory handler
	void	 (*cbFreePayloadResource)(size_t mp4handle, size_t reshandle);							// free payload memory handler
	uint32_t (*cbGetPayloadTime)(size_t mp4handle, uint32_t index, double* in, double* out); //MP4 timestamps for the payload
	uint32_t (*cbGetEditListOffsetRationalTime)(size_t mp4handle,						// get any time offset for GPMF track
		int32_t	 *offset_numerator, uint32_t* denominator);
} mp4callbacks;

double GetGPMFSampleRate(mp4callbacks cbobject, uint32_t fourcc, uint32_t timeBaseFourCC, uint32_t flags, double* in, double* out);

#ifdef __cplusplus
}
#endif

#endif
