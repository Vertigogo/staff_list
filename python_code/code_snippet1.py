
try:
    from functools import recursive_repr as _recursive_repr
    # pylint: disable=invalid-name
    _recursive_repr_if_available = _recursive_repr()
except ImportError:
    def _recursive_repr_if_available(function):
        return function


__all__ = ('compose',)
__version__ = '1.1.1'


def _name(obj):