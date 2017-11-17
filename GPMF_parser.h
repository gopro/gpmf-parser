/*! @file GPMF_parser.h
 * 
 *  @brief GPMF Parser library include
 * 
 *  @version 1.1.0
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

#ifndef _GPMF_PARSER_H
#define _GPMF_PARSER_H

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
} GPMF_stream;

typedef enum GPMF_ERROR
{
	GPMF_OK = 0,
	GPMF_ERROR_MEMORY,
	GPMF_ERROR_BAD_STRUCTURE,
	GPMF_ERROR_BUFFER_END,
	GPMF_ERROR_FIND,
	GPMF_ERROR_LAST,
	GPMF_ERROR_TYPE_NOT_SUPPORTED,
	GPMF_ERROR_SCALE_NOT_SUPPORTED,
	GPMF_ERROR_SCALE_COUNT,
	GPMF_ERROR_RESERVED
} GPMF_ERROR;

typedef enum GPMF_LEVELS
{
	GPMF_CURRENT_LEVEL = 0,
	GPMF_RECURSE_LEVELS
} GPMF_LEVELS;

#define GPMF_ERR	uint32_t

typedef enum
{
	GPMF_TYPE_STRING_ASCII = 'c', //single byte 'c' style character string
	GPMF_TYPE_SIGNED_BYTE = 'b',//single byte signed number
	GPMF_TYPE_UNSIGNED_BYTE = 'B', //single byte unsigned number
	GPMF_TYPE_SIGNED_SHORT = 's',//16-bit integer
	GPMF_TYPE_UNSIGNED_SHORT = 'S',//16-bit integer
	GPMF_TYPE_FLOAT = 'f', //32-bit single precision float (IEEE 754)
	GPMF_TYPE_FOURCC = 'F', //32-bit four character tag 
	GPMF_TYPE_SIGNED_LONG = 'l',//32-bit integer
	GPMF_TYPE_UNSIGNED_LONG = 'L', //32-bit integer
	GPMF_TYPE_Q15_16_FIXED_POINT = 'q', // Q number Q15.16 - 16-bit signed integer (A) with 16-bit fixed point (B) for A.B value (range -32768.0 to 32767.99998). 
	GPMF_TYPE_Q31_32_FIXED_POINT = 'Q', // Q number Q31.32 - 32-bit signed integer (A) with 32-bit fixed point (B) for A.B value. 
	GPMF_TYPE_SIGNED_64BIT_INT = 'j', //64 bit signed long
	GPMF_TYPE_UNSIGNED_64BIT_INT = 'J', //64 bit unsigned long	
	GPMF_TYPE_DOUBLE = 'd', //64 bit double precision float (IEEE 754)
	GPMF_TYPE_UTC_DATE_TIME = 'U', //128-bit ASCII Date + UTC Time format yymmddhhmmss.sss - 16 bytes ASCII (years 20xx covered)
	GPMF_TYPE_GUID = 'G', //128-bit ID (like UUID)

	GPMF_TYPE_COMPLEX = '?', //for sample with complex data structures, base size in bytes.  Data is either opaque, or the stream has a TYPE structure field for the sample.

	GPMF_TYPE_NEST = 0, // used to nest more GPMF formatted metadata 

} GPMF_SampleType;



#define MAKEID(a,b,c,d)			(((d&0xff)<<24)|((c&0xff)<<16)|((b&0xff)<<8)|(a&0xff))
#define STR2FOURCC(s)			((s[0]<<0)|(s[1]<<8)|(s[2]<<16)|(s[3]<<24))

#define BYTESWAP64(a)			(((a&0xff)<<56)|((a&0xff00)<<40)|((a&0xff0000)<<24)|((a&0xff000000)<<8) | ((a>>56)&0xff)|((a>>40)&0xff00)|((a>>24)&0xff0000)|((a>>8)&0xff000000) )
#define BYTESWAP32(a)			(((a&0xff)<<24)|((a&0xff00)<<8)|((a>>8)&0xff00)|((a>>24)&0xff))
#define BYTESWAP16(a)			((((a)>>8)&0xff)|(((a)<<8)&0xff00))
#define NOSWAP8(a)				(a)

#define GPMF_SAMPLES(a)			(((a>>24) & 0xff)|(((a>>16)&0xff)<<8))
#define GPMF_SAMPLE_SIZE(a)		(((a)>>8)&0xff)
#define GPMF_SAMPLE_TYPE(a)		(a&0xff)
#define GPMF_MAKE_TYPE_SIZE_COUNT(t,s,c)		((t)&0xff)|(((s)&0xff)<<8)|(((c)&0xff)<<24)|(((c)&0xff00)<<8)
#define GPMF_DATA_SIZE(a)		((GPMF_SAMPLE_SIZE(a)*GPMF_SAMPLES(a)+3)&~0x3)
#define GPMF_DATA_PACKEDSIZE(a)	((GPMF_SAMPLE_SIZE(a)*GPMF_SAMPLES(a)))
#define GPMF_VALID_FOURCC(a)	(((((a>>24)&0xff)>='a'&&((a>>24)&0xff)<='z') || (((a>>24)&0xff)>='A'&&((a>>24)&0xff)<='Z') || (((a>>24)&0xff)>='0'&&((a>>24)&0xff)<='9') || (((a>>24)&0xff)==' ') ) && \
								( (((a>>16)&0xff)>='a'&&((a>>24)&0xff)<='z') || (((a>>16)&0xff)>='A'&&((a>>16)&0xff)<='Z') || (((a>>16)&0xff)>='0'&&((a>>16)&0xff)<='9') || (((a>>16)&0xff)==' ') ) && \
								( (((a>>8)&0xff)>='a'&&((a>>24)&0xff)<='z') || (((a>>8)&0xff)>='A'&&((a>>8)&0xff)<='Z') || (((a>>8)&0xff)>='0'&&((a>>8)&0xff)<='9') || (((a>>8)&0xff)==' ') ) && \
								( (((a>>0)&0xff)>='a'&&((a>>24)&0xff)<='z') || (((a>>0)&0xff)>='A'&&((a>>0)&0xff)<='Z') || (((a>>0)&0xff)>='0'&&((a>>0)&0xff)<='9') || (((a>>0)&0xff)==' ') )) 
#define GPMF_KEY_TYPE(a)		(a&0xff)

#define PRINTF_4CC(k)			((k) >> 0) & 0xff, ((k) >> 8) & 0xff, ((k) >> 16) & 0xff, ((k) >> 24) & 0xff

 
typedef enum GPMFKey // TAG in all caps are GoPro preserved (are defined by GoPro, but can be used by others.)
{
	// Internal Metadata structure and formatting tags
	GPMF_KEY_DEVICE =			MAKEID('D','E','V','C'),//DEVC - nested device data to speed the parsing of multiple devices in post 
	GPMF_KEY_DEVICE_ID =		MAKEID('D','V','I','D'),//DVID - unique id per stream for a metadata source (in camera or external input) (single 4 byte int)
	GPMF_KEY_DEVICE_NAME =		MAKEID('D','V','N','M'),//DVNM - human readable device type/name (char string)
	GPMF_KEY_STREAM =			MAKEID('S','T','R','M'),//STRM - nested channel/stream of telemetry data
	GPMF_KEY_STREAM_NAME =		MAKEID('S','T','N','M'),//STNM - human readable telemetry/metadata stream type/name (char string)
	GPMF_KEY_SI_UNITS =			MAKEID('S','I','U','N'),//SIUN - Display string for metadata units where inputs are in SI units "uT","rad/s","km/s","m/s","mm/s" etc.
	GPMF_KEY_UNITS =			MAKEID('U','N','I','T'),//UNIT - Freedform display string for metadata units (char sting like "RPM", "MPH", "km/h", etc)
	GPMF_KEY_SCALE =			MAKEID('S','C','A','L'),//SCAL - divisor for input data to scale to the correct units.
	GPMF_KEY_TYPE =				MAKEID('T','Y','P','E'),//TYPE - Type define for complex data structures
	GPMF_KEY_TOTAL_SAMPLES =	MAKEID('T','S','M','P'),//TOTL - Total Sample Count including the current payload 	
	GPMF_KEY_TICK =				MAKEID('T','I','C','K'),//TICK - Used for slow data. Beginning of data timing in milliseconds. 
	GPMF_KEY_TOCK =				MAKEID('T','O','C','K'),//TOCK - Used for slow data. End of data timing in milliseconds. 
	GPMF_KEY_EMPTY_PAYLOADS =	MAKEID('E','M','P','T'),//EMPT - Payloads that are empty since the device start (e.g. BLE disconnect.)
	GPMF_KEY_REMARK =			MAKEID('R','M','R','K'),//RMRK - addcing comments to the bitstream (debugging)

	GPMF_KEY_END = 0//(null)
} GPMFKey;



// Prepare GPMF data 
GPMF_ERR GPMF_Init(GPMF_stream *gs, uint32_t *buffer, int datasize);							//Initialize a GPMF_stream for parsing a particular buffer.
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
uint32_t GPMF_Type(GPMF_stream *gs);															//return the current Type (GPMF_Type)
uint32_t GPMF_StructSize(GPMF_stream *gs);														//return the current sample structure size
uint32_t GPMF_Repeat(GPMF_stream *gs);															//return the current repeat or the number of samples of this structure
uint32_t GPMF_PayloadSampleCount(GPMF_stream *gs);														//return the current number of samples of this structure, supporting multisample entries.
uint32_t GPMF_ElementsInStruct(GPMF_stream *gs);												//return the current number elements within the structure (e.g. 3-axis gyro)
uint32_t GPMF_RawDataSize(GPMF_stream *gs);														//return the data size for the current GPMF KLV 
void *   GPMF_RawData(GPMF_stream *gs);															//return a pointer the KLV data (which is Bigendian if the type is known.)

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
GPMF_ERR GPMF_FormattedData(GPMF_stream *gs, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples);  // extract 'n' samples into local endian memory format.
GPMF_ERR GPMF_ScaledData(GPMF_stream *gs, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples, GPMF_SampleType type); // extract 'n' samples into local endian memory format										// return a point the KLV data.






#ifdef __cplusplus
}
#endif

#endif
