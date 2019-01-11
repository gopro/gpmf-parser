/*! @file GPMF_bitstream.h
*
*  @brief GPMF Parser library include
* 
*  Some GPMF streams may contain compressed data, this is useful for high frequency 
*  sensor data that is highly correlated like IMU data.  The compression is Huffman 
*  coding of the delta between samples, with addition codewords for runs of zeros, 
*  and optional quantization. The compression scheme is similar to the Huffman coding 
*  in JPEG. As it intended for lossless compression (with quantize set to 1) it can 
*  only comrpess/decompress integer based streams.  
*
*  @version 1.2.0
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

#ifndef _GPMF_BITSTREAM_H
#define _GPMF_BITSTREAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct rlv {	// Codebook entries for arbitrary runs
	uint16_t size;		// Size of code word in bits
	uint16_t bits;		// Code word bits right justified
	uint16_t count;		// Run length for zeros
	int16_t  value;		// Value for difference
} RLV;

typedef const struct {
	int length;			// Number of entries in the code book
	RLV entries[39];
} RLVTABLE;

#define BITSTREAM_WORD_TYPE			uint16_t		// use 16-bit buffer for compression
#define BITSTREAM_WORD_SIZE			16				// use 16-bit buffer for compression
#define BITSTREAM_ERROR_OVERFLOW	1

#define BITMASK(n)		_bitmask[n]
#define _BITMASK(n)		((((BITSTREAM_WORD_TYPE )1 << (n))) - 1)

static const BITSTREAM_WORD_TYPE  _bitmask[] =
{
	_BITMASK(0),  _BITMASK(1),  _BITMASK(2),  _BITMASK(3),
	_BITMASK(4),  _BITMASK(5),  _BITMASK(6),  _BITMASK(7),
	_BITMASK(8),  _BITMASK(9),  _BITMASK(10), _BITMASK(11),
	_BITMASK(12), _BITMASK(13), _BITMASK(14), _BITMASK(15),
	0xFFFF
};



typedef struct bitstream
{
	int32_t error;				// Error parsing the bitstream
	int32_t bitsFree;			// Number of bits available in the current word
	uint8_t *lpCurrentWord;		// Pointer to next word in block
	int32_t wordsUsed;			// Number of words used in the block
	int32_t dwBlockLength;		// Number of entries in the block
	uint16_t wBuffer;			// Current word bit buffer
	uint16_t bits_per_src_word;	// Bitused in the source word. e.g. 's' = 16-bits
} BITSTREAM;


static RLVTABLE enchuftable = {
	39,
	{
	  { 1, 0b0,   1,  0 },	// m0
	  { 2, 0b10,   1,  1 },	// m1
	  { 4, 0b1100,   1,  2 },	// m2
	  { 5, 0b11011,   1,  3 },	// m3
	  { 5, 0b11101,   1,  4 },	// m4
	  { 6, 0b110100,   1,  5 },	// m5
	  { 6, 0b110101,   1,  6 },	// m6
	  { 6, 0b111110,   1,  7 },	// m7
	  { 7, 0b1110000,   1,  8 },	// m8
	  { 7, 0b1110011,   1,  9 },	// m9
	  { 7, 0b1111000,   1, 10 },	// m10
	  { 7, 0b1111001,   1, 11 },	// m11
	  { 7, 0b1111011,   1, 12 },	// m12
	  { 8, 0b11100100,   1, 13 },	// m13
	  { 8, 0b11100101,   1, 14 },	// m14
	  { 8, 0b11110100,   1, 15 },	// m15
	  { 9, 0b111000101,   1, 16 },	// m16
	  { 9, 0b111000110,   1, 17 },	// m17
	  { 9, 0b111101010,   1, 18 },	// m18
	  { 10, 0b1110001000,   1, 19 },	// m19
	  { 10, 0b1110001110,   1, 20 },	// m20
	  { 10, 0b1111010110,   1, 21 },	// m21
	  { 10, 0b1111111100,   1, 22 },	// m22
	  { 11, 0b11100010010,   1, 23 },	// m23
	  { 11, 0b11100011111,   1, 24 },	// m24
	  { 11, 0b11110101110,   1, 25 },	// m25
	  { 12, 0b111000100111,   1, 26 },	// m26
	  { 12, 0b111000111101,   1, 27 },	// m27
	  { 12, 0b111101011111,   1, 28 },	// m28
	  { 13, 0b1110001001101,   1, 29 },	// m29
	  { 13, 0b1110001111001,   1, 30 },	// m30
	  { 13, 0b1111010111101,   1, 31 },	// m31
	  { 14, 0b11100010011000,   1, 32 },	// m32
	  { 14, 0b11100011110000,   1, 33 },	// m33
	  { 14, 0b11110101111000,   1, 34 },	// m34
	  { 14, 0b11110101111001,   1, 35 },	// m35
	  { 15, 0b111000100110010,   1, 36 },	// m36
	  { 15, 0b111000100110011,   1, 37 },	// m37
	  { 15, 0b111000111100011,   1, 38 },	// m38
	}		  
};			  



static RLVTABLE enczerorunstable = {
	4,
	{
		{ 7, 0b1111110,  16,  0 },		// z16
		{ 8, 0b11111110,  32,  0 },		// z32
		{ 9, 0b111111111,  64,  0 },	// z64
		{ 10,0b1111111101, 128,  0 },	// z128
	}
};	

#define HUFF_ESC_CODE_ENTRY		0	
#define HUFF_END_CODE_ENTRY		1	
static RLVTABLE enccontrolcodestable = {
	2,
	{
		{ 16, 0b1110001111000100, 0, 0 },	// escape code for direct data <ESC><data>Continue
		{ 16, 0b1110001111000101, 0, 0 },	// end code.  Ends each compressed stream
	}
};



#ifdef __cplusplus
}
#endif

#endif
