import os.path as op
import py.path

import pytest


here = op.dirname(__file__)


@pytest.fixture
def samples_dir():
    """
    Return a :class:`py.path.local` object pointing to the "samples" directory.
    """
    return py.path.local(here).join('..', '..', 'samples')
