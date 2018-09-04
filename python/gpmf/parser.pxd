from libc.stdint cimport uint32_t


cdef extern from "GPMF_parser.h":

    struct GPMF_stream:
        pass

    ctypedef uint32_t GPMF_ERR

    enum GPMF_ERROR:
        GPMF_OK

    enum GPMF_SampleType:
        pass

    enum GPMF_LEVELS:
        pass

    enum GPMFKey:
        GPMF_KEY_STREAM
        GPMF_KEY_UNITS
        GPMF_KEY_SI_UNITS
        GPMF_KEY_STREAM_NAME

    enum GPMF_LEVELS:
        GPMF_CURRENT_LEVEL
        GPMF_RECURSE_LEVELS

    enum GPMF_SampleType:
        GPMF_TYPE_DOUBLE

    GPMF_ERR GPMF_Init(GPMF_stream *gs, uint32_t *buffer, int datasize)
    GPMF_ERR GPMF_ResetState(GPMF_stream *gs)
    GPMF_ERR GPMF_CopyState(GPMF_stream *src, GPMF_stream *dst)
    GPMF_ERR GPMF_Validate(GPMF_stream *gs, GPMF_LEVELS recurse)

    GPMF_ERR GPMF_Next(GPMF_stream *gs, GPMF_LEVELS recurse)
    GPMF_ERR GPMF_FindPrev(GPMF_stream *gs, uint32_t fourCC, GPMF_LEVELS recurse)
    GPMF_ERR GPMF_FindNext(GPMF_stream *gs, uint32_t fourCC, GPMF_LEVELS recurse)
    GPMF_ERR GPMF_SeekToSamples(GPMF_stream *gs)

    uint32_t GPMF_Key(GPMF_stream *gs)
    uint32_t GPMF_Type(GPMF_stream *gs)
    uint32_t GPMF_StructSize(GPMF_stream *gs)
    uint32_t GPMF_Repeat(GPMF_stream *gs)
    uint32_t GPMF_PayloadSampleCount(GPMF_stream *gs)
    uint32_t GPMF_ElementsInStruct(GPMF_stream *gs)
    uint32_t GPMF_RawDataSize(GPMF_stream *gs)
    void *   GPMF_RawData(GPMF_stream *gs)

    uint32_t GPMF_NestLevel(GPMF_stream *gs)
    uint32_t GPMF_DeviceID(GPMF_stream *gs)
    GPMF_ERR GPMF_DeviceName(GPMF_stream *gs, char *devicename_buf, uint32_t devicename_buf_size)

    uint32_t GPMF_SizeofType(GPMF_SampleType type)
    uint32_t GPMF_ExpandComplexTYPE(char *src, uint32_t srcsize, char *dst, uint32_t *dstsize)
    uint32_t GPMF_SizeOfComplexTYPE(char *typearray, uint32_t typestringlength)
    GPMF_ERR GPMF_Reserved(uint32_t key)

    GPMF_ERR GPMF_FormattedData(GPMF_stream *gs, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples)
    GPMF_ERR GPMF_ScaledData(GPMF_stream *gs, void *buffer, uint32_t buffersize, uint32_t sample_offset, uint32_t read_samples, GPMF_SampleType type)
