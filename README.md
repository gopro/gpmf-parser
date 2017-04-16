# GPMF Introduction

General Purpose Metadata Format (although it originals are GoPro Metadata Format.)  The GPMF structured storage format was originally proposed to store high-frequency periodic sensor within a video file like an MP4.  Action cameras like that from GoPro have limited computing resources beyond that need to store video and audio, so any telemetry storage needed to be lightweight in computation, memory usage and storage bandwidth. While JSON and XML systems where initially considered, the burden of the embedded camera system was too great, so something simpler was needed. While the proposed GPMF structure could be used stand-alone, our intended implementation uses an additional time-indexed track with an MP4, and with an application marker within JPEG images. GPMF share a Key, Length, Value structure (KLV), similar to Quicktime atoms or Interchange File Format (IFF), but better for self-describing sensor data.  Problems solved:

* The contents of new Keys can be parsed without prior knowledge.
* Nested structures can be defined without &#39;Key&#39; dictionary.
* Structure prevents naming collisions between multiple sources.
* Nested structures allow for the communication of metadata for telemetry, such as scale, units, and data ranges etc.
* Somewhat human (engineer) readable (i.e. hex-editor friendly.)
* Timing and index for metadata can be stored within the wrapping MP4 of similar container format.

GoPro Metadata Format (GPMF) is a modified Key, Length, Value solution, with a 32-bit aligned payload, that is both compact, full extensible and somewhat human readable in a hex editor.  GPMF allows for dependent creation of new FourCC tags, without requiring central registration to define the contents and whether the data is in a nested structure.

## GPMF-parser

### Quick Start for Developers

#### Setup

Clone the project from Github (git clone https://github.com/gopro/gpmf-parser).

#### Sample Code

GPMF-parser.c and .h provide a payload decoder for any raw stream stored in compliant GPMF.  Extraction of the RAW GPMF from a video or image file is not covered by this tool.

```
#include <GPMF-parser.h>
GPMF_stream gs_stream;
if(GPMF_OK == GPMF_Init(&gs_stream, buffer_with_GPMF_data, size_of_the_buffer))
{
	do
	{
		switch(GPMF_Key(&gs_stream))
		{
		case STR2FOURCC(“ACCL”):
		  // Found accelerometer
		  samples = GPMF_Repeat(&gs_stream);
		  if(GPMF_OK == GPMF_ScaledData(&gs, temp_buffer, temp_buffersize, 0, samples, GPMF_TYPE_FLOAT)) 
		    {  /* Process scaled values */ }
		  break;
	  
		case STR2FOURCC(“cust”): 
		  // Found my custom data
		  samples = GPMF_Repeat(&gs_stream);
		  if(GPMF_OK == GPMF_FormattedData(&gs, temp_buffer, temp_buffersize, 0, samples)) 
		    { /* Process raw formatted data -- unscaled */ }
		  break;
	  
		default: // if you don’t know the Key you can skip to the next
		  break;
	}
} while (GPMF_OK == GPMF_Next(&gs_stream, GPMF_RECURVSE_LEVELS)); // Scan through all GPMF data
```

If you only want particular a piece of data

```
#include <GPMF-parser.h>
GPMF_stream gs_stream;
if(GPMF_OK == GPMF_Init(&gs_stream, buffer_with_GPMF_data, size_of_the_buffer))
{
  if(GPMF_OK == GPMF_FindNext(&gs_stream, STR2FOURCC("ACCL"), GPMF_RECURVSE_LEVELS))) 
   {
   	uint32_t key = GPMF_Key(&gs_stream);
   	char type = GPMF_Type(&gs_stream);
   	uint32_t samples = GPMF_Repeat(&gs_stream);
   	void *data = GPMF_RawData(&gs_stream);
	
	/* do your own byte-swapping and handling */	
   }
}
```

## GMFP Deeper Dive

## Definitions

| **Word** | **Definition** |
| --- | --- |
| Track | An MP4 container element used for storing video, audio or sub-titling over time |
| FourCC | The four character Key that marks a segment of data |
| Key | FourCC is the first element in Key-Length-Value 3-tuple |
| Length | This field describes the data that follows, including basic type, structural hints, sample counts |
| Value | Raw data to be stored in Big Endian |
| Device | Metadata sources below to devices, a camera my device and a connected BlueTooth sensor would be a separate device |
| Stream | Each metadata Device can have multiple streams on sensor data, e.g. Camera Device could have GPS, Accelerometer and Gyro streams |

### KLV Design

All data is Big Endian.

![](http://miscdata.com/gpmf/KLVDesign.png)

### FourCC

7-bit ASCII Key created for readability.  All uppercase fourCCs are reserved Keys, mixed case is for any third party ingested data.

A few Keys reserved for communicating structure, although only DEVC is required at the beginning of any GPMF payload.

| **FourCC** | **Definition** | **Comment** |
| --- | --- | --- |
| DEVC | unique device source for metadata | Each connected device starts with DEVC. A GoPro camera or Karma drone would have their own DEVC for nested metadata to follow. |
| DVID | device/track ID | Auto generated unique-ID for managing a large number of connect devices, camera, karma and external BLE devices |
| DVNM | device name | Display name of the device like &quot;Karma 1.0&quot;, this is for communicating to the user the data recorded, so it should be informative. |
| STRM | Nested signal stream of metadata/telemetry | Metadata streams are each nested with STRM |
| STNM | Stream name | Display name for a stream like &quot;GPS RAW&quot;, this is for communicating to the user the data recorded, so it should be informative. |
| RMRK | Comments for any stream | Add more human readable information about the stream |
| SCAL | Scaling factor (divisor) | Sensor data often needs to be scaled to be presented with the correct units. SCAL is a divisor. |
| SIUN | Standard Units (like SI) | If the data can be formatted in GPMF&#39;s standard units, this is best. E.g. acceleration as &quot;m/s²&quot;.  SIUN allows for simple format conversions. |
| UNIT | Display units | While SIUN is preferred, not everything communicates well via standard units. e.g. engine speed as &quot;RPM&quot; is more user friendly than &quot;rad/s&quot;. |
| TYPE | Typedefs for complex structures | Not everything has a simple repeating type. For complex structure TYPE is used to describe the data packed within each sample. |
| TSMP | Total Samples delivered | Internal field that counts all the sample delivered since record start, and is automatically computed. |
| EMPT | Empty payload count | Internal field that reports the number of payloads that contain no new data. TSMP and EMPT simplify the extraction of clock. |

### Length (type-size-repeat structure)

The tradition 32-bit &quot;Length&quot; is broken up to help describe the data that follows.

#### Structure Size

8-bits is used for a sample size, each sample is limited to 255 bytes or less.

#### Repeat

16-bits is used to indicate the number of samples in a GPMF payload, this is the Repeat field.  Struct Size and the Repeat allow for up to 16.7MB of data in a single KLV GPMF payload.

#### Type

The final 8-bits, Type, is used to describe the data format within the sample.  Just as FOURCC Keys are human readable, the TYPE is using a ASCII character to describe the data stored. A character &#39;f&#39; would describe float data, &#39;d&#39; for double precision, etc. All types are reserved, and are not end user definable.

Current types:

| **Type Char** | **Definition** | **Comment** |
| --- | --- | --- |
| b | single byte signed number | -128 to 127 |
| B | single byte unsigned number | 0 to 255 |
| c | single byte &#39;c&#39; style ASCII character string | Does not need to be NULL terminated as the size/repeat set the length |
| s | 16-bit signed number (int16\_t) | -32768 to 32768 |
| S | 16-bit unsigned number (uint16\_t) | 0 to 65536 |
| l | 32-bit signed unsigned number (int32\_t) |   |
| L | 32-bit unsigned unsigned number (uint32\_t) |   |
| d | 64-bit double precision float (IEEE 754) |   |
| f | 32-bit single precision float (IEEE 754) |   |
| F | 32-bit four character key -- FourCC |   |
| G | 128-bit ID (like UUID) |   |
| j | 64-bit signed unsigned number |   |
| J | 64-bit unsigned unsigned number |   |
| q | 32-bit Q Number Q15.16 | 16-bit signed integer (A) with 16-bit fixed point (B) for A.B value (range -32768.0 to 32767.99998) |
| Q | 64-bit Q Number Q31.32 | 32-bit signed integer (A) with 32-bit fixed point (B) for A.B value. |
| U | UTC Time 16-byte standard date and time string | Date + UTC Time format yymmddhhmmss.sss - (years 20xx covered) |
| ? | data structure is complex | Structure is defined with a preceding TYPE |
| null | Nested metadata | The data within is GPMF structured KLV data |
|   |   |   |

## Alignment and Storage

All GPMF data is 32-bit aligned and stored as big-endian.  For data types that are not 32-bit, they are packed in their native byte or short or complex structure storage size, and only padded at the very end to meet GPMF KLV alignment needs.  Storage of a single 1 byte, would have 3-pad bytes.

DEMO,  &#39;b&#39; 1  1,  &lt;byte value&gt; 0 0 0

![](http://miscdata.com/gpmf/demo1.png)

the same data type stored 15 times would have only a only byte pad at the end.

DEMO,  &#39;b&#39; 1  15,  &lt;15 bytes data&gt; 0


![](http://miscdata.com/gpmf/demo2.png)

Packed data will all maintain a 32-bit alignment between GPMF KLV 3-tuples.

DMO1, b  1 1, &lt;byte value&gt; 0 0 0 DMO2 b 1 15 &lt;values&gt; 0 DMO3 L 4 1 &lt;32-bit values&gt;


![](http://miscdata.com/gpmf/demo3.png)

While padding is shown as null values, any value can be used, as this data is simply ignored.

The packed data size with in a GPMF KLV is the structure size times the number of samples. The storage size is rounded up to the next 32-bit aligned size.

## Multiple axis sensor data

As sensor data like gyro and accelerometer commonly have three (or more) axes of the same data type, the combination of Type and Structure Size, will indicate the type of data within. Three axis GYRO data could have a Type of &#39;s&#39; (short 16-bit signed integer) with a Structure size of 6.  As the size of the Type is known, the number of axes in each sample is Structure size / sizeof (Type).  An examples of 6 samples of a 6 byte x,y,z structure GYRO data is shown here:


![](http://miscdata.com/gpmf/demo4.png)

## GPMF Nesting

Standalone GPMF does little for communicating the relationship between data, so that we need to nest so that metadata can describe other data.  Type of null is the flag that indicates the data field is any other GPMF KLV series.  We use nesting to describe any telemetry device like follows:

```
DEVC null 4 7
   DVID L 4 1 0x1001
   DVNM c 1 6 Camera
 ```

This is a valid nested GPMF structure. DEVC describe 4\*7 = 28 bytes of data, which are packed and aligned GPMF KLV values describing a camera device with a Device ID and a Device Name.

![](http://miscdata.com/gpmf/demo5.png)

### Property Hierarchy

Metadata, data that applies to other data, is forward looking within each nested GPMF entry. In the previous example the Device ID is applied to the Device Name, as they&#39;re part of the same nest and Device ID came before Device Name. This order of properties is particularly import when KLV 3-tuples modifies the meaning of data to follow in the same nest level, which is how SCAL and TYPE are applied.  Several modify properties can be transmitted, each adding metadata to modify the meaning of the **last** KLV in the nest (at this nest level.)  The SCAL key is used as sensors that measure physical properties typically output integers that must be scaled to produce a real-world value.  This scaling converting raw data to float or Q-numbers (fixed point float) could be performed in the device, but is often stored more efficiently as the original sensor values and a scale property. These are equivalent:

```
STRM null <size><repeat>
   ACCL 'f' 12 100  <100 samples of x,y,z accelerometer data as 32-bit floats>
```
 
```
STRM null <size><repeat>
   SCAL 's' 2 1 scale   
   ACCL 's' 6 100 <100 samples of x,y,z accelerometer data as 16-bit shorts>
```

The second data stream is almost half the size of the first (1216 vs 628 bytes) for the same resulting precision.

When adding units, the SCAL doesn&#39;t apply to SUIN, but only the ACCL the latest KLV in the stream&#39;s (STRM) nest.

```
STRM null <size><repeat> 
   SCAL 's' 2 1 scale   
   SIUN 'c' 4 1 "m/s²"   
   ACCL 's' 6 100 <100 samples of x,y,z accelerometer data as 16-bit shorts>
```

Note: The SI unit of &quot;m/s²&quot; is applied to each x,y,z axis, there is no need to declare the unit as

```
SIUN 'c' 4 3 "m/s²","m/s²","m/s²"
```

A complete stream from a device could be:

```
STRM null <size><repeat>
   TSMP 'L' 4 1  196   
   STNM 'c' 50 1  "Accelerometer (up/down, right/left, forward/back)"   
   TMPC 'f' 4 1  56.0723   
   SIUN 'c' 4 1  "m/s²"   
   SCAL 's' 2 1  418   
   ACCL 's' 6 100  4418, -628, -571, ...
```

Including a stream name to improve readability and TMPC for the temperature of the device that is collecting the accelerometer data.

## Multi-sample and Non-periodic data.

Virtual sensors, CV or computationally extracted metadata will become a common storage type.  The data form this new sources could produce multiple samples for a given time (5 faces in this frame) and the results may be delayed in delivering their findings (frame 1000 has 7 faces, yet is was reported on frame 1003.) Both the potential for delay timing and multiple samples requires custom storage.

In the example below is for a fast, periodic (once per frame), face detection algorithm (no delays):

```
STRM null <size><repeat> 
 TSMP 'L' 4 1 196 
 STNM 'c' 50 1 "Face bounding boxes (age, gender, x1,y1,x2,y2)" 
 TYPE 'c' 1 6  "SSffff", 
 FACE '?' 20 3 <face1, face2, face3> 
 FACE '?' 20 4 <face1, face2, face3, face4> 
 FACE '?' 20 0  
 FACE '?' 20 2 <face1, face2> 
 ...
```

The timing information is extracted just like all other sensor, yet the multiple entries of &quot;FACE&quot; vertically represent samples over time, and &quot;faceX&quot; are multiple samples at the same time. When no FACE are discovered, &quot;FACE ? 20 0&quot; is used to preserve the timing, just as GYRO report zeros when then camera is stationary.

If the data where to occur with a significantly slower algorithm that is not periodic, say the first detection took 300ms, the second 400ms,, the third 100ms, the last 250ms, the timing relationship to the source frame would be lost.  While most of GPMF data can rely of the timing provided by MP4 indexing, to handling delayed and aperiodic data introduces TICK and TOCK to make the in and out times (in time is the beginning of the processing, out-time the end.

```
DEVC null <size0><repeat0> 
 DVID 'L' 4 1 1001
 DVNM 'c' 6 1 "Camera" 
 TICK 'L' 4 1 10140 
 STRM null <size><repeat>  
   TSMP 'L' 4 1 196   
   STNM 'c' 50 1 "Face bounding boxes (x1,y1,x2,y2,age,gender,flags,confidence)"   
   TYPE 'c' 1  6 "ffffBBBB",   
   FACE null <size1><repeat1>
     TICK 'L' 4 1 10023     
     TOCK 'L' 4 1 10320     
     FACE '?' 20 3 <face1, face2, face3>     
   FACE null <size2><repeat2>   
     TICK 'L' 4 1 10347     
     TOCK 'L' 4 1 10751     
     FACE '?' 20 3 <face1, face2, face3, face4>     
   FACE null <size3><repeat3>   
     TICK 'L' 4 1 10347     
     TOCK 'L' 4 1 10751     
     FACE '?' 20 0     
   FACE null <size4><repeat4>   
    TICK 'L' 4 1 10347    
    TOCK 'L' 4 1 11005    
    FACE '?' 20 3 <face1, face2>   
```

As the CV processing in this example can take time, it will be common for the processing to begin before the payload frame it which it is written.  The first FACE samples begin their processing at 10023, yet the payload for normal sample data began at 10140 (within the top DEVC structure).

## Standard Units for physical properties supported by SUIN

### Base Units

| length | meter | m | SI Unit |
| --- | --- | --- | --- |
| mass | gram       | g | SI Unit |
| time | second | s | SI Unit |
| electric current | ampere | A | SI Unit |
| temperature       | kelvin | K | SI Unit |
| luminous intensity | candela | cd | SI Unit |
| magnetic flux density | tesla | T | non-SI |
| angle | radian | rad | non-SI |
| temperature | Celsius | °C | non-SI |
| frequency | hertz | Hz | non-SI |
| memory | Byte | B | non-SI |

### Modifiers supported

| p (pico) | 10x-12
 |
| --- | --- |
| n (nano) | 10x^9 |
| µ (micro) | 10x^6 |
| m (milli) | 10x^3 |
| k (kilo) | 10^3 (1000) |
| M (mega) | 10^6 (1,000,000) |
| G (Giga) | 10^9 (1,000,000,000) |
| T (Tera) | 10^12 |

##### Common Properties in SIUN

| speed, velocity | meter per second | m/s |
| --- | --- | --- |
| acceleration | meter per second squared   | m/s^2 |
| luminance | candela per square meter | cd/m^2 |
| gyro | radians per second | rad/s |
| Compass readings | milli Tesla or micro Tesla | mT or µT |
| distance | kilometers, millimeter, etc | km or mm or .. |
| memory used/free | MegaByte, GigiaByte, etc | kB, MB, GB ... |
| frequency | kiliHertz, GigaHertz, etc | kHz, GHz, ... |

##### Special ASCII Characters

As the SIUN is declared as an ASCII the character for degrees, squared, cubed and micro use the single byte values: 0xB0 (°), 0xB2 (²), 0xB3 (³) and 0xB5 (µ).

**Complex structures**

Not all telemetry data can be described as an array of a single basic type. To support complex structures, a TYPE field is added to the property hierarchy.   TYPE is always defined with &#39;c&#39; and used the same ASCII Type Chars to describe a complex structure.  Examples structure:

```
typedef struct
{
   float field_strength;
   short x_offset, y_offset;
   unsigned long status_flags;
} myDeviceData; //myDD
```

The TYPE for the structure be a &#39;f&#39;,&#39;s&#39;,&#39;s&#39;,&#39;L&#39; (float, short, short, unsigned long) is declared as follows:

```
STRM null <size><repeat> 
   TYPE 'c' 1 4 "fssL"
   myDD '?' 12 <repeat> <n samples of myDeviceData>
```

The &#39;?&#39; for the type field in &#39;myDD&#39; is used to indicate a complex structure type.

As this will likely have units that differ per parameter, we need to send units for each element of the complex structure

```
   TYPE 'c' 1 4 "fssL"
   SIUN 'c' 2 4 "µTmmmm " // for units µT mm mm and NONE
   myDD '?' 12 <repeat> <n samples of myDeviceData>
```

The same for scale, each unit will likely have a different scale. If no scale is required use 1, likely the flags field will not be scaled.

```
   TYPE 'c' 1 4 "fssL"
   SIUN 'c' 3 4 "µT mm mm " // for units µT mm mm and NONE, here padded for readability (optional)
   SCAL 's' 2 4 1000000, 1000, 1000, 1
   myDD '?' 12 <repeat> <n samples of myDeviceData>
```

Arrays can be defined within the string TYPE as follows:

```
typedef struct
{
   float farray[8];
   unsigned long flags;
} myDeviceData; //myDD
```

The use of &#39;[n]&#39; indicates the field is repeated n times.  So an array of eight floats as described with &quot;f[8]&quot;

```
TYPE 'c' 1 5 "f[8]L"
  myDD '?' 36 <repeat> <n samples of myDeviceData>
```

## Sticky Metadata

The metadata writing API should have a mechanism for creating and writing &quot;sticky&quot; metadata.  Data of type sticky doesn&#39;t need to be re-transmitted if it is not changing, and it will only store the last value it is does change.  Sticky data is good for slow changing properties like the sensor&#39;s temperature, or any data that is sampled near the payload frequency or slower.  Any metadata the modifies the meaning of sensor data should be written as sticky data: all TYPEs, SUINs, SCALs, naming fields (DVNM, STNM) and comments that are not changing over time.  This data needs to be within each payload in the event that the file is cut in to smaller segments, each segment must contain all relevant metadata so that no meaning is lost in the extracted clip.

## Style Guide

The addition of structure is not to make device vendor&#39;s life more difficult, but to communicate the meaning of data to a future reader, that may not even be aware of the device used in the original capture. The first useful metadata is a human readable name of the device. While DVNM (DeviceName) &quot;Camera&quot; is in the current metadata, saying &quot;GoPro Hero5 Black&quot; would be much better.  Having a stream (STRM) with ACCL data, doesn&#39;t communicate what it contains, when adding a STNM or RMRK with &quot;Accelerometer (up/down, right/left, forward/back)&quot; adds a lot of clarity to the future data reader.  SUIN, UNIT, SCAL and even TYPE is completely optional if you don&#39;t intend anyone other than for your own tools to process this data, but it is so much better to allow many tools to extract and process all data.  Use of a standard unit (SIUN) allows downstream tools to convert accurately from m/s to MPH or kmH etc. under the end users control.

## MP4 Implementation

GPMF data is stored much like every other media track within the MP4, where the indexing and offsets are presented in the MP4 structure, not the data payload.  GPMF storage is most similar to PCM audio, as it contains RAW uncompressed sensor data, however the sample rate for the track is for the GPMF payload, not the data within.  GPMF might be stored at 1Hz (stored in the track description), but contain gyro data at 400Hz, accelerometer at 200Hz and GPS at 18Hz (HERO5 launch data-rates.)

### GoPro&#39;s MP4 Structure

Telemetry carrying MP4 files will have a minimum of four tracks: Video, audio, timecode and telemetry (GPMF.)  A fifth track (&#39;SOS&#39;) is used in file recovery in HERO4 and HERO5, can be ignored.

File structure:
```
  ftyp [type ‘mp41’]
  mdat [all the data for all tracks are interleaved]
  moov [all the header/index info]
    ‘trak’ subtype ‘vide’, name “GoPro AVC”, H.264 video data 
    ‘trak’ subtype ‘soun’, name “GoPro AAC”, to AAC audio data
    ‘trak’ subtype ‘tmcd’, name “GoPro TCD”, starting timecode (time of day as frame since midnight)
    ‘trak’ subtype ‘meta’, name “GoPro MET”, GoPro Metadata (telemetry)
 ```
