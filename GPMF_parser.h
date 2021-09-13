/*! @file GPMF_parser.h
 * 
 *  @brief GPMF Parser library include
 * 
 *  @version 2.0.0
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

#ifndef _GPMF_PARSER_H
#define _GPMF_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include "GPMF_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPMF_NEST_LIMIT 16

typedef struct GPMF_stream
{
	uint32_t *buffer;
	uint32_t buffer_size_longs;
	uint32_t pos;
	uint32_t last_level_pos[GPMF_NEST_LIMIT];
	uint32_t nest_size[GPMF_NEST_LIMIT];
	uint32_t last_seek[GPMF_NEST_LIMIT];
	uint32_t nest_level;
	uint32_t device_count;
	uint32_t device_id;
	char device_name[32];
	size_t cbhandle; // compression handler
} GPMF_stream;



typedef enum GPMF_LEVELS
{
	GPMF_CURRENT_LEVEL = 0,  // search or validate within the current GPMF next level
	GPMF_RECURSE_LEVELS = 1, // search or validate recursing all levels
	GPMF_TOLERANT = 2		 // Ignore minor errors like unknown datatypes if the structure is otherwise valid. 
} GPMF_LEVELS;

 

// Prepare GPMF data 
GPMF_ERR GPMF_Init(GPMF_stream *gs, uint32_t *buffer, uint32_t datasize);							//Initialize a GPMF_stream for parsing a particular buffer.
GPMF_ERR GPMF_ResetState(GPMF_stream *gs);														//Read from beginning of the buffer again
GPMF_ERR GPMF_CopyState(GPMF_stream *src, GPMF_stream *dst);									//Copy state, 
GPMF_ERR GPMF_Validate(GPMF_stream *gs, GPMF_LEVELS recurse);									//Is the nest structure valid GPMF? 

// Navigate through GPMF data 
GPMF_ERR GPMF_Next(GPMF_stream *gs, GPMF_LEVELS recurse);										//Step to the next GPMF KLV entrance, optionally recurse up or down nesting levels.
GPMF_ERR GPMF_FindPrev(GPMF_stream *gs, uint32_t fourCC, GPMF_LEVELS recurse);					//find a previous FourCC -- at the current level only if recurse is false
GPMF_ERR GPMF_FindNext(GPMF_stream *gs, uint32_t fourCC, GPMF_LEVELS recurse);					//find a particular FourCC upcoming -- at the current level only if recurse is false
GPMF_ERR GPMF_SeekToSamples(GPMF_stream *gs);													//find the last FourCC in the current level, this is raw data for any STRM

// Get information about the current GPMF KLV
uint32_t GPMF_Key(GPMF_stream *gs);																//return the current Key (FourCC)
GPMF_SampleType GPMF_Type(GPMF_stream *gs);														//return the current Type (GPMF_Type)
uint32_t GPMF_StructSize(GPMF_stream *gs);														//return the current sample structure size
uint32_t GPMF_Repeat(GPMF_stream *gs);															//return the current repeat or the number of samples of this structure
uint32_t GPMF_PayloadSampleCount(GPMF_stream *gs);														//return the current number of samples of this structure, supporting multisample entries.
uint32_t GPMF_ElementsInStruct(GPMF_stream *gs);												//return the current number elements within the structure (e.g. 3-axis gyro)
uint32_t GPMF_RawDataSize(GPMF_stream *gs);														//return the data size for the current GPMF KLV 
void *   GPMF_RawData(GPMF_stream *gs);															//return a pointer the KLV data (which is Bigendian if the type is known.)


GPMF_ERR GPMF_Modify(GPMF_stream* gs,                                                           //find and inplace overwrite a GPMF KLV with new KLV, if the lengths match.
    uint32_t origfourCC, uint32_t newfourCC, GPMF_SampleType newType, uint32_t newStructSize, uint32_t newRepeat, void* newData);

// Get information about where the GPMF KLV is nested
uint32_t GPMF_NestLevel(GPMF_stream *gs);														//return the current nest level
uint32_t GPMF_DeviceID(GPMF_stream *gs);														//return the current device ID (DVID), to seperate match sensor data from difference devices.
GPMF_ERR GPMF_DeviceName(GPMF_stream *gs, char *devicename_buf, uint32_t devicename_buf_size);	//return the current device name (DVNM), to seperate match sensor data from difference devices.

// Utilities for data types
uint32_t GPMF_SizeofType(GPMF_SampleType type);													// GPMF equivalent to sizeof(type)
uint32_t GPMF_ExpandComplexTYPE(char *src, uint32_t srcsize, char *dst, uint32_t *dstsize);		// GPMF using TYPE for cmple structure.  { float val[16],uin32_t flags; } has type "f[8]L", this tools expands to the simpler format "ffffffffL"
uint32_t GPMF_SizeOfComplexTYPE(char *typearray, uint32_t typestringlength);					// GPMF equivalent to sizeof(typedef) for complex types. 
GPMF_ERR GPMF_Reserved(uint32_t key);															// Test for a reverse GPMF Key, returns GPMF_OK is not reversed.

//Tools for extracting sensor data 
uint32_t GPMF_FormattedDataSize(GPMF_stream *gs);												//return the decompressed data size for the current GPMF KLV
uint32_t GPMF_ScaledDataSize(GPMF_stream *gs, GPMF_SampleType type);												//return the decompressed data size for the current GPMF KLV
GPMF_ERR GPMF_FormattedData(GPMF_stream *gs, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples);  // extract 'n' samples into local endian memory format.
GPMF_ERR GPMF_ScaledData(GPMF_stream *gs, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples, GPMF_SampleType type); // extract 'n' samples into local endian memory format										// return a point the KLV data.

//Tools for Compressed datatypes

typedef struct GPMF_codebook
{
	int16_t value;			//value to store
	uint8_t offset;			//0 to 128+ bytes to skip before store (leading zeros)
	uint8_t bits_used;		//1 to 16,32 (if escape code > 16 then read from bit-steam), 
	int8_t bytes_stored;	//bytes stored in value: 0, 1 or 2
	int8_t command;		//0 - OKAY,  -1 valid code, 1 - end
} GPMF_codebook;


GPMF_ERR GPMF_AllocCodebook(size_t *cbhandle);
GPMF_ERR GPMF_FreeCodebook(size_t cbhandle);
GPMF_ERR GPMF_DecompressedSize(GPMF_stream *gs, uint32_t *neededsize);
GPMF_ERR GPMF_Decompress(GPMF_stream *gs, uint32_t *localbuf, uint32_t localbuf_size);
GPMF_ERR GPMF_Free(GPMF_stream* gs); 


#ifdef __cplusplus
}
#endif

#endif
