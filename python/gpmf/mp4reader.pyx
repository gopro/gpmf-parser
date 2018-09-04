from . import exceptions

from libc.stdint cimport uint32_t
from libc.stdlib cimport free
cimport parser


cdef class MP4Source:

    cdef size_t handle
    cdef unicode filename

    def __cinit__(self, filename, uint32_t traktype, uint32_t subtype):
        filename = unicode(filename)
        self.handle = OpenMP4Source(filename, traktype, subtype)
        self.filename = filename

    def __dealloc__(self):
        CloseSource(self.handle)

    def get_duration(self):
        return GetDuration(self.handle)

    def get_num_payloads(self):
        return GetNumberPayloads(self.handle)

    def get_payload(self, index):
        return MP4Payload(self, index)


cdef class MP4Payload:

    def __cinit__(self, MP4Source mp4_source, uint32_t index):
        # XXX: passing NULL as lastpayload skips the realloc() optimization,
        # but also makes the memory management cleaner
        self.buf = GetPayload(mp4_source.handle, NULL, index)
        if self.buf == NULL:
            raise exceptions.PayloadError('could not find payload #%s '
                                          'in file: %s' %
                                          (index, mp4_source.filename))
        self.size = GetPayloadSize(mp4_source.handle, index)
        err = GetPayloadTime(mp4_source.handle, index, &self.start_time,
                             &self.end_time)
        if err != parser.GPMF_OK:
            raise exceptions.PayloadError('error retrieving payload '
                                          'times in file: %s' %
                                          mp4_source.filename)

    def __dealloc__(self):
        FreePayload(self.buf)
