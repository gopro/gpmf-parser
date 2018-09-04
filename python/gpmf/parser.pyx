from . import exceptions
from . import mp4reader
import pprint

cimport mp4reader
from cpython.mem cimport PyMem_Malloc, PyMem_Free
from cpython.string cimport PyString_FromStringAndSize


def parse_gpmf(filename):
    cdef GPMF_stream gp_stream
    cdef mp4reader.MP4Payload payload

    # Check there is some metadata in the file
    source = mp4reader.MP4Source(filename, mp4reader.MOV_GPMF_TRAK_TYPE,
                                 mp4reader.MOV_GPMF_TRAK_SUBTYPE)
    metadata_length = source.get_duration()
    if metadata_length <= 0:
        raise exceptions.NoMetadata('no metadata found in file: %s' % filename)

    # Read GPMF streams
    streams = {}
    for payload_index in range(source.get_num_payloads()):
        payload = source.get_payload(payload_index)
        err = GPMF_Init(&gp_stream, payload.buf, payload.size)
        if err != GPMF_OK:
            raise exceptions.StreamInitFailed(
                'payload gp_stream initialization failed'
            )
        while GPMF_FindNext(&gp_stream, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS) == GPMF_OK:
            if GPMF_SeekToSamples(&gp_stream) == GPMF_OK:
                _read_stream_chunk(&gp_stream, streams)

    return streams


cdef _read_stream_chunk(GPMF_stream *gp_stream, streams):
    cdef uint32_t key
    cdef uint32_t num_elements
    cdef uint32_t num_samples
    cdef size_t buf_size
    cdef double *buf
    cdef size_t value_index

    key = GPMF_Key(gp_stream)
    key_name = _key_to_str(key)
    num_elements = GPMF_ElementsInStruct(gp_stream)
    num_samples = GPMF_PayloadSampleCount(gp_stream)
    if num_samples:
        buf_size = num_samples * num_elements * sizeof(double)
        buf = <double*>PyMem_Malloc(buf_size)
        if not buf:
            raise MemoryError()
        try:
            GPMF_ScaledData(gp_stream, buf, buf_size, 0, num_samples, GPMF_TYPE_DOUBLE)
            stream = streams.setdefault(key_name, {})
            values = stream.setdefault('values', [])
            for value_index in range(buf_size):
                values.append(buf[value_index])
            if 'units' not in stream:
                stream['units'] = _find_stream_units(gp_stream)
            if 'name' not in stream:
                stream['name'] = _find_stream_name(gp_stream)
        finally:
            PyMem_Free(buf)


cdef _find_stream_units(GPMF_stream *gp_stream):
    cdef GPMF_stream find_stream
    cdef char *data
    cdef int ssize
    cdef uint32_t unit_samples
    cdef uint32_t sample_index

    units = []
    GPMF_CopyState(gp_stream, &find_stream)
    if (GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL) == GPMF_OK or
        GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL) == GPMF_OK):
        data = <char*>GPMF_RawData(&find_stream)
        ssize = GPMF_StructSize(&find_stream)
        unit_samples = GPMF_Repeat(&find_stream)
        for sample_index in range(unit_samples):
            unit_string = PyString_FromStringAndSize(data, ssize)
            unit_string = _decode_gp_string(unit_string)
            units.append(unit_string)
            data += ssize
    return units


cdef _find_stream_name(GPMF_stream *gp_stream):
    cdef GPMF_stream find_stream
    cdef char *data
    cdef uint32_t nchars

    GPMF_CopyState(gp_stream, &find_stream)
    if GPMF_FindPrev(&find_stream, GPMF_KEY_STREAM_NAME, GPMF_CURRENT_LEVEL) == GPMF_OK:
        data = <char*>GPMF_RawData(&find_stream)
        nchars = GPMF_Repeat(&find_stream)
        name = PyString_FromStringAndSize(data, nchars)
        name = _decode_gp_string(name)
    else:
        name = '[undefined]'
    return name


cdef object _key_to_str(uint32_t key):
    cdef char ret[4]
    cdef int i

    for i in range(4):
        ret[i] = (key >> (i * 8)) & 0xff
    return PyString_FromStringAndSize(ret, 4)


def _decode_gp_string(value):
    value = value.replace('\x00', '')
    value = value.decode('latin1')
    return value
