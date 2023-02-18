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
DTYPE = np.dtype("float32")


HeaderIndices = Tuple[int, ...]
FileIndex = DefaultDict[str, Dict[HeaderIndices, str]]  # file -> Dict
FileIndices = DefaultDict[str, FileIndex]  # variable name -> FileIndex

WGRIB2_LOCK = SerializableLock()


class Dataset(NamedTuple):
    dims: Dict[str, int]
    vars: Dict[str, _Variable]
    attrs: Dict[str, Any]


def find_1st(array, value):
    return np.nonzero(array == value)[0][0]


def build_file_index(
    items: Sequence[MetaData],
    template: Template,
) -> FileIndices:
    file_indices: FileIndices = defaultdict(cast(Callable, partial(defaultdict, dict)))
    for item in (i for i in items if template.item_match(i)):
        varname = template.item_to_varname(item)
        try:
            specs = template.var_specs[varname]
        except KeyError:
            logger.info("Variable {!s} not found in template, skipping".format(varname))
            continue
        time_coord = specs.time_coord
        level_coord = specs.level_coord
        fcst_time = item.end_ft - item.reftime
        header_indices: Tuple[int, ...] = ()
        found = True
        if time_coord in specs.dims:
            try:
                i = find_1st(template.coords[time_coord].data, fcst_time)
                header_indices = (i,)
            except IndexError:
                found = False
        else:
            if template.coords[time_coord].data != fcst_time:
                found = False
        if not found:
            logger.info(
                "Variable {:s} forecast time {!r} not found in template, "
                "skipping".format(varname, fcst_time)
            )
            continue
        if level_coord in specs.dims:
            try:
                i = find_1st(template.coords[level_coord].data, item.level_value)
                header_indices += (i,)
            except IndexError:
                logger.info(
                    "Variable {:s} level {!r} not found in template, "
                    "skipping".format(varname, item.level_value)
                )
                continue
        file_indices[varname][item.file][header_indices] = item.offset
    return file_indices


def expand_item(item: Sequence[Any], shape: T