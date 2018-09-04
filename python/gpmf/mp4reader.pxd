from libc.stdint cimport uint32_t


cdef extern from "demo/GPMF_mp4reader.h":

    cpdef int MOV_GPMF_TRAK_TYPE
    cpdef int MOV_GPMF_TRAK_SUBTYPE

    enum GPMF_stream:
        pass

    size_t OpenMP4Source(char *filename, uint32_t traktype, uint32_t subtype)
    size_t OpenMP4SourceUDTA(char *filename)
    void CloseSource(size_t handle)
    float GetDuration(size_t handle)
    uint32_t GetNumberPayloads(size_t handle)
    uint32_t *GetPayload(size_t handle, uint32_t *lastpayload, uint32_t index)
    void SavePayload(size_t handle, uint32_t *payload, uint32_t index)
    void FreePayload(uint32_t *lastpayload)
    uint32_t GetPayloadSize(size_t handle, uint32_t index)
    uint32_t GetPayloadTime(size_t handle, uint32_t index, float* in_, float *out)
    double GetGPMFSampleRate(size_t handle, uint32_t fourcc, uint32_t flags)
    double GetGPMFSampleRateAndTimes(size_t handle, GPMF_stream *gs, double lastrate, uint32_t index, double *in_, double *out)


cdef class MP4Payload:

    cdef uint32_t* buf
    cdef uint32_t size
    cdef float start_time
    cdef float end_time
