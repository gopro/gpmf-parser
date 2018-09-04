class GPMFError(Exception):
    pass


class NoMetadata(GPMFError):
    pass


class PayloadError(GPMFError):
    pass


class StreamInitFailed(GPMFError):
    pass
