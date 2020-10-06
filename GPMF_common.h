/*! @file GPMF_parser.h
 * 
 *  @brief GPMF Parser library include
 * 
 *  @version 2.1.0
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

#ifndef _GPMF_COMMON_H
#define _GPMF_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GPMF_ERROR
{
	GPMF_OK = 0,
	GPMF_ERROR_MEMORY,				// NULL Pointer or insufficient memory 
	GPMF_ERROR_BAD_STRUCTURE,		// Non-complaint GPMF structure detected
	GPMF_ERROR_BUFFER_END,			// reached the end of the provided buffer
	GPMF_ERROR_FIND,				// Find failed to return the requested data, but structure is valid.
	GPMF_ERROR_LAST,				// reached the end of a search at the current nest level
	GPMF_ERROR_TYPE_NOT_SUPPORTED,	// a needed TYPE tuple is missing or has unsupported elements.
	GPMF_ERROR_SCALE_NOT_SUPPORTED, // scaling for an non-scaling type, e.g. scaling a FourCC field to a float.
	GPMF_ERROR_SCALE_COUNT,			// A SCAL tuple has a mismatching element count.
	GPMF_ERROR_UNKNOWN_TYPE,		// Potentially valid data with a new or unknown type.
	GPMF_ERROR_RESERVED				// internal usage
} GPMF_ERROR;

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
	GPMF_TYPE_STRING_UTF8 = 'u', //UTF-8 formatted text string.  As the character storage size varies, the size is in bytes, not UTF characters.
	GPMF_TYPE_UTC_DATE_TIME = 'U', //128-bit ASCII Date + UTC Time format yymmddhhmmss.sss - 16 bytes ASCII (years 20xx covered)
	GPMF_TYPE_GUID = 'G', //128-bit ID (like UUID)

	GPMF_TYPE_COMPLEX = '?', //for sample with complex data structures, base size in bytes.  Data is either opaque, or the stream has a TYPE structure field for the sample.
	GPMF_TYPE_COMPRESSED = '#', //Huffman compression STRM payloads.  4-CC <type><size><rpt> <data ...> is compressed as 4-CC '#'<new size/rpt> <type><size><rpt> <compressed data ...>

	GPMF_TYPE_NEST = 0, // used to nest more GPMF formatted metadata 

	/* ------------- Internal usage only ------------- */
	GPMF_TYPE_EMPTY = 0xfe, // used to distinguish between grouped metadata (like FACE) with no data (no faces detected) and an empty payload (FACE device reported no samples.)
	GPMF_TYPE_ERROR = 0xff // used to report an error
} GPMF_SampleType;



#define MAKEID(a,b,c,d)			(((d&0xff)<<24)|((c&0xff)<<16)|((b&0xff)<<8)|(a&0xff))
#define STR2FOURCC(s)			((s[0]<<0)|(s[1]<<8)|(s[2]<<16)|(s[3]<<24))

#define BYTESWAP64(a)			(((a&0xff)<<56)|((a&0xff00)<<40)|((a&0xff0000)<<24)|((a&0xff000000)<<8) | ((a>>56)&0xff)|((a>>40)&0xff00)|((a>>24)&0xff0000)|((a>>8)&0xff000000) )
#define BYTESWAP32(a)			(((a&0xff)<<24)|((a&0xff00)<<8)|((a>>8)&0xff00)|((a>>24)&0xff))
#define BYTESWAP16(a)			((((a)>>8)&0xff)|(((a)<<8)&0xff00))
#define BYTESWAP2x16(a)			(((a>>8)&0xff)|((a<<8)&0xff00)|((a>>8)&0xff0000)|((a<<8)&0xff000000))
#define NOSWAP8(a)				(a)

#define GPMF_SAMPLES(a)			(((a>>24) & 0xff)|(((a>>16)&0xff)<<8))
#define GPMF_SAMPLE_SIZE(a)		(((a)>>8)&0xff)
#define GPMF_SAMPLE_TYPE(a)		(a&0xff)
#define GPMF_MAKE_TYPE_SIZE_COUNT(t,s,c)		((t)&0xff)|(((s)&0xff)<<8)|(((c)&0xff)<<24)|(((c)&0xff00)<<8)
#define GPMF_DATA_SIZE(a)		((GPMF_SAMPLE_SIZE(a)*GPMF_SAMPLES(a)+3)&~0x3)
#define GPMF_DATA_PACKEDSIZE(a)	((GPMF_SAMPLE_SIZE(a)*GPMF_SAMPLES(a)))
#define GPMF_VALID_FOURCC(a)	(((((a>>24)&0xff)>='a'&&((a>>24)&0xff)<='z') || (((a>>24)&0xff)>='A'&&((a>>24)&0xff)<='Z') || (((a>>24)&0xff)>='0'&&((a>>24)&0xff)<='9') || (((a>>24)&0xff)==' ') ) && \
								( (((a>>16)&0xff)>='a'&&((a>>16)&0xff)<='z') || (((a>>16)&0xff)>='A'&&((a>>16)&0xff)<='Z') || (((a>>16)&0xff)>='0'&&((a>>16)&0xff)<='9') || (((a>>16)&0xff)==' ') ) && \
								( (((a>>8)&0xff)>='a'&&((a>>8)&0xff)<='z') || (((a>>8)&0xff)>='A'&&((a>>8)&0xff)<='Z') || (((a>>8)&0xff)>='0'&&((a>>8)&0xff)<='9') || (((a>>8)&0xff)==' ') ) && \
								( (((a>>0)&0xff)>='a'&&((a>>0)&0xff)<='z') || (((a>>0)&0xff)>='A'&&((a>>0)&0xff)<='Z') || (((a>>0)&0xff)>='0'&&((a>>0)&0xff)<='9') || (((a>>0)&0xff)==' ') )) 

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
	GPMF_KEY_MATRIX =			MAKEID('M','T','R','X'),//MTRX - 2D matrix for any sensor calibration.
	GPMF_KEY_ORIENTATION_IN =	MAKEID('O','R','I','N'),//ORIN - input 'n' channel data orientation, lowercase is negative, e.g. "Zxy" or "ABGR".
	GPMF_KEY_ORIENTATION_OUT =	MAKEID('O','R','I','O'),//ORIO - output 'n' channel data orientation, e.g. "XYZ" or "RGBA".
	GPMF_KEY_SCALE =			MAKEID('S','C','A','L'),//SCAL - divisor for input data to scale to the correct units.
	GPMF_KEY_TYPE =				MAKEID('T','Y','P','E'),//TYPE - Type define for complex data structures
	GPMF_KEY_TOTAL_SAMPLES =	MAKEID('T','S','M','P'),//TOTL - Total Sample Count including the current payload 	
	GPMF_KEY_TICK =				MAKEID('T','I','C','K'),//TICK - Beginning of data timing (arrival) in milliseconds. 
	GPMF_KEY_TOCK =				MAKEID('T','O','C','K'),//TOCK - End of data timing (arrival)  in milliseconds. 
	GPMF_KEY_TIME_OFFSET =      MAKEID('T','I','M','O'),//TIMO - Time offset of the metadata stream that follows (single 4 byte float)
	GPMF_KEY_TIMING_OFFSET =    MAKEID('T','I','M','O'),//TIMO - duplicated, as older code might use the other version of TIMO
	GPMF_KEY_TIME_STAMP =		MAKEID('S','T','M','P'),//STMP - Time stamp for the first sample. 
	GPMF_KEY_TIME_STAMPS =		MAKEID('S','T','P','S'),//STPS - Stream of all the timestamps delivered (Generally don't use this. This would be if your sensor has no peroidic times, yet precision is required, or for debugging.) 
	GPMF_KEY_PREFORMATTED =		MAKEID('P','F','R','M'),//PFRM - GPMF data
	GPMF_KEY_TEMPERATURE_C =	MAKEID('T','M','P','C'),//TMPC - Temperature in Celsius
	GPMF_KEY_EMPTY_PAYLOADS =	MAKEID('E','M','P','T'),//EMPT - Payloads that are empty since the device start (e.g. BLE disconnect.)
	GPMF_KEY_QUANTIZE =			MAKEID('Q','U','A','N'),//QUAN - quantize used to enable stream compression - 1 -  enable, 2+ enable and quantize by this value
	GPMF_KEY_VERSION =			MAKEID('V','E','R','S'),//VERS - version of the metadata stream (debugging)
	GPMF_KEY_FREESPACE =		MAKEID('F','R','E','E'),//FREE - n bytes reserved for more metadata added to an existing stream
	GPMF_KEY_REMARK =			MAKEID('R','M','R','K'),//RMRK - adding comments to the bitstream (debugging)

	GPMF_KEY_END = 0//(null)
} GPMFKey;



#ifdef __cplusplus
}
#endif

#endif
