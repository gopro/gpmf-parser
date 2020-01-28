/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 1.5.0
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

#include "GPMF_mp4reader.h"

int main(int argc, char *argv[])
{
	int32_t ret = GPMF_OK;

	// get file return data
	if (argc != 2)
	{
		printf("usage: %s <file_with_GPMF>\n", argv[0]);
		return -1;
	}

	size_t mp4 = OpenMP4Source(argv[1], STR2FOURCC("soun"), STR2FOURCC("in32"));
	mp4object* mp4o = (mp4object*)mp4;
	if (mp4o->trak_filepos)
		printf("trak offset %lld\n", mp4o->trak_filepos);
	else
		printf("trak type not found\n");

	CloseSource(mp4);

	return ret;
}
