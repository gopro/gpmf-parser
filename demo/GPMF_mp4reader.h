/*! @file GPMF_mp4reader.h
*
*  @brief Way Too Crude MP4 reader 
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

#ifndef _GPMF_MP4READER_H
#define _GPMF_MP4READER_H

#ifdef __cplusplus
extern "C" {
#endif

float OpenGPMFSource(char *filename);
void CloseGPMFSource(void);
uint32_t GetNumberGPMFPayloads(void);
uint32_t *GetGPMFPayload(uint32_t index);
uint32_t GetGPMFPayloadSize(uint32_t index);
float GetGPMFSampleRate(uint32_t fourcc, uint32_t payloads);



#ifdef __cplusplus
}
#endif

#endif
