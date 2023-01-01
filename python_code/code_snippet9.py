from collections import defaultdict
from functools import partial
import logging
from typing import (
    Any,
    Callable,
    DefaultDict,
    Dict,
    List,
    NamedTuple,
    Sequence,
    Tuple,
    Union,
    cast,
)

try:
    from numpy.typing import ArrayLike
except ImportError:
    ArrayLike = Any

import numpy as np
from xarray.backends.locks import SerializableLock

from . import UNDEFINED, _Variable, WgribError
from .wgrib2 import MemoryBuffer, wgrib, free_files
from .inventory import MetaData
from .template import Template

logger = logging.getLogger(__name__)

# wgrib2 returns C float arrays
DTYPE = np.dt